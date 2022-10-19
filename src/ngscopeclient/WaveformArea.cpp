/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of WaveformArea
 */
#include "ngscopeclient.h"
#include "WaveformArea.h"
#include "MainWindow.h"
#include "../../scopehal/TwoLevelTrigger.h"

#include "imgui_internal.h"	//for SetItemUsingMouseWheel

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DisplayedChannel

DisplayedChannel::DisplayedChannel(StreamDescriptor stream)
		: m_stream(stream)
		, m_rasterizedWaveform("DisplayedChannel.m_rasterizedWaveform")
		, m_indexBuffer("DisplayedChannel.m_indexBuffer")
		, m_rasterizedX(0)
		, m_rasterizedY(0)
		, m_cachedX(0)
		, m_cachedY(0)
		, m_persistenceEnabled(false)
		, m_toneMapPipe("shaders/WaveformToneMap.spv", 1, sizeof(ToneMapArgs), 1)
{
	stream.m_channel->AddRef();

	//Use GPU-side memory for rasterized waveform
	//TODO: instead of using CPU-side mirror, use a shader to memset it when clearing?
	m_rasterizedWaveform.SetCpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);
	m_rasterizedWaveform.SetGpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);

	//Use pinned memory for index buffer since it should only be read once
	m_indexBuffer.SetCpuAccessHint(AcceleratorBuffer<int64_t>::HINT_LIKELY);
	m_indexBuffer.SetGpuAccessHint(AcceleratorBuffer<int64_t>::HINT_UNLIKELY);
}

/**
	@brief Handles a change in size of the displayed waveform

	@param newSize	New size of WaveformArea

	@return true if size has changed, false otherwose
 */
bool DisplayedChannel::UpdateSize(ImVec2 newSize, MainWindow* top)
{
	size_t x = newSize.x;
	size_t y = newSize.y;

	if( (m_cachedX != x) || (m_cachedY != y) )
	{
		m_cachedX = x;
		m_cachedY = y;

		LogTrace("Displayed channel resized (to %zu x %zu), reallocating texture\n", x, y);

		vector<uint32_t> queueFamilies;
		vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
		queueFamilies.push_back(g_computeQueueType);	//TODO: separate transfer queue?
		if(g_renderQueueType != g_computeQueueType)
		{
			queueFamilies.push_back(g_renderQueueType);
			sharingMode = vk::SharingMode::eConcurrent;
		}
		vk::ImageCreateInfo imageInfo(
			{},
			vk::ImageType::e2D,
			vk::Format::eR32G32B32A32Sfloat,
			vk::Extent3D(x, y, 1),
			1,
			1,
			VULKAN_HPP_NAMESPACE::SampleCountFlagBits::e1,
			VULKAN_HPP_NAMESPACE::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			sharingMode,
			queueFamilies,
			vk::ImageLayout::eUndefined
			);

		//Keep a reference to the old texture around for one more frame
		//in case the previous frame hasn't fully completed rendering yet
		top->AddTextureUsedThisFrame(m_texture);

		//Make the new texture and mark that as in use too
		m_texture = make_shared<Texture>(
			*g_vkComputeDevice, imageInfo, top->GetTextureManager(), "DisplayedChannel.m_texture");
		top->AddTextureUsedThisFrame(m_texture);

		//Add a barrier to convert the image format to "general"
		lock_guard<mutex> lock(g_vkTransferMutex);
		vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		vk::ImageMemoryBarrier barrier(
			vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eShaderWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			m_texture->GetImage(),
			range);
		g_vkTransferCommandBuffer->begin({});
		g_vkTransferCommandBuffer->pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eComputeShader,
				{},
				{},
				{},
				barrier);
		g_vkTransferCommandBuffer->end();
		SubmitAndBlock(*g_vkTransferCommandBuffer, *g_vkTransferQueue);

		return true;
	}

	return false;
}

/**
	@brief Prepares to rasterize the waveform at the specified resolution
 */
void DisplayedChannel::PrepareToRasterize(size_t x, size_t y)
{
	bool sizeChanged = (m_rasterizedX != x) || (m_rasterizedY != y);

	m_rasterizedX = x;
	m_rasterizedY = y;

	if(sizeChanged)
	{
		size_t npixels = x*y;
		m_rasterizedWaveform.resize(npixels);

		//fill with black
		m_rasterizedWaveform.PrepareForCpuAccess();
		memset(m_rasterizedWaveform.GetCpuPointer(), 0, npixels * sizeof(float));
		m_rasterizedWaveform.MarkModifiedFromCpu();
	}

	//Allocate index buffer for sparse waveforms
	if(!IsDensePacked())
		m_indexBuffer.resize(x);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformArea::WaveformArea(StreamDescriptor stream, shared_ptr<WaveformGroup> group, MainWindow* parent)
	: m_width(1)
	, m_height(1)
	, m_yAxisOffset(0)
	, m_ymid(0)
	, m_pixelsPerYAxisUnit(1)
	, m_yAxisUnit(stream.GetYAxisUnits())
	, m_dragState(DRAG_STATE_NONE)
	, m_lastDragState(DRAG_STATE_NONE)
	, m_group(group)
	, m_parent(parent)
	, m_tLastMouseMove(GetTime())
	, m_mouseOverTriggerArrow(false)
	, m_triggerLevelDuringDrag(0)
	, m_triggerDuringDrag(nullptr)
	, m_lastRightClickOffset(0)
{
	m_displayedChannels.push_back(make_shared<DisplayedChannel>(stream));
}

WaveformArea::~WaveformArea()
{
	m_displayedChannels.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stream management

/**
	@brief Adds a new stream to this plot
 */
void WaveformArea::AddStream(StreamDescriptor desc)
{
	m_displayedChannels.push_back(make_shared<DisplayedChannel>(desc));
}

/**
	@brief Removes the stream at a specified index
 */
void WaveformArea::RemoveStream(size_t i)
{
	m_channelsToRemove.push_back(m_displayedChannels[i]);
	m_displayedChannels.erase(m_displayedChannels.begin() + i);
}

/**
	@brief Returns the first analog stream displayed in this area.

	If no analog waveforms are visible, returns a null stream.
 */
StreamDescriptor WaveformArea::GetFirstAnalogStream()
{
	for(auto chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(stream.GetType() == Stream::STREAM_TYPE_ANALOG)
			return stream;
	}

	return StreamDescriptor(nullptr, 0);
}

/**
	@brief Returns the first analog stream or eye pattern displayed in this area.

	If no analog waveforms or eye patterns are visible, returns a null stream.
 */
StreamDescriptor WaveformArea::GetFirstAnalogOrEyeStream()
{
	for(auto chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(stream.GetType() == Stream::STREAM_TYPE_ANALOG)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_EYE)
			return stream;
	}

	return StreamDescriptor(nullptr, 0);
}

/**
	@brief Marks all of our waveform textures as being used this frame
 */
void WaveformArea::ReferenceWaveformTextures()
{
	for(auto chan : m_displayedChannels)
		m_parent->AddTextureUsedThisFrame(chan->GetTexture());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Y axis helpers

float WaveformArea::PixelsToYAxisUnits(float pix)
{
	return pix / m_pixelsPerYAxisUnit;
}

float WaveformArea::YAxisUnitsToPixels(float volt)
{
	return volt * m_pixelsPerYAxisUnit;
}

float WaveformArea::YAxisUnitsToYPosition(float volt)
{
	return m_ymid - YAxisUnitsToPixels(volt + m_yAxisOffset);
}

float WaveformArea::YPositionToYAxisUnits(float y)
{
	return PixelsToYAxisUnits(-1 * (y - m_ymid) ) - m_yAxisOffset;
}

float WaveformArea::PickStepSize(float volts_per_half_span, int min_steps, int max_steps)
{
	static const float steps[3] = {1, 2, 5};

	for(int exp = -4; exp < 12; exp ++)
	{
		for(int i=0; i<3; i++)
		{
			float step = pow(10, exp) * steps[i];

			float steps_per_half_span = volts_per_half_span / step;
			if(steps_per_half_span > max_steps)
				continue;
			if(steps_per_half_span < min_steps)
				continue;
			return step;
		}
	}

	//if no hits
	return FLT_MAX;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GUI widget rendering

/**
	@brief Returns true if a channel was being dragged at the start of this frame

	(including if the mouse button was released this frame)
 */
bool WaveformArea::IsChannelBeingDragged()
{
	return (m_dragState == DRAG_STATE_CHANNEL) || (m_lastDragState == DRAG_STATE_CHANNEL);
}

/**
	@brief Returns the channel being dragged, if one exists
 */
StreamDescriptor WaveformArea::GetChannelBeingDragged()
{
	if(!IsChannelBeingDragged())
		return StreamDescriptor(nullptr, 0);
	else
		return m_dragStream;
}

/**
	@brief Renders a waveform area

	Returns false if the area should be closed (no more waveforms visible in it)
 */
bool WaveformArea::Render(int iArea, int numAreas, ImVec2 clientArea)
{
	m_lastDragState = m_dragState;
	if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		OnMouseUp();
	if(m_dragState != DRAG_STATE_NONE)
		OnDragUpdate();

	//Clear channels from last frame
	if(!m_channelsToRemove.empty())
	{
		g_vkComputeDevice->waitIdle();
		m_channelsToRemove.clear();
	}

	//Save timestamps if we right clicked
	if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		m_lastRightClickOffset = m_group->XPositionToXAxisUnits(ImGui::GetMousePos().x);

	//Detect mouse movement
	double tnow = GetTime();
	auto mouseDelta = ImGui::GetIO().MouseDelta;
	if( (mouseDelta.x != 0) || (mouseDelta.y != 0) )
		m_tLastMouseMove = tnow;

	ImGui::PushID(to_string(iArea).c_str());

	float totalHeightAvailable = floor(clientArea.y - ImGui::GetFrameHeightWithSpacing());
	float spacing = m_group->GetSpacing();
	float heightPerArea = totalHeightAvailable / numAreas;
	float unspacedHeightPerArea = floor(heightPerArea - spacing);

	//Update cached scale
	m_height = unspacedHeightPerArea;
	auto first = GetFirstAnalogOrEyeStream();
	if(first)
	{
		//Don't touch scale if we're dragging, since the dragged value is newer than the hardware value
		if(m_dragState != DRAG_STATE_Y_AXIS)
			m_yAxisOffset = first.GetOffset();

		m_pixelsPerYAxisUnit = unspacedHeightPerArea / first.GetVoltageRange();
		m_yAxisUnit = first.GetYAxisUnits();
	}

	//Size of the Y axis view at the right of the plot
	float yAxisWidth = m_group->GetYAxisWidth();
	float yAxisWidthSpaced = yAxisWidth + spacing;

	//Settings calculated by RenderGrid() then reused in RenderYAxis()
	map<float, float> gridmap;
	float vbot;
	float vtop;

	if(ImGui::BeginChild(ImGui::GetID(this), ImVec2(clientArea.x - yAxisWidthSpaced, unspacedHeightPerArea)))
	{
		auto csize = ImGui::GetContentRegionAvail();
		auto pos = ImGui::GetWindowPos();

		m_width = csize.x;

		//Calculate midpoint of our plot
		m_ymid = pos.y + unspacedHeightPerArea / 2;

		//Draw the background
		RenderBackgroundGradient(pos, csize);
		RenderGrid(pos, csize, gridmap, vbot, vtop);

		//Blank out space for the actual waveform
		ImGui::InvisibleButton("plot", ImVec2(csize.x, csize.y));
		ImGui::SetItemAllowOverlap();

		//Check for context menu if we didn't do one yet
		PlotContextMenu();

		//Draw actual waveforms (and protocol decode overlays)
		RenderWaveforms(pos, csize);

		ImGui::SetItemUsingMouseWheel();
		if(ImGui::IsItemHovered())
		{
			auto wheel = ImGui::GetIO().MouseWheel;
			if(wheel != 0)
				OnMouseWheelPlotArea(wheel);

			//Overlays / targets for drag-and-drop
			if(m_parent->IsChannelBeingDragged())
				DragDropOverlays(pos, csize, iArea, numAreas);
		}

		//Cursors have to be drawn over the waveform
		RenderCursors(pos, csize);

		//Make sure all channels have same vertical scale and warn if not
		CheckForScaleMismatch(pos, csize);

		//Draw control widgets
		ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
		ImGui::BeginGroup();

			for(size_t i=0; i<m_displayedChannels.size(); i++)
				ChannelButton(m_displayedChannels[i], i);

		ImGui::EndGroup();
		ImGui::SetItemAllowOverlap();
	}
	ImGui::EndChild();

	//Draw the vertical scale on the right side of the plot
	RenderYAxis(ImVec2(yAxisWidth, unspacedHeightPerArea), gridmap, vbot, vtop);

	ImGui::PopID();

	//Update scale again in case we had a mouse wheel event
	//(we need scale to be accurate for the re-render in the background thread)
	if(first)
		m_pixelsPerYAxisUnit = unspacedHeightPerArea / first.GetVoltageRange();

	if(m_displayedChannels.empty())
		return false;
	return true;
}

/**
	@brief Run the context menu for the main plot area
 */
void WaveformArea::PlotContextMenu()
{
	if(ImGui::BeginPopupContextItem())
	{
		//Look for markers that might be near our right click location
		float lastRightClickPos = m_group->XAxisUnitsToXPosition(m_lastRightClickOffset);
		auto& markers = m_parent->GetSession().GetMarkers(GetWaveformTimestamp());
		bool hitMarker = false;
		size_t selectedMarker = 0;
		for(size_t i=0; i<markers.size(); i++)
		{
			auto& m = markers[i];
			float xpos = round(m_group->XAxisUnitsToXPosition(m.m_offset));
			float searchRadius = 0.25 * ImGui::GetFontSize();
			if(fabs(xpos - lastRightClickPos) < searchRadius)
			{
				hitMarker = true;
				selectedMarker = i;
				break;
			}
		}

		//If we right clicked on or very close to a marker, show "delete" menu instead
		if(hitMarker)
		{
			if(ImGui::MenuItem("Delete"))
				markers.erase(markers.begin() + selectedMarker);
		}

		//Otherwise, normal GUI context menu
		else
		{
			if(ImGui::BeginMenu("Cursors"))
			{
				if(ImGui::BeginMenu("X axis"))
				{
					if(ImGui::MenuItem("None", nullptr, (m_group->m_xAxisCursorMode == WaveformGroup::X_CURSOR_NONE)))
						m_group->m_xAxisCursorMode = WaveformGroup::X_CURSOR_NONE;
					if(ImGui::MenuItem("Single", nullptr, (m_group->m_xAxisCursorMode == WaveformGroup::X_CURSOR_SINGLE)))
						m_group->m_xAxisCursorMode = WaveformGroup::X_CURSOR_SINGLE;
					if(ImGui::MenuItem("Dual", nullptr, (m_group->m_xAxisCursorMode == WaveformGroup::X_CURSOR_DUAL)))
						m_group->m_xAxisCursorMode = WaveformGroup::X_CURSOR_DUAL;

					ImGui::EndMenu();
				}

				if(ImGui::BeginMenu("Y axis"))
				{
					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			if(ImGui::MenuItem("Add Marker"))
			{
				auto& session = m_parent->GetSession();
				session.AddMarker(Marker(GetWaveformTimestamp(), m_lastRightClickOffset, session.GetNextMarkerName()));
			}
		}

		ImGui::EndPopup();
	}
}

/**
	@brief Renders our waveforms
 */
void WaveformArea::RenderWaveforms(ImVec2 start, ImVec2 size)
{
	for(auto& chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
				RenderAnalogWaveform(chan, start, size);
				break;

			default:
				LogWarning("Unimplemented stream type %d, don't know how to render it\n", stream.GetType());
				break;
		}
	}
}

/**
	@brief Renders a single analog waveform
 */
void WaveformArea::RenderAnalogWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Mark the waveform as resized
	if(channel->UpdateSize(size, m_parent))
		m_parent->SetNeedRender();

	//Render the tone mapped output (if we have it)
	auto tex = channel->GetTexture();
	if(tex != nullptr)
		list->AddImage(tex->GetTexture(), start, ImVec2(start.x+size.x, start.y+size.y), ImVec2(0, 1), ImVec2(1, 0) );
}

/**
	@brief Tone map our waveforms
 */
void WaveformArea::ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf)
{
	for(auto& chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
				ToneMapAnalogWaveform(chan, cmdbuf);
				break;

			default:
				LogWarning("Unimplemented stream type %d, don't know how to tone map it\n", stream.GetType());
				break;
		}
	}
}

/**
	@brief Runs the rendering shader on all of our waveforms

	Called from WaveformThread

	@param cmdbuf				Command buffer to record rendering commands into
	@param chans				Set of channels we rendered into
								Used to keep references active until rendering completes if we close them this frame
	@param clearPersistence		True if persistence maps should be erased before rendering
 */
void WaveformArea::RenderWaveformTextures(
	vk::raii::CommandBuffer& cmdbuf,
	vector<shared_ptr<DisplayedChannel> >& chans,
	bool clearPersistence)
{
	chans = m_displayedChannels;

	bool clearThisAreaOnly = m_clearPersistence.exchange(false);
	bool clearing = clearThisAreaOnly || clearPersistence;

	for(auto& chan : chans)
	{
		auto stream = chan->GetStream();
		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
				RasterizeAnalogWaveform(chan, cmdbuf, clearing);
				break;

			default:
				LogWarning("Unimplemented stream type %d, don't know how to rasterize it\n", stream.GetType());
				break;
		}
	}
}

void WaveformArea::RasterizeAnalogWaveform(
	shared_ptr<DisplayedChannel> channel,
	vk::raii::CommandBuffer& cmdbuf,
	bool clearPersistence
	)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();

	//Prepare the memory so we can rasterize it
	//If no data, set to 0x0 pixels and return
	if(data == nullptr)
	{
		channel->PrepareToRasterize(0, 0);
		return;
	}
	size_t w = m_width;
	size_t h = m_height;
	channel->PrepareToRasterize(w, h);

	shared_ptr<ComputePipeline> comp;

	//Calculate a bunch of constants
	int64_t offset = m_group->GetXAxisOffset();
	int64_t innerxoff = offset / data->m_timescale;
	int64_t fractional_offset = offset % data->m_timescale;
	int64_t offset_samples = (offset - data->m_triggerPhase) / data->m_timescale;
	double pixelsPerX = m_group->GetPixelsPerXUnit();
	double xscale = data->m_timescale * pixelsPerX;

	//Bind input buffers
	auto udata = dynamic_cast<UniformAnalogWaveform*>(data);
	auto sdata = dynamic_cast<SparseAnalogWaveform*>(data);
	if(udata)
	{
		comp = channel->GetUniformAnalogPipeline();
		comp->BindBufferNonblocking(1, udata->m_samples, cmdbuf);
		//don't bind offsets or indexes as they're not used
	}

	else
	{
		comp = channel->GetSparseAnalogPipeline();
		comp->BindBufferNonblocking(1, sdata->m_samples, cmdbuf);

		//Map offsets and, if requested, durations
		comp->BindBufferNonblocking(2, sdata->m_offsets, cmdbuf);
		if(channel->ShouldMapDurations())
			comp->BindBufferNonblocking(4, sdata->m_durations, cmdbuf);

		//Calculate indexes for X axis
		auto& ibuf = channel->GetIndexBuffer();
		ibuf.PrepareForCpuAccess();
		sdata->m_offsets.PrepareForCpuAccess();
		for(size_t i=0; i<w; i++)
		{
			int64_t target = floor(i / xscale) + offset_samples;
			ibuf[i] = BinarySearchForGequal(
				sdata->m_offsets.GetCpuPointer(),
				data->size(),
				target-2);
		}
		ibuf.MarkModifiedFromCpu();
		comp->BindBufferNonblocking(3, ibuf, cmdbuf);
	}

	if(!comp)
		return;

	//Bind output texture
	auto& imgOut = channel->GetRasterizedWaveform();
	comp->BindBufferNonblocking(0, imgOut, cmdbuf);

	//Scale alpha by zoom.
	//As we zoom out more, reduce alpha to get proper intensity grading
	//TODO: make this constant, then apply a second alpha pass in tone mapping?
	//This will eliminate the need for a (potentially heavy) re-render when adjusting the slider.
	float alpha = m_parent->GetTraceAlpha();
	auto end = data->size() - 1;
	int64_t lastOff = GetOffsetScaled(sdata, udata, end);
	float capture_len = lastOff;
	float avg_sample_len = capture_len / data->size();
	float samplesPerPixel = 1.0 / (pixelsPerX * avg_sample_len);
	float alpha_scaled = alpha / sqrt(samplesPerPixel);
	alpha_scaled = min(1.0f, alpha_scaled) * 2;

	//Fill shader configuration
	ConfigPushConstants config;
	config.innerXoff = -innerxoff;
	config.windowHeight = h;
	config.windowWidth = w;
	config.memDepth = data->size();
	config.offset_samples = offset_samples - 2;
	config.alpha = alpha_scaled;
	config.xoff = (data->m_triggerPhase - fractional_offset) * pixelsPerX;
	config.xscale = xscale;
	config.ybase = h * 0.5f;
	config.yscale = m_pixelsPerYAxisUnit;
	config.yoff = stream.GetOffset();
	if(channel->IsPersistenceEnabled() && !clearPersistence)
		config.persistScale = m_parent->GetPersistDecay();
	else
		config.persistScale = 0;

	//Dispatch the shader
	comp->Dispatch(cmdbuf, config, w, 1, 1);
	comp->AddComputeMemoryBarrier(cmdbuf);
	imgOut.MarkModifiedFromGpu();
}

/**
	@brief Tone maps an analog waveform by converting the internal fp32 buffer to RGBA
 */
void WaveformArea::ToneMapAnalogWaveform(shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf)
{
	auto tex = channel->GetTexture();
	if(tex == nullptr)
		return;

	//Nothing to draw? Early out if we haven't processed the window resize yet or there's no data
	auto width = channel->GetRasterizedX();
	auto height = channel->GetRasterizedY();
	if( (width == 0) || (height == 0) )
		return;

	//Run the actual compute shader
	auto& pipe = channel->GetToneMapPipeline();
	pipe.BindBufferNonblocking(0, channel->GetRasterizedWaveform(), cmdbuf);
	pipe.BindStorageImage(
		1,
		**m_parent->GetTextureManager()->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	auto color = ImGui::ColorConvertU32ToFloat4(ColorFromString(channel->GetStream().m_channel->m_displaycolor));
	ToneMapArgs args(color, width, height);
	pipe.Dispatch(cmdbuf, args, GetComputeBlockCount(width, 64), height);

	//Add a barrier before we read from the fragment shader
	vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	vk::ImageMemoryBarrier barrier(
		vk::AccessFlagBits::eShaderWrite,
		vk::AccessFlagBits::eShaderRead,
		vk::ImageLayout::eGeneral,
		vk::ImageLayout::eGeneral,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		tex->GetImage(),
		range);
	cmdbuf.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier);
}

/**
	@brief Renders the background of the main plot area

	For now, simple gray gradient.
 */
void WaveformArea::RenderBackgroundGradient(ImVec2 start, ImVec2 size)
{
	auto& prefs = m_parent->GetSession().GetPreferences();
	auto color_bottom = prefs.GetColor("Appearance.Graphs.bottom_color");
	auto color_top = prefs.GetColor("Appearance.Graphs.top_color");

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilledMultiColor(
		start,
		ImVec2(start.x + size.x, start.y + size.y),
		color_top,
		color_top,
		color_bottom,
		color_bottom);
}

/**
	@brief Renders grid lines
 */
void WaveformArea::RenderGrid(ImVec2 start, ImVec2 size, map<float, float>& gridmap, float& vbot, float& vtop)
{
	//Early out if we're not displaying any analog waveforms
	auto stream = GetFirstAnalogOrEyeStream();
	if(!stream)
		return;

	float ytop = start.y;
	float ybot = start.y + size.y;
	float halfheight = m_height / 2;

	//Volts from the center line of our graph to the top. May not be the max value in the signal.
	float volts_per_half_span = PixelsToYAxisUnits(halfheight);

	//Sanity check invalid values
	if( (volts_per_half_span < -FLT_MAX/2) || (volts_per_half_span > FLT_MAX/2) )
	{
		LogWarning("WaveformArea: invalid grid span (%f)\n", volts_per_half_span);
		return;
	}

	float theight = ImGui::GetFontSize();

	//Decide what voltage step to use. Pick from a list (in volts)
	int min_steps = 1;								//Always have at least one division
	int max_steps = floor(halfheight / theight);	//Do not have more divisions than can fit given our font size
	max_steps = min(max_steps, 10);					//Do not have more than ten divisions regardless of font size
	float selected_step = PickStepSize(volts_per_half_span, min_steps, max_steps);

	//Special case a few scenarios
	if(stream.GetYAxisUnits() == Unit::UNIT_LOG_BER)
		selected_step = 2;

	float bottom_edge = (ybot - theight/2);
	float top_edge = (ytop + theight/2);

	//Offset things so that the grid lines are at sensible locations
	vbot = YPositionToYAxisUnits(ybot);
	vtop = YPositionToYAxisUnits(ytop);
	float vmid = (vbot + vtop)/2;
	float yzero = YAxisUnitsToYPosition(0);
	float zero_offset = fmodf(vmid, selected_step);
	vmid -= zero_offset;

	//Calculate grid positions
	for(float dv=0; ; dv += selected_step)
	{
		float vp = vmid + dv;
		float vn = vmid - dv;

		float yt = YAxisUnitsToYPosition(vp);
		float yb = YAxisUnitsToYPosition(vn);

		if(dv != 0)
		{
			if( (yb <= bottom_edge) && (yb >= top_edge ) )
				gridmap[vn] = yb;

			if( (yt <= bottom_edge ) && (yt >= top_edge) )
				gridmap[vp] = yt;
		}
		else
			gridmap[vp] = yt;

		if(gridmap.size() > 50)
			break;

		//Stop if we're off the edge
		if( (yb > ybot) && (yt < ytop) )
			break;
	}

	//Style settings
	auto& prefs = m_parent->GetSession().GetPreferences();
	auto axisColor = prefs.GetColor("Appearance.Graphs.grid_centerline_color");
	auto gridColor = prefs.GetColor("Appearance.Graphs.grid_color");
	auto axisWidth = prefs.GetReal("Appearance.Graphs.grid_centerline_width");
	auto gridWidth = prefs.GetReal("Appearance.Graphs.grid_width");

	auto list = ImGui::GetWindowDrawList();
	float left = start.x;
	float right = start.x + size.x;
	for(auto it : gridmap)
	{
		float y = round(it.second);
		list->AddLine(
			ImVec2(left, y),
			ImVec2(right, y),
			gridColor,
			gridWidth);
	}

	//draw Y=0 line
	if( (yzero > ytop) && (yzero < ybot) )
	{
		float y = round(yzero);
		list->AddLine(
			ImVec2(left, y),
			ImVec2(right, y),
			axisColor,
			axisWidth);
	}
}

/**
	@brief Renders the Y axis scale
 */
void WaveformArea::RenderYAxis(ImVec2 size, map<float, float>& gridmap, float vbot, float vtop)
{
	ImGui::SameLine(0, 0);
	ImGui::BeginChild("yaxis", size);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	auto origin = ImGui::GetWindowPos();
	float ytop = origin.y;
	float ybot = origin.y + size.y;

	//Style settings
	auto font = m_parent->GetFontPref("Appearance.Graphs.y_axis_font");
	auto& prefs = m_parent->GetSession().GetPreferences();
	float theight = font->FontSize;
	auto textColor = prefs.GetColor("Appearance.Graphs.y_axis_text_color");

	//Reserve an empty area we're going to draw into
	ImGui::Dummy(size);

	//Catch mouse wheel events
	ImGui::SetItemUsingMouseWheel();
	if(ImGui::IsItemHovered())
	{
		auto wheel = ImGui::GetIO().MouseWheel;
		if(wheel != 0)
			OnMouseWheelYAxis(wheel);
	}

	//Trigger level arrow(s)
	RenderTriggerLevelArrows(origin, size);

	//Help tooltip
	//Only show if mouse has been still for 1 sec
	//(shorter delays interfere with dragging)
	double tnow = GetTime();
	if( (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) &&
		(tnow - m_tLastMouseMove > 1) &&
		(m_dragState == DRAG_STATE_NONE) )
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);

		if(m_mouseOverTriggerArrow)
			ImGui::TextUnformatted("Drag arrow to adjust trigger level.");
		else
			ImGui::TextUnformatted("Click and drag to adjust offset.\nUse mouse wheel to adjust scale.");

		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	//Draw text for the Y axis labels
	float xmargin = 5;
	for(auto it : gridmap)
	{
		float vlo = YPositionToYAxisUnits(it.second - 0.5);
		float vhi = YPositionToYAxisUnits(it.second + 0.5);
		auto label = m_yAxisUnit.PrettyPrintRange(vlo, vhi, vbot, vtop);

		float y = it.second - theight/2;
		if(y > ybot)
			continue;
		if(y < ytop)
			continue;

		auto tsize = font->CalcTextSizeA(theight, FLT_MAX, 0, label.c_str());

		draw_list->AddText(font, theight, ImVec2(origin.x + size.x - tsize.x - xmargin, y), textColor, label.c_str());
	}

	ImGui::EndChild();

	//Start dragging
	if(ImGui::IsItemHovered() && !m_mouseOverTriggerArrow)
	{
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			LogTrace("Start dragging Y axis\n");
			m_dragState = DRAG_STATE_Y_AXIS;
		}
	}
}

/**
	@brief Cursors and related stuff
 */
void WaveformArea::RenderCursors(ImVec2 start, ImVec2 size)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	//Draw dashed line at trigger level
	if(m_dragState == DRAG_STATE_TRIGGER_LEVEL)
	{
		ImU32 triggerColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1));
		float dashSize = ImGui::GetFontSize() * 0.5;

		float y = YAxisUnitsToYPosition(m_triggerLevelDuringDrag);
		for(float dx = 0; (dx + dashSize) < size.x; dx += 2*dashSize)
			draw_list->AddLine(ImVec2(start.x + dx, y), ImVec2(start.x + dx + dashSize, y), triggerColor);
	}
}

/**
	@brief Look for mismatched vertical scales and display warning message
 */
void WaveformArea::CheckForScaleMismatch(ImVec2 start, ImVec2 size)
{
	//No analog streams? No mismatch possible
	auto firstStream = GetFirstAnalogOrEyeStream();
	if(!firstStream)
		return;

	//Look for a mismatch
	float firstRange = firstStream.GetVoltageRange();
	bool mismatchFound = true;
	StreamDescriptor mismatchStream;
	for(auto c : m_displayedChannels)
	{
		auto stream = c->GetStream();
		if(stream.GetVoltageRange() > 1.2 * firstRange)
		{
			mismatchFound = true;
			mismatchStream = stream;
			break;
		}
	}
	if(!mismatchFound || !mismatchStream)
		return;

	//If we get here, we had a mismatch. Prepare to draw the warning message centered in the plot
	//above everything else
	ImVec2 center(start.x + size.x/2, start.y + size.y/2);
	float warningSize = ImGui::GetFontSize() * 3;

	//Warning icon
	auto list = ImGui::GetWindowDrawList();
	list->AddImage(
		m_parent->GetTextureManager()->GetTexture("warning"),
		ImVec2(center.x - 0.5, center.y - warningSize/2 - 0.5),
		ImVec2(center.x + warningSize + 0.5, center.y + warningSize/2 + 0.5));

	//Prepare to draw text
	center.x += warningSize;
	float fontHeight = ImGui::GetFontSize();
	auto font = m_parent->GetFontPref("Appearance.General.default_font");
	string str = "Caution: Potential for instrument damage!\n\n";
	str += string("The channel ") + mismatchStream.GetName() + " has a full-scale range of " +
		mismatchStream.GetYAxisUnits().PrettyPrint(mismatchStream.GetVoltageRange()) + ",\n";
	str += string("but this plot has a full-scale range of ") +
		firstStream.GetYAxisUnits().PrettyPrint(firstRange) + ".\n\n";
	str += "Setting this channel to match the plot scale may result\n";
	str += "in overdriving the instrument input.\n";
	str += "\n";
	str += string("If the instrument \"") + mismatchStream.m_channel->GetScope()->m_nickname +
		"\" can safely handle the applied signal at this plot's scale setting,\n";
	str += "adjust the vertical scale of this plot slightly to set all signals to the same scale\n";
	str += "and eliminate this message.\n\n";
	str += "If it cannot, move the channel to another plot.\n";

	//Draw background for text
	float wrapWidth = 40 * fontHeight;
	auto textsize = font->CalcTextSizeA(
		fontHeight,
		FLT_MAX,
		wrapWidth,
		str.c_str());
	float padding = 5;
	float wrounding = 2;
	list->AddRectFilled(
		ImVec2(center.x, center.y - textsize.y/2 - padding),
		ImVec2(center.x + textsize.x + 2*padding, center.y + textsize.y/2 + padding),
		ImGui::GetColorU32(ImGuiCol_PopupBg),
		wrounding);

	//Draw the text
	list->AddText(
		font,
		fontHeight,
		ImVec2(center.x + padding, center.y - textsize.y/2),
		ImGui::GetColorU32(ImGuiCol_Text),
		str.c_str(),
		nullptr,
		wrapWidth);
}

/**
	@brief Arrows pointing to trigger levels
 */
void WaveformArea::RenderTriggerLevelArrows(ImVec2 start, ImVec2 /*size*/)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	float arrowsize = ImGui::GetFontSize() * 0.6;
	float caparrowsize = ImGui::GetFontSize() * 1;

	//Make a list of scope channels we're displaying and what scopes they came from
	//(ignore any filter based channels)
	set<Oscilloscope*> scopes;
	set<StreamDescriptor> channels;
	for(auto c : m_displayedChannels)
	{
		auto stream = c->GetStream();
		auto scope = stream.m_channel->GetScope();
		if(scope == nullptr)
			continue;

		channels.emplace(stream);
		scopes.emplace(scope);
	}

	//For each scope, see if we are displaying the trigger input in this area
	float arrowright = start.x + arrowsize;
	float caparrowright = start.x + caparrowsize;
	m_mouseOverTriggerArrow = false;
	auto mouse = ImGui::GetMousePos();
	for(auto s : scopes)
	{
		//No trigger? Nothing to display
		auto trig = s->GetTrigger();
		if(trig == nullptr)
			continue;

		//Input not visible in this plot? Nothing to do
		auto stream = trig->GetInput(0);
		if(channels.find(stream) == channels.end())
			continue;

		auto color = ColorFromString(stream.m_channel->m_displaycolor);

		//Draw the arrow
		//If currently dragging, show at mouse position rather than actual hardware trigger level
		float level = trig->GetLevel();
		float y = YAxisUnitsToYPosition(level);
		if( (m_dragState == DRAG_STATE_TRIGGER_LEVEL) && (trig == m_triggerDuringDrag) )
			y = mouse.y;
		float arrowtop = y - arrowsize/2;
		float arrowbot = y + arrowsize/2;
		draw_list->AddTriangleFilled(
			ImVec2(start.x, y), ImVec2(arrowright, arrowtop), ImVec2(arrowright, arrowbot), color);

		//Check mouse position
		//Use slightly expanded hitbox to make it easier to capture
		float caparrowtop = y - caparrowsize/2;
		float caparrowbot = y + caparrowsize/2;
		if( (mouse.x >= start.x) && (mouse.x <= caparrowright) && (mouse.y >= caparrowtop) && (mouse.y <= caparrowbot) )
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			m_mouseOverTriggerArrow = true;

			if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				LogTrace("Start dragging trigger level\n");
				m_dragState = DRAG_STATE_TRIGGER_LEVEL;
				m_triggerDuringDrag = trig;
			}
		}

		//TODO: second level
		auto sl = dynamic_cast<TwoLevelTrigger*>(trig);
		if(sl)
			LogWarning("Two-level triggers not implemented\n");

		//Handle dragging
		if(m_dragState == DRAG_STATE_TRIGGER_LEVEL)
			m_triggerLevelDuringDrag = YPositionToYAxisUnits(mouse.y);
	}
}

/**
	@brief Drag-and-drop overlay areas
 */
void WaveformArea::DragDropOverlays(ImVec2 start, ImVec2 size, int iArea, int numAreas)
{
	//Drag/drop areas for splitting
	float heightOfVerticalRegion = size.y * 0.25;
	float widthOfVerticalEdge = size.x*0.25;
	float leftOfMiddle = start.x + widthOfVerticalEdge;
	float rightOfMiddle = start.x + size.x*0.75;
	float topOfMiddle = start.y;
	float bottomOfMiddle = start.y + size.y;
	float widthOfMiddle = rightOfMiddle - leftOfMiddle;
	if(iArea == 0)
	{
		EdgeDropArea(
			"top",
			ImVec2(leftOfMiddle, start.y),
			ImVec2(widthOfMiddle, heightOfVerticalRegion),
			ImGuiDir_Up);

		topOfMiddle += heightOfVerticalRegion;
	}

	if(iArea == (numAreas-1))
	{
		bottomOfMiddle -= heightOfVerticalRegion;
		EdgeDropArea(
			"bottom",
			ImVec2(leftOfMiddle, bottomOfMiddle),
			ImVec2(widthOfMiddle, heightOfVerticalRegion),
			ImGuiDir_Down);
	}

	float heightOfMiddle = bottomOfMiddle - topOfMiddle;

	//Center drop area should only be displayed if we are not the source area of the drag
	if(m_dragState == DRAG_STATE_NONE)
		CenterDropArea(ImVec2(leftOfMiddle, topOfMiddle), ImVec2(widthOfMiddle, heightOfMiddle));

	ImVec2 edgeSize(widthOfVerticalEdge, heightOfMiddle);
	EdgeDropArea("left", ImVec2(start.x, topOfMiddle), edgeSize, ImGuiDir_Left);
	EdgeDropArea("right", ImVec2(rightOfMiddle, topOfMiddle), edgeSize, ImGuiDir_Right);
}

/**
	@brief Drop area for edge of the plot

	Dropping a waveform in here splits and forms a new group
 */
void WaveformArea::EdgeDropArea(const string& name, ImVec2 start, ImVec2 size, ImGuiDir splitDir)
{
	ImGui::SetCursorScreenPos(start);
	ImGui::InvisibleButton(name.c_str(), size);
	//ImGui::Button(name.c_str(), size);
	ImGui::SetItemAllowOverlap();

	//Add drop target
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("Waveform", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		if( (payload != nullptr) && (payload->DataSize == sizeof(DragDescriptor)) )
		{
			LogTrace("splitting\n");

			auto desc = reinterpret_cast<DragDescriptor*>(payload->Data);
			auto stream = desc->first->GetStream(desc->second);

			//Add request to split our current group
			m_parent->QueueSplitGroup(m_group, splitDir, stream);

			//Remove the stream from the originating waveform area
			desc->first->RemoveStream(desc->second);
		}

		ImGui::EndDragDropTarget();
	}

	//Draw overlay target
	const float rounding = max(3.0f, ImGui::GetStyle().FrameRounding);
	const ImU32 bgBase = ImGui::GetColorU32(ImGuiCol_DockingPreview, 0.70f);
	const ImU32 bgHovered = ImGui::GetColorU32(ImGuiCol_DockingPreview, 1.00f);
	const ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_NavWindowingHighlight, 0.60f);
	ImVec2 center(start.x + size.x/2, start.y + size.y/2);
	float fillSizeX = 34;
	float lineSizeX = 32;
	float fillSizeY = 34;
	float lineSizeY = 32;

	//L-R split: make target half size in X axis
	if( (splitDir == ImGuiDir_Left) || (splitDir == ImGuiDir_Right) )
	{
		fillSizeX /= 2;
		lineSizeX /= 2;
	}

	//T/B split: make target half size in Y axis
	else
	{
		fillSizeY /= 2;
		lineSizeY /= 2;
	}

	//Shift center by appropriate direction to be close to the edge
	switch(splitDir)
	{
		case ImGuiDir_Left:
			center.x = start.x + fillSizeX;
			break;
		case ImGuiDir_Right:
			center.x = start.x + size.x - fillSizeX;
			break;
		case ImGuiDir_Up:
			center.y = start.y + fillSizeY;
			break;
		case ImGuiDir_Down:
			center.y = start.y + size.y - fillSizeY;
			break;
	}

	//Draw background and outline
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(
		ImVec2(center.x - fillSizeX/2 - 0.5, center.y - fillSizeY/2 - 0.5),
		ImVec2(center.x + fillSizeX/2 + 0.5, center.y + fillSizeY/2 + 0.5),
		ImGui::IsItemHovered() ? bgHovered : bgBase,
		rounding);
	draw_list->AddRect(
		ImVec2(center.x - lineSizeX/2 - 0.5, center.y - lineSizeY/2 - 0.5),
		ImVec2(center.x + lineSizeX/2 + 0.5, center.y + lineSizeY/2 + 0.5),
		lineColor,
		rounding);

	//Draw line to show split
	if( (splitDir == ImGuiDir_Left) || (splitDir == ImGuiDir_Right) )
	{
		draw_list->AddLine(
			ImVec2(center.x - 0.5, center.y - lineSizeY/2 - 0.5),
			ImVec2(center.x - 0.5, center.y + lineSizeY/2 + 0.5),
			lineColor);
	}
	else
	{
		draw_list->AddLine(
			ImVec2(center.x - lineSizeX/2 - 0.5, center.y - 0.5),
			ImVec2(center.x + lineSizeX/2 + 0.5, center.y - 0.5),
			lineColor);
	}
}

/**
	@brief Drop area for the middle of the plot

	Dropping a waveform in here adds it to the plot
 */
void WaveformArea::CenterDropArea(ImVec2 start, ImVec2 size)
{
	//Reject streams with incompatible Y axis units
	//TODO: display nice error message
	auto stream = m_parent->GetChannelBeingDragged();
	auto first = GetFirstAnalogOrEyeStream();
	if(first)
	{
		if(first.GetYAxisUnits() != stream.GetYAxisUnits())
			return;
	}

	ImGui::SetCursorScreenPos(start);
	ImGui::InvisibleButton("center", size);
	//ImGui::Button("center", size);
	ImGui::SetItemAllowOverlap();

	//Add drop target
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("Waveform", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		if( (payload != nullptr) && (payload->DataSize == sizeof(DragDescriptor)) )
		{
			auto desc = reinterpret_cast<DragDescriptor*>(payload->Data);
			auto sdrag = desc->first->GetStream(desc->second);

			//Add the new stream to us
			//TODO: copy view settings from the DisplayedChannel over?
			AddStream(sdrag);

			//Remove the stream from the originating waveform area
			desc->first->RemoveStream(desc->second);
		}

		ImGui::EndDragDropTarget();
	}

	//Draw overlay target
	const float rounding = max(3.0f, ImGui::GetStyle().FrameRounding);
	const ImU32 bgBase = ImGui::GetColorU32(ImGuiCol_DockingPreview, 0.70f);
	const ImU32 bgHovered = ImGui::GetColorU32(ImGuiCol_DockingPreview, 1.00f);
	const ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_NavWindowingHighlight, 0.60f);
	ImVec2 center(start.x + size.x/2, start.y + size.y/2);
	float fillSize = 34;
	float lineSize = 32;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(
		ImVec2(center.x - fillSize/2 - 0.5, center.y - fillSize/2 - 0.5),
		ImVec2(center.x + fillSize/2 + 0.5, center.y + fillSize/2 + 0.5),
		ImGui::IsItemHovered() ? bgHovered : bgBase,
		rounding);
	draw_list->AddRect(
		ImVec2(center.x - lineSize/2 - 0.5, center.y - lineSize/2 - 0.5),
		ImVec2(center.x + lineSize/2 + 0.5, center.y + lineSize/2 + 0.5),
		lineColor,
		rounding);

	//If trying to drop into the center marker, display warning if incompatible scales
	//(new signal has significantly wider range) and not a filter
	if(ImGui::IsItemHovered())
	{
		if(!stream)
			return;
		if(stream.GetType() != Stream::STREAM_TYPE_ANALOG)
			return;
		auto chan = stream.m_channel;
		if(dynamic_cast<Filter*>(chan) != nullptr)
			return;

		center.x += fillSize;
		DrawDropRangeMismatchMessage(draw_list, center, GetFirstAnalogStream(), stream);
	}
}

/**
	@brief Display a warning message about mismatched vertical scale
 */
void WaveformArea::DrawDropRangeMismatchMessage(
		ImDrawList* list,
		ImVec2 center,
		StreamDescriptor ourStream,
		StreamDescriptor theirStream)
{
	float ourRange = ourStream.GetVoltageRange();
	float theirRange = theirStream.GetVoltageRange();
	if(theirRange > 1.2*ourRange)
	{
		float warningSize = ImGui::GetFontSize() * 3;

		//Warning icon
		list->AddImage(
			m_parent->GetTextureManager()->GetTexture("warning"),
			ImVec2(center.x - 0.5, center.y - warningSize/2 - 0.5),
			ImVec2(center.x + warningSize + 0.5, center.y + warningSize/2 + 0.5));

		//Prepare to draw text
		center.x += warningSize;
		float fontHeight = ImGui::GetFontSize();
		auto font = m_parent->GetFontPref("Appearance.General.default_font");
		string str = "Caution: Potential for instrument damage!\n\n";
		str += string("The channel being dragged has a full-scale range of ") +
			theirStream.GetYAxisUnits().PrettyPrint(theirRange) + ",\n";
		str += string("but this plot has a full-scale range of ") +
			ourStream.GetYAxisUnits().PrettyPrint(ourRange) + ".\n\n";
		str += "Setting this channel to match the plot scale may result\n";
		str += "in overdriving the instrument input.";

		//Draw background for text
		float wrapWidth = 40 * fontHeight;
		auto textsize = font->CalcTextSizeA(
			fontHeight,
			FLT_MAX,
			wrapWidth,
			str.c_str());
		float padding = 5;
		float wrounding = 2;
		list->AddRectFilled(
			ImVec2(center.x, center.y - textsize.y/2 - padding),
			ImVec2(center.x + textsize.x + 2*padding, center.y + textsize.y/2 + padding),
			ImGui::GetColorU32(ImGuiCol_PopupBg),
			wrounding);

		//Draw the text
		list->AddText(
			font,
			fontHeight,
			ImVec2(center.x + padding, center.y - textsize.y/2),
			ImGui::GetColorU32(ImGuiCol_Text),
			str.c_str(),
			nullptr,
			wrapWidth);
	}
}

/**
	@brief Handles a button for a channel
 */
void WaveformArea::ChannelButton(shared_ptr<DisplayedChannel> chan, size_t index)
{
	auto stream = chan->GetStream();
	auto rchan = stream.m_channel;
	auto data = stream.GetData();

	//Foreground color is used to determine background color and hovered/active colors
	float bgmul = 0.2;
	float hmul = 0.4;
	float amul = 0.6;
	auto color = ColorFromString(rchan->m_displaycolor);
	auto fcolor = ImGui::ColorConvertU32ToFloat4(color);
	auto bcolor = ImGui::ColorConvertFloat4ToU32(ImVec4(fcolor.x*bgmul, fcolor.y*bgmul, fcolor.z*bgmul, fcolor.w) );
	auto hcolor = ImGui::ColorConvertFloat4ToU32(ImVec4(fcolor.x*hmul, fcolor.y*hmul, fcolor.z*hmul, fcolor.w) );
	auto acolor = ImGui::ColorConvertFloat4ToU32(ImVec4(fcolor.x*amul, fcolor.y*amul, fcolor.z*amul, fcolor.w) );

	//The actual button
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::PushStyleColor(ImGuiCol_Button, bcolor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hcolor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, acolor);
		ImGui::Button(chan->GetName().c_str());
	ImGui::PopStyleColor(4);

	if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		m_dragState = DRAG_STATE_CHANNEL;
		m_dragStream = stream;

		DragDescriptor desc(this, index);
		ImGui::SetDragDropPayload("Waveform", &desc, sizeof(desc));

		//Preview of what we're dragging
		ImGui::Text("Drag %s", chan->GetName().c_str());

		ImGui::EndDragDropSource();
	}

	if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		m_parent->ShowChannelProperties(rchan);

	//Display channel information and help text in tooltip
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		string tooltip;
		auto scope = rchan->GetScope();
		if(scope)
			tooltip += string("Channel ") + rchan->GetHwname() + " of instrument " + scope->m_nickname + "\n\n";

		//See if we have data
		if(data)
		{
			Unit samples(Unit::UNIT_SAMPLEDEPTH);
			tooltip += samples.PrettyPrint(data->size()) + "\n";

			if(dynamic_cast<UniformWaveformBase*>(data))
			{
				Unit rate(Unit::UNIT_SAMPLERATE);
				if(data->m_timescale > 1)
					tooltip += string("Uniformly sampled, ") + rate.PrettyPrint(FS_PER_SECOND / data->m_timescale) + "\n";
			}
			else
			{
				Unit fs(Unit::UNIT_FS);
				if(data->m_timescale > 1)
					tooltip += string("Sparsely sampled, ") + fs.PrettyPrint(data->m_timescale) + " resolution\n";
			}
			tooltip += "\n";
		}

		tooltip +=
			"Drag to move this waveform to another plot.\n"
			"Double click to view/edit channel properties.\n"
			"Right click for display settings menu.";

		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(tooltip.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	//Context menu
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Delete"))
			RemoveStream(index);
		ImGui::Separator();
		bool persist = chan->IsPersistenceEnabled();
		if(ImGui::MenuItem("Persistence", nullptr, persist))
			chan->SetPersistenceEnabled(!persist);
		ImGui::Separator();

		FilterMenu(chan);

		ImGui::EndPopup();
	}
}

/**
	@brief
 */
void WaveformArea::FilterMenu(shared_ptr<DisplayedChannel> chan)
{
	FilterSubmenu(chan, "Bus", Filter::CAT_BUS);
	FilterSubmenu(chan, "Clocking", Filter::CAT_CLOCK);
	FilterSubmenu(chan, "Generation", Filter::CAT_GENERATION);
	FilterSubmenu(chan, "Math", Filter::CAT_MATH);
	FilterSubmenu(chan, "Measurement", Filter::CAT_MEASUREMENT);
	FilterSubmenu(chan, "Memory", Filter::CAT_MEMORY);
	FilterSubmenu(chan, "Miscellaneous", Filter::CAT_MISC);
	FilterSubmenu(chan, "Power", Filter::CAT_POWER);
	FilterSubmenu(chan, "RF", Filter::CAT_RF);
	FilterSubmenu(chan, "Serial", Filter::CAT_SERIAL);
	FilterSubmenu(chan, "Signal integrity", Filter::CAT_ANALYSIS);
}

/**
	@brief Run the submenu for a single filter category
 */
void WaveformArea::FilterSubmenu(shared_ptr<DisplayedChannel> chan, const string& name, Filter::Category cat)
{
	auto& refs = m_parent->GetSession().GetReferenceFilters();
	auto stream = chan->GetStream();

	if(ImGui::BeginMenu(name.c_str()))
	{
		//Find all filters in this category and sort them alphabetically
		vector<string> sortedNames;
		for(auto it : refs)
		{
			if(it.second->GetCategory() == cat)
				sortedNames.push_back(it.first);
		}
		std::sort(sortedNames.begin(), sortedNames.end());

		//Do all of the menu items
		for(auto fname : sortedNames)
		{
			auto it = refs.find(fname);
			bool valid = false;
			if(it->second->GetInputCount() == 0)		//No inputs? Always valid
				valid = true;
			else
				valid = it->second->ValidateChannel(0, stream);

			//Hide import filters to avoid cluttering the UI
			if( (cat == Filter::CAT_GENERATION) && (fname.find("Import") != string::npos))
				continue;

			//TODO: measurements should have summary option

			if(ImGui::MenuItem(fname.c_str(), nullptr, false, valid))
				m_parent->CreateFilter(fname, this, stream);
		}

		ImGui::EndMenu();
	}
}

void WaveformArea::ClearPersistence()
{
	m_clearPersistence = true;
}

/**
	@brief Clear persistence iff we are displaying the specified channel

	TODO: can we clear *only* that channel and nothing else?
	TODO: clear if we have any dependency chain leading to the specified channel
 */
void WaveformArea::ClearPersistenceOfChannel(OscilloscopeChannel* chan)
{
	for(auto c : m_displayedChannels)
	{
		auto stream = c->GetStream();
		if(stream.m_channel == chan)
		{
			m_clearPersistence = true;
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input handling

void WaveformArea::OnMouseUp()
{
	switch(m_dragState)
	{
		case DRAG_STATE_Y_AXIS:
			LogTrace("End dragging Y axis\n");
			for(auto c : m_displayedChannels)
				c->GetStream().SetOffset(m_yAxisOffset);
			ClearPersistence();
			m_parent->SetNeedRender();
			break;

		case DRAG_STATE_TRIGGER_LEVEL:
			{
				Unit volts(Unit::UNIT_VOLTS);
				LogTrace("End dragging trigger level (at %s)\n", volts.PrettyPrint(m_triggerLevelDuringDrag).c_str());

				m_triggerDuringDrag->SetLevel(m_triggerLevelDuringDrag);
			}
			break;

		default:
			break;
	}

	m_dragState = DRAG_STATE_NONE;
}

void WaveformArea::OnDragUpdate()
{
	switch(m_dragState)
	{
		case DRAG_STATE_Y_AXIS:
			{
				float dy = ImGui::GetIO().MouseDelta.y * ImGui::GetWindowDpiScale();
				m_yAxisOffset -= PixelsToYAxisUnits(dy);

				//TODO: push to hardware at a controlled rate (after each trigger?)
			}
			break;

		case DRAG_STATE_TRIGGER_LEVEL:
			//TODO: push to hardware at a controlled rate (after each trigger?)
			break;

		default:
			break;
	}
}

/**
	@brief Handles a mouse wheel scroll step on the plot area
 */
void WaveformArea::OnMouseWheelPlotArea(float delta)
{
	//TODO: if shift is held, scroll horizontally

	int64_t target = m_group->XPositionToXAxisUnits(ImGui::GetIO().MousePos.x);

	//Zoom in
	if(delta > 0)
		m_group->OnZoomInHorizontal(target, pow(1.5, delta));
	else
		m_group->OnZoomOutHorizontal(target, pow(1.5, -delta));
}

/**
	@brief Handles a mouse wheel scroll step on the Y axis
 */
void WaveformArea::OnMouseWheelYAxis(float delta)
{
	if(delta > 0)
	{
		auto range = m_displayedChannels[0]->GetStream().GetVoltageRange();
		range *= pow(0.9, delta);

		for(size_t i=0; i<m_displayedChannels.size(); i++)
			m_displayedChannels[i]->GetStream().SetVoltageRange(range);
	}
	else
	{
		auto range = m_displayedChannels[0]->GetStream().GetVoltageRange();
		range /= pow(0.9, -delta);

		for(size_t i=0; i<m_displayedChannels.size(); i++)
			m_displayedChannels[i]->GetStream().SetVoltageRange(range);
	}

	ClearPersistence();
	m_parent->SetNeedRender();
}

/**
	@brief Gets the timestamp of our current waveform (if we have one)
 */
TimePoint WaveformArea::GetWaveformTimestamp()
{
	for(auto d : m_displayedChannels)
	{
		auto data = d->GetStream().GetData();
		if(data != nullptr)
			return TimePoint(data->m_startTimestamp, data->m_startFemtoseconds);
	}

	return TimePoint(0, 0);
}

/**
	@brief Checks if this area is compatible with a provided stream
 */
bool WaveformArea::IsCompatible(StreamDescriptor desc)
{
	//Can't go anywhere in our group if the X unit is different (e.g. frequency vs time)
	if(m_group->GetXAxisUnit() != desc.GetXAxisUnits())
		return false;

	//Can't go in this area if the Y unit is different
	if(m_yAxisUnit != desc.GetYAxisUnits())
		return false;

	//TODO: can't mix eye and non-eye

	//All good if we get here
	return true;
}
