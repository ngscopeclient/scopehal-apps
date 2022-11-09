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
	@brief Implementation of ProtocolAnalyzerDialog
 */

#include "ngscopeclient.h"
#include "ProtocolAnalyzerDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolAnalyzerDialog::ProtocolAnalyzerDialog(
	PacketDecoder* filter, shared_ptr<PacketManager> mgr, Session& session, MainWindow& wnd)
	: Dialog(string("Protocol: ") + filter->GetDisplayName(), ImVec2(425, 350))
	, m_filter(filter)
	, m_mgr(mgr)
	, m_session(session)
	, m_parent(wnd)
	, m_rowHeight(0)
	, m_selectionChanged(false)
{
	//Hold a reference open to the filter so it doesn't disappear on us
	m_filter->AddRef();
}

ProtocolAnalyzerDialog::~ProtocolAnalyzerDialog()
{
	m_filter->Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ProtocolAnalyzerDialog::DoRender()
{
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY;

	float width = ImGui::GetFontSize();

	//Figure out channel setup
	//Default is timestamp plus all headers, add optional other channels as needed
	auto cols = m_filter->GetHeaders();
	int ncols = 1 + cols.size();
	if(m_filter->GetShowDataColumn())
		ncols ++;
	if(m_filter->GetShowImageColumn())
		ncols ++;
	//TODO: integrate length natively vs having to make the filter calculate it??

	if(ImGui::BeginTable("table", ncols, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 12*width);
		for(auto c : cols)
			ImGui::TableSetupColumn(c.c_str(), ImGuiTableColumnFlags_WidthFixed, 0.0f);
		if(m_filter->GetShowDataColumn())
			ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		if(m_filter->GetShowImageColumn())
			ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		ImGui::TableHeadersRow();

		//TODO: actual packet stuff

		/*
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
					ImGui::InputText("###nick", &m.m_name);

					ImGui::PopID();
				}

				//Execute deletion after drawing the rest of the list
				if(deletingMarker)
					markers.erase(markers.begin() + markerToDelete);

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
		*/

		ImGui::EndTable();
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
