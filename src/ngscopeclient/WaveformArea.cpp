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
// Construction / destruction

WaveformArea::WaveformArea(StreamDescriptor stream, shared_ptr<WaveformGroup> group, MainWindow* parent)
	: m_height(1)
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
	, m_toneMapPipe("shaders/WaveformToneMap.spv", 1, sizeof(ToneMapArgs), 1)
{
	m_displayedChannels.push_back(make_shared<DisplayedChannel>(stream));

	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		g_renderQueueType );
	m_cmdPool = make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(**m_cmdPool, vk::CommandBufferLevel::ePrimary, 1);
	m_cmdBuffer = make_unique<vk::raii::CommandBuffer>(
		move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));
}

WaveformArea::~WaveformArea()
{
	m_cmdBuffer = nullptr;
	m_cmdPool = nullptr;
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

	//Detect mouse movement
	double tnow = GetTime();
	auto mouseDelta = ImGui::GetIO().MouseDelta;
	if( (mouseDelta.x != 0) || (mouseDelta.y != 0) )
		m_tLastMouseMove = tnow;

	ImGui::PushID(to_string(iArea).c_str());

	float totalHeightAvailable = clientArea.y - ImGui::GetFrameHeightWithSpacing();
	float spacing = ImGui::GetFrameHeightWithSpacing() - ImGui::GetFrameHeight();
	float heightPerArea = totalHeightAvailable / numAreas;
	float unspacedHeightPerArea = heightPerArea - spacing;

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
	float yAxisWidth = 5 * ImGui::GetFontSize() * ImGui::GetWindowDpiScale();
	float yAxisWidthSpaced = yAxisWidth + spacing;

	//Settings calculated by RenderGrid() then reused in RenderYAxis()
	map<float, float> gridmap;
	float vbot;
	float vtop;

	if(ImGui::BeginChild(ImGui::GetID(this), ImVec2(clientArea.x - yAxisWidthSpaced, unspacedHeightPerArea)))
	{
		auto csize = ImGui::GetContentRegionAvail();
		auto pos = ImGui::GetWindowPos();

		//Calculate midpoint of our plot
		m_ymid = pos.y + unspacedHeightPerArea / 2;

		//Draw the background
		RenderBackgroundGradient(pos, csize);
		RenderGrid(pos, csize, gridmap, vbot, vtop);

		//Blank out space for the actual waveform
		ImGui::Dummy(ImVec2(csize.x, csize.y));
		ImGui::SetItemAllowOverlap();

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

		//Draw control widgets
		ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
		ImGui::BeginGroup();

			for(size_t i=0; i<m_displayedChannels.size(); i++)
				DraggableButton(m_displayedChannels[i], i);

		ImGui::EndGroup();
		ImGui::SetItemAllowOverlap();
	}
	ImGui::EndChild();

	//Draw the vertical scale on the right side of the plot
	RenderYAxis(ImVec2(yAxisWidth, unspacedHeightPerArea), gridmap, vbot, vtop);

	ImGui::PopID();

	if(m_displayedChannels.empty())
		return false;
	return true;
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
	//auto chan = stream.m_channel;
	auto data = stream.GetData();
	if(data == nullptr)
		return;

	auto list = ImGui::GetWindowDrawList();

	//Tone map the waveform
	//TODO: only do this if the texture was updated
	//TODO: what kind of mutexing etc do we need, if any?
	ToneMapAnalogWaveform(channel, size);

	//Render the tone mapped output
	list->AddImage(channel->GetTextureHandle(), start, ImVec2(start.x+size.x, start.y+size.y));

	//DEBUG: simple quick-and-dirty renderer for testing
	/*auto u = dynamic_cast<UniformAnalogWaveform*>(data);
	if(u)
	{
		auto n = data->size();
		auto color = ColorFromString(chan->m_displaycolor);
		for(size_t i=1; i<n; i++)
		{
			ImVec2 start(
				m_group->XAxisUnitsToXPosition(((i-1) * data->m_timescale) + data->m_triggerPhase),
				YAxisUnitsToYPosition(u->m_samples[i-1]));

			ImVec2 end(
				m_group->XAxisUnitsToXPosition((i * data->m_timescale) + data->m_triggerPhase),
				YAxisUnitsToYPosition(u->m_samples[i]));

			list->AddLine(start, end, color);
		}
	}
	*/
}

/**
	@brief Tone maps an analog waveform by converting the internal fp32 buffer to RGBA
 */
void WaveformArea::ToneMapAnalogWaveform(shared_ptr<DisplayedChannel> channel, ImVec2 size)
{
	m_cmdBuffer->begin({});

	//Reallocate the texture if its size has changed
	if(channel->UpdateSize(size))
	{
		LogTrace("Waveform area resized (to %.0f x %.0f), reallocating texture\n", size.x, size.y);

		vector<uint32_t> queueFamilies;
		vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
		queueFamilies.push_back(g_computeQueueType);	//FIXME: separate transfer queue?
		if(g_renderQueueType != g_computeQueueType)
		{
			queueFamilies.push_back(g_renderQueueType);
			sharingMode = vk::SharingMode::eConcurrent;
		}
		vk::ImageCreateInfo imageInfo(
			{},
			vk::ImageType::e2D,
			vk::Format::eR32G32B32A32Sfloat,
			vk::Extent3D(size.x, size.y, 1),
			1,
			1,
			VULKAN_HPP_NAMESPACE::SampleCountFlagBits::e1,
			VULKAN_HPP_NAMESPACE::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			sharingMode,
			queueFamilies,
			vk::ImageLayout::eUndefined
			);

		auto tex = make_shared<Texture>(*g_vkComputeDevice, imageInfo, m_parent->GetTextureManager());
		channel->SetTexture(tex);

		//Add a barrier to convert the image format to "general"
		vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		vk::ImageMemoryBarrier barrier(
			vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eShaderWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			tex->GetImage(),
			range);
		m_cmdBuffer->pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eComputeShader,
				{},
				{},
				{},
				barrier);
	}

	//Temporary buffer until we have real rendering shader done
	int width = size.x;
	int npixels = width * (int)size.y;
	AcceleratorBuffer<float> temp;
	temp.resize(npixels);
	for(int y=0; y<size.y; y++)
	{
		for(int x=0; x<width; x++)
			temp[y*width + x] = fabs(sin(x*1.0 / 50) + sin(y*1.0 / 30));
	}
	temp.MarkModifiedFromCpu();

	//Run the actual compute shader
	auto tex = channel->GetTexture();
	m_toneMapPipe.BindBuffer(0, temp);
	m_toneMapPipe.BindStorageImage(
		1,
		**m_parent->GetTextureManager()->GetSampler(),
		tex->GetView(),
		vk::ImageLayout::eGeneral);
	auto color = ImGui::ColorConvertU32ToFloat4(ColorFromString(channel->GetStream().m_channel->m_displaycolor));
	ToneMapArgs args(color, size.x, size.y);
	m_toneMapPipe.Dispatch(*m_cmdBuffer, args, GetComputeBlockCount(size.x, 64), (uint32_t)size.y);

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
	m_cmdBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier);
	m_cmdBuffer->end();
	SubmitAndBlock(*m_cmdBuffer, m_parent->GetRenderQueue());
}

/**
	@brief Renders the background of the main plot area

	For now, simple gray gradient.
 */
void WaveformArea::RenderBackgroundGradient(ImVec2 start, ImVec2 size)
{
	ImU32 color_bottom = ImGui::GetColorU32(IM_COL32(0, 0, 0, 255));
	ImU32 color_top = ImGui::GetColorU32(IM_COL32(32, 32, 32, 255));

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

	//Decide what voltage step to use. Pick from a list (in volts)
	float selected_step = PickStepSize(volts_per_half_span);

	//Special case a few scenarios
	if(stream.GetYAxisUnits() == Unit::UNIT_LOG_BER)
		selected_step = 2;

	float theight = ImGui::GetFontSize();
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
	//TODO: get some/all of this from preferences
	ImU32 gridColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.75, 0.75, 0.75, 0.25));
	ImU32 axisColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.75, 0.75, 0.75, 1));
	float axisWidth = 2;
	float gridWidth = 2;

	auto list = ImGui::GetWindowDrawList();
	float left = start.x;
	float right = start.x + size.x;
	for(auto it : gridmap)
	{
		list->PathLineTo(ImVec2(left, it.second));
		list->PathLineTo(ImVec2(right, it.second));
		list->PathStroke(gridColor, gridWidth);
	}

	//draw Y=0 line
	if( (yzero > ytop) && (yzero < ybot) )
	{
		list->PathLineTo(ImVec2(left, yzero));
		list->PathLineTo(ImVec2(right, yzero));
		list->PathStroke(axisColor, axisWidth);
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
	//TODO: get some/all of this from preferences
	auto font = m_parent->GetDefaultFont();
	float theight = ImGui::GetFontSize();
	ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1));

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

		draw_list->AddText(font, theight, ImVec2(origin.x + size.x - tsize.x - xmargin, y), color, label.c_str());
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
	//TODO: set ImGuiCol_DragDropTarget to invisible (zero alpha)
	//and/or set ImGuiDragDropFlags_AcceptNoDrawDefaultRect

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

	//Draw the icons for landing spots
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
			auto stream = desc->first->GetStream(desc->second);

			//Add the new stream to us
			//TODO: copy view settings from the DisplayedChannel over?
			AddStream(stream);

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
}

void WaveformArea::DraggableButton(shared_ptr<DisplayedChannel> chan, size_t index)
{
	auto rchan = chan->GetStream().m_channel;

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
		tooltip += string("Channel ") + rchan->GetHwname() + " of instrument " + rchan->GetScope()->m_nickname + "\n\n";

		//See if we have data
		auto data = chan->GetStream().GetData();
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
			"Double click to view/edit channel properties.";

		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(tooltip.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void WaveformArea::ClearPersistence()
{
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
	auto pos = ImGui::GetWindowPos();
	float relativeMouseX = ImGui::GetIO().MousePos.x - pos.x;
	relativeMouseX *= ImGui::GetWindowDpiScale();

	//TODO: if shift is held, scroll horizontally

	int64_t target = m_group->XPositionToXAxisUnits(relativeMouseX);

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
}
