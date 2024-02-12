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
	@brief Implementation of HistoryDialog
 */

#include "ngscopeclient.h"
#include "HistoryDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TimePoint

string TimePoint::PrettyPrint() const
{
	auto base = GetSec();
	auto offset = GetFs();

	//If offset is >1 sec, shift the timestamp
	if(offset > FS_PER_SECOND)
	{
		base += (offset / FS_PER_SECOND);
		offset = offset % (int64_t)FS_PER_SECOND;
	}

	//Format timestamp
	char tmp[128];
	struct tm ltime;

#ifdef _WIN32
	localtime_s(&ltime, &base);
#else
	localtime_r(&base, &ltime);
#endif

	//round to nearest 100ps for display
	//TODO: do we want to include date as an optional column or something??
	strftime(tmp, sizeof(tmp), "%H:%M:%S.", &ltime);
	string stime = tmp;
	snprintf(tmp, sizeof(tmp), "%010zu", static_cast<size_t>(offset / 100000));
	stime += tmp;
	return stime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HistoryDialog::HistoryDialog(HistoryManager& mgr, Session& session, MainWindow& wnd)
	: Dialog("History", "History", ImVec2(425, 350))
	, m_mgr(mgr)
	, m_session(session)
	, m_parent(wnd)
	, m_rowHeight(0)
	, m_selectionChanged(false)
	, m_selectedMarker(nullptr)
{
}

HistoryDialog::~HistoryDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool HistoryDialog::DoRender()
{
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY;

	float width = ImGui::GetFontSize();

	ImGui::InputInt("History Depth", &m_mgr.m_maxDepth, 1, 10);
	HelpMarker(
		"Adjust the cap on total history depth, in waveforms.\n"
		"Large history depths can use significant amounts of RAM with deep memory.");

	if(ImGui::BeginTable("history", 3, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 12*width);
		ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		ImGui::TableSetupColumn("Label");
		ImGui::TableHeadersRow();

		list<shared_ptr<HistoryPoint> >::iterator itDelete;
		bool deleting = false;
		for(auto it = m_mgr.m_history.begin(); it != m_mgr.m_history.end(); it++)
		{
			auto point = *it;
			ImGui::PushID(point.get());

			ImGui::TableNextRow(ImGuiTableRowFlags_None, m_rowHeight);

			//Timestamp (and row selection logic)
			bool rowIsSelected = (m_selectedPoint == point);
			ImGui::TableSetColumnIndex(0);
			auto open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::SameLine();
			if(ImGui::Selectable(
				point->m_time.PrettyPrint().c_str(),
				rowIsSelected && !m_selectedMarker,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
				ImVec2(0, m_rowHeight)))
			{
				m_selectedPoint = point;
				rowIsSelected = true;
				m_selectionChanged = true;
				m_selectedMarker = nullptr;
			}

			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::MenuItem("Delete"))
				{
					itDelete = it;
					deleting = true;
				}
				ImGui::EndPopup();
			}

			//Force pin if we have a nickname or markers
			auto& markers = m_session.GetMarkers(point->m_time);
			bool forcePin = false;
			if(!point->m_nickname.empty() || !markers.empty())
			{
				forcePin = true;
				point->m_pinned = true;
			}

			//Pin box
			ImGui::TableSetColumnIndex(1);
			if(forcePin)
				ImGui::BeginDisabled();
			ImGui::Checkbox("###pin", &point->m_pinned);
			m_rowHeight = ImGui::GetItemRectSize().y;
			if(forcePin)
				ImGui::EndDisabled();
			Dialog::Tooltip(
				"Check to \"pin\" this waveform and keep it in history rather\n"
				"than rolling off the end of the buffer as new data comes in.\n\n"
				"Waveforms with a nickname, or containing any labeled timestamps,\n"
				"are automatically pinned.", true);

			//Editable nickname box
			ImGui::TableSetColumnIndex(2);
			if(rowIsSelected)
			{
				if(m_selectionChanged)
					ImGui::SetKeyboardFocusHere();
				ImGui::SetNextItemWidth(ImGui::GetColumnWidth() - 4);
				ImGui::InputText("###nick", &point->m_nickname);
			}
			else
				ImGui::TextUnformatted(point->m_nickname.c_str());

			//Child nodes for markers
			if(open)
			{
				size_t markerToDelete = 0;
				bool deletingMarker = false;

				for(size_t i=0; i<markers.size(); i++)
				{
					auto& m = markers[i];

					ImGui::PushID(i);
					ImGui::TableNextRow();

					//Timestamp
					bool markerIsSelected = (m_selectedMarker == &m);
					ImGui::TableSetColumnIndex(0);
					if(ImGui::Selectable(
						m.GetMarkerTime().PrettyPrint().c_str(),
						markerIsSelected,
						ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
						ImVec2(0, m_rowHeight)))
					{
						//Select the marker
						m_selectedMarker = &m;
						markerIsSelected = true;

						//Navigate to the selected waveform
						if(!rowIsSelected)
						{
							rowIsSelected = true;
							m_selectedPoint = point;
							m_selectionChanged = true;
						}

						m_parent.NavigateToTimestamp(m.m_offset);
					}

					if(ImGui::BeginPopupContextItem())
					{
						if(ImGui::MenuItem("Delete"))
						{
							deletingMarker = true;
							markerToDelete = i;
						}
						ImGui::EndPopup();
					}

					//Nothing in pin box
					ImGui::TableSetColumnIndex(1);

					//Nickname box
					ImGui::TableSetColumnIndex(2);
					if(ImGui::InputText("###nick", &m.m_name))
						m_parent.GetSession().OnMarkerChanged();

					ImGui::PopID();
				}

				//Execute deletion after drawing the rest of the list
				if(deletingMarker)
				{
					markers.erase(markers.begin() + markerToDelete);
					m_parent.GetSession().OnMarkerChanged();
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		//Deleting a row?
		if(deleting)
		{
			//Deleting selected row? Select the last row (if we have one)
			bool deletedSelection = false;
			if(*itDelete == m_selectedPoint)
				deletedSelection = true;

			//Delete the selected row
			//(manual delete applies even if we have markers or a pin)
			m_session.RemoveMarkers((*itDelete)->m_time);
			m_session.RemovePackets((*itDelete)->m_time);
			m_mgr.m_history.erase(itDelete);

			if(deletedSelection)
			{
				m_selectionChanged = true;
				if(m_mgr.m_history.empty())
					m_selectedPoint = nullptr;
				else
					m_selectedPoint = *m_mgr.m_history.rbegin();
			}
		}

		ImGui::EndTable();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

/**
	@brief Applies waveforms from the currently selected history row to the scopes

	This is done at the very end of the frame following the actual selection change, to avoid inconsistent UI state
	from making the change mid-frame.
 */
void HistoryDialog::LoadHistoryFromSelection(Session& session)
{
	if(m_selectedPoint)
	{
		LogTrace("Valid point selected\n");
		m_selectedPoint->LoadHistoryToSession(session);
	}
	else
	{
		LogTrace("Empty point selected\n");
		m_mgr.LoadEmptyHistoryToSession(session);
	}
}

/**
	@brief Selects the last row in the history
 */
void HistoryDialog::UpdateSelectionToLatest()
{
	LogTrace("Selecting most recent waveform\n");
	m_selectedPoint = *m_mgr.m_history.rbegin();
}

/**
	@brief Selects the row with a specified timestamp
 */
void HistoryDialog::SelectTimestamp(TimePoint t)
{
	LogTrace("Selecting timestamp %s\n", t.PrettyPrint().c_str());
	m_selectedPoint = m_mgr.GetHistory(t);
}

/**
	@brief Gets the timestamp of our selection
 */
TimePoint HistoryDialog::GetSelectedPoint()
{
	if(m_selectedPoint)
		return m_selectedPoint->m_time;
	else
		return TimePoint(0, 0);
}
