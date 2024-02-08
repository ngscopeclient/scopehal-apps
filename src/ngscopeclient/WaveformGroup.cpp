/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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

#include "../../scopeprotocols/EyePattern.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformGroup::WaveformGroup(MainWindow* parent, const string& title)
	: m_parent(parent)
	, m_xpos(0)
	, m_width(0)
	, m_pixelsPerXUnit(0.00005)
	, m_xAxisOffset(0)
	, m_title(title)
	, m_id(title)
	, m_xAxisUnit(Unit::UNIT_FS)
	, m_dragState(DRAG_STATE_NONE)
	, m_dragMarker(nullptr)
	, m_tLastMouseMove(GetTime())
	, m_timelineHeight(0)
	, m_mouseOverTriggerArrow(false)
	, m_scopeTriggerDuringDrag(nullptr)
	, m_displayingEye(false)
	, m_xAxisCursorMode(X_CURSOR_NONE)
{
	m_xAxisCursorPositions[0] = 0;
	m_xAxisCursorPositions[1] = 0;
}

WaveformGroup::~WaveformGroup()
{
	Clear();
}

void WaveformGroup::Clear()
{
	lock_guard<mutex> lock(m_areaMutex);

	LogTrace("Destroying areas\n");
	LogIndenter li;

	m_areas.clear();

	LogTrace("All areas removed\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Area management

void WaveformGroup::AddArea(shared_ptr<WaveformArea>& area)
{
	lock_guard<mutex> lock(m_areaMutex);
	{
		//If this is our first area, adopt its X axis unit as our own
		if(m_areas.empty())
			m_xAxisUnit = area->GetStream(0).GetXAxisUnits();

		m_areas.push_back(area);
	}

	m_parent->RefreshTimebasePropertiesDialog();
}

/**
	@brief Returns true if a channel is being dragged from any WaveformArea within the group
 */
bool WaveformGroup::IsChannelBeingDragged()
{
	auto areas = GetWaveformAreas();
	for(auto a : areas)
	{
		if(a->IsChannelBeingDragged())
			return true;
	}
	return false;
}

/**
	@brief Returns the channel being dragged, if one exists
 */
StreamDescriptor WaveformGroup::GetChannelBeingDragged()
{
	auto areas = GetWaveformAreas();
	for(auto a : areas)
	{
		auto stream = a->GetChannelBeingDragged();
		if(stream)
			return stream;
	}
	return StreamDescriptor(nullptr, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Run the tone-mapping shader on all of our waveforms

	Called by MainWindow::ToneMapAllWaveforms() at the start of each frame if new data is ready to render
 */
void WaveformGroup::ToneMapAllWaveforms(vk::raii::CommandBuffer& cmdbuf)
{
	auto areas = GetWaveformAreas();

	for(auto a : areas)
		a->ToneMapAllWaveforms(cmdbuf);
}

void WaveformGroup::ReferenceWaveformTextures()
{
	auto areas = GetWaveformAreas();
	for(auto a : areas)
		a->ReferenceWaveformTextures();
}

void WaveformGroup::RenderWaveformTextures(
	vk::raii::CommandBuffer& cmdbuf,
	vector<shared_ptr<DisplayedChannel> >& channels,
	bool clearPersistence)
{
	bool clearThisGroupOnly = m_clearPersistence.exchange(false);

	auto areas = GetWaveformAreas();
	for(auto a : areas)
		a->RenderWaveformTextures(cmdbuf, channels, clearThisGroupOnly || clearPersistence);
}

bool WaveformGroup::Render()
{
	auto areas = GetWaveformAreas();

	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Appearing);
	if(!ImGui::Begin(GetID().c_str(), &open))
	{
		//tabbed out, don't draw anything until we're back in the foreground
		ImGui::End();
		return true;
	}

	//Check for right click on the title bar
	//see https://github.com/ocornut/imgui/issues/316
	//(for now, it doesn't work if the window is docked)
	if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		auto rect = ImGui::GetCurrentWindow()->TitleBarRect();
		if(ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false))
			ImGui::OpenPopup("Rename Group");
	}

	if(ImGui::BeginPopup("Rename Group"))
	{
		ImGui::InputText("Name", &m_title);
		ImGui::EndPopup();
	}

	auto pos = ImGui::GetCursorScreenPos();
	ImVec2 clientArea = ImGui::GetContentRegionMax();
	m_width = clientArea.x;

	float yAxisWidthSpaced = GetYAxisWidth() + GetSpacing();
	float plotWidth = clientArea.x - yAxisWidthSpaced;

	//Update X axis unit
	if(!areas.empty())
	{
		m_displayingEye = false;
		m_xAxisUnit = areas[0]->GetStream(0).GetXAxisUnits();

		//Autoscale eye patterns
		auto firstStream = areas[0]->GetFirstAnalogOrDensityStream();
		if(firstStream && (firstStream.GetType() == Stream::STREAM_TYPE_EYE))
		{
			auto eye = dynamic_cast<EyeWaveform*>(firstStream.GetData());
			if(eye && eye->GetWidth())
			{
				m_pixelsPerXUnit = plotWidth / (2*eye->GetUIWidth());
				m_xAxisOffset = -PixelsToXAxisUnits(plotWidth/2);
				m_displayingEye = true;
			}
		}
	}

	//Render the timeline
	m_timelineHeight = 2.5 * ImGui::GetFontSize();
	clientArea.y -= m_timelineHeight;
	RenderTimeline(plotWidth, m_timelineHeight);

	//Close any areas that we destroyed last frame
	//Block until all background processing completes to ensure no command buffers are still pending
	if(!m_areasToClose.empty())
	{
		g_vkComputeDevice->waitIdle();
		m_areasToClose.clear();
	}

	//Render our waveform areas
	//TODO: waveform areas full of protocol or digital decodes should be fixed size while analog will fill the gap?
	//Anything we closed is removed from the list THIS frame, so we stop rendering to them etc
	//but not actually destroyed until next frame
	for(size_t i=0; i<areas.size(); i++)
	{
		if(!areas[i]->Render(i, areas.size(), clientArea))
			m_areasToClose.push_back(i);
	}
	for(ssize_t i=static_cast<ssize_t>(m_areasToClose.size()) - 1; i >= 0; i--)
		m_areas.erase(m_areas.begin() + m_areasToClose[i]);
	if(!m_areasToClose.empty())
		m_parent->RefreshTimebasePropertiesDialog();

	//If we no longer have any areas in the group, close the group
	if(areas.empty())
		open = false;

	//Render cursors over everything else
	ImVec2 plotSize(plotWidth, clientArea.y);
	RenderXAxisCursors(pos, plotSize);
	if(m_xAxisCursorMode != X_CURSOR_NONE)
		DoCursorReadouts();
	RenderMarkers(pos, plotSize);

	ImGui::End();

	return open;
}

/**
	@brief Run the popup window with cursor values
 */
void WaveformGroup::DoCursorReadouts()
{
	auto areas = GetWaveformAreas();

	bool hasSecondCursor = (m_xAxisCursorMode == X_CURSOR_DUAL);

	string name = string("Cursors (") + m_title + ")";
	float width = ImGui::GetFontSize();
	ImGui::SetNextWindowSize(ImVec2(45*width, 15*width), ImGuiCond_Appearing);
	if(ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_NoCollapse))
	{
		static ImGuiTableFlags flags =
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_ScrollY;

		//Add columns for second cursor if enabled
		int ncols = 2;
		if(hasSecondCursor)
			ncols += 3;

		if(ImGui::BeginTable("cursors", ncols, flags))
		{
			//Header row
			//TODO: only show in-band power column if units match up?
			ImGui::TableSetupScrollFreeze(0, 1); 	//Header row does not scroll
			ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 10*width);
			ImGui::TableSetupColumn("Value 1", ImGuiTableColumnFlags_WidthFixed, 8*width);
			if(hasSecondCursor)
			{
				ImGui::TableSetupColumn("Value 2", ImGuiTableColumnFlags_WidthFixed, 8*width);
				ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 8*width);
				ImGui::TableSetupColumn("Band", ImGuiTableColumnFlags_WidthFixed, 8*width);
			}
			ImGui::TableHeadersRow();

			//Readout for each channel in all of our waveform areas
			for(auto a : areas)
			{
				for(size_t i=0; i<a->GetStreamCount(); i++)
				{
					auto stream = a->GetStream(i);
					auto sname = stream.GetName();

					//Prepare to pretty print
					auto data = stream.GetData();
					string sv1 = "(no data)";
					string sv2 = "(no data)";
					string svd = "(no data)";

					switch(stream.GetType())
					{
						//Analog path
						case Stream::STREAM_TYPE_ANALOG:
							{
								bool zhold = (stream.GetFlags() & Stream::STREAM_DO_NOT_INTERPOLATE) ? true : false;
								auto v1 = GetValueAtTime(data, m_xAxisCursorPositions[0], zhold);
								auto v2 = GetValueAtTime(data, m_xAxisCursorPositions[1], zhold);
								if(v1)
									sv1 = stream.GetYAxisUnits().PrettyPrint(v1.value());
								if(v2)
									sv2 = stream.GetYAxisUnits().PrettyPrint(v2.value());
								if(v1 && v2)
									svd = stream.GetYAxisUnits().PrettyPrint(v2.value() - v1.value());
							}
						break;

						//Digital path
						case Stream::STREAM_TYPE_DIGITAL:
							{
								auto v1 = GetDigitalValueAtTime(data, m_xAxisCursorPositions[0]);
								auto v2 = GetDigitalValueAtTime(data, m_xAxisCursorPositions[1]);
								if(v1)
									sv1 = to_string(v1.value());
								if(v2)
									sv2 = to_string(v2.value());

								svd = "";
							}
						break;

						//TODO
						case Stream::STREAM_TYPE_DIGITAL_BUS:
							sv1 = "(unimplemented)";
							sv2 = "(unimplemented)";
							svd = "(unimplemented)";
							break;

						//Cursor readout on density plots makes no sense
						//TODO: read out eye height or something for eyes?
						case Stream::STREAM_TYPE_EYE:
						case Stream::STREAM_TYPE_SPECTROGRAM:
						case Stream::STREAM_TYPE_WATERFALL:
						case Stream::STREAM_TYPE_TRIGGER:
						case Stream::STREAM_TYPE_UNDEFINED:
						case Stream::STREAM_TYPE_ANALOG_SCALAR:
							sv1 = "";
							sv2 = "";
							svd = "";
							break;

						//Read out protocol decode stuff
						case Stream::STREAM_TYPE_PROTOCOL:
							{
								auto v1 = GetProtocolValueAtTime(data, m_xAxisCursorPositions[0]);
								auto v2 = GetProtocolValueAtTime(data, m_xAxisCursorPositions[1]);
								if(v1)
									sv1 = v1.value();
								if(v2)
									sv2 = v2.value();
								svd = "";
							}
							break;
					}

					ImGui::PushID(sname.c_str());
					ImGui::TableNextRow(ImGuiTableRowFlags_None, 0);

					//Channel name
					ImGui::TableSetColumnIndex(0);
					auto color = ColorFromString(stream.m_channel->m_displaycolor);
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextUnformatted(sname.c_str());
					ImGui::PopStyleColor();

					//Cursor 0 value
					ImGui::TableSetColumnIndex(1);
					RightJustifiedText(sv1);

					if(hasSecondCursor)
					{
						//Cursor 1 value
						ImGui::TableSetColumnIndex(2);
						RightJustifiedText(sv2);

						//Delta
						ImGui::TableSetColumnIndex(3);
						RightJustifiedText(svd);

						//In-band power
						Unit punit(Unit::UNIT_COUNTS);
						bool ok = true;
						switch(stream.GetYAxisUnits().GetType())
						{
							case Unit::UNIT_DBM:
								punit = Unit(Unit::UNIT_DBM);
								break;

							case Unit::UNIT_W_M2_NM:
								punit = Unit(Unit::UNIT_W_M2);
								break;

							default:
								ok = false;
						}

						if(ok)
						{
							auto power = GetInBandPower(
								data,
								stream.GetYAxisUnits(),
								m_xAxisCursorPositions[0],
								m_xAxisCursorPositions[1]);
							ImGui::TableSetColumnIndex(4);
							RightJustifiedText(punit.PrettyPrint(power));
						}
					}

					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}

/**
	@brief Calculates the in-band power between two frequencies
 */
float WaveformGroup::GetInBandPower(WaveformBase* wfm, Unit yunit, int64_t t1, int64_t t2)
{
	auto swfm = dynamic_cast<SparseAnalogWaveform*>(wfm);
	auto uwfm = dynamic_cast<UniformAnalogWaveform*>(wfm);

	//Make sure we have data
	if(!swfm && !uwfm)
		return 0;
	if(!wfm->size())
		return 0;

	//Get the samples and start/end indexex
	auto& samples = swfm ? swfm->m_samples : uwfm->m_samples;
	bool err1;
	bool err2;
	auto ileft = GetIndexNearestAtOrBeforeTimestamp(wfm, t1, err1);
	auto iright = GetIndexNearestAtOrBeforeTimestamp(wfm, t2, err2);
	if(err1)
		ileft = 0;
	if(err2)
		iright = wfm->size() - 1;

	//Sum the in-band power
	//Note that if it's in dBm we have to go to linear units and back
	bool is_log = (yunit == Unit::UNIT_DBM);
	bool is_irradiance = (yunit == Unit::UNIT_W_M2_NM);
	float total = 0;
	for(size_t i=ileft; i <= iright; i++)
	{
		float f = samples[i];
		if(is_log)
			total += pow(10, (f - 30) / 10);	//assume
		else if(is_irradiance)
			total += f * GetDurationScaled(swfm, uwfm, i) * 1e-3;	//scale by pm to nm
		else
			total += f;
	}
	if(is_log)
		total = 10 * log10(total) + 30;

	return total;
}

/**
	@brief Render our markers
 */
void WaveformGroup::RenderMarkers(ImVec2 pos, ImVec2 size)
{
	//Don't draw anything if our unit isn't fs
	//TODO: support units for frequency domain channels etc?
	//TODO: early out if eye pattern
	if(m_xAxisUnit != Unit(Unit::UNIT_FS))
		return;

	//Don't crash if we have no areas
	if(m_areas.empty())
		return;

	auto& markers = m_parent->GetSession().GetMarkers(m_areas[0]->GetWaveformTimestamp());

	//Create a child window for all of our drawing
	//(this is needed so we're above the WaveformArea's in z order, but behind popup windows)
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	if(ImGui::BeginChild("markers", size, false, ImGuiWindowFlags_NoInputs))
	{
		auto list = ImGui::GetWindowDrawList();

		auto& prefs = m_parent->GetSession().GetPreferences();
		auto color = prefs.GetColor("Appearance.Cursors.marker_color");
		auto font = m_parent->GetFontPref("Appearance.Cursors.label_font");
		auto fontSize = font->FontSize * ImGui::GetIO().FontGlobalScale;
		//Draw the markers
		for(auto& m : markers)
		{
			//Lines
			float xpos = round(XAxisUnitsToXPosition(m.m_offset));
			list->AddLine(ImVec2(xpos, pos.y), ImVec2(xpos, pos.y + size.y), color);

			//Text
			//Anchor bottom right at the cursor
			auto str = m.m_name + ": " + m_xAxisUnit.PrettyPrint(m.m_offset);
			auto tsize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0, str.c_str());
			float padding = 2;
			float wrounding = 2;
			float textTop = pos.y + m_timelineHeight - (padding + tsize.y);
			list->AddRectFilled(
				ImVec2(xpos - (2*padding + tsize.x), textTop - padding ),
				ImVec2(xpos - 1, pos.y + m_timelineHeight),
				ImGui::GetColorU32(ImGuiCol_PopupBg),
				wrounding);
			list->AddText(
				font,
				fontSize,
				ImVec2(xpos - (padding + tsize.x), textTop),
				color,
				str.c_str());
		}
	}
	ImGui::EndChild();

	auto mouse = ImGui::GetMousePos();
	if(!IsMouseOverButtonInWaveformArea())
	{
		for(auto& m : markers)
		{
			//Child window doesn't get mouse events (this flag is needed so we can pass mouse events to the WaveformArea's)
			//So we have to do all of our interaction processing inside the top level window
			//TODO: this is basically DoCursor(), can we de-duplicate this code?
			float xpos = round(XAxisUnitsToXPosition(m.m_offset));
			float searchRadius = 0.25 * ImGui::GetFontSize();

			//Check if the mouse hit us
			if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
			{
				if( fabs(mouse.x - xpos) < searchRadius)
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

					//Start dragging if clicked
					if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						if(m_dragState == DRAG_STATE_NONE)
						{
							LogTrace("starting to drag marker %s\n", m.m_name.c_str());
							m_dragState = DRAG_STATE_MARKER;
							m_dragMarker = &m;
						}
						else
							LogTrace("ignoring click on marker because m_dragState = %d\n", m_dragState);
					}
				}
			}
		}
	}

	//If dragging, move the cursor to track
	if(m_dragState == DRAG_STATE_MARKER)
	{
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			LogTrace("done dragging marker %s\n", m_dragMarker->m_name.c_str());
			m_dragState = DRAG_STATE_NONE;
		}

		auto newpos = XPositionToXAxisUnits(mouse.x);
		if(m_dragMarker->m_offset != newpos)
		{
			auto name = m_dragMarker->m_name;

			m_dragMarker->m_offset = newpos;
			m_parent->GetSession().OnMarkerChanged();

			//Find the marker again
			//This is needed because OnMarkerChanged() sorts the list of markers
			//which can potentially invalidate our pointer
			for(auto& m : markers)
			{
				if(m.m_name == name)
				{
					m_dragMarker = &m;
					break;
				}
			}
		}

		if(m_dragState == DRAG_STATE_NONE)
			m_dragMarker = nullptr;
	}
}

/**
	@brief Returns true if the mouse is over a channel button or similar UI element in a WaveformArea
 */
bool WaveformGroup::IsMouseOverButtonInWaveformArea()
{
	for(auto& p : m_areas)
	{
		if(p->IsMouseOverButtonAtEndOfRender())
			return true;
	}

	return false;
}

/**
	@brief Render our cursors
 */
void WaveformGroup::RenderXAxisCursors(ImVec2 pos, ImVec2 size)
{
	//No cursors? Nothing to do
	if(m_xAxisCursorMode == X_CURSOR_NONE)
	{
		//Exit cursor drag state if we no longer have a cursor to drag
		if( (m_dragState == DRAG_STATE_X_CURSOR0) || (m_dragState == DRAG_STATE_X_CURSOR1) )
			m_dragState = DRAG_STATE_NONE;
		return;
	}

	//Create a child window for all of our drawing
	//(this is needed so we're above the WaveformArea's in z order, but behind popup windows)
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	if(ImGui::BeginChild("cursors", size, false, ImGuiWindowFlags_NoInputs))
	{
		auto list = ImGui::GetWindowDrawList();

		auto& prefs = m_parent->GetSession().GetPreferences();
		auto cursor0_color = prefs.GetColor("Appearance.Cursors.cursor_1_color");
		auto cursor1_color = prefs.GetColor("Appearance.Cursors.cursor_2_color");
		auto fill_color = prefs.GetColor("Appearance.Cursors.cursor_fill_color");
		auto font = m_parent->GetFontPref("Appearance.Cursors.label_font");

		float xpos0 = round(XAxisUnitsToXPosition(m_xAxisCursorPositions[0]));
		float xpos1 = round(XAxisUnitsToXPosition(m_xAxisCursorPositions[1]));

		//Fill between if dual cursor
		if(m_xAxisCursorMode == X_CURSOR_DUAL)
			list->AddRectFilled(ImVec2(xpos0, pos.y), ImVec2(xpos1, pos.y + size.y), fill_color);

		//First cursor
		list->AddLine(ImVec2(xpos0, pos.y), ImVec2(xpos0, pos.y + size.y), cursor0_color, 1);

		//Text
		//Anchor bottom right at the cursor
		auto str = string("X1: ") + m_xAxisUnit.PrettyPrint(m_xAxisCursorPositions[0]);
		auto fontSize = font->FontSize * ImGui::GetIO().FontGlobalScale;
		auto tsize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0, str.c_str());
		float padding = 2;
		float wrounding = 2;
		float textTop = pos.y + m_timelineHeight - (padding + tsize.y);
		list->AddRectFilled(
			ImVec2(xpos0 - (2*padding + tsize.x), textTop - padding ),
			ImVec2(xpos0 - 1, pos.y + m_timelineHeight),
			ImGui::GetColorU32(ImGuiCol_PopupBg),
			wrounding);
		list->AddText(
			font,
			fontSize,
			ImVec2(xpos0 - (padding + tsize.x), textTop),
			cursor0_color,
			str.c_str());

		//Second cursor
		if(m_xAxisCursorMode == X_CURSOR_DUAL)
		{
			list->AddLine(ImVec2(xpos1, pos.y), ImVec2(xpos1, pos.y + size.y), cursor1_color, 1);

			int64_t delta = m_xAxisCursorPositions[1] - m_xAxisCursorPositions[0];
			str = string("X2: ") + m_xAxisUnit.PrettyPrint(m_xAxisCursorPositions[1]) + "\n" +
				"ΔX = " + m_xAxisUnit.PrettyPrint(delta);

			//If X axis is time domain, show frequency dual
			Unit hz(Unit::UNIT_HZ);
			if(m_xAxisUnit.GetType() == Unit::UNIT_FS)
				str += string(" (") + hz.PrettyPrint(FS_PER_SECOND / delta) + ")";

			//Text
			tsize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0, str.c_str());
			textTop = pos.y + m_timelineHeight - (padding + tsize.y);
			list->AddRectFilled(
				ImVec2(xpos1 + 1, textTop - padding ),
				ImVec2(xpos1 + (2*padding + tsize.x), pos.y + m_timelineHeight),
				ImGui::GetColorU32(ImGuiCol_PopupBg),
				wrounding);
			list->AddText(
				font,
				fontSize,
				ImVec2(xpos1 + padding, textTop),
				cursor1_color,
				str.c_str());
		}

		//not dragging if we no longer have a second cursor
		else if(m_dragState == DRAG_STATE_X_CURSOR1)
			m_dragState = DRAG_STATE_NONE;

		//TODO: text for value readouts, in-band power, etc
	}
	ImGui::EndChild();

	//Child window doesn't get mouse events (this flag is needed so we can pass mouse events to the WaveformArea's)
	//So we have to do all of our interaction processing inside the top level window
	DoCursor(0, DRAG_STATE_X_CURSOR0);
	if(m_xAxisCursorMode == X_CURSOR_DUAL)
		DoCursor(1, DRAG_STATE_X_CURSOR1);

	//If not currently dragging, a click places cursor 0 and starts dragging cursor 1 (if enabled)
	if( ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
		(m_dragState == DRAG_STATE_NONE) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!IsMouseOverButtonInWaveformArea())
	{
		auto xpos = ImGui::GetMousePos().x;

		//Don't check for clicks outside of the main plot area
		//(clicks on the Y axis should not be treated as cursor events)
		if(xpos < (pos.x + m_width - GetYAxisWidth()) )
		{
			m_xAxisCursorPositions[0] = XPositionToXAxisUnits(xpos);
			m_parent->OnCursorMoved(m_xAxisCursorPositions[0]);
			if(m_xAxisCursorMode == X_CURSOR_DUAL)
			{
				m_dragState = DRAG_STATE_X_CURSOR1;
				m_xAxisCursorPositions[1] = m_xAxisCursorPositions[0];
			}
			else
				m_dragState = DRAG_STATE_X_CURSOR0;
		}
	}

	//Cursor 0 should always be left of cursor 1 (if both are enabled).
	//If they get swapped, exchange them.
	if( (m_xAxisCursorPositions[0] > m_xAxisCursorPositions[1]) && (m_xAxisCursorMode == X_CURSOR_DUAL) )
	{
		//Swap the cursors themselves
		int64_t tmp = m_xAxisCursorPositions[0];
		m_xAxisCursorPositions[0] = m_xAxisCursorPositions[1];
		m_xAxisCursorPositions[1] = tmp;

		//If dragging one cursor, switch to dragging the other
		if(m_dragState == DRAG_STATE_X_CURSOR0)
			m_dragState = DRAG_STATE_X_CURSOR1;
		else if(m_dragState == DRAG_STATE_X_CURSOR1)
			m_dragState = DRAG_STATE_X_CURSOR0;
	}
}

void WaveformGroup::DoCursor(int iCursor, DragState state)
{
	float xpos = round(XAxisUnitsToXPosition(m_xAxisCursorPositions[iCursor]));
	float searchRadius = 0.25 * ImGui::GetFontSize();

	//Check if the mouse hit us
	auto mouse = ImGui::GetMousePos();
	if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !IsMouseOverButtonInWaveformArea())
	{
		if( fabs(mouse.x - xpos) < searchRadius)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

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
		m_xAxisCursorPositions[iCursor] = XPositionToXAxisUnits(mouse.x);

		if(iCursor == 0)
			m_parent->OnCursorMoved(m_xAxisCursorPositions[iCursor]);
	}
}

void WaveformGroup::RenderTimeline(float width, float height)
{
	ImGui::BeginChild("timeline", ImVec2(width, height));

	auto list = ImGui::GetWindowDrawList();

	//Style settings
	auto& prefs = m_parent->GetSession().GetPreferences();
	auto color = prefs.GetColor("Appearance.Timeline.axis_color");
	auto textcolor = prefs.GetColor("Appearance.Timeline.text_color");
	auto font = m_parent->GetFontPref("Appearance.Timeline.x_axis_font");
	float fontSize = font->FontSize * ImGui::GetIO().FontGlobalScale;

	//Reserve an empty area for the timeline
	auto pos = ImGui::GetWindowPos();
	m_xpos = pos.x;
	ImGui::Dummy(ImVec2(width, height));

	//Detect mouse movement
	double tnow = GetTime();
	auto mouseDelta = ImGui::GetIO().MouseDelta;
	if( (mouseDelta.x != 0) || (mouseDelta.y != 0) )
		m_tLastMouseMove = tnow;

	ImGui::SetItemUsingMouseWheel();
	if(ImGui::IsItemHovered())
	{
		//Catch mouse wheel events
		auto wheel = ImGui::GetIO().MouseWheel;
		if(wheel != 0)
			OnMouseWheel(wheel);

		//Double click to open the timebase properties
		if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			m_parent->ShowTimebaseProperties();

		//Start dragging
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if(!m_displayingEye)
				m_dragState = DRAG_STATE_TIMELINE;
		}

		//Autoscale on middle mouse
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
		{
			LogTrace("middle mouse autoscale\n");

			//Find beginning and end of all waveforms in the group
			int64_t start = INT64_MAX;
			int64_t end = -INT64_MAX;
			auto areas = GetWaveformAreas();
			for(auto a : areas)
			{
				for(size_t i=0; i<a->GetStreamCount(); i++)
				{
					auto stream = a->GetStream(i);
					auto data = stream.GetData();
					if(data == nullptr)
						continue;
					auto sdata = dynamic_cast<SparseWaveformBase*>(data);
					auto udata = dynamic_cast<UniformWaveformBase*>(data);
					auto ddata = dynamic_cast<DensityFunctionWaveform*>(data);

					//Regular waveform
					if(sdata || udata)
					{
						int64_t wstart = GetOffsetScaled(sdata, udata, 0);
						int64_t wend =
							GetOffsetScaled(sdata, udata, data->size()-1) +
							GetDurationScaled(sdata, udata, data->size()-1);

						start = min(start, wstart);
						end = max(end, wend);
					}

					//Density plot
					else if(ddata)
					{
						start = 0;
						end = ddata->GetWidth() * ddata->m_timescale;
					}
				}
			}
			int64_t sigwidth = end - start;

			//Don't divide by zero if no data!
			if(sigwidth > 1)
			{
				m_pixelsPerXUnit = width / sigwidth;
				m_xAxisOffset = start;
				ClearPersistence();
			}
		}
	}

	//Handle dragging
	//(Mouse is allowed to leave the window, as long as original click was within us)
	if(m_dragState == DRAG_STATE_TIMELINE)
	{
		//Use relative delta, not drag delta, since we update the offset every frame
		float dx = mouseDelta.x * ImGui::GetWindowDpiScale();
		if(dx != 0)
		{
			m_xAxisOffset -= PixelsToXAxisUnits(dx);
			ClearPersistence();
		}

		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			m_dragState = DRAG_STATE_NONE;
	}

	//Dimensions for various things
	float dpiScale = ImGui::GetWindowDpiScale();
	float fineTickLength = 10 * dpiScale;
	float coarseTickLength = height;
	const double min_label_grad_width = 75 * dpiScale;	//Minimum distance between text labels
	float thickLineWidth = 2;
	float thinLineWidth = 1;
	float ymid = pos.y + height/2;

	//Top line
	list->PathLineTo(pos);
	list->PathLineTo(ImVec2(pos.x + width, pos.y));
	list->PathStroke(color, 0, thickLineWidth);

	//Figure out rounding granularity, based on our time scales
	float xscale = m_pixelsPerXUnit;
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
	{
		ImGui::EndChild();
		return;
	}

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
		list->AddText(
			font,
			fontSize,
			ImVec2(x + textMargin, ymid),
			textcolor,
			m_xAxisUnit.PrettyPrint(t).c_str());
	}

	RenderTriggerPositionArrows(pos, height);

	//Help tooltip
	//Only show if mouse has been still for 250ms
	if( (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) &&
		(tnow - m_tLastMouseMove > 0.25) &&
		(m_dragState == DRAG_STATE_NONE)
		)
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);

		if(m_mouseOverTriggerArrow)
		{
			ImGui::TextUnformatted("Click and drag to move trigger position\n");
		}
		else
		{
			ImGui::TextUnformatted(
				"Click and drag to scroll the timeline.\n"
				"Use mouse wheel to zoom.\n"
				"Middle click to zoom to fit the entire waveform.\n"
				"Double-click to open timebase properties.");
		}
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	ImGui::EndChild();
}

/**
	@brief Draws an arrow for each scope's trigger position
 */
void WaveformGroup::RenderTriggerPositionArrows(ImVec2 pos, float height)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	float arrowsize = ImGui::GetFontSize() * 0.6;
	float caparrowsize = ImGui::GetFontSize() * 1;

	auto mouse = ImGui::GetMousePos();

	//Make a list of all scope triggers
	float ybot = pos.y + height;
	auto scopes = m_parent->GetSession().GetScopes();
	m_mouseOverTriggerArrow = false;
	for(auto scope : scopes)
	{
		auto trig = scope->GetTrigger();
		if(!trig)
			continue;
		auto din = trig->GetInput(0);
		if(!din)
			continue;

		//Get the timestamp of the trigger
		auto off = scope->GetTriggerOffset();

		//If we have a skew calibration offset for this scope, display the virtual trigger there instead
		int64_t skewCal = m_parent->GetSession().GetDeskew(scope);
		if(skewCal != 0)
			off = -skewCal;

		auto xpos = XAxisUnitsToXPosition(off);

		//Check if the mouse is within the expanded hitbox
		float exleft = xpos - caparrowsize/2;
		float exright = xpos + caparrowsize/2;
		float extop = ybot - caparrowsize;
		if( (mouse.x >= exleft) && (mouse.x <= exright) && (mouse.y >= extop) && (mouse.y <= ybot) )
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			m_mouseOverTriggerArrow = true;

			if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				LogTrace("Start dragging trigger position\n");
				m_scopeTriggerDuringDrag = scope;
				m_dragState = DRAG_STATE_TRIGGER;
			}
		}

		//If actively dragging the trigger, show the arrow at the current mouse position
		if(m_dragState == DRAG_STATE_TRIGGER)
			xpos = mouse.x;

		//Draw the arrow
		auto color = ColorFromString(din.m_channel->m_displaycolor);
		float aleft = xpos - arrowsize/2;
		float aright = xpos + arrowsize/2;
		draw_list->AddTriangleFilled(
			ImVec2(aleft, ybot - arrowsize),
			ImVec2(aright, ybot - arrowsize),
			ImVec2(xpos, ybot),
			color);
	}

	//Check for end of drag
	if(m_dragState == DRAG_STATE_TRIGGER)
	{
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			auto newTriggerPos = XPositionToXAxisUnits(mouse.x);
			Unit fs(Unit::UNIT_FS);

			//Primary of a multiscope group? Might have to realign secondaries since trigger can snap
			//rather than moving with sample-level resolution
			auto& sess = m_parent->GetSession();
			if(sess.IsPrimaryOfMultiScopeGroup(m_scopeTriggerDuringDrag))
			{
				int64_t oldoff = m_scopeTriggerDuringDrag->GetTriggerOffset();
				m_scopeTriggerDuringDrag->SetTriggerOffset(newTriggerPos);
				int64_t newoff = m_scopeTriggerDuringDrag->GetTriggerOffset();
				int64_t delta = newoff - oldoff;

				//Adjust skew calibration of each secondary in the group
				auto group = sess.GetTriggerGroupForScope(m_scopeTriggerDuringDrag);
				for(auto sec : group->m_secondaries)
					sess.SetDeskew(sec, sess.GetDeskew(sec) - delta);
			}

			//Secondary of a multiscope group? Shift trigger position, account for deltas
			else if(sess.IsSecondaryOfMultiScopeGroup(m_scopeTriggerDuringDrag))
			{
				//Figure out how much we moved: mouse position minus rendered trigger position
				int64_t renderedTriggerPos = -sess.GetDeskew(m_scopeTriggerDuringDrag);
				int64_t triggerShift = renderedTriggerPos - newTriggerPos;
				LogTrace("Trigger was dragged by %s\n", fs.PrettyPrint(triggerShift).c_str());

				//Attempt to move the trigger by that much
				int64_t oldoff = m_scopeTriggerDuringDrag->GetTriggerOffset();
				int64_t targetTriggerPos = m_scopeTriggerDuringDrag->GetTriggerOffset() + triggerShift;
				m_scopeTriggerDuringDrag->SetTriggerOffset(targetTriggerPos);

				//Figure out how much we actually moved
				int64_t newoff = m_scopeTriggerDuringDrag->GetTriggerOffset();
				int64_t delta = newoff - oldoff;
				LogTrace("Trigger actually moved by %s\n", fs.PrettyPrint(delta).c_str());

				//Update the deskew coefficient by the remainder
				sess.SetDeskew(m_scopeTriggerDuringDrag, sess.GetDeskew(m_scopeTriggerDuringDrag) + delta);
			}

			//Normal mode, just move the trigger
			else
				m_scopeTriggerDuringDrag->SetTriggerOffset(newTriggerPos);

			m_dragState = DRAG_STATE_NONE;
		}
	}
}

/**
	@brief Handles a mouse wheel scroll step
 */
void WaveformGroup::OnMouseWheel(float delta)
{
	auto areas = GetWaveformAreas();

	//Do not allow changing zoom on eye patterns
	if(!areas.empty())
	{
		auto firstStream = areas[0]->GetFirstAnalogOrDensityStream();
		if(firstStream && (firstStream.GetType() == Stream::STREAM_TYPE_EYE))
			return;
	}

	//TODO: if shift is held, scroll horizontally

	int64_t target = XPositionToXAxisUnits(ImGui::GetIO().MousePos.x);

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
	m_parent->SetNeedRender();
	m_clearPersistence = true;
}

/**
	@brief Clear saved persistence waveforms of any WaveformArea's within this group containing a stream of one channel

	Typically called when a channel is reconfigured.
 */
void WaveformGroup::ClearPersistenceOfChannel(OscilloscopeChannel* chan)
{
	auto areas = GetWaveformAreas();
	for(auto a : areas)
		a->ClearPersistenceOfChannel(chan);
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

/**
	@brief Scrolls the group so the specified timestamp is visible

	If the duration is nonzero:
	* If the entire requested region is visible, center the packet in the visible area of the plot
	* If the region is too large to see, move the start to the left 10% of the view.

	If a target stream is requested, we should only navigate if the provided stream is displayed somewhere
	within this group.
 */
void WaveformGroup::NavigateToTimestamp(int64_t timestamp, int64_t duration, StreamDescriptor target)
{
	//If X axis unit is not fs, don't scroll
	if(m_xAxisUnit != Unit(Unit::UNIT_FS))
		return;

	//Check if target is in one of our areas
	if(target)
	{
		auto areas = GetWaveformAreas();

		bool found = false;
		for(auto& a : areas)
		{
			if(a->IsStreamBeingDisplayed(target))
			{
				found = true;
				break;
			}
		}
		if(!found)
			return;
	}

	//TODO: support markers with other units? how to handle that?
	//TODO: early out if eye pattern

	if(duration > 0)
	{
		//If the packet is too long to fit on screen at the current zoom, have it start 10% of the way across
		int64_t viewWidth = PixelsToXAxisUnits(m_width);
		if(duration > viewWidth)
			m_xAxisOffset = timestamp - viewWidth*0.1;

		//Otherwise, the entire packet fits. Center it.
		else
			m_xAxisOffset = timestamp - viewWidth/2 + duration/2;
	}

	//Just center the packet
	else
		m_xAxisOffset = timestamp - 0.5*(m_width / m_pixelsPerXUnit);

	//If it's a packet, and we have a single vertical cursor, move it there
	if( (duration > 0) && (m_xAxisCursorMode == X_CURSOR_SINGLE) )
		m_xAxisCursorPositions[0] = timestamp;

	ClearPersistence();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization

bool WaveformGroup::LoadConfiguration(const YAML::Node& node)
{
	//Scale if needed
	bool timestamps_are_ps = true;
	if(node["timebaseResolution"])
	{
		if(node["timebaseResolution"].as<string>() == "fs")
			timestamps_are_ps = false;
	}

	m_pixelsPerXUnit = node["pixelsPerXUnit"].as<float>();
	m_xAxisOffset = node["xAxisOffset"].as<long long>();

	//Default to no cursors
	m_xAxisCursorMode = WaveformGroup::X_CURSOR_NONE;

	//Cursor config
	//Y axis cursor configs from legacy file format are ignored
	string cursor = node["cursorConfig"].as<string>();
	if(cursor == "none")
		m_xAxisCursorMode = WaveformGroup::X_CURSOR_NONE;
	else if(cursor == "x_single")
		m_xAxisCursorMode = WaveformGroup::X_CURSOR_SINGLE;
	else if(cursor == "x_dual")
		m_xAxisCursorMode = WaveformGroup::X_CURSOR_DUAL;
	/*
	else if(cursor == "y_single")
		m_cursorConfig = WaveformGroup::CURSOR_Y_SINGLE;
	else if(cursor == "y_dual")
		m_cursorConfig = WaveformGroup::CURSOR_Y_DUAL;
	*/
	m_xAxisCursorPositions[0] = node["xcursor0"].as<long long>();
	m_xAxisCursorPositions[1] = node["xcursor1"].as<long long>();
	/*
	m_yCursorPos[0] = node["ycursor0"].as<float>();
	m_yCursorPos[1] = node["ycursor1"].as<float>();
	*/

	auto inode = node["id"];
	if(inode)
		m_id = inode.as<string>();

	if(timestamps_are_ps)
	{
		m_pixelsPerXUnit /= 1000;
		m_xAxisOffset *= 1000;
		m_xAxisCursorPositions[0] *= 1000;
		m_xAxisCursorPositions[1] *= 1000;
	}

	return true;
}

YAML::Node WaveformGroup::SerializeConfiguration(IDTable& table)
{
	auto areas = GetWaveformAreas();

	YAML::Node node;
	node["timebaseResolution"] = "fs";
	node["pixelsPerXUnit"] = m_pixelsPerXUnit;
	node["xAxisOffset"] = m_xAxisOffset;
	node["name"] = m_title;
	node["id"] = m_id;

	switch(m_xAxisCursorMode)
	{
		case WaveformGroup::X_CURSOR_SINGLE:
			node["cursorConfig"] = "x_single";
			break;

		case WaveformGroup::X_CURSOR_DUAL:
			node["cursorConfig"] = "x_dual";
			break;

		case WaveformGroup::X_CURSOR_NONE:
		default:
			node["cursorConfig"] = "none";
	}

	node["xcursor0"] = m_xAxisCursorPositions[0];
	node["xcursor1"] = m_xAxisCursorPositions[1];

	for(size_t i=0; i<areas.size(); i++)
	{
		auto id = table[areas[i].get()];
		node["areas"][string("area") + to_string(id)]["id"] = id;
	}

	return node;
}
