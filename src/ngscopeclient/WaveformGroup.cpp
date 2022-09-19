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
	@brief Implementation of WaveformGroup
 */
#include "ngscopeclient.h"
#include "WaveformGroup.h"
#include "MainWindow.h"
#include "imgui_internal.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformGroup::WaveformGroup(MainWindow* parent, const string& title)
	: m_parent(parent)
	, m_pixelsPerXUnit(0.00005)
	, m_xAxisOffset(0)
	, m_title(title)
	, m_xAxisUnit(Unit::UNIT_FS)
	, m_draggingTimeline(false)
	, m_tLastMouseMove(GetTime())
{
}

WaveformGroup::~WaveformGroup()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Area management

void WaveformGroup::AddArea(shared_ptr<WaveformArea>& area)
{
	m_areas.push_back(area);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool WaveformGroup::Render()
{
	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Appearing);
	if(!ImGui::Begin(m_title.c_str(), &open))
	{
		ImGui::End();
		return false;
	}

	ImVec2 clientArea = ImGui::GetContentRegionAvail();

	//Render the timeline
	auto timelineHeight = 2.5 * ImGui::GetFontSize();
	clientArea.y -= timelineHeight;
	RenderTimeline(clientArea.x, timelineHeight);

	//Render our waveform areas
	//TODO: waveform areas full of protocol or digital decodes should be fixed size
	//while analog will fill the gap?
	vector<size_t> areasToClose;
	for(size_t i=0; i<m_areas.size(); i++)
	{
		if(!m_areas[i]->Render(i, m_areas.size(), clientArea))
			areasToClose.push_back(i);
	}

	//Close any areas that are now empty
	for(ssize_t i=static_cast<ssize_t>(areasToClose.size()) - 1; i >= 0; i--)
		m_areas.erase(m_areas.begin() + areasToClose[i]);

	//If we no longer have any areas in the group, close the group
	if(m_areas.empty())
		open = false;

	//Render cursors over everything else

	ImGui::End();
	return open;
}

void WaveformGroup::RenderTimeline(float width, float height)
{
	ImGui::BeginChild("timeline", ImVec2(width, height));

	//TODO: handle mouse wheel on the timeline

	auto list = ImGui::GetWindowDrawList();

	//Style settings
	//TODO: get some/all of this from preferences
	ImU32 color = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));
	auto font = m_parent->GetDefaultFont();

	//Reserve an empty area for the timeline
	auto pos = ImGui::GetWindowPos();
	ImGui::Dummy(ImVec2(width, height));

	//Detect mouse movement
	double tnow = GetTime();
	auto mouseDelta = ImGui::GetIO().MouseDelta;
	if( (mouseDelta.x != 0) || (mouseDelta.y != 0) )
		m_tLastMouseMove = tnow;

	//Help tooltip
	//Only show if mouse has been still for 250ms
	if( (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) && (tnow - m_tLastMouseMove > 0.25) )
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted("Click and drag to scroll the timeline.\nUse mouse wheel to zoom.");
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	//Catch mouse wheel events
	ImGui::SetItemUsingMouseWheel();
	if(ImGui::IsItemHovered())
	{
		auto wheel = ImGui::GetIO().MouseWheel;
		if(wheel != 0)
			OnMouseWheel(wheel);
	}

	//Handle dragging
	//(Mouse is allowed to leave the window, as long as original click was within us)
	if(ImGui::IsItemHovered())
	{
		if(ImGui::IsMouseClicked(0))
			m_draggingTimeline = true;
	}
	if(m_draggingTimeline)
	{
		//Use relative delta, not drag delta, since we update the offset every frame
		float dx = mouseDelta.x * ImGui::GetWindowDpiScale();
		if(dx != 0)
		{
			m_xAxisOffset -= PixelsToXAxisUnits(dx);
			ClearPersistence();
		}

		if(ImGui::IsMouseReleased(0))
			m_draggingTimeline = false;
	}

	//Dimensions for various things
	float dpiScale = ImGui::GetWindowDpiScale();
	float fineTickLength = 10 * dpiScale;
	float coarseTickLength = height;
	const double min_label_grad_width = 75 * dpiScale;	//Minimum distance between text labels
	float thickLineWidth = 2;
	float thinLineWidth = 1;
	float fontSize = ImGui::GetFontSize();
	float ymid = pos.y + height/2;

	//Top line
	list->PathLineTo(pos);
	list->PathLineTo(ImVec2(pos.x + width, pos.y));
	list->PathStroke(color, 0, thickLineWidth);

	//Figure out rounding granularity, based on our time scales
	float xscale = m_pixelsPerXUnit / dpiScale;
	int64_t width_xunits = width / xscale;
	auto round_divisor = GetRoundingDivisor(width_xunits);

	//Figure out about how much time per graduation to use
	double grad_xunits_nominal = min_label_grad_width / xscale;

	//Round so the division sizes are sane
	double units_per_grad = grad_xunits_nominal * 1.0 / round_divisor;
	double base = 5;
	double log_units = log(units_per_grad) / log(base);
	double log_units_rounded = ceil(log_units);
	double units_rounded = pow(base, log_units_rounded);
	float textMargin = 2;
	int64_t grad_xunits_rounded = round(units_rounded * round_divisor);

	//avoid divide-by-zero in weird cases with no waveform etc
	if(grad_xunits_rounded == 0)
		return;

	//Calculate number of ticks within a division
	double nsubticks = 5;
	double subtick = grad_xunits_rounded / nsubticks;

	//Find the start time (rounded as needed)
	double tstart = round(m_xAxisOffset / grad_xunits_rounded) * grad_xunits_rounded;

	//Print tick marks and labels
	for(double t = tstart; t < (tstart + width_xunits + grad_xunits_rounded); t += grad_xunits_rounded)
	{
		double x = (t - m_xAxisOffset) * xscale;

		//Draw fine ticks first (even if the labeled graduation doesn't fit)
		for(int tick=1; tick < nsubticks; tick++)
		{
			double subx = (t - m_xAxisOffset + tick*subtick) * xscale;

			if(subx < 0)
				continue;
			if(subx > width)
				break;
			subx += pos.x;

			list->PathLineTo(ImVec2(subx, pos.y));
			list->PathLineTo(ImVec2(subx, pos.y + fineTickLength));
			list->PathStroke(color, 0, thinLineWidth);
		}

		if(x < 0)
			continue;
		if(x > width)
			break;

		//Coarse ticks
		x += pos.x;
		list->PathLineTo(ImVec2(x, pos.y));
		list->PathLineTo(ImVec2(x, pos.y + coarseTickLength));
		list->PathStroke(color, 0, thickLineWidth);

		//Render label
		//TODO: is using the default font faster? by enough to matter?
		list->AddText(
			font,
			fontSize,
			ImVec2(x + textMargin, ymid),
			color,
			m_xAxisUnit.PrettyPrint(t).c_str());
	}

	ImGui::EndChild();
}

/**
	@brief Handles a mouse wheel scroll step
 */
void WaveformGroup::OnMouseWheel(float delta)
{
	auto pos = ImGui::GetWindowPos();
	float relativeMouseX = ImGui::GetIO().MousePos.x - pos.x;
	relativeMouseX *= ImGui::GetWindowDpiScale();

	//TODO: if shift is held, scroll horizontally

	int64_t target = XPositionToXAxisUnits(relativeMouseX);

	//Zoom in
	if(delta > 0)
		OnZoomInHorizontal(target, pow(1.5, delta));
	else
		OnZoomOutHorizontal(target, pow(1.5, -delta));
}

/**
	@brief Decide on reasonable rounding intervals for X axis scale ticks
 */
int64_t WaveformGroup::GetRoundingDivisor(int64_t width_xunits)
{
	int64_t round_divisor = 1;

	if(width_xunits < 1E7)
	{
		//fs, leave default
		if(width_xunits < 1e2)
			round_divisor = 1e1;
		else if(width_xunits < 1e5)
			round_divisor = 1e4;
		else if(width_xunits < 5e5)
			round_divisor = 5e4;
		else if(width_xunits < 1e6)
			round_divisor = 1e5;
		else if(width_xunits < 2.5e6)
			round_divisor = 2.5e5;
		else if(width_xunits < 5e6)
			round_divisor = 5e5;
		else
			round_divisor = 1e6;
	}
	else if(width_xunits < 1e9)
		round_divisor = 1e6;
	else if(width_xunits < 1e12)
	{
		if(width_xunits < 1e11)
			round_divisor = 1e8;
		else
			round_divisor = 1e9;
	}
	else if(width_xunits < 1E14)
		round_divisor = 1E12;
	else
		round_divisor = 1E15;

	return round_divisor;
}

/**
	@brief Clear saved persistence waveforms
 */
void WaveformGroup::ClearPersistence()
{
	LogTrace("Not implemented\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Zooming

/**
	@brief Zoom in, keeping timestamp "target" at the same pixel position
 */
void WaveformGroup::OnZoomInHorizontal(int64_t target, float step)
{
	//Calculate the *current* position of the target within the window
	float delta = target - m_xAxisOffset;

	//Change the zoom
	m_pixelsPerXUnit *= step;
	m_xAxisOffset = target - (delta/step);

	ClearPersistence();
}

void WaveformGroup::OnZoomOutHorizontal(int64_t target, float step)
{
	//TODO: Clamp to bounds of all waveforms in the group
	//(not width of single widest waveform, as they may have different offsets)

	//Calculate the *current* position of the target within the window
	float delta = target - m_xAxisOffset;

	//Change the zoom
	m_pixelsPerXUnit /= step;
	m_xAxisOffset = target - (delta*step);

	ClearPersistence();
}
