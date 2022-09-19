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

#include "imgui_internal.h"	//for SetItemUsingMouseWheel

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformArea::WaveformArea(StreamDescriptor stream, shared_ptr<WaveformGroup> group, MainWindow* parent)
	: m_height(1)
	, m_yAxisOffset(0)
	, m_pixelsPerYAxisUnit(1)
	, m_yAxisUnit(stream.GetYAxisUnits())
	, m_dragContext(this)
	, m_group(group)
	, m_parent(parent)
	, m_tLastMouseMove(GetTime())
{
	m_displayedChannels.push_back(make_shared<DisplayedChannel>(stream));
}

WaveformArea::~WaveformArea()
{
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
	@brief Renders a waveform area

	Returns false if the area should be closed (no more waveforms visible in it)
 */
bool WaveformArea::Render(int iArea, int numAreas, ImVec2 clientArea)
{
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

		m_pixelsPerYAxisUnit = totalHeightAvailable / first.GetVoltageRange();
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

		//Draw the background
		RenderBackgroundGradient(pos, csize);
		RenderGrid(pos, csize, gridmap, vbot, vtop);

		//Calculate midpoint of our plot
		m_ymid = pos.y + unspacedHeightPerArea / 2;

		//Blank out space for the actual waveform
		ImGui::Dummy(ImVec2(csize.x, csize.y));
		ImGui::SetItemAllowOverlap();

		//Draw texture for the actual waveform
		//(todo: repeat for each channel)
		//ImTextureID my_tex_id = m_parent->GetTexture("foo");
		//ImGui::Image(my_tex_id, ImVec2(csize.x, csize.y), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));

		//Catch mouse wheel events
		ImGui::SetItemUsingMouseWheel();
		if(ImGui::IsItemHovered())
		{
			auto wheel = ImGui::GetIO().MouseWheel;
			if(wheel != 0)
				OnMouseWheelPlotArea(wheel);
		}

		//Overlays for drag-and-drop
		DragDropOverlays(iArea, numAreas);

		//TODO: cursors, protocol decodes, etc

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

	//Help tooltip
	//Only show if mouse has been still for 1 sec
	//(shorter delays interfere with dragging)
	double tnow = GetTime();
	if( (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) && (tnow - m_tLastMouseMove > 1) )
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
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

	//TODO: trigger level arrow(s)

	ImGui::EndChild();

	//Start dragging
	if(ImGui::IsItemHovered())
	{
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			LogTrace("Start dragging Y axis\n");
			m_dragState = DRAG_STATE_Y_AXIS;
		}
	}
}

/**
	@brief Drag-and-drop overlay areas
 */
void WaveformArea::DragDropOverlays(int iArea, int numAreas)
{
	auto csize = ImGui::GetContentRegionAvail();
	auto start = ImGui::GetWindowContentRegionMin();

	//Drag/drop areas for splitting
	float widthOfVerticalEdge = csize.x*0.25;
	float leftOfMiddle = start.x + widthOfVerticalEdge;
	float rightOfMiddle = start.x + csize.x*0.75;
	float topOfMiddle = start.y;
	float bottomOfMiddle = start.y + csize.y;
	float widthOfMiddle = rightOfMiddle - leftOfMiddle;
	if(iArea == 0)
	{
		EdgeDropArea("top", ImVec2(leftOfMiddle, start.y), ImVec2(widthOfMiddle, csize.y*0.125), ImGuiDir_Up);
		topOfMiddle += csize.y * 0.125;
	}
	if(iArea == (numAreas-1))
	{
		bottomOfMiddle -= csize.y * 0.125;
		ImVec2 pos(leftOfMiddle, bottomOfMiddle);
		ImVec2 size(widthOfMiddle, csize.y*0.125);
		EdgeDropArea("bottom", pos, size, ImGuiDir_Down);
	}
	float heightOfMiddle = bottomOfMiddle - topOfMiddle;
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
	ImGui::SetCursorPos(start);
	ImGui::InvisibleButton(name.c_str(), size);
	ImGui::SetItemAllowOverlap();

	//Add drop target
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("Waveform");
		if( (payload != nullptr) && (payload->DataSize == sizeof(WaveformDragContext*)) )
		{
			auto context = reinterpret_cast<WaveformDragContext*>(payload->Data);
			auto stream = context->m_sourceArea->GetStream(context->m_streamIndex);

			//Add request to split our current group
			m_parent->QueueSplitGroup(m_group, splitDir, stream);

			//Remove the stream from the originating waveform area
			context->m_sourceArea->RemoveStream(context->m_streamIndex);
		}

		ImGui::EndDragDropTarget();
	}
}

/**
	@brief Drop area for the middle of the plot

	Dropping a waveform in here adds it to the plot
 */
void WaveformArea::CenterDropArea(ImVec2 start, ImVec2 size)
{
	ImGui::SetCursorPos(start);
	ImGui::InvisibleButton("center", size);
	ImGui::SetItemAllowOverlap();

	//Add drop target
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("Waveform");
		if( (payload != nullptr) && (payload->DataSize == sizeof(WaveformDragContext*)) )
		{
			auto context = reinterpret_cast<WaveformDragContext*>(payload->Data);
			auto stream = context->m_sourceArea->GetStream(context->m_streamIndex);

			//Add the new stream to us
			//TODO: copy view settings from the DisplayedChannel over?
			AddStream(stream);

			//Remove the stream from the originating waveform area
			context->m_sourceArea->RemoveStream(context->m_streamIndex);
		}

		ImGui::EndDragDropTarget();
	}
}


void WaveformArea::DraggableButton(shared_ptr<DisplayedChannel> chan, size_t index)
{
	ImGui::Button(chan->GetName().c_str());

	if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		m_dragContext.m_streamIndex = index;
		ImGui::SetDragDropPayload("Waveform", &m_dragContext, sizeof(WaveformDragContext*));

		//Preview of what we're dragging
		ImGui::Text("Drag %s", chan->GetName().c_str());

		ImGui::EndDragDropSource();
	}

	auto rchan = chan->GetStream().m_channel;

	if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		m_parent->ShowChannelProperties(rchan);

	//Display channel information and help text in tooltip
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		string tooltip;
		tooltip += string("Channel ") + rchan->GetHwname() + " of instrument " + rchan->GetScope()->m_nickname + "\n\n";

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
	auto pos = ImGui::GetWindowPos();

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
