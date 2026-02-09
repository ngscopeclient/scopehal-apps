/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
#include "../../scopeprotocols/ConstellationFilter.h"
#include "../../scopeprotocols/EyePattern.h"
#include "../../scopeprotocols/SpectrogramFilter.h"
#include "../../scopeprotocols/Waterfall.h"
#include "../../scopehal/DensityFunctionWaveform.h"

#include "imgui_internal.h"	//for SetItemUsingMouseWheel

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DisplayedChannel

DisplayedChannel::DisplayedChannel(StreamDescriptor stream, Session& session)
		: m_colorRamp("eye-gradient-viridis")
		, m_stream(stream)
		, m_session(session)
		, m_rasterizedWaveform("DisplayedChannel.m_rasterizedWaveform")
		, m_indexBuffer("DisplayedChannel.m_indexBuffer")
		, m_rasterizedX(0)
		, m_rasterizedY(0)
		, m_cachedX(0)
		, m_cachedY(0)
		, m_persistenceEnabled(false)
		, m_yButtonPos(0)
{
	auto schan = dynamic_cast<OscilloscopeChannel*>(stream.m_channel);
	if(schan)
		schan->AddRef();

	vk::CommandPoolCreateInfo cmdPoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		session.GetMainWindow()->GetRenderQueue()->m_family );
	m_utilCmdPool = std::make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, cmdPoolInfo);
	vk::CommandBufferAllocateInfo bufinfo(**m_utilCmdPool, vk::CommandBufferLevel::ePrimary, 1);

	m_utilCmdBuffer = make_unique<vk::raii::CommandBuffer>(
			std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Use GPU-side memory for rasterized waveform
	//TODO: instead of using CPU-side mirror, use a shader to memset it when clearing?
	m_rasterizedWaveform.SetCpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);
	m_rasterizedWaveform.SetGpuAccessHint(AcceleratorBuffer<float>::HINT_LIKELY);

	//Create tone map pipeline depending on waveform type
	switch(m_stream.GetType())
	{
		case Stream::STREAM_TYPE_EYE:
			m_toneMapPipe = make_shared<ComputePipeline>(
				"shaders/EyeToneMap.spv", 1, sizeof(EyeToneMapArgs), 1, 1);
			break;

		case Stream::STREAM_TYPE_CONSTELLATION:
			m_toneMapPipe = make_shared<ComputePipeline>(
				"shaders/ConstellationToneMap.spv", 1, sizeof(ConstellationToneMapArgs), 1, 1);
			break;

		case Stream::STREAM_TYPE_WATERFALL:
			m_toneMapPipe = make_shared<ComputePipeline>(
				"shaders/WaterfallToneMap.spv", 1, sizeof(WaterfallToneMapArgs), 1, 1);
			break;

		case Stream::STREAM_TYPE_SPECTROGRAM:
			m_toneMapPipe = make_shared<ComputePipeline>(
				"shaders/SpectrogramToneMap.spv", 1, sizeof(SpectrogramToneMapArgs), 1, 1);
			break;

		default:
			m_toneMapPipe = make_shared<ComputePipeline>(
				"shaders/WaveformToneMap.spv", 1, sizeof(WaveformToneMapArgs), 1);
	}

	//If we have native int64 support we can do the index search for sparse waveforms on the GPU
	if(g_hasShaderInt64)
	{
		m_indexSearchComputePipeline = make_shared<ComputePipeline>(
				"shaders/IndexSearch.spv", 2, sizeof(IndexSearchConstants));

		//Use GPU local memory for index buffer
		m_indexBuffer.SetCpuAccessHint(AcceleratorBuffer<uint32_t>::HINT_LIKELY);
		m_indexBuffer.SetGpuAccessHint(AcceleratorBuffer<uint32_t>::HINT_LIKELY);
	}
	else
	{
		//Use pinned memory for index buffer since it should only be read once
		m_indexBuffer.SetCpuAccessHint(AcceleratorBuffer<uint32_t>::HINT_LIKELY);
		m_indexBuffer.SetGpuAccessHint(AcceleratorBuffer<uint32_t>::HINT_UNLIKELY);
	}
}

DisplayedChannel::~DisplayedChannel()
{
	auto schan = dynamic_cast<OscilloscopeChannel*>(m_stream.m_channel);
	if(schan)
	{
		//Remove pausable filters from trigger group when they're deleted
		//TODO: potential race condition here?
		auto pf = dynamic_cast<PausableFilter*>(schan);
		if(pf && (pf->GetRefCount() == 1))
		{
			LogTrace("Deleting last copy of pausable filter, removing from trigger group\n");
			m_session.GetTriggerGroupForFilter(pf)->RemoveFilter(pf);
		}

		auto scope = schan->GetScope();
		schan->Release();

		//If the channel is part of an instrument and we turned it off, we may have changed the set of sample rates
		//Refresh the sidebar!
		//TODO: make this more narrow and only refresh this scope etc?
		if( (scope != nullptr) && !schan->IsEnabled())
			m_session.GetMainWindow()->RefreshStreamBrowserDialog();
	}
}

/**
	@brief Handles a change in size of the displayed waveform

	@param newSize	New size of WaveformArea

	@return true if size has changed, false otherwise
 */
bool DisplayedChannel::UpdateSize(ImVec2 newSize, MainWindow* top)
{
	size_t x = newSize.x;
	size_t y = newSize.y;

	//Special processing needed for eyes coming from BERTs or sampling scopes
	//These can change size on their own even if the window isn't resized
	auto eye = dynamic_cast<EyePattern*>(m_stream.m_channel);
	auto constellation = dynamic_cast<ConstellationFilter*>(m_stream.m_channel);
	auto waterfall = dynamic_cast<Waterfall*>(m_stream.m_channel);
	auto data = m_stream.GetData();
	auto eyedata = dynamic_cast<EyeWaveform*>(data);
	if( (m_stream.GetType() == Stream::STREAM_TYPE_EYE) && eyedata && !eye)
	{
		x = eyedata->GetWidth();
		y = eyedata->GetHeight();
		if( (m_cachedX != x) || (m_cachedY != y) )
			LogTrace("Hardware eye resolution changed, processing resize\n");
	}

	if( (m_cachedX != x) || (m_cachedY != y) )
	{
		m_cachedX = x;
		m_cachedY = y;

		//Don't actually create an image object if the image is degenerate (zero pixels)
		if( (x == 0) || (y == 0) )
			return true;

		//If this is an eye pattern filter, need to reallocate the texture
		//To avoid constantly destroying the integrated eye if we slightly resize stuff, round up to next power of 2
		size_t roundedX = pow(2, ceil(log2(x)));
		size_t roundedY = pow(2, ceil(log2(y)));
		if(eye)
		{
			if( (eye->GetWidth() != roundedX) || (eye->GetHeight() != roundedY) )
			{
				eye->SetWidth(roundedX);
				eye->SetHeight(roundedY);
				eye->Refresh(*m_utilCmdBuffer, m_session.GetMainWindow()->GetRenderQueue());
			}

			x = roundedX;
			y = roundedY;
		}

		//Eyes coming from BERTs, sampling scopes, etc cannot be reallocated
		//TODO: what about if someone else makes their own filter that outputs eyes?
		else if(m_stream.GetType() == Stream::STREAM_TYPE_EYE)
		{
			if(eyedata)
			{
				x = eyedata->GetWidth();
				y = eyedata->GetHeight();
			}
		}

		//Waterfalls only care about height
		else if(waterfall)
		{
			if(waterfall->GetHeight() != roundedY)
			{
				waterfall->SetHeight(roundedY);

				FilterGraphExecutor ex;
				set<FlowGraphNode*> nodesToUpdate;
				nodesToUpdate.emplace(waterfall);
				ex.RunBlocking(nodesToUpdate);
			}

			//Rendered image should be the actual plot size
		}

		else if(constellation)
		{
			if( (constellation->GetWidth() != roundedX) || (constellation->GetHeight() != roundedY) )
			{
				constellation->SetWidth(roundedX);
				constellation->SetHeight(roundedY);

				FilterGraphExecutor ex;
				set<FlowGraphNode*> nodesToUpdate;
				nodesToUpdate.emplace(constellation);
				ex.RunBlocking(nodesToUpdate);
			}

			x = roundedX;
			y = roundedY;
		}

		LogTrace("Displayed channel resized (to %zu x %zu), reallocating texture\n", x, y);

		//NOTE: Assumes the render queue is also capable of transfers (see QueueManager)
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
			vk::SharingMode::eExclusive,
			{},
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
		g_vkTransferQueue->SubmitAndBlock(*g_vkTransferCommandBuffer);

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
		//TODO: do this in a shader
		m_rasterizedWaveform.PrepareForCpuAccess();
		memset(m_rasterizedWaveform.GetCpuPointer(), 0, npixels * sizeof(float));
		m_rasterizedWaveform.MarkModifiedFromCpu();
	}

	//Allocate index buffer for sparse waveforms
	if(!IsDensePacked())
		m_indexBuffer.resize(x);
}

/**
	@brief Serializes the configuration for this channel
 */
YAML::Node DisplayedChannel::Serialize(IDTable& table) const
{
	YAML::Node node;

	node["persistence"] = m_persistenceEnabled;
	node["channel"] = table[m_stream.m_channel];
	node["stream"] = m_stream.m_stream;
	node["colorRamp"] = m_colorRamp;

	return node;
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
	, m_group(group)
	, m_parent(parent)
	, m_tLastMouseMove(GetTime())
	, m_mouseOverTriggerArrow(false)
	, m_mouseOverBERTarget(false)
	, m_triggerLevelDuringDrag(0)
	, m_xAxisPosDuringDrag(0)
	, m_triggerDuringDrag(nullptr)
	, m_bertChannelDuringDrag(nullptr)
	, m_lastRightClickOffset(0)
	, m_channelButtonHeight(0)
	, m_dragPeakLabel(nullptr)
	, m_mouseOverButton(false)
	, m_yAxisCursorMode(Y_CURSOR_NONE)
{
	m_yAxisCursorPositions[0] = 0;
	m_yAxisCursorPositions[1] = 0;

	m_displayedChannels.push_back(make_shared<DisplayedChannel>(stream, m_parent->GetSession()));
}

WaveformArea::~WaveformArea()
{
	m_displayedChannels.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization

void WaveformArea::SerializeConfiguration(YAML::Node& node)
{
	YAML::Node ycursors;

	switch(m_yAxisCursorMode)
	{
		case Y_CURSOR_SINGLE:
			ycursors["mode"] = "single";
			break;

		case Y_CURSOR_DUAL:
			ycursors["mode"] = "dual";
			break;

		case Y_CURSOR_NONE:
		default:
			ycursors["mode"] = "none";
			break;
	}

	ycursors["pos0"] = m_yAxisCursorPositions[0];
	ycursors["pos1"] = m_yAxisCursorPositions[1];

	node["ycursors"] = ycursors;
}

void WaveformArea::LoadConfiguration(YAML::Node& node)
{
	auto cursornode = node["ycursors"];
	if(cursornode)
	{
		m_yAxisCursorPositions[0] = cursornode["pos0"].as<float>();
		m_yAxisCursorPositions[1] = cursornode["pos1"].as<float>();

		auto mode = cursornode["mode"].as<string>();
		if(mode == "single")
			m_yAxisCursorMode = Y_CURSOR_SINGLE;
		else if(mode == "dual")
			m_yAxisCursorMode = Y_CURSOR_DUAL;
		else
			m_yAxisCursorMode = Y_CURSOR_NONE;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stream management

/**
	@brief Adds a new stream to this plot
 */
void WaveformArea::AddStream(StreamDescriptor desc, bool persistence, const string& ramp)
{
	auto chan = make_shared<DisplayedChannel>(desc, m_parent->GetSession());
	chan->SetPersistenceEnabled(persistence);
	chan->m_colorRamp = ramp;
	m_displayedChannels.push_back(chan);
}

/**
	@brief Removes the stream at a specified index
 */
void WaveformArea::RemoveStream(size_t i)
{
	//See if we're dragging the stream being removed (i.e. we just dropped it somewhere).
	//If so, immediately stop all drag activity.
	//This normally happens in Render() but if we're in an off-screen tab, it might be indefinitely delayed
	//until we re-activate that tab and we want the drag/drop overlays to disappear immediately
	//(https://github.com/ngscopeclient/scopehal-apps/issues/651)
	auto stream = m_displayedChannels[i]->GetStream();
	if( (m_dragState == DRAG_STATE_CHANNEL) && (stream == m_dragStream) )
		m_dragState = DRAG_STATE_NONE;

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
	@brief Returns the first analog, spectrogram or eye pattern stream displayed in this area.

	(this really means "has a useful Y axis")

	If no suitable waveforms returns a null stream.
 */
StreamDescriptor WaveformArea::GetFirstAnalogOrDensityStream()
{
	for(auto chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(stream.GetType() == Stream::STREAM_TYPE_ANALOG)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_SPECTROGRAM)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_EYE)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_CONSTELLATION)
			return stream;
	}

	return StreamDescriptor(nullptr, 0);
}

/**
	@brief Returns the first eye pattern displayed in this area.

	If no eye patterns are visible, returns a null stream.
 */
StreamDescriptor WaveformArea::GetFirstEyeStream()
{
	for(auto chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(stream.GetType() == Stream::STREAM_TYPE_EYE)
			return stream;
	}

	return StreamDescriptor(nullptr, 0);
}

/**
	@brief Returns the first constellation diagram displayed in this area.

	If no constellation diagrams are visible, returns a null stream.
 */
StreamDescriptor WaveformArea::GetFirstConstellationStream()
{
	for(auto chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(stream.GetType() == Stream::STREAM_TYPE_CONSTELLATION)
			return stream;
	}

	return StreamDescriptor(nullptr, 0);
}

/**
	@brief Returns the first density plot displayed in this area.

	If none are visible, returns a null stream.
 */
StreamDescriptor WaveformArea::GetFirstDensityFunctionStream()
{
	for(auto chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(stream.GetType() == Stream::STREAM_TYPE_EYE)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_CONSTELLATION)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_SPECTROGRAM)
			return stream;
		if(stream.GetType() == Stream::STREAM_TYPE_WATERFALL)
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
	@brief Returns true if a channel is currently being dragged
 */
bool WaveformArea::IsChannelBeingDragged()
{
	return (m_dragState == DRAG_STATE_CHANNEL) || (m_dragState == DRAG_STATE_CHANNEL_LAST);
}

/**
	@brief Returns the channel being dragged, if one exists
 */
StreamDescriptor WaveformArea::GetChannelBeingDragged()
{
	if(IsChannelBeingDragged())
		return m_dragStream;
	else
		return StreamDescriptor(nullptr, 0);
}

/**
	@brief Renders a waveform area

	Returns false if the area should be closed (no more waveforms visible in it)
 */
bool WaveformArea::Render(int iArea, int numAreas, ImVec2 clientArea)
{
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

	float totalHeightAvailable = floor(clientArea.y - 2*ImGui::GetFrameHeightWithSpacing());
	float spacing = m_group->GetSpacing();
	float heightPerArea = totalHeightAvailable / numAreas;
	float totalSpacing = (numAreas-1)*spacing;
	float unspacedHeightPerArea = floor( (totalHeightAvailable - totalSpacing) / numAreas);
	unspacedHeightPerArea -= ImGui::GetStyle().FramePadding.y;
	if(numAreas == 1)
		unspacedHeightPerArea = heightPerArea;

	//Update cached scale
	m_height = unspacedHeightPerArea;
	auto first = GetFirstAnalogOrDensityStream();
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
	float vbot = 0.0f;
	float vtop = 0.0f;

	auto cpos = ImGui::GetCursorPos();
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
		ImGui::Dummy(ImVec2(csize.x, csize.y));
		ImGui::SetNextItemAllowOverlap();

		//Check for context menu if we didn't do one yet
		PlotContextMenu();

		//Draw actual waveforms (and protocol decode overlays)
		RenderWaveforms(pos, csize);

		ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
		ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
		if(ImGui::IsItemHovered())
		{
			auto wheel = ImGui::GetIO().MouseWheel;
			auto wheel_h = ImGui::GetIO().MouseWheelH;
			if((wheel != 0) || (wheel_h != 0))
				OnMouseWheelPlotArea(wheel, wheel_h);
		}

		//Make targets for drag-and-drop
		if(ImGui::GetDragDropPayload() != nullptr)
			DragDropOverlays(pos, csize, iArea, numAreas);

		//Cursors have to be drawn over the waveform
		RenderCursors(pos, csize);
		RenderBERSamplingPoint(pos, csize);

		//Make sure all channels have same vertical scale and warn if not
		CheckForScaleMismatch(pos, csize);

		//Tooltip for eye pattern
		RenderEyePatternTooltip(pos, csize);

		//Draw control widgets
		m_mouseOverButton = false;
		ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
		ImGui::BeginGroup();

			for(size_t i=0; i<m_displayedChannels.size(); i++)
				ChannelButton(m_displayedChannels[i], i);

		ImGui::EndGroup();
		ImGui::SetNextItemAllowOverlap();
	}
	else
		m_mouseOverButton = false;
	ImGui::EndChild();

	//Handle help messages
	if(ImGui::IsItemHovered() && !m_mouseOverButton)
		m_parent->AddStatusHelp("mouse_wheel", "Zoom horizontal axis");

	//Draw the vertical scale on the right side of the plot
	RenderYAxis(ImVec2(yAxisWidth, unspacedHeightPerArea), gridmap, vbot, vtop);

	//Render the Y axis cursors (if we have any) over the top of everything else
	{
		auto csize = ImGui::GetContentRegionAvail();
		auto pos = ImGui::GetWindowPos();
		ImGui::SetCursorPos(cpos);
		RenderYAxisCursors(pos, csize, yAxisWidth);
	}

	//Cursor should now be at end of window
	ImGui::SetCursorPos(ImVec2(cpos.x, cpos.y + unspacedHeightPerArea));

	//Add spacing
	if(iArea != numAreas-1)
		ImGui::Dummy(ImVec2(0, spacing));

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
	@brief Render horizontal cursors over the plot
 */
void WaveformArea::RenderYAxisCursors(ImVec2 pos, ImVec2 size, float yAxisWidth)
{
	//No cursors? Nothing to do
	if(m_yAxisCursorMode == Y_CURSOR_NONE)
	{
		//Exit cursor drag state if we no longer have a cursor to drag
		if( (m_dragState == DRAG_STATE_Y_CURSOR0) || (m_dragState == DRAG_STATE_Y_CURSOR1) )
			m_dragState = DRAG_STATE_NONE;
		return;
	}

	//Create a child window for all of our drawing
	//(this is needed so we're above the WaveformArea content in z order, but behind popup windows)
	if(ImGui::BeginChild("ycursors", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoInputs))
	{
		auto list = ImGui::GetWindowDrawList();

		auto& prefs = m_parent->GetSession().GetPreferences();
		auto cursor0_color = prefs.GetColor("Appearance.Cursors.cursor_1_color");
		auto cursor1_color = prefs.GetColor("Appearance.Cursors.cursor_2_color");
		auto fill_color = prefs.GetColor("Appearance.Cursors.cursor_fill_color");
		auto font = m_parent->GetFontPref("Appearance.Cursors.label_font");
		ImGui::PushFont(font.first, font.second);

		float ypos0 = round(YAxisUnitsToYPosition(m_yAxisCursorPositions[0]));
		float ypos1 = round(YAxisUnitsToYPosition(m_yAxisCursorPositions[1]));

		//Fill between if dual cursor
		if(m_yAxisCursorMode == Y_CURSOR_DUAL)
			list->AddRectFilled(ImVec2(pos.x, ypos0), ImVec2(pos.x + size.x, ypos1), fill_color);

		//First cursor
		list->AddLine(ImVec2(pos.x, ypos0), ImVec2(pos.x + size.x, ypos0), cursor0_color, 1);

		//Text
		//Anchor bottom left at the cursor
		auto str = string("Y1: ") + m_yAxisUnit.PrettyPrint(m_yAxisCursorPositions[0]);
		auto tsize = ImGui::CalcTextSize(str.c_str());
		float padding = 2;
		float wrounding = 2;
		float textTop = ypos0 - (3*padding + tsize.y);
		float plotRight = pos.x + size.x - yAxisWidth;
		float textLeft = plotRight - (2*padding + tsize.x);
		list->AddRectFilled(
			ImVec2(textLeft, textTop - padding ),
			ImVec2(plotRight, ypos0 - padding),
			ImGui::GetColorU32(ImGuiCol_PopupBg),
			wrounding);
		list->AddText(
			ImVec2(textLeft + padding, textTop + padding),
			cursor0_color,
			str.c_str());

		//Second cursor
		if(m_yAxisCursorMode == Y_CURSOR_DUAL)
		{
			list->AddLine(ImVec2(pos.x, ypos1), ImVec2(pos.x + size.x, ypos1), cursor1_color, 1);

			float delta = m_yAxisCursorPositions[0] - m_yAxisCursorPositions[1];
			str = string("Y2: ") + m_yAxisUnit.PrettyPrint(m_yAxisCursorPositions[1]) + "\n" +
				"ΔY = " + m_yAxisUnit.PrettyPrint(delta);

			//Text
			tsize = ImGui::CalcTextSize(str.c_str());
			textTop = ypos1 - (3*padding + tsize.y);
			textLeft = plotRight - (2*padding + tsize.x);
			list->AddRectFilled(
				ImVec2(textLeft, textTop - padding ),
				ImVec2(plotRight, ypos1 - padding),
				ImGui::GetColorU32(ImGuiCol_PopupBg),
				wrounding);
			list->AddText(
				ImVec2(textLeft + padding, textTop + padding),
				cursor1_color,
				str.c_str());
		}

		//not dragging if we no longer have a second cursor
		else if(m_dragState == DRAG_STATE_Y_CURSOR1)
			m_dragState = DRAG_STATE_NONE;

		//TODO: text for value readouts, in-band power, etc?

		ImGui::PopFont();
	}
	ImGui::EndChild();

	//Default help text related to cursors (may change if we're over a cursor)
	if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
		!m_mouseOverButton && (m_dragState == DRAG_STATE_NONE) )
	{
		if(m_yAxisCursorMode != Y_CURSOR_NONE)
			m_parent->AddStatusHelp("mouse_lmb", "Place first cursor");
		if(m_yAxisCursorMode == Y_CURSOR_DUAL)
			m_parent->AddStatusHelp("mouse_lmb_drag", "Place second cursor");
	}
	if( (m_dragState == DRAG_STATE_Y_CURSOR0) || (m_dragState == DRAG_STATE_Y_CURSOR1) )
		m_parent->AddStatusHelp("mouse_lmb_drag", "Move cursor");

	//Child window doesn't get mouse events (this flag is needed so we can pass mouse events to the WaveformArea's)
	//So we have to do all of our interaction processing inside the top level window
	DoCursor(0, DRAG_STATE_Y_CURSOR0);
	if(m_yAxisCursorMode == Y_CURSOR_DUAL)
		DoCursor(1, DRAG_STATE_Y_CURSOR1);

	//If not currently dragging, a click places cursor 0 and starts dragging cursor 1 (if enabled)
	//Don't process this if a popup is open
	if( ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
		(m_dragState == DRAG_STATE_NONE) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!m_mouseOverButton &&
		!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
	{
		auto ypos = ImGui::GetMousePos().y;

		m_yAxisCursorPositions[0] = YPositionToYAxisUnits(ypos);
		if(m_yAxisCursorMode == Y_CURSOR_DUAL)
		{
			m_dragState = DRAG_STATE_Y_CURSOR1;
			m_yAxisCursorPositions[1] = m_yAxisCursorPositions[0];
		}
		else
			m_dragState = DRAG_STATE_Y_CURSOR0;
	}

	//Cursor 0 should always be above cursor 1 (if both are enabled).
	//If they get swapped, exchange them.
	if( (m_yAxisCursorPositions[0] < m_yAxisCursorPositions[1]) && (m_yAxisCursorMode == Y_CURSOR_DUAL) )
	{
		//Swap the cursors themselves
		float tmp = m_yAxisCursorPositions[0];
		m_yAxisCursorPositions[0] = m_yAxisCursorPositions[1];
		m_yAxisCursorPositions[1] = tmp;

		//If dragging one cursor, switch to dragging the other
		if(m_dragState == DRAG_STATE_Y_CURSOR0)
			m_dragState = DRAG_STATE_Y_CURSOR1;
		else if(m_dragState == DRAG_STATE_Y_CURSOR1)
			m_dragState = DRAG_STATE_Y_CURSOR0;
	}
}

/**
	@brief Run interaction processing for dragging a Y axis cursor
 */
void WaveformArea::DoCursor(int iCursor, DragState state)
{
	float ypos = round(YAxisUnitsToYPosition(m_yAxisCursorPositions[iCursor]));
	float searchRadius = 0.5 * ImGui::GetFontSize();

	//Check if the mouse hit us
	auto mouse = ImGui::GetMousePos();
	if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !m_mouseOverButton)
	{
		if( fabs(mouse.y - ypos) < searchRadius)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			m_parent->AddStatusHelp("mouse_lmb", "");
			m_parent->AddStatusHelp("mouse_lmb_drag", "Move cursor");

			//Start dragging if clicked
			if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				m_dragState = state;
		}
	}

	//If dragging, move the cursor to track
	if(m_dragState == state)
	{
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			m_dragState = DRAG_STATE_NONE;
		m_yAxisCursorPositions[iCursor] = YPositionToYAxisUnits(mouse.y);
	}
}

/**
	@brief Run the context menu for the main plot area
 */
void WaveformArea::PlotContextMenu()
{
	if(ImGui::BeginPopupContextWindow())
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
			{
				markers.erase(markers.begin() + selectedMarker);
				m_parent->GetSession().OnMarkerChanged();
			}
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
					if(ImGui::MenuItem("None", nullptr, (m_yAxisCursorMode == Y_CURSOR_NONE)))
						m_yAxisCursorMode = Y_CURSOR_NONE;
					if(ImGui::MenuItem("Single", nullptr, (m_yAxisCursorMode == Y_CURSOR_SINGLE)))
						m_yAxisCursorMode = Y_CURSOR_SINGLE;
					if(ImGui::MenuItem("Dual", nullptr, (m_yAxisCursorMode == Y_CURSOR_DUAL)))
						m_yAxisCursorMode = Y_CURSOR_DUAL;

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
	vector<size_t> displayedChannelsToRemove;

	for(size_t i=0; i<m_displayedChannels.size(); i++)
	{
		auto& chan = m_displayedChannels[i];

		//Make sure the stream exists. If it was removed (filter config changed, etc) it may no longer be there
		if(chan->GetStream().IsOutOfRange())
		{
			m_channelsToRemove.push_back(m_displayedChannels[i]);
			displayedChannelsToRemove.push_back(i);
			continue;
		}

		auto stream = chan->GetStream();

		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
				RenderAnalogWaveform(chan, start, size);
				break;

			case Stream::STREAM_TYPE_EYE:
				RenderEyeWaveform(chan, start, size);
				break;

			case Stream::STREAM_TYPE_CONSTELLATION:
				RenderConstellationWaveform(chan, start, size);
				break;

			case Stream::STREAM_TYPE_WATERFALL:
				RenderWaterfallWaveform(chan, start, size);
				break;

			case Stream::STREAM_TYPE_SPECTROGRAM:
				RenderSpectrogramWaveform(chan, start, size);
				break;

			case Stream::STREAM_TYPE_DIGITAL:
				RenderDigitalWaveform(chan, start, size);
				break;

			case Stream::STREAM_TYPE_PROTOCOL:
				RenderProtocolWaveform(chan, start, size);
				break;

			//nothing to draw, it's not a waveform (shouldn't even be here)
			case Stream::STREAM_TYPE_ANALOG_SCALAR:
				break;

			default:
				LogWarning("Unimplemented stream type %d, don't know how to render it\n", stream.GetType());
				break;
		}
	}

	if(!displayedChannelsToRemove.empty())
	{
		for(ssize_t i=displayedChannelsToRemove.size()-1; i>=0; i--)
			m_displayedChannels.erase(m_displayedChannels.begin() + displayedChannelsToRemove[i]);
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

	//If it's a peak detection filter, draw the peaks and annotations
	auto pf = dynamic_cast<PeakDetectionFilter*>(stream.m_channel);
	if(pf)
		RenderSpectrumPeaks(list, channel);
}

/**
	@brief Renders a single waterfall
 */
void WaveformArea::RenderWaterfallWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Mark the waveform as resized
	if(channel->UpdateSize(size, m_parent))
	{
		m_parent->SetNeedRender();
		if(data != stream.GetData())
			return;
	}

	//Render the tone mapped output (if we have it)
	auto tex = channel->GetTexture();
	if(tex != nullptr)
		list->AddImage(tex->GetTexture(), start, ImVec2(start.x+size.x, start.y+size.y), ImVec2(0, 1), ImVec2(1, 0) );
}

/**
	@brief Renders a single spectrogram (same as waterfall for now)
 */
void WaveformArea::RenderSpectrogramWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Mark the waveform as resized
	if(channel->UpdateSize(size, m_parent))
	{
		m_parent->SetNeedRender();
		if(data != stream.GetData())
			return;
	}

	//Render the tone mapped output (if we have it)
	auto tex = channel->GetTexture();
	if(tex != nullptr)
		list->AddImage(tex->GetTexture(), start, ImVec2(start.x+size.x, start.y+size.y), ImVec2(0, 1), ImVec2(1, 0) );
}

/**
	@brief Renders a single eye pattern
 */
void WaveformArea::RenderEyeWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Mark the waveform as resized
	if(channel->UpdateSize(size, m_parent))
	{
		m_parent->SetNeedRender();
		if(data != stream.GetData())
			return;
	}

	//Render the tone mapped output (if we have it)
	auto tex = channel->GetTexture();
	if(tex != nullptr)
		list->AddImage(tex->GetTexture(), start, ImVec2(start.x+size.x, start.y+size.y), ImVec2(0, 1), ImVec2(1, 0) );

	//Draw the mask (if there is one)
	auto eye = dynamic_cast<EyePattern*>(stream.m_channel);
	auto edata = dynamic_cast<EyeWaveform*>(data);
	auto bichan = dynamic_cast<BERTInputChannel*>(stream.m_channel);
	if(eye || bichan)
	{
		auto& prefs = m_parent->GetSession().GetPreferences();
		auto color = prefs.GetColor("Appearance.Eye Patterns.mask_color");
		auto borderpass = prefs.GetColor("Appearance.Eye Patterns.border_color_pass");
		auto borderfailed = prefs.GetColor("Appearance.Eye Patterns.border_color_fail");

		auto& mask = eye ? eye->GetMask() : bichan->GetMask();
		auto polygons = mask.GetPolygons();
		bool relative = mask.IsTimebaseRelative();
		bool failed = edata->GetMaskHitRate() > mask.GetAllowedHitRate();
		for(auto poly : polygons)
		{
			//NOTE: must use clockwise winding for correct antialiasing
			vector<ImVec2> points;

			for(auto point : poly.m_points)
			{
				float x = 0;
				float y = YAxisUnitsToYPosition(point.m_voltage);

				//Ratiometric, scaled by UI width
				if(relative)
					x = m_group->XAxisUnitsToXPosition(point.m_time * edata->GetUIWidth());

				//Absolute
				else
					x = m_group->XAxisUnitsToXPosition(point.m_time);

				points.push_back(ImVec2(x, y));
			}

			list->AddConvexPolyFilled(&points[0], points.size(), color);

			//Form a closed loop
			points.push_back(points[0]);

			if(failed)
				list->AddPolyline(&points[0], points.size(), borderfailed, 0, 1);
			else
				list->AddPolyline(&points[0], points.size(), borderpass, 0, 1);
		}
	}
}

/**
	@brief Renders a single constellation diagram
 */
void WaveformArea::RenderConstellationWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Mark the waveform as resized
	if(channel->UpdateSize(size, m_parent))
	{
		m_parent->SetNeedRender();
		if(data != stream.GetData())
			return;
	}

	//Render the tone mapped output (if we have it)
	auto tex = channel->GetTexture();
	if(tex != nullptr)
		list->AddImage(tex->GetTexture(), start, ImVec2(start.x+size.x, start.y+size.y), ImVec2(0, 1), ImVec2(1, 0) );

	//Draw nominal point locations
	auto cfilt = dynamic_cast<ConstellationFilter*>(stream.m_channel);
	if(cfilt)
	{
		auto& prefs = m_parent->GetSession().GetPreferences();
		auto& points = cfilt->GetNominalPoints();

		auto color = prefs.GetColor("Appearance.Constellations.point_color");

		//TODO: dynamic size?
		float pointsize = ImGui::GetFontSize() * 0.5;

		for(auto p : points)
		{
			list->AddCircle(
				ImVec2( m_group->XAxisUnitsToXPosition(p.m_xval),  YAxisUnitsToYPosition(p.m_yval) ),
				pointsize,
				color,
				0,
				2);
		}
	}
}

/**
	@brief Computes the closest point on a line segment (given the endpoints) to a given point.

	Reference: https://en.wikibooks.org/wiki/Linear_Algebra/Orthogonal_Projection_Onto_a_Line
 */
ImVec2 WaveformArea::ClosestPointOnLineSegment(ImVec2 lineA, ImVec2 lineB, ImVec2 pt)
{
	//Offset the line and point so the first endpoint of the line is at the origin
	ImVec2 vseg(lineB.x - lineA.x, lineB.y - lineA.y);
	ImVec2 vpt(pt.x - lineA.x, pt.y - lineA.y);

	//Project the point onto the line (may be beyond endpoints)
	float scale = (vseg.x*vpt.x + vseg.y*vpt.y) / (vseg.x*vseg.x + vseg.y*vseg.y);
	ImVec2 vproj(vseg.x*scale, vseg.y*scale);

	//Clamp to endpoints
	if(scale < 0)
		return lineA;
	if(scale > 1)
		return lineB;

	//Otherwise return the projection
	return ImVec2(vproj.x + lineA.x, vproj.y + lineA.y);
}

/**
	@brief Draw peaks from a FFT or similar waveform
 */
void WaveformArea::RenderSpectrumPeaks(ImDrawList* list, shared_ptr<DisplayedChannel> channel)
{
	auto stream = channel->GetStream();
	auto& peaks = dynamic_cast<PeakDetectionFilter*>(stream.m_channel)->GetPeaks();

	//TODO: add a preference for peak circle color and size?
	ImU32 circleColor = 0xffffffff;
	ImU32 lineColor = ColorFromString("#00ff00ff");
	float radius = ImGui::GetFontSize() * 0.5;

	//Distance within which two peaks are considered to be the same
	float neighborThresholdPixels = 3 * ImGui::GetFontSize();
	int64_t neighborThresholdXUnits = m_group->PixelsToXAxisUnits(neighborThresholdPixels);

	//Go through the list of peaks and decay all of the alpha values
	vector<size_t> peaksToDelete;
	for(size_t i=0; i<channel->m_peakLabels.size(); i++)
	{
		channel->m_peakLabels[i].m_peakAlpha -= 4;
		if(channel->m_peakLabels[i].m_peakAlpha < -255)
		{
			peaksToDelete.push_back(i);

			//Stop dragging if we're deleting it
			if( (&channel->m_peakLabels[i] == m_dragPeakLabel) && (m_dragState = DRAG_STATE_PEAK_MARKER) )
			{
				m_dragState = DRAG_STATE_NONE;
				m_dragPeakLabel = nullptr;
			}
		}
	}
	if(!peaksToDelete.empty())
	{
		for(ssize_t n=peaksToDelete.size() - 1; n >= 0; n--)
			channel->m_peakLabels.erase(channel->m_peakLabels.begin() + n);
	}

	//Initial peak processing
	for(auto p : peaks)
	{
		//Draw the circle for the peak
		list->AddCircle(
			ImVec2(m_group->XAxisUnitsToXPosition(p.m_x), YAxisUnitsToYPosition(p.m_y)),
			radius,
			circleColor,
			0,
			1);

		//Check for peaks fairly close to this one
		bool hit = false;
		for(size_t i=0; i<channel->m_peakLabels.size(); i++)
		{
			if( llabs(channel->m_peakLabels[i].m_peakXpos - p.m_x) < neighborThresholdXUnits )
			{
				//This peak is close enough we'll call it the same. Update the position.
				hit = true;
				channel->m_peakLabels[i].m_peakXpos = p.m_x;
				channel->m_peakLabels[i].m_peakYpos = p.m_y;
				channel->m_peakLabels[i].m_peakAlpha = 255;
				channel->m_peakLabels[i].m_fwhm = p.m_fwhm;
				break;
			}
		}

		//Not found, create a new peak
		if(!hit)
		{
			PeakLabel npeak;

			//Initial X position is just left of the peak
			npeak.m_labelXpos = p.m_x - m_group->PixelsToXAxisUnits(5 * ImGui::GetFontSize());
			npeak.m_peakXpos = p.m_x;
			npeak.m_peakYpos = p.m_y;
			npeak.m_fwhm = p.m_fwhm;

			//Initial Y position is above the peak if in the bottom half, otherwise below
			if(p.m_y > stream.GetOffset())
				npeak.m_labelYpos = p.m_y + PixelsToYAxisUnits(3*ImGui::GetFontSize());
			else
				npeak.m_labelYpos = p.m_y - PixelsToYAxisUnits(3*ImGui::GetFontSize());

			//Default to 100% alpha
			npeak.m_peakAlpha = 255;

			channel->m_peakLabels.push_back(npeak);
		}
	}

	//Foreground color is used to determine background color and hovered/active colors
	auto chancolor = ColorFromString(stream.m_channel->m_displaycolor);
	auto fcolor = ImGui::ColorConvertU32ToFloat4(chancolor);

	auto wmin = ImGui::GetWindowPos();
	auto wsize = ImGui::GetWindowSize();
	ImVec2 wmax(wmin.x + wsize.x, wmin.y + wsize.y);

	//Draw the peaks and update X/Y size for collision detection
	auto font = m_parent->GetFontPref("Appearance.Peaks.label_font");
	ImGui::PushFont(font.first, font.second);
	auto& prefs = m_parent->GetSession().GetPreferences();
	auto textColor = prefs.GetColor("Appearance.Peaks.peak_text_color");
	auto mousePos = ImGui::GetMousePos();
	float springMaxLength = 15 * ImGui::GetFontSize();
	for(size_t i=0; i<channel->m_peakLabels.size(); i++)
	{
		auto& label = channel->m_peakLabels[i];

		//Figure out text size
		string str =
			"X = " + stream.GetXAxisUnits().PrettyPrint(label.m_peakXpos) + "\n" +
			"Y = " + stream.GetYAxisUnits().PrettyPrint(label.m_peakYpos) + "\n" +
			"FWHM = " + stream.GetXAxisUnits().PrettyPrint(label.m_fwhm);
		auto textSizePixels = ImGui::CalcTextSize(str.c_str());

		//Create rectangle for box around centroid
		float padding = 2;
		float labelXpos = m_group->XAxisUnitsToXPosition(label.m_labelXpos);
		float labelYpos = YAxisUnitsToYPosition(label.m_labelYpos);
		float xrad = textSizePixels.x/2 + padding;
		float yrad = textSizePixels.y/2 + padding;
		float labelLeft = labelXpos - xrad;
		float labelRight = labelXpos + xrad;
		float labelTop = labelYpos - yrad;
		float labelBottom = labelYpos + yrad;

		//Update alpha
		if(label.m_peakAlpha < 0)
			continue;
		lineColor &= ~(0xff << IM_COL32_A_SHIFT);
		lineColor |= (label.m_peakAlpha << IM_COL32_A_SHIFT);

		ImVec2 peak(m_group->XAxisUnitsToXPosition(label.m_peakXpos), YAxisUnitsToYPosition(label.m_peakYpos));

		//Line from peak to closest point on label perimeter
		//TODO: this doesn't account for rounding of rectangle corners
		ImVec2 corner;
		bool left = labelRight < peak.x;
		bool right = labelLeft > peak.x;
		bool below = labelTop > peak.y;
		bool above = labelBottom < peak.y;
		ImVec2 tl(labelLeft, labelTop);
		ImVec2 tr(labelRight, labelTop);
		ImVec2 bl(labelLeft, labelBottom);
		ImVec2 br(labelRight, labelBottom);
		if(left && below)
			corner = tr;
		else if(left && above)
			corner = br;
		else if(right && below)
			corner = tl;
		else if(right && above)
			corner = bl;
		else if(left)
			corner = ClosestPointOnLineSegment(tr, br, peak);
		else if(right)
			corner = ClosestPointOnLineSegment(tl, bl, peak);
		else if(below)
			corner = ClosestPointOnLineSegment(tl, tr, peak);
		else /*if(above) */
			corner = ClosestPointOnLineSegment(bl, br, peak);
		list->AddLine(
			peak,
			corner,
			lineColor,
			1);

		//Mouse drag hit testing
		bool mouseHit = false;
		if( (mousePos.x >= labelLeft) && (mousePos.x <= labelRight) &&
			(mousePos.y >= labelTop) && (mousePos.y <= labelBottom) )
		{
			mouseHit = true;
		}

		//Start dragging
		if(mouseHit && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			m_dragPeakLabel = &label;
			m_dragState = DRAG_STATE_PEAK_MARKER;
			m_dragPeakAnchorOffset = ImVec2(labelXpos - mousePos.x, labelYpos - mousePos.y);
		}

		//Make background lighter if dragging
		float fmul = 0.3;
		if( (m_dragState == DRAG_STATE_PEAK_MARKER) && (m_dragPeakLabel == &label) )
			fmul = 0.5;

		//Draw rectangle filling
		float rounding = 3;
		auto fillColor = ImGui::ColorConvertFloat4ToU32(
			ImVec4(fcolor.x*fmul, fcolor.y*fmul, fcolor.z*fmul, label.m_peakAlpha/255.0f) );
		list->AddRectFilled(
			ImVec2(labelLeft, labelTop),
			ImVec2(labelRight, labelBottom),
			fillColor,
			rounding);

		//Draw rectangle outline
		list->AddRect(
			ImVec2(labelLeft, labelTop),
			ImVec2(labelRight, labelBottom),
			chancolor,
			rounding);

		//Draw text
		list->AddText(
			ImVec2(labelLeft + padding, labelTop + padding),
			textColor,
			str.c_str());

		//Update label info for physics
		label.m_labelXsize = m_group->PixelsToXAxisUnits(textSizePixels.x);
		label.m_labelYsize = PixelsToYAxisUnits(textSizePixels.y);

		//Skip physics on anything being dragged
		//TODO: omit springs if manually positioning a label?
		bool draggingThis = (m_dragState == DRAG_STATE_PEAK_MARKER) && (&label == m_dragPeakLabel);
		float step = 3;
		if(!draggingThis)
		{
			//Calculate magnitude (in pixels) and unit vector for direction of line
			float dx = labelXpos - peak.x;
			float dy = labelYpos - peak.y;
			float mag = sqrtf(dx*dx + dy*dy);
			float ux = dx / mag;
			float uy = dy / mag;

			//Physics 1: Spring to pull labels closer to peaks if they're too far away
			if(mag > springMaxLength)
			{
				label.m_labelXpos -= m_group->PixelsToXAxisUnits(step * ux);
				label.m_labelYpos += PixelsToYAxisUnits(step * uy);
			}

			//Physics 2: If peak is on screen but label is not, move label closer to peak
			//TODO: omit if label is manually positioned?
			bool peakIsOnScreen = (peak.x >= wmin.x) && (peak.x <= wmax.x) && (peak.y >= wmin.y) && (peak.y <= wmax.y);
			bool labelIsOnScreen =
				(labelLeft >= wmin.x) && (labelRight <= wmax.x) && (labelTop >= wmin.y) && (labelBottom <= wmax.y);
			if(peakIsOnScreen && !labelIsOnScreen)
			{
				label.m_labelXpos -= m_group->PixelsToXAxisUnits(step * ux);
				label.m_labelYpos += PixelsToYAxisUnits(step * uy);
			}
		}

		//Physics 3: If labels collide, move them apart
		//Only search labels after this one, to avoid moving stuff twice
		for(size_t j=i+1; j<channel->m_peakLabels.size(); j++)
		{
			auto& jlabel = channel->m_peakLabels[j];
			ImVec2 jpos(m_group->XAxisUnitsToXPosition(jlabel.m_labelXpos), YAxisUnitsToYPosition(jlabel.m_labelYpos));
			ImVec2 jsize(m_group->XAxisUnitsToPixels(jlabel.m_labelXsize), YAxisUnitsToPixels(jlabel.m_labelYsize));

			if(RectIntersect(
				ImVec2(labelXpos, labelYpos),
				ImVec2(xrad*2, yrad*2),
				jpos,
				jsize))
			{
				float jdx = labelXpos - jpos.x;
				float jdy = labelYpos - jpos.y;
				float jmag = sqrt(jdx*jdx + jdy*jdy);
				float jux = jdx / jmag;
				float juy = jdy / jmag;

				if(!draggingThis)
				{
					label.m_labelXpos += m_group->PixelsToXAxisUnits(jux * step);
					label.m_labelYpos -= PixelsToYAxisUnits(juy * step);
				}

				//Don't move the other label if we're dragging it
				if( (m_dragState == DRAG_STATE_PEAK_MARKER) && (&jlabel == m_dragPeakLabel) )
					continue;

				jlabel.m_labelXpos -= m_group->PixelsToXAxisUnits(jux * step);
				jlabel.m_labelYpos += PixelsToYAxisUnits(juy * step);
			}
		}
	}
	ImGui::PopFont();
}

/**
	@brief Renders a single digital waveform
 */
void WaveformArea::RenderDigitalWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Mark the waveform as resized
	if(channel->UpdateSize(ImVec2(size.x, m_channelButtonHeight), m_parent))
		m_parent->SetNeedRender();

	//Render the tone mapped output (if we have it)
	auto tex = channel->GetTexture();
	if(tex != nullptr)
	{
		auto ypos = channel->GetYButtonPos() + start.y;
		list->AddImage(
			tex->GetTexture(),
			ImVec2(start.x, ypos - m_channelButtonHeight),
			ImVec2(start.x+size.x, ypos),
			ImVec2(0, 1),
			ImVec2(1, 0) );
	}
}

/**
	@brief Renders a single protocol waveform (assume it's sparse)

	TODO: should we ever support uniform protocol data? maybe coming off some kind of analyzer?
 */
void WaveformArea::RenderProtocolWaveform(std::shared_ptr<DisplayedChannel> channel, ImVec2 start, ImVec2 size)
{
	auto stream = channel->GetStream();
	auto data = dynamic_cast<SparseWaveformBase*>(stream.GetData());
	if(data == nullptr)
		return;
	data->CacheColors();

	auto list = ImGui::GetWindowDrawList();

	//Calculate a bunch of constants
	int64_t offset = m_group->GetXAxisOffset();
	int64_t offset_samples = (offset - data->m_triggerPhase) / data->m_timescale;

	//Find the index of the first sample visible on screen
	data->PrepareForCpuAccess();
	auto ifirst = BinarySearchForGequal(
		data->m_offsets.GetCpuPointer(),
		data->size(),
		offset_samples);

	//Go left by one sample
	//The last sample BEFORE the left side of our view might extend into the visible space
	if(ifirst > 0)
		ifirst --;

	float ybot = channel->GetYButtonPos() + start.y;
	float ytop = ybot - m_channelButtonHeight;
	float ymid = ybot - m_channelButtonHeight/2;

	//Draw the actual stuff
	size_t len = data->size();
	size_t xend = start.x + size.x;
	for(size_t i=ifirst; i<len; i++)
	{
		int64_t tstart = (data->m_offsets[i] * data->m_timescale) + data->m_triggerPhase;
		int64_t end = tstart + (data->m_durations[i] * data->m_timescale);

		double xs = m_group->XAxisUnitsToXPosition(tstart);
		double xe = m_group->XAxisUnitsToXPosition(end);

		if(xe < start.x)
			continue;
		if(xs > xend)
			break;

		double cellwidth = xe - xs;
		auto color = data->GetColorCached(i);
		if(cellwidth < 2)
		{
			//This sample is really skinny. There's no text to render so don't waste time with that.

			//Average the color of all samples touching this pixel
			size_t nmerged = 1;
			float sum_red = (color >> IM_COL32_R_SHIFT) & 0xff;
			float sum_green = (color >> IM_COL32_G_SHIFT) & 0xff;
			float sum_blue = (color >> IM_COL32_B_SHIFT) & 0xff;
			for(size_t j=i+1; j<len; j++)
			{
				int64_t cellstart = (data->m_offsets[j] * data->m_timescale) + data->m_triggerPhase;
				double cellxs = m_group->XAxisUnitsToXPosition(cellstart);

				if(cellxs > xs+2)
					break;

				auto c = data->GetColorCached(j);

				sum_red += (c >> IM_COL32_R_SHIFT) & 0xff;
				sum_green += (c >> IM_COL32_G_SHIFT) & 0xff;
				sum_blue += (c >> IM_COL32_B_SHIFT) & 0xff;
				nmerged ++;

				//Skip these samples in the outer loop
				i = j-1;
			}

			//Render a single box for them all
			sum_red /= nmerged;
			sum_green /= nmerged;
			sum_blue /= nmerged;
			color =
				((static_cast<int>(sum_red) & 0xff) << IM_COL32_R_SHIFT) |
				((static_cast<int>(sum_green) & 0xff) << IM_COL32_G_SHIFT) |
				((static_cast<int>(sum_blue) & 0xff) << IM_COL32_B_SHIFT) |
				(0xff << IM_COL32_A_SHIFT);


			RenderComplexSignal(
				list,
				start.x, xend,
				xs, xe, 5,
				ybot, ymid, ytop,
				"",
				color);
		}
		else
		{
			RenderComplexSignal(
				list,
				start.x, xend,
				xs, xe, 5,
				ybot, ymid, ytop,
				data->GetText(i),
				color);
		}
	}
}

void WaveformArea::RenderComplexSignal(
		ImDrawList* list,
		int visleft, int visright,
		float xstart, float xend, float xoff,
		float ybot, float ymid, float ytop,
		string str,
		ImU32 color)
{
	//Clamp start point to left side of display
	if(xstart < visleft)
		xstart = visleft;

	//First-order guess of position: center of the value
	float xp = xstart + (xend-xstart)/2;

	//Width within this signal outline
	float available_width = xend - xstart - 2*xoff;

	//Convert all whitespace in text to spaces
	for(size_t i=0; i<str.length(); i++)
	{
		if(isspace(str[i]))
			str[i] = ' ';
	}

	//If the space is tiny, don't even attempt to render it.
	//Figuring out text size is expensive when we have hundreds or thousands of packets on screen, but in this case
	//we *know* it won't fit.
	bool drew_text = false;
	if(available_width > 15)
	{
		auto font = m_parent->GetFontPref("Appearance.Decodes.protocol_font");
		ImGui::PushFont(font.first, font.second);
		auto textsize = ImGui::CalcTextSize(str.c_str());

		//Minimum width (if outline ends up being smaller than this, just fill)
		float min_width = 40;
		if(textsize.x < min_width)
			min_width = textsize.x;

		//Does the string fit at all? If not, skip all of the messy math
		if(available_width < min_width)
			str = "";
		else
		{
			//Center the text by moving it left half a width
			xp -= textsize.x/2;

			//Off the left end? Push it right
			float new_width = available_width;
			int padding = 5;
			if(xp < (visleft + padding))
			{
				xp = visleft + padding;
				new_width = xend - xp - xoff;
			}

			//Off the right end? Push it left
			else if( (xp + textsize.x + padding) > visright)
			{
				xp = visright - (textsize.x + padding + xoff);
				if(xp < xstart)
					xp = xstart + xoff;

				if(xend < visright)
					new_width = xend - xp - xoff;
				else
					new_width = visright - xp - xoff;
			}

			if(new_width < available_width)
				available_width = new_width;

			//If we don't fit under the new constraints, give up
			if(available_width < min_width)
				str = "";
		}

		//Draw the text
		if(str != "")
		{
			//If we need to trim, decide which way to do it.
			//If the text is all caps and includes an underscore, it's probably a macro with a prefix.
			//Trim from the left in this case. Otherwise, trim from the right.
			bool trim_from_right = true;
			bool is_all_upper = true;
			for(size_t i=0; i<str.length(); i++)
			{
				if(islower(str[i]))
					is_all_upper = false;
			}
			if(is_all_upper && (str.find("_") != string::npos))
				trim_from_right = false;

			//Some text fits, but maybe not all of it
			//We know there's enough room for "some" text
			//Try shortening the string a bit at a time until it fits
			//(Need to do an O(n) search since character width is variable and unknown to us without knowing details
			//of the font currently in use)
			string str_render = str;
			if(textsize.x > available_width)
			{
				for(int len = str.length() - 1; len > 1; len--)
				{
					if(trim_from_right)
						str_render = str.substr(0, len) + "...";
					else
						str_render = "..." + str.substr(str.length() - len - 1);

					textsize = ImGui::CalcTextSize(str_render.c_str());
					if(textsize.x < available_width)
					{
						//Re-center text in available space
						xp += (available_width - textsize.x)/2;
						if(xp < (xstart + xoff))
							xp = (xstart + xoff);
						break;
					}
				}
			}

			//Draw filler to darken background for better contrast
			ImU32 bgcolor = (0xc0 << IM_COL32_A_SHIFT);
			MakePathSignalBody(list, xstart, xend, ybot, ymid, ytop);
			list->PathFillConvex(bgcolor);

			drew_text = true;
			ImU32 textcolor = 0xffffffff;	//TODO: figure out color based on theme or something
			list->AddText(ImVec2(xp, ymid-textsize.y/2), textcolor, str_render.c_str());
		}

		ImGui::PopFont();
	}

	if(xend > visright)
		xend = visright;

	//If no text fit, draw filler instead
	if(!drew_text)
	{
		float r = ((color >> IM_COL32_R_SHIFT) & 0xff) / 4;
		float g = ((color >> IM_COL32_G_SHIFT) & 0xff) / 4;
		float b = ((color >> IM_COL32_B_SHIFT) & 0xff) / 4;
		ImU32 darkcolor =
			((static_cast<int>(r) & 0xff) << IM_COL32_R_SHIFT) |
			((static_cast<int>(g) & 0xff) << IM_COL32_G_SHIFT) |
			((static_cast<int>(b) & 0xff) << IM_COL32_B_SHIFT) |
			(0xff << IM_COL32_A_SHIFT);

		MakePathSignalBody(list, xstart, xend, ybot, ymid, ytop);
		list->PathFillConvex(darkcolor);
	}

	//Draw the body outline after any filler so it shows up on top
	MakePathSignalBody(list, xstart, xend, ybot, ymid, ytop);
	list->PathStroke(color, 0, 2);
}

void WaveformArea::MakePathSignalBody(ImDrawList* list, float xstart, float xend, float ybot, float ymid, float ytop)
{
	//Square off edges if really tiny
	float rounding = 5;
	if((xend-xstart) < 2*rounding)
		rounding = 0;

	list->PathLineTo(ImVec2(xstart, 			ymid));	//left point
	list->PathLineTo(ImVec2(xstart + rounding,	ytop));	//top left corner
	list->PathLineTo(ImVec2(xend - rounding, 	ytop));	//top right corner
	list->PathLineTo(ImVec2(xend,				ymid));	//right point
	list->PathLineTo(ImVec2(xend - rounding,	ybot));	//bottom right corner
	list->PathLineTo(ImVec2(xstart + rounding,	ybot));	//bottom left corner
	list->PathLineTo(ImVec2(xstart, 			ymid));	//left point again
}

/**
	@brief Tone map our waveforms
 */
void WaveformArea::ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf)
{
	for(auto& chan : m_displayedChannels)
	{
		auto stream = chan->GetStream();
		if(chan->GetStream().IsOutOfRange())
			continue;

		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
			case Stream::STREAM_TYPE_DIGITAL:
				ToneMapAnalogOrDigitalWaveform(chan, cmdbuf);
				break;

			case Stream::STREAM_TYPE_WATERFALL:
				ToneMapWaterfallWaveform(chan, cmdbuf);
				break;

			case Stream::STREAM_TYPE_SPECTROGRAM:
				ToneMapSpectrogramWaveform(chan, cmdbuf);
				break;

			case Stream::STREAM_TYPE_EYE:
				ToneMapEyeWaveform(chan, cmdbuf);
				break;

			case Stream::STREAM_TYPE_CONSTELLATION:
				ToneMapConstellationWaveform(chan, cmdbuf);
				break;

			//no tone mapping required
			case Stream::STREAM_TYPE_PROTOCOL:
				break;

			//nothing to draw, it's not a waveform (shouldn't even be here)
			case Stream::STREAM_TYPE_ANALOG_SCALAR:
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
		if(chan->GetStream().IsOutOfRange())
			continue;

		switch(stream.GetType())
		{
			case Stream::STREAM_TYPE_ANALOG:
			case Stream::STREAM_TYPE_DIGITAL:
				RasterizeAnalogOrDigitalWaveform(chan, cmdbuf, clearing);
				break;

			//no background rendering required, we do everything in Refresh()
			case Stream::STREAM_TYPE_EYE:
			case Stream::STREAM_TYPE_CONSTELLATION:
			case Stream::STREAM_TYPE_WATERFALL:
			case Stream::STREAM_TYPE_SPECTROGRAM:
				break;

			//no background rendering required, we draw everything live
			case Stream::STREAM_TYPE_PROTOCOL:
				break;

			//nothing to draw, it's not a waveform (shouldn't even be here)
			case Stream::STREAM_TYPE_ANALOG_SCALAR:
				break;

			default:
				LogWarning("Unimplemented stream type %d, don't know how to rasterize it\n", stream.GetType());
				break;
		}
	}
}

void WaveformArea::RasterizeAnalogOrDigitalWaveform(
	shared_ptr<DisplayedChannel> channel,
	vk::raii::CommandBuffer& cmdbuf,
	bool clearPersistence
	)
{
	if(m_height < 0)
	{
		LogWarning("WaveformArea has negative height, cannot render\n");
		return;
	}

	auto stream = channel->GetStream();
	auto data = stream.GetData();

	//Prepare the memory so we can rasterize it
	//If no data (or an empty buffer with no samples), set to 0x0 pixels and return
	if( (data == nullptr) || data->empty() )
	{
		channel->PrepareToRasterize(0, 0);
		return;
	}
	size_t w = m_width;
	size_t h = m_height;
	if(channel->GetStream().GetType() == Stream::STREAM_TYPE_DIGITAL)
		h = m_channelButtonHeight;
	channel->PrepareToRasterize(w, h);

	shared_ptr<ComputePipeline> comp;

	//Calculate a bunch of constants
	int64_t offset = m_group->GetXAxisOffset();
	int64_t innerxoff = offset / data->m_timescale;
	int64_t fractional_offset = offset % data->m_timescale;
	int64_t offset_samples = (offset - data->m_triggerPhase) / data->m_timescale;
	double pixelsPerX = m_group->GetPixelsPerXUnit();
	double xscale = data->m_timescale * pixelsPerX;

	//Figure out which shader to use
	auto udata = dynamic_cast<UniformWaveformBase*>(data);
	auto sdata = dynamic_cast<SparseWaveformBase*>(data);
	auto uadata = dynamic_cast<UniformAnalogWaveform*>(data);
	auto sadata = dynamic_cast<SparseAnalogWaveform*>(data);
	auto uddata = dynamic_cast<UniformDigitalWaveform*>(data);
	auto sddata = dynamic_cast<SparseDigitalWaveform*>(data);
	if(uadata)
	{
		if(channel->ShouldFillUnder())
			comp = channel->GetHistogramPipeline();
		else
			comp = channel->GetUniformAnalogPipeline();
	}
	else if(uddata)
		comp = channel->GetUniformDigitalPipeline();
	else if(sadata)
		comp = channel->GetSparseAnalogPipeline();
	else if(sddata)
		comp = channel->GetSparseDigitalPipeline();
	if(!comp)
	{
		LogWarning("no pipeline found\n");
		return;
	}

	//Bind input buffers
	if(sdata)
	{
		//Calculate indexes for X axis
		auto& ibuf = channel->GetIndexBuffer();

		//If we have native int64, do this on the GPU
		if(g_hasShaderInt64)
		{
			IndexSearchConstants cfg;
			cfg.len = data->size();
			cfg.w = w;
			cfg.xscale = xscale;
			cfg.offset_samples = offset_samples;

			const uint32_t threadsPerBlock = 64;
			const uint32_t numBlocks = GetComputeBlockCount(w, threadsPerBlock);

			auto ipipe = channel->GetIndexSearchPipeline();
			ipipe->BindBufferNonblocking(0, sdata->m_offsets, cmdbuf);
			ipipe->BindBufferNonblocking(1, ibuf, cmdbuf, true);
			ipipe->Dispatch(cmdbuf, cfg, numBlocks);
			ipipe->AddComputeMemoryBarrier(cmdbuf);
			ibuf.MarkModifiedFromGpu();
		}

		//otherwise CPU fallback
		else
		{
			ibuf.PrepareForCpuAccess();
			sdata->m_offsets.PrepareForCpuAccess();
			for(size_t i=0; i<w; i++)
			{
				int64_t target = floor(i / xscale) + offset_samples;
				ibuf[i] = BinarySearchForGequal(
					sdata->m_offsets.GetCpuPointer(),
					data->size(),
					target);
			}
			ibuf.MarkModifiedFromCpu();
		}

		//Bind the buffers
		if(sadata)
			comp->BindBufferNonblocking(1, sadata->m_samples, cmdbuf);
		if(sddata)
			comp->BindBufferNonblocking(1, sddata->m_samples, cmdbuf);

		//Map offsets and, if requested, durations
		comp->BindBufferNonblocking(2, sdata->m_offsets, cmdbuf);
		comp->BindBufferNonblocking(3, ibuf, cmdbuf);
		if(channel->ShouldMapDurations())
			comp->BindBufferNonblocking(4, sdata->m_durations, cmdbuf);
	}

	if(uadata)
		comp->BindBufferNonblocking(1, uadata->m_samples, cmdbuf);
	if(uddata)
		comp->BindBufferNonblocking(1, uddata->m_samples, cmdbuf);

	//Bind output texture and bail if there's nothing there
	auto& imgOut = channel->GetRasterizedWaveform();
	if(imgOut.empty())
		return;
	comp->BindBufferNonblocking(0, imgOut, cmdbuf);

	//Scale alpha by zoom.
	//As we zoom out more, reduce alpha to get proper intensity grading
	//TODO: make this constant, then apply a second alpha pass in tone mapping?
	//This will eliminate the need for a (potentially heavy) re-render when adjusting the slider.
	float alpha = m_parent->GetTraceAlpha();
	auto end = data->size() - 1;
	int64_t firstOff;
	int64_t lastOff;
	if(sdata)
	{
		//Data is sparse. Do a special peek copy to reduce the overhead vs a full copy
		sdata->m_offsets.PrepareForCpuAccessFirstAndLastOnly();
		firstOff = GetOffsetScaled(sdata, 0);
		lastOff = GetOffsetScaled(sdata, end);
	}
	else
	{
		//This doesn't need the waveform on the CPU, the count is all we care about
		firstOff = GetOffsetScaled(udata, 0);
		lastOff = GetOffsetScaled(udata, end);
	}
	float capture_len = lastOff - firstOff;
	float avg_sample_len = capture_len / data->size();
	float samplesPerPixel = 1.0 / (pixelsPerX * avg_sample_len);
	float alpha_scaled = alpha / sqrt(samplesPerPixel);
	alpha_scaled = min(1.0f, alpha_scaled) * 2;

	//Trigger phase can't go entirely in ConfigPushConstants::xoff due to limited dynamic range
	//so pass only the fractional part there and put the integer part in innerxoff
	int64_t triggerPhaseSamples	= data->m_triggerPhase / data->m_timescale;
	int64_t fractionalTriggerPhase = data->m_triggerPhase % data->m_timescale;
	innerxoff -= triggerPhaseSamples;

	//Fill shader configuration
	ConfigPushConstants config;
	config.innerXoff = -innerxoff;
	config.windowHeight = h;
	config.windowWidth = w;
	config.memDepth = data->size();
	config.offset_samples = offset_samples - 2;
	config.alpha = alpha_scaled;
	config.xoff = (fractionalTriggerPhase - fractional_offset) * pixelsPerX;
	config.xscale = xscale;
	if(sadata || uadata)	//analog
	{
		config.yscale = m_pixelsPerYAxisUnit;
		config.yoff = stream.GetOffset();
		config.ybase = h * 0.5f;
	}
	else					//digital
	{
		config.yoff = 0;
		config.yscale = m_channelButtonHeight - 1;
		config.ybase = 0;
	}
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
	@brief Tone maps an analog or digital waveform by converting the internal fp32 buffer to RGBA
 */
void WaveformArea::ToneMapAnalogOrDigitalWaveform(shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf)
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
	auto pipe = channel->GetToneMapPipeline();
	pipe->BindBufferNonblocking(0, channel->GetRasterizedWaveform(), cmdbuf);
	pipe->BindStorageImage(
		1,
		**m_parent->GetTextureManager()->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	auto color = ImGui::ColorConvertU32ToFloat4(ColorFromString(channel->GetStream().m_channel->m_displaycolor));
	WaveformToneMapArgs args(color, width, height);
	pipe->Dispatch(cmdbuf, args, GetComputeBlockCount(width, 64), height);

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
	@brief Tone maps a density function waveform by converting the internal fp32 buffer to RGBA and cropping/scaling
 */
void WaveformArea::ToneMapWaterfallWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf)
{
	auto tex = channel->GetTexture();
	if(tex == nullptr)
		return;

	auto data = dynamic_cast<DensityFunctionWaveform*>(channel->GetStream().GetData());
	if(data == nullptr)
		return;

	//Nothing to draw? Early out if we haven't processed the window resize yet or there's no data
	auto width = data->GetWidth();
	auto height = data->GetHeight();
	if( (width == 0) || (height == 0) )
		return;

	//Run the actual compute shader
	auto pipe = channel->GetToneMapPipeline();
	const auto& texmgr = m_parent->GetTextureManager();
	pipe->BindBufferNonblocking(0, data->GetOutData(), cmdbuf);
	pipe->BindStorageImage(
		1,
		**texmgr->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	pipe->BindSampledImage(
		2,
		**texmgr->GetSampler(),
		texmgr->GetView(channel->m_colorRamp),
		vk::ImageLayout::eShaderReadOnlyOptimal);

	int64_t offset = m_group->GetXAxisOffset();
	int64_t offset_samples = (offset - data->m_triggerPhase) / data->m_timescale;

	double pixelsPerX = m_group->GetPixelsPerXUnit();
	double xscale = data->m_timescale * pixelsPerX;

	WaterfallToneMapArgs args(width, height, m_width, m_height, offset_samples, xscale );
	pipe->Dispatch(cmdbuf, args, GetComputeBlockCount(m_width, 64), m_height);

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
	@brief Tone maps a spectrogram waveform by converting the internal fp32 buffer to RGBA and cropping/scaling
 */
void WaveformArea::ToneMapSpectrogramWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf)
{
	auto tex = channel->GetTexture();
	if(tex == nullptr)
		return;

	auto data = dynamic_cast<SpectrogramWaveform*>(channel->GetStream().GetData());
	if(data == nullptr)
		return;

	//Nothing to draw? Early out if we haven't processed the window resize yet or there's no data
	auto width = data->GetWidth();
	auto height = data->GetHeight();
	if( (width == 0) || (height == 0) )
		return;

	//Run the actual compute shader
	auto pipe = channel->GetToneMapPipeline();
	const auto& texmgr = m_parent->GetTextureManager();
	pipe->BindBufferNonblocking(0, data->GetOutData(), cmdbuf);
	pipe->BindStorageImage(
		1,
		**texmgr->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	pipe->BindSampledImage(
		2,
		**texmgr->GetSampler(),
		texmgr->GetView(channel->m_colorRamp),
		vk::ImageLayout::eShaderReadOnlyOptimal);

	int64_t offset = m_group->GetXAxisOffset();
	int64_t offset_samples = (offset - data->m_triggerPhase) / data->m_timescale;

	//Invert X (and Y) scales because multiply in the shader is faster than divide
	double xscale = 1.0 / (data->m_timescale * m_group->GetPixelsPerXUnit());

	//Rescale Y offset to screen pixels
	//Note that we actually care about offset from our *bottom*, but GetOffset() returns offset from our *midpoint*
	int32_t yoff = YAxisUnitsToPixels(-channel->GetStream().GetOffset()) - m_height/2;

	//Spectrograms aren't necessarily centered at zero so there's an extra offset based on our center frequency
	yoff -= YAxisUnitsToPixels(data->GetBottomEdgeFrequency());

	//Rescale Y to "spectrogram bins per pixel" vs "Hz per pixel"
	float yscale = 1.0 / (m_pixelsPerYAxisUnit * data->GetBinSize());

	SpectrogramToneMapArgs args(width, height, m_width, m_height, offset_samples, xscale, yoff, yscale);
	pipe->Dispatch(cmdbuf, args, GetComputeBlockCount(m_width, 64), m_height);

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
	@brief Tone maps an eye waveform by converting the internal fp32 buffer to RGBA
 */
void WaveformArea::ToneMapEyeWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf)
{
	auto tex = channel->GetTexture();
	if(tex == nullptr)
		return;

	auto data = dynamic_cast<DensityFunctionWaveform*>(channel->GetStream().GetData());
	if(data == nullptr)
		return;

	//Nothing to draw? Early out if we haven't processed the window resize yet or there's no data
	auto width = data->GetWidth();
	auto height = data->GetHeight();
	if( (width == 0) || (height == 0) )
		return;

	//Run the actual compute shader
	auto pipe = channel->GetToneMapPipeline();
	const auto& texmgr = m_parent->GetTextureManager();
	pipe->BindBufferNonblocking(0, data->GetOutData(), cmdbuf);
	pipe->BindStorageImage(
		1,
		**texmgr->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	pipe->BindSampledImage(
		2,
		**texmgr->GetSampler(),
		texmgr->GetView(channel->m_colorRamp),
		vk::ImageLayout::eShaderReadOnlyOptimal);

	EyeToneMapArgs args(width, height);
	pipe->Dispatch(cmdbuf, args, GetComputeBlockCount(width, 64), height);

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
	@brief Tone maps a constellation waveform by converting the internal fp32 buffer to RGBA
 */
void WaveformArea::ToneMapConstellationWaveform(std::shared_ptr<DisplayedChannel> channel, vk::raii::CommandBuffer& cmdbuf)
{
	auto tex = channel->GetTexture();
	if(tex == nullptr)
		return;

	auto data = dynamic_cast<DensityFunctionWaveform*>(channel->GetStream().GetData());
	if(data == nullptr)
		return;

	//Nothing to draw? Early out if we haven't processed the window resize yet or there's no data
	auto width = data->GetWidth();
	auto height = data->GetHeight();
	if( (width == 0) || (height == 0) )
		return;

	//Run the actual compute shader
	auto pipe = channel->GetToneMapPipeline();
	const auto& texmgr = m_parent->GetTextureManager();
	pipe->BindBufferNonblocking(0, data->GetOutData(), cmdbuf);
	pipe->BindStorageImage(
		1,
		**texmgr->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	pipe->BindSampledImage(
		2,
		**texmgr->GetSampler(),
		texmgr->GetView(channel->m_colorRamp),
		vk::ImageLayout::eShaderReadOnlyOptimal);

	ConstellationToneMapArgs args(width, height);
	pipe->Dispatch(cmdbuf, args, GetComputeBlockCount(width, 64), height);

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

	//TODO: add preference or per-area setting for this
	//For now, always draw if X axis units are distance/wavelength
	if(m_group->GetXAxisUnit() == Unit::UNIT_PM)
	{
		//Visible spectrum texture covers 380 - 750 nm
		draw_list->AddImage(
			m_parent->GetTextureManager()->GetTexture("visible-spectrum-380nm-750nm"),
			ImVec2(m_group->XAxisUnitsToXPosition(380000), start.y),
			ImVec2(m_group->XAxisUnitsToXPosition(750000), start.y + size.y));
	}
}

/**
	@brief Renders grid lines
 */
void WaveformArea::RenderGrid(ImVec2 start, ImVec2 size, map<float, float>& gridmap, float& vbot, float& vtop)
{
	//Early out if we're not displaying any analog waveforms
	auto stream = GetFirstAnalogOrDensityStream();
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
	if(stream.GetYAxisUnits() == Unit::UNIT_HEXNUM)
	{
		//round to next power of two
		selected_step = pow(2, round(log2(selected_step)));
	}


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
	size_t igrid = 0;
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

		//avoid infinite loop if grid settings are borked (zero range etc)
		igrid ++;
		if(igrid > 50)
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
	ImGui::PushFont(font.first, font.second);
	auto& prefs = m_parent->GetSession().GetPreferences();
	auto textColor = prefs.GetColor("Appearance.Graphs.y_axis_text_color");

	//Reserve an empty area we're going to draw into
	ImGui::Dummy(size);

	//Catch mouse wheel events
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
	if(ImGui::IsItemHovered())
	{
		auto wheel = ImGui::GetIO().MouseWheel;
		if(wheel != 0)
			OnMouseWheelYAxis(wheel);
	}

	if(ImGui::BeginPopupContextWindow())
	{
		if(ImGui::MenuItem("Autofit")){
			AutofitVertical();
		}
		ImGui::EndPopup();
	}

	//Trigger level arrow(s)
	RenderTriggerLevelArrows(origin, size);

	//BER level arrows (for BERT readout)
	RenderBERLevelArrows(origin, size);

	//Help message
	if( (m_dragState == DRAG_STATE_TRIGGER_LEVEL) || (m_dragState == DRAG_STATE_TRIGGER_SECONDARY_LEVEL) )
		m_parent->AddStatusHelp("mouse_lmb_drag", "Adjust trigger level");
	if(ImGui::IsItemHovered())
	{
		if(m_mouseOverTriggerArrow)
			m_parent->AddStatusHelp("mouse_lmb_drag", "Adjust trigger level");
		else
		{
			m_parent->AddStatusHelp("mouse_lmb_drag", "Adjust offset");
			m_parent->AddStatusHelp("mouse_mmb", "Autofit range and offset");
			m_parent->AddStatusHelp("mouse_wheel", "Adjust range");
		}
	}

	//Draw text for the Y axis labels
	float xmargin = 5;
	for(auto it : gridmap)
	{
		float vlo = YPositionToYAxisUnits(it.second - 0.5);
		float vhi = YPositionToYAxisUnits(it.second + 0.5);
		auto label = m_yAxisUnit.PrettyPrintRange(vlo, vhi, vbot, vtop);

		auto tsize = ImGui::CalcTextSize(label.c_str());
		float y = it.second - tsize.y/2;
		if(y > ybot)
			continue;
		if(y < ytop)
			continue;

		draw_list->AddText(ImVec2(origin.x + size.x - tsize.x - xmargin, y), textColor, label.c_str());
	}

	ImGui::PopFont();
	ImGui::EndChild();

	//Don't allow drag processing if our first waveform is an eye or constellation
	auto aestream = GetFirstAnalogOrDensityStream();
	bool canDragYAxis = true;
	if(aestream && (aestream.GetType() == Stream::STREAM_TYPE_EYE))
		canDragYAxis = false;
	if(aestream && (aestream.GetType() == Stream::STREAM_TYPE_CONSTELLATION))
		canDragYAxis = false;

	if(ImGui::IsItemHovered() && !m_mouseOverTriggerArrow && canDragYAxis && (m_dragState == DRAG_STATE_NONE))
	{
		//Start dragging
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			LogTrace("Start dragging Y axis\n");
			m_dragState = DRAG_STATE_Y_AXIS;
		}

		if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
		{
			AutofitVertical();
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
	if( (m_dragState == DRAG_STATE_TRIGGER_LEVEL) || (m_dragState == DRAG_STATE_BER_LEVEL) )
	{
		ImU32 triggerColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1));
		float dashSize = ImGui::GetFontSize() * 0.5;

		float y = YAxisUnitsToYPosition(m_triggerLevelDuringDrag);
		for(float dx = 0; (dx + dashSize) < size.x; dx += 2*dashSize)
			draw_list->AddLine(ImVec2(start.x + dx, y), ImVec2(start.x + dx + dashSize, y), triggerColor);
	}

	//See if the waveform group is dragging a trigger
	auto mouse = ImGui::GetMousePos();
	if(m_group->IsDraggingTrigger())
	{
		auto color = m_parent->GetSession().GetPreferences().GetColor("Appearance.Timeline.axis_color");
		draw_list->AddLine(
			ImVec2(mouse.x, start.y),
			ImVec2(mouse.x, start.y + size.y),
			color,
			1);
	}

}

/**
	@brief Sampling point for BER
 */
void WaveformArea::RenderBERSamplingPoint(ImVec2 /*start*/, ImVec2 /*size*/)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	auto mouse = ImGui::GetMousePos();

	m_mouseOverBERTarget = false;
	for(auto c : m_displayedChannels)
	{
		auto stream = c->GetStream();
		if(stream.GetType() != Stream::STREAM_TYPE_EYE)
			continue;
		auto ichan = dynamic_cast<BERTInputChannel*>(stream.m_channel);
		if(!ichan)
			continue;

		int64_t dx;
		float dy;
		ichan->GetBERSamplingPoint(dx, dy);

		float x = m_group->XAxisUnitsToXPosition(dx);
		float y = YAxisUnitsToYPosition(dy);

		float delta = ImGui::GetFontSize() * 0.5;
		float weight = 3;

		if(m_dragState == DRAG_STATE_BER_BOTH)
		{
			m_mouseOverBERTarget = true;
			x = mouse.x;
			y = mouse.y;
		}

		ImVec2 points[5] =
		{
			ImVec2(x-delta, y),
			ImVec2(x, 		y+delta),
			ImVec2(x+delta, y),
			ImVec2(x, 		y-delta),
			ImVec2(x-delta, y)
		};

		draw_list->AddPolyline(points, 5,  ColorFromString(stream.m_channel->m_displaycolor), 0, weight);

		if(m_dragState != DRAG_STATE_BER_BOTH)
		{
			//Check mouse position
			float left = points[0].x;
			float right = points[2].x;
			float top = points[1].y;
			float bottom = points[3].y;
			if( (mouse.x >= left) && (mouse.x <= right) && (mouse.y >= bottom) && (mouse.y <= top) )
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
				m_mouseOverBERTarget = true;

				if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					LogTrace("Start dragging BERT sampling [pomt\n");
					m_dragState = DRAG_STATE_BER_BOTH;
					m_bertChannelDuringDrag = ichan;
				}
			}
		}
	}
}

/**
	@brief Draw the tooltip on an eye pattern
 */
void WaveformArea::RenderEyePatternTooltip(ImVec2 start, ImVec2 size)
{
	//If no waveform or data, we can't get a BER
	auto firstStream = GetFirstEyeStream();
	if(!firstStream)
		return;
	auto eyedata = dynamic_cast<EyeWaveform*>(firstStream.GetData());
	if(!eyedata)
		return;
	if(!eyedata->GetAccumData())
		return;

	//Rescale mouse position since raw integration buffer may not be the same resolution as the viewport
	ImVec2 delta = ImGui::GetMousePos() - start;
	delta.x *= eyedata->GetWidth() * 1.0 / size.x;
	delta.y *= eyedata->GetHeight() * 1.0 / size.y;

	//Invert the Y axis coordinates to match row ordering
	delta.y = eyedata->GetHeight() - 1 - delta.y;

	//Bounds check rescaled coordinates
	if( (delta.x < 0) || (delta.y < 0) || (delta.x >= eyedata->GetWidth()) || (delta.y >= eyedata->GetHeight()) )
		return;

	//Make sure we aren't doing anything else that would preclude display of the tooltip
	//(e.g. dragging something or displaying a more important tooltip)
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone) && (m_dragState == DRAG_STATE_NONE) && !m_mouseOverButton )
	{
		//Calculate the BER at this point
		//Figure out which level we're targeting
		int ymid = 0;
		switch(eyedata->m_numLevels)
		{
			//NRZ: always use the midpoint
			case 2:
				ymid = eyedata->m_midpoints[0];
				break;

			//PAM3 / MLT3: decide upper or lower
			case 3:
				if(delta.y > (eyedata->GetHeight() / 2))
					ymid = eyedata->m_midpoints[1];
				else
					ymid = eyedata->m_midpoints[0];
				break;

			default:
				break;
		}
		auto ber = eyedata->GetBERAtPoint(delta.x, delta.y, eyedata->GetWidth() / 2, ymid);

		MainWindow::SetTooltipPosition();
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);

		Unit unit(Unit::UNIT_RATIO_SCI);
		Unit rtunit(Unit::UNIT_LOG_BER);

		//If we are over a BER sampling marker for a BERT, do more
		auto bchan = dynamic_cast<BERTInputChannel*>(firstStream.m_channel);
		if(m_mouseOverBERTarget && bchan)
		{
			auto rtber = bchan->GetScalarValue(BERTInputChannel::STREAM_BER);
			ImGui::Text("Realtime BER: %s\nEye BER: %s", rtunit.PrettyPrint(rtber).c_str(), unit.PrettyPrint(ber).c_str());
		}
		else
			ImGui::Text("BER: %s", unit.PrettyPrint(ber).c_str());

		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

/**
	@brief Look for mismatched vertical scales and display warning message
 */
void WaveformArea::CheckForScaleMismatch(ImVec2 start, ImVec2 size)
{
	//No analog streams? No mismatch possible
	auto firstStream = GetFirstAnalogOrDensityStream();
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

	//If the mismatched stream isn't part of a scope, don't bother showing any warnings etc
	//Filters can't be overdriven, so just silently clip
	auto ochan = dynamic_cast<OscilloscopeChannel*>(mismatchStream.m_channel);
	Oscilloscope* scope = nullptr;
	if(ochan)
	{
		scope = ochan->GetScope();
		if(!scope)
			return;
	}

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
	string str = "Caution: Potential for instrument damage!\n\n";
	str += string("The channel ") + mismatchStream.GetName() + " has a full-scale range of " +
		mismatchStream.GetYAxisUnits().PrettyPrint(mismatchStream.GetVoltageRange()) + ",\n";
	str += string("but this plot has a full-scale range of ") +
		firstStream.GetYAxisUnits().PrettyPrint(firstRange) + ".\n\n";
	str += "Setting this channel to match the plot scale may result\n";
	str += "in overdriving the instrument input.\n";
	str += "\n";
	str += string("If the instrument \"") + scope->m_nickname +
		"\" can safely handle the applied signal at this plot's scale setting,\n";
	str += "adjust the vertical scale of this plot slightly to set all signals to the same scale\n";
	str += "and eliminate this message.\n\n";
	str += "If it cannot, move the channel to another plot.\n";

	//Draw background for text
	float wrapWidth = 40 * ImGui::GetFontSize();
	auto textsize = ImGui::CalcTextSize(str.c_str(), nullptr, false, wrapWidth);
	float padding = 5;
	float wrounding = 2;
	list->AddRectFilled(
		ImVec2(center.x, center.y - textsize.y/2 - padding),
		ImVec2(center.x + textsize.x + 2*padding, center.y + textsize.y/2 + padding),
		ImGui::GetColorU32(ImGuiCol_PopupBg),
		wrounding);

	//Draw the text
	list->AddText(
		ImGui::GetFont(),
		ImGui::GetFontSize(),
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
		auto ochan = dynamic_cast<OscilloscopeChannel*>(stream.m_channel);
		if(!ochan)
			continue;
		auto scope = ochan->GetScope();
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

		//Second level
		auto sl = dynamic_cast<TwoLevelTrigger*>(trig);
		if(sl)
		{
			//Draw the arrow
			//If currently dragging, show at mouse position rather than actual hardware trigger level
			level = sl->GetLowerBound();
			y = YAxisUnitsToYPosition(level);
			if( (m_dragState == DRAG_STATE_TRIGGER_SECONDARY_LEVEL) && (trig == m_triggerDuringDrag) )
				y = mouse.y;
			arrowtop = y - arrowsize/2;
			arrowbot = y + arrowsize/2;
			draw_list->AddTriangleFilled(
				ImVec2(start.x, y), ImVec2(arrowright, arrowtop), ImVec2(arrowright, arrowbot), color);

			//Check mouse position
			//Use slightly expanded hitbox to make it easier to capture
			caparrowtop = y - caparrowsize/2;
			caparrowbot = y + caparrowsize/2;
			if( (mouse.x >= start.x) && (mouse.x <= caparrowright) && (mouse.y >= caparrowtop) && (mouse.y <= caparrowbot) )
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				m_mouseOverTriggerArrow = true;

				if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					LogTrace("Start dragging secondary trigger level\n");
					m_dragState = DRAG_STATE_TRIGGER_SECONDARY_LEVEL;
					m_triggerDuringDrag = trig;
				}
			}
		}

		//Handle dragging
		if( (m_dragState == DRAG_STATE_TRIGGER_LEVEL) || (m_dragState == DRAG_STATE_TRIGGER_SECONDARY_LEVEL) )
			m_triggerLevelDuringDrag = YPositionToYAxisUnits(mouse.y);
	}
}

/**
	@brief Arrows pointing to BERT sampling level
 */
void WaveformArea::RenderBERLevelArrows(ImVec2 start, ImVec2 /*size*/)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	float arrowsize = ImGui::GetFontSize() * 0.6;
	float caparrowsize = ImGui::GetFontSize() * 1;

	//Make a list of BERT eye patterns we're displaying
	set<BERTInputChannel*> channels;
	for(auto c : m_displayedChannels)
	{
		auto stream = c->GetStream();
		if(stream.GetType() != Stream::STREAM_TYPE_EYE)
			continue;
		auto ichan = dynamic_cast<BERTInputChannel*>(stream.m_channel);
		if(!ichan)
			continue;
		channels.emplace(ichan);
	}

	//Display the arrow for each one
	float arrowright = start.x + arrowsize;
	float caparrowright = start.x + caparrowsize;
	m_mouseOverTriggerArrow = false;
	auto mouse = ImGui::GetMousePos();
	for(auto chan : channels)
	{
		auto color = ColorFromString(chan->m_displaycolor);

		int64_t dx;
		float level;
		chan->GetBERSamplingPoint(dx, level);

		//Draw the arrow
		//If currently dragging, show at mouse position rather than actual hardware trigger level
		float y = YAxisUnitsToYPosition(level);
		if( ( (m_dragState == DRAG_STATE_BER_LEVEL) || (m_dragState == DRAG_STATE_BER_BOTH) ) &&
			(chan == m_bertChannelDuringDrag) )
		{
			y = mouse.y;
		}
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
				LogTrace("Start dragging BERT sampling level\n");
				m_dragState = DRAG_STATE_BER_LEVEL;
				m_bertChannelDuringDrag = chan;
			}
		}

		//Handle dragging
		if( (m_dragState == DRAG_STATE_BER_LEVEL) || (m_dragState == DRAG_STATE_BER_BOTH) )
			m_triggerLevelDuringDrag = YPositionToYAxisUnits(mouse.y);
		if(m_dragState == DRAG_STATE_BER_BOTH)
			m_xAxisPosDuringDrag = m_group->XPositionToXAxisUnits(mouse.x);
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

	CenterLeftDropArea(ImVec2(leftOfMiddle, topOfMiddle), ImVec2(widthOfMiddle/2, heightOfMiddle));

	//Only show split in bottom (or top?) area
	if( iArea == (numAreas-1) /*|| (iArea == 0)*/ )
		CenterRightDropArea(ImVec2(leftOfMiddle + widthOfMiddle/2, topOfMiddle), ImVec2(widthOfMiddle/2, heightOfMiddle));

	ImVec2 edgeSize(widthOfVerticalEdge, heightOfMiddle);
	EdgeDropArea("left", ImVec2(start.x, topOfMiddle), edgeSize, ImGuiDir_Left);
	EdgeDropArea("right", ImVec2(rightOfMiddle, topOfMiddle), edgeSize, ImGuiDir_Right);

	//Cannot drop scalars into a waveform view. Make this a bit more obvious
	auto payload = ImGui::GetDragDropPayload();
	if(payload)
	{
		if(payload->IsDataType("Scalar"))
		{
			ImGui::SetCursorScreenPos(start);
			ImGui::InvisibleButton("scalarDrop", size);
			ImGui::SetNextItemAllowOverlap();

			if(ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
			{
				DrawDropScalarMessage(
					ImGui::GetWindowDrawList(),
					ImVec2(start.x + size.x/4, start.y + size.y/2));
			}
		}
	}
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
	ImGui::SetNextItemAllowOverlap();

	auto payload = ImGui::GetDragDropPayload();
	if(!payload)
		return;

	bool isWaveform = payload->IsDataType("Waveform");
	bool isStream = payload->IsDataType("Stream");
	if(!isWaveform && !isStream)
		return;

	//Add drop target
	StreamDescriptor stream;
	bool hover = false;
	if(ImGui::BeginDragDropTarget())
	{
		auto wpay = ImGui::AcceptDragDropPayload("Waveform", ImGuiDragDropFlags_AcceptPeekOnly);
		if(wpay)
		{
			hover = true;
			auto desc = reinterpret_cast<DragDescriptor*>(payload->Data);
			stream = desc->first->GetStream(desc->second);

			if(payload->IsDelivery())
			{
				LogTrace("splitting\n");

				//Add request to split our current group, then remove from origin
				auto sdc = desc->first->GetDisplayedChannel(desc->second);
				m_parent->QueueSplitGroup(m_group, splitDir, stream, sdc->m_colorRamp);
				desc->first->RemoveStream(desc->second);
			}
		}

		auto spay = ImGui::AcceptDragDropPayload("Stream", ImGuiDragDropFlags_AcceptPeekOnly);
		if(spay)
		{
			stream = *reinterpret_cast<StreamDescriptor*>(spay->Data);
			hover = true;

			//Add request to split our current group
			if(payload->IsDelivery())
			{
				LogTrace("splitting\n");
				m_parent->QueueSplitGroup(m_group, splitDir, stream, "");
			}
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
		default:
			break;
	}

	//Draw background and outline
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(
		ImVec2(center.x - fillSizeX/2 - 0.5, center.y - fillSizeY/2 - 0.5),
		ImVec2(center.x + fillSizeX/2 + 0.5, center.y + fillSizeY/2 + 0.5),
		hover ? bgHovered : bgBase,
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
void WaveformArea::CenterLeftDropArea(ImVec2 start, ImVec2 size)
{
	ImGui::SetCursorScreenPos(start);
	ImGui::InvisibleButton("center", size);
	//ImGui::Button("center", size);
	ImGui::SetNextItemAllowOverlap();

	auto payload = ImGui::GetDragDropPayload();
	if(!payload)
		return;
	bool isWaveform = payload->IsDataType("Waveform");
	bool isStream = payload->IsDataType("Stream");
	if(!isWaveform && !isStream)
		return;

	//Peek the payload. If not compatible, don't even display the target
	StreamDescriptor stream;
	auto peekPayload = ImGui::GetDragDropPayload();
	if(peekPayload)
	{
		if(isWaveform)
		{
			auto peekDesc = reinterpret_cast<DragDescriptor*>(peekPayload->Data);
			stream = peekDesc->first->GetStream(peekDesc->second);
			if( (peekDesc->first == this) || !IsCompatible(stream))
				return;
		}
		else if(isStream)
		{
			stream = *reinterpret_cast<StreamDescriptor*>(peekPayload->Data);
			if(!IsCompatible(stream))
				return;
		}
	}

	//Add drop target
	bool ok = true;
	bool hover = false;
	if(ImGui::BeginDragDropTarget())
	{
		//Accept drag/drop payloads from another WaveformArea
		auto wpay = ImGui::AcceptDragDropPayload("Waveform",
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		if(wpay)
		{
			hover = true;

			auto desc = reinterpret_cast<DragDescriptor*>(wpay->Data);
			stream = desc->first->GetStream(desc->second);

			//Don't allow dropping in the same area
			//Reject streams not compatible with this plot
			//TODO: display nice error message
			if( (desc->first == this) || !IsCompatible(stream))
				ok = false;

			else if(payload->IsDelivery())
			{
				//Add the new stream to us
				//TODO: copy view settings from the DisplayedChannel over?
				AddStream(stream);

				//Remove the stream from the originating waveform area
				desc->first->RemoveStream(desc->second);
			}
		}

		//Accept drag/drop payloads from the stream browser
		auto spay = ImGui::AcceptDragDropPayload("Stream",
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		if(spay)
		{
			hover = true;
			stream = *reinterpret_cast<StreamDescriptor*>(spay->Data);

			//Reject streams not compatible with this plot
			//TODO: display nice error message if not
			if(!IsCompatible(stream))
				ok = false;

			else if(payload->IsDelivery())
				AddStream(stream);
		}

		ImGui::EndDragDropTarget();
	}

	if(!ok)
		return;

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
		hover ? bgHovered : bgBase,
		rounding);
	draw_list->AddRect(
		ImVec2(center.x - lineSize/2 - 0.5, center.y - lineSize/2 - 0.5),
		ImVec2(center.x + lineSize/2 + 0.5, center.y + lineSize/2 + 0.5),
		lineColor,
		rounding);

	//If trying to drop into the center marker, display warning if incompatible scales
	//(new signal has significantly wider range) and not a filter
	if(hover)
	{
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
	@brief Drop area for the middle of the plot

	Dropping a waveform in here adds it to a new plot in the same group
 */
void WaveformArea::CenterRightDropArea(ImVec2 start, ImVec2 size)
{
	ImGui::SetCursorScreenPos(start);
	ImGui::InvisibleButton("centersplit", size);
	//ImGui::Button("center", size);
	ImGui::SetNextItemAllowOverlap();

	auto payload = ImGui::GetDragDropPayload();
	if(!payload)
		return;
	bool isWaveform = payload->IsDataType("Waveform");
	bool isStream = payload->IsDataType("Stream");
	if(!isWaveform && !isStream)
		return;

	//Add drop target
	StreamDescriptor stream;
	bool hover = false;
	if(ImGui::BeginDragDropTarget())
	{
		auto wpay = ImGui::AcceptDragDropPayload("Waveform", ImGuiDragDropFlags_AcceptPeekOnly);
		if(wpay)
		{
			hover = true;

			auto desc = reinterpret_cast<DragDescriptor*>(wpay->Data);
			stream = desc->first->GetStream(desc->second);

			if( (stream.GetXAxisUnits() == m_group->GetXAxisUnit()) && payload->IsDelivery() )
			{
				auto area = make_shared<WaveformArea>(stream, m_group, m_parent);
				m_group->AddArea(area);

				//If the stream is is a density function, propagate the color ramp selection
				switch(stream.GetType())
				{
					case Stream::STREAM_TYPE_EYE:
					case Stream::STREAM_TYPE_SPECTROGRAM:
					case Stream::STREAM_TYPE_WATERFALL:
					case Stream::STREAM_TYPE_CONSTELLATION:
						{
							auto sdc = desc->first->GetDisplayedChannel(desc->second);
							area->GetDisplayedChannel(0)->m_colorRamp = sdc->m_colorRamp;
						}
						break;

					default:
						break;
				}

				//Remove the stream from the originating waveform area
				desc->first->RemoveStream(desc->second);
			}
		}

		//Accept drag/drop payloads from the stream browser
		auto spay = ImGui::AcceptDragDropPayload("Stream", ImGuiDragDropFlags_AcceptPeekOnly);
		if(spay)
		{
			hover = true;
			stream = *reinterpret_cast<StreamDescriptor*>(spay->Data);

			if( (stream.GetXAxisUnits() == m_group->GetXAxisUnit()) && payload->IsDelivery() )
			{
				auto area = make_shared<WaveformArea>(stream, m_group, m_parent);
				m_group->AddArea(area);
			}
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

	//Draw background and outline
	draw_list->AddRectFilled(
		ImVec2(center.x - fillSize/2 - 0.5, center.y - fillSize/2 - 0.5),
		ImVec2(center.x + fillSize/2 + 0.5, center.y + fillSize/2 + 0.5),
		hover ? bgHovered : bgBase,
		rounding);
	draw_list->AddRect(
		ImVec2(center.x - lineSize/2 - 0.5, center.y - lineSize/2 - 0.5),
		ImVec2(center.x + lineSize/2 + 0.5, center.y - 0.5),
		lineColor,
		rounding);

	draw_list->AddRect(
		ImVec2(center.x - lineSize/2 - 0.5, center.y + 0.5),
		ImVec2(center.x + lineSize/2 + 0.5, center.y + lineSize/2 + 0.5),
		lineColor,
		rounding);
}

void WaveformArea::DrawDropScalarMessage(ImDrawList* list, ImVec2 center)
{
	float iconSize = ImGui::GetFontSize() * 3;

	//Warning icon
	list->AddImage(
		m_parent->GetTextureManager()->GetTexture("info"),
		ImVec2(center.x - 0.5, center.y - iconSize/2 - 0.5),
		ImVec2(center.x + iconSize + 0.5, center.y + iconSize/2 + 0.5));

	//Prepare to draw text
	center.x += iconSize;
	string str = "Cannot add a scalar value to a waveform view.\nTry dragging to the measurements window.";

	//Draw background for text
	float wrapWidth = 40 * ImGui::GetFontSize();
	auto textsize = ImGui::CalcTextSize(str.c_str(), nullptr, false, wrapWidth);
	float padding = 5;
	float wrounding = 2;
	list->AddRectFilled(
		ImVec2(center.x, center.y - textsize.y/2 - padding),
		ImVec2(center.x + textsize.x + 2*padding, center.y + textsize.y/2 + padding),
		ImGui::GetColorU32(ImGuiCol_PopupBg),
		wrounding);

	//Draw the text
	list->AddText(
		ImGui::GetFont(),
		ImGui::GetFontSize(),
		ImVec2(center.x + padding, center.y - textsize.y/2),
		ImGui::GetColorU32(ImGuiCol_Text),
		str.c_str(),
		nullptr,
		wrapWidth);
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
		string str = "Caution: Potential for instrument damage!\n\n";
		str += string("The channel being dragged has a full-scale range of ") +
			theirStream.GetYAxisUnits().PrettyPrint(theirRange) + ",\n";
		str += string("but this plot has a full-scale range of ") +
			ourStream.GetYAxisUnits().PrettyPrint(ourRange) + ".\n\n";
		str += "Setting this channel to match the plot scale may result\n";
		str += "in overdriving the instrument input.";

		//Draw background for text
		float wrapWidth = 40 * ImGui::GetFontSize();
		auto textsize = ImGui::CalcTextSize(str.c_str(), nullptr, false, wrapWidth);
		float padding = 5;
		float wrounding = 2;
		list->AddRectFilled(
			ImVec2(center.x, center.y - textsize.y/2 - padding),
			ImVec2(center.x + textsize.x + 2*padding, center.y + textsize.y/2 + padding),
			ImGui::GetColorU32(ImGuiCol_PopupBg),
			wrounding);

		//Draw the text
		list->AddText(
			ImGui::GetFont(),
			ImGui::GetFontSize(),
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
	auto udata = dynamic_cast<UniformWaveformBase*>(data);
	//auto sdata = dynamic_cast<SparseWaveformBase*>(data);
	auto edata = dynamic_cast<EyeWaveform*>(data);
	auto cdata = dynamic_cast<ConstellationWaveform*>(data);
	auto ddata = dynamic_cast<DensityFunctionWaveform*>(data);

	//Qualify name by scope if we have multiple scopes in the session
	//If name is an empty string, change it to a space so imgui doesn't bork
	auto fqname = chan->GetName();
	if(fqname.empty())
		fqname = ' ';
	auto ochan = dynamic_cast<OscilloscopeChannel*>(rchan);
	if(ochan)
	{
		auto scope = ochan->GetScope();
		if( (scope != nullptr) && m_parent->GetSession().IsMultiScope())
			fqname = scope->m_nickname + ":" + fqname;
	}

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
	float ystart = ImGui::GetCursorScreenPos().y;
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::PushStyleColor(ImGuiCol_Button, bcolor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hcolor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, acolor);
		ImGui::Button(fqname.c_str());
	ImGui::PopStyleColor(4);
	m_channelButtonHeight = (ImGui::GetCursorScreenPos().y - ystart) - (ImGui::GetStyle().ItemSpacing.y);
	chan->SetYButtonPos(ImGui::GetCursorPosY());

	if(ImGui::IsItemHovered())
		m_mouseOverButton = true;

	if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		m_dragState = DRAG_STATE_CHANNEL;
		m_dragStream = stream;

		DragDescriptor desc(this, index);
		ImGui::SetDragDropPayload("Waveform", &desc, sizeof(desc));

		//Preview of what we're dragging
		ImGui::Text("Drag %s", fqname.c_str());

		ImGui::EndDragDropSource();
	}

	if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		if(ochan)
			m_parent->ShowChannelProperties(ochan);

		//TODO: properties of non-scope channels?
	}

	//Display channel information and help text in tooltip
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
	{
		string tooltip;
		if(ochan)
		{
			auto scope = ochan->GetScope();
			if(scope)
				tooltip += string("Channel ") + rchan->GetHwname() + " of instrument " + scope->m_nickname + "\n\n";
		}

		//See if we have data
		if(data)
		{
			if(edata)
			{
				Unit ui(Unit::UNIT_UI);
				tooltip += ui.PrettyPrint(edata->GetTotalUIs()) + "\n";

				auto echan = dynamic_cast<EyePattern*>(rchan);
				if(echan && !echan->GetMask().empty())
				{
					char tmp[128];
		 			auto hitrate = edata->GetMaskHitRate();
					snprintf(tmp, sizeof(tmp), "Mask hit rate: %.2e ", hitrate);
					tooltip += tmp;

					if(echan->GetMask().GetAllowedHitRate() > hitrate)
						tooltip += "(PASS)";
					else
						tooltip += "(FAIL)";
				}
			}
			else if(cdata)
			{
				Unit ui(Unit::UNIT_UI);
				tooltip += ui.PrettyPrint(cdata->GetTotalSymbols()) + "\n";

				Unit v(Unit::UNIT_VOLTS);
				Unit pct(Unit::UNIT_PERCENT);
				StreamDescriptor evmRaw(stream.m_channel, 1);
				StreamDescriptor evmNorm(stream.m_channel, 2);
				tooltip += string("EVM: ") + v.PrettyPrint(evmRaw.GetScalarValue()) +
					" (" + pct.PrettyPrint(evmNorm.GetScalarValue()) + ")";
			}
			else
			{
				Unit samples(Unit::UNIT_SAMPLEDEPTH);
				tooltip +=
					samples.PrettyPrint(data->size()) +
					" (memory allocated for " +
					samples.PrettyPrint(data->capacity()) +
					")\n";

				if(udata)
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
			}
		}
		tooltip = Trim(tooltip);

		MainWindow::SetTooltipPosition();
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(tooltip.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
	if(ImGui::IsItemHovered())
	{
		m_parent->AddStatusHelp("mouse_lmb_drag", "Move this waveform to another plot");
		m_parent->AddStatusHelp("mouse_lmb_double", "Open channel properties");
		m_parent->AddStatusHelp("mouse_rmb", "Channel context menu");
	}

	//Context menu
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Delete"))
			RemoveStream(index);
		ImGui::Separator();

		//Color ramp if it's a density plot
		if(ddata)
		{
			if(ImGui::BeginMenu("Color ramp"))
			{
				auto& gradients = m_parent->GetEyeGradients();

				//Figure out how big the gradients should be
				float height = ImGui::GetFontSize();
				ImVec2 gradsize(8*height, height);

				auto list = ImGui::GetWindowDrawList();
				for(auto internalName : gradients)
				{
					auto displayName = m_parent->GetEyeGradientFriendlyName(internalName);

					ImVec2 p = ImGui::GetCursorScreenPos();
					list->AddImage(
						m_parent->GetTexture(internalName),
						p,
						ImVec2(p.x + gradsize.x, p.y + gradsize.y),
						ImVec2(0, 0),
						ImVec2(1, 1));
					ImGui::Dummy(gradsize);
					ImGui::SameLine();

					if(ImGui::MenuItem(displayName.c_str(), nullptr, (internalName == chan->m_colorRamp) ))
					{
						chan->m_colorRamp = internalName;

						//TODO: more efficient to request new tone map but not render
						m_parent->SetNeedRender();
					}
				}

				ImGui::EndMenu();
			}
			ImGui::Separator();
		}

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
	FilterSubmenu(chan, "Export", Filter::CAT_EXPORT);
	FilterSubmenu(chan, "Generation", Filter::CAT_GENERATION);
	FilterSubmenu(chan, "Math", Filter::CAT_MATH);
	FilterSubmenu(chan, "Measurement", Filter::CAT_MEASUREMENT);
	FilterSubmenu(chan, "Memory", Filter::CAT_MEMORY);
	FilterSubmenu(chan, "Miscellaneous", Filter::CAT_MISC);
	FilterSubmenu(chan, "Optical", Filter::CAT_OPTICAL);
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

			//Measurements should have summary option and not show properties by default
			if( (cat == Filter::CAT_MEASUREMENT) && (it->second->GetStreamCount() > 1) )
			{
				if(ImGui::BeginMenu(fname.c_str(), valid))
				{
					if(ImGui::MenuItem("Trend"))
						m_parent->CreateFilter(fname, this, stream, false);

					if(ImGui::MenuItem("Summary"))
						m_parent->CreateFilter(fname, this, stream, false, false);

					ImGui::EndMenu();
				}
			}

			else
			{
				if(ImGui::MenuItem(fname.c_str(), nullptr, false, valid))
					m_parent->CreateFilter(fname, this, stream);
			}
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

		case DRAG_STATE_BER_LEVEL:
			{
				float ignored;
				int64_t dx;
				m_bertChannelDuringDrag->GetBERSamplingPoint(dx, ignored);
				m_bertChannelDuringDrag->SetBERSamplingPoint(dx, m_triggerLevelDuringDrag);
			}
			break;

		case DRAG_STATE_BER_BOTH:
			m_bertChannelDuringDrag->SetBERSamplingPoint(m_xAxisPosDuringDrag, m_triggerLevelDuringDrag);
			break;

		case DRAG_STATE_TRIGGER_LEVEL:
			{
				Unit volts(Unit::UNIT_VOLTS);
				LogTrace("End dragging trigger level (at %s)\n", volts.PrettyPrint(m_triggerLevelDuringDrag).c_str());

				//If two-level trigger, make sure we have the levels in the right order
				auto tlt = dynamic_cast<TwoLevelTrigger*>(m_triggerDuringDrag);
				if(tlt)
				{
					//We're dragging the primary level, which should always be higher than the secondary
					auto sec = tlt->GetLowerBound();
					if(m_triggerLevelDuringDrag >= sec)
					{
						tlt->SetUpperBound(m_triggerLevelDuringDrag);
						tlt->GetScope()->PushTrigger();
					}

					//But if we dragged past the secondary level, invert (otherwise we'll never trigger)
					else
					{
						tlt->SetUpperBound(sec);
						tlt->SetLowerBound(m_triggerLevelDuringDrag);
						tlt->GetScope()->PushTrigger();
					}
				}

				else
				{
					m_triggerDuringDrag->SetLevel(m_triggerLevelDuringDrag);
					m_triggerDuringDrag->GetScope()->PushTrigger();
				}

				m_parent->RefreshTriggerPropertiesDialog();
			}
			break;

		case DRAG_STATE_TRIGGER_SECONDARY_LEVEL:
			{
				Unit volts(Unit::UNIT_VOLTS);
				LogTrace("End dragging secondary trigger level (at %s)\n",
					volts.PrettyPrint(m_triggerLevelDuringDrag).c_str());

				//If two-level trigger, make sure we have the levels in the right order
				auto tlt = dynamic_cast<TwoLevelTrigger*>(m_triggerDuringDrag);
				if(tlt)
				{
					//We're dragging the secondary level, which should always be lower than the secondary
					auto prim = tlt->GetUpperBound();
					if(m_triggerLevelDuringDrag < prim)
					{
						tlt->SetLowerBound(m_triggerLevelDuringDrag);
						tlt->GetScope()->PushTrigger();
					}

					//But if we dragged past the secondary level, invert (otherwise we'll never trigger)
					else
					{
						tlt->SetUpperBound(m_triggerLevelDuringDrag);
						tlt->SetLowerBound(prim);
						tlt->GetScope()->PushTrigger();
					}
				}

				m_parent->RefreshTriggerPropertiesDialog();
			}
			break;

		case DRAG_STATE_PEAK_MARKER:
			m_dragPeakLabel = nullptr;
			break;

		//stay in sorta-dragging state until end of this frame
		case DRAG_STATE_CHANNEL:
			m_dragState = DRAG_STATE_CHANNEL_LAST;
			return;

		default:
			break;
	}

	m_dragState = DRAG_STATE_NONE;
}

void WaveformArea::OnDragUpdate()
{
	//If mouse is not currently down, but we're still dragging, synthesize a mouse up event
	if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		OnMouseUp();
		return;
	}

	switch(m_dragState)
	{
		case DRAG_STATE_Y_AXIS:
			{
				float dy = ImGui::GetIO().MouseDelta.y;
				m_yAxisOffset -= PixelsToYAxisUnits(dy);

				for(auto chan : m_displayedChannels)
				{
					//Update filters and such instantly
					auto stream = chan->GetStream();
					auto f = dynamic_cast<Filter*>(stream.m_channel);
					if(f != nullptr)
						stream.SetOffset(m_yAxisOffset);

					//TODO: push to hardware at a controlled rate (after each trigger? fixed rate in Hz?)
				}

				m_parent->SetNeedRender();
			}
			break;

		case DRAG_STATE_BER_LEVEL:
		case DRAG_STATE_BER_BOTH:
			//TODO: push to hardware at a controlled rate (after each trigger?)
			break;

		case DRAG_STATE_TRIGGER_LEVEL:
			//TODO: push to hardware at a controlled rate (after each trigger?)
			break;

		case DRAG_STATE_PEAK_MARKER:
			if(m_dragPeakLabel != nullptr)
			{
				auto mouse = ImGui::GetMousePos();

				float anchorX = mouse.x + m_dragPeakAnchorOffset.x;
				float anchorY = mouse.y + m_dragPeakAnchorOffset.y;

				m_dragPeakLabel->m_labelXpos = m_group->XPositionToXAxisUnits(anchorX);
				m_dragPeakLabel->m_labelYpos = YPositionToYAxisUnits(anchorY);
			}
			break;

		default:
			break;
	}
}

/**
	@brief Handles a mouse wheel scroll step on the plot area
	@param delta Vertical scroll steps
	@param delta_h Horizontal scroll steps
 */
void WaveformArea::OnMouseWheelPlotArea(float delta, float delta_h)
{
	if (ImGui::IsKeyDown(ImGuiMod_Shift))
	{
		delta_h += delta;
		delta = 0;
	}

	int64_t target = m_group->XPositionToXAxisUnits(ImGui::GetIO().MousePos.x);

	//If we have both X and Y deltas, use the larger one and ignore incidental movement in the other axis
	if(fabs(delta) > fabs(delta_h) )
	{
		//Zoom in
		if(delta > 0)
		{
			m_group->OnZoomInHorizontal(target, pow(1.5, delta));

			//If in the tutorial, ungate the wizard
			auto tutorial = m_parent->GetTutorialWizard();
			if(tutorial && (tutorial->GetCurrentStep() == TutorialWizard::TUTORIAL_04_SCROLLZOOM) )
				tutorial->EnableNextStep();
		}
		else if (delta < 0)
		{
			m_group->OnZoomOutHorizontal(target, pow(1.5, -delta));

			//If in the tutorial, ungate the wizard
			auto tutorial = m_parent->GetTutorialWizard();
			if(tutorial && (tutorial->GetCurrentStep() == TutorialWizard::TUTORIAL_04_SCROLLZOOM) )
				tutorial->EnableNextStep();
		}
	}

	//Pan horizontally
	else if (delta_h != 0)
		m_group->OnPanHorizontal(delta_h);
}

/**
	@brief Handles a mouse wheel scroll step on the Y axis
 */
void WaveformArea::OnMouseWheelYAxis(float delta)
{
	//Cannot zoom eye patterns
	auto stream = GetFirstEyeStream();
	if(stream)
		return;
	stream = GetFirstConstellationStream();
	if(stream)
		return;

	stream = GetFirstAnalogOrDensityStream();

	if(delta > 0)
	{
		auto range = stream.GetVoltageRange();
		range *= pow(0.9, delta);

		for(size_t i=0; i<m_displayedChannels.size(); i++)
			m_displayedChannels[i]->GetStream().SetVoltageRange(range);
	}
	else
	{
		auto range = stream.GetVoltageRange();
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

	//If our current view is a density function, most stacking is disallowed
	auto estream = GetFirstDensityFunctionStream();
	if(estream)
	{
		//Allow protocol and digital overlays on spectrograms
		if(estream.GetType() == Stream::STREAM_TYPE_SPECTROGRAM)
		{
			if( (desc.GetType() == Stream::STREAM_TYPE_PROTOCOL) || (desc.GetType() == Stream::STREAM_TYPE_DIGITAL) )
				return true;
		}

		//anything else is a no go
		return false;
	}

	switch(desc.GetType())
	{
		//Allow stacking protocol overlays on spectrograms
		case Stream::STREAM_TYPE_SPECTROGRAM:
			if( (GetFirstAnalogStream() == nullptr) && (GetFirstDensityFunctionStream() == nullptr) )
				return true;
			break;

		//All other density plots must be in their own views and cannot stack
		case Stream::STREAM_TYPE_EYE:
		case Stream::STREAM_TYPE_CONSTELLATION:
		case Stream::STREAM_TYPE_WATERFALL:
			return false;

		//Digital and protocol channels can be overlaid on anything other than a density plot
		case Stream::STREAM_TYPE_DIGITAL:
		case Stream::STREAM_TYPE_PROTOCOL:
			return true;

		default:
			break;
	}

	//Can't go in this area if the Y unit is different
	if(m_yAxisUnit != desc.GetYAxisUnits())
		return false;

	//All good if we get here
	return true;
}

/**
	@brief Checks if this area is currently displaying a provided stream
 */
bool WaveformArea::IsStreamBeingDisplayed(StreamDescriptor target)
{
	for(auto& c : m_displayedChannels)
	{
		if(c->GetStream() == target)
			return true;
	}
	return false;
}

void WaveformArea::AutofitVertical()
{
	//Find the min and max of all currently displayed analog channels
	//TODO: do we want to not allow autoscale on instrument inputs?
	float vmax = FLT_MIN;
	float vmin = FLT_MAX;
	bool found = false;
	for(auto& c : m_displayedChannels)
	{
		auto data = c->GetStream().GetData();
		data->PrepareForCpuAccess();
		auto sdata = dynamic_cast<SparseAnalogWaveform*>(data);
		auto udata = dynamic_cast<UniformAnalogWaveform*>(data);
		if(!sdata && !udata)
			continue;

		found = true;
		vmax = max(vmax, Filter::GetMaxVoltage(sdata, udata));
		vmin = min(vmin, Filter::GetMinVoltage(sdata, udata));
	}

	if(found)
	{
		auto off = (vmax + vmin) / 2;
		auto range = (vmax - vmin) * 1.05;
		for(auto& c : m_displayedChannels)
		{
			c->GetStream().SetOffset(-off);
			c->GetStream().SetVoltageRange(range);
		}
	}

	m_parent->SetNeedRender();
}

