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
	@brief Implementation of HistoryDialog
 */

#include "ngscopeclient.h"
#include "HistoryDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HistoryDialog::HistoryDialog(HistoryManager& mgr)
	: Dialog("History", ImVec2(400, 350))
	, m_mgr(mgr)
	, m_rowHeight(0)
	, m_selectionChanged(false)
{
}

HistoryDialog::~HistoryDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

string HistoryDialog::FormatTimestamp(time_t base, int64_t offset)
{
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
	strftime(tmp, sizeof(tmp), "%X.", &ltime);
	string stime = tmp;
	snprintf(tmp, sizeof(tmp), "%010zu", static_cast<size_t>(offset / 100000));
	stime += tmp;
	return stime;
}

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
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 10*width);
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
			if(ImGui::Selectable(
				FormatTimestamp(point->m_timestamp, point->m_fs).c_str(),
				rowIsSelected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
				ImVec2(0, m_rowHeight)))
			{
				m_selectedPoint = point;
				rowIsSelected = true;
				m_selectionChanged = true;
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

			//Force pin if we have a nickname
			//TODO: force pin if we have markers
			bool forcePin = false;
			if(!point->m_nickname.empty())
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
				ImGui::InputText("###nick", &point->m_nickname);
			}
			else
				ImGui::TextUnformatted(point->m_nickname.c_str());

			ImGui::PopID();
		}

		//Deleting a row?
		if(deleting)
		{
			//Deleting selected row? Select the last row (if we have one)
			bool deletedSelection = false;
			if( (*itDelete == m_selectedPoint) && (m_mgr.m_history.size() > 1) )
				deletedSelection = true;

			//Delete the selected row
			m_mgr.m_history.erase(itDelete);

			if(deletedSelection)
			{
				m_selectionChanged = true;
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

	This is done at the very of the frame following the actual selection change, to avoid inconsistent UI state
	from making the change mid-frame.
 */
void HistoryDialog::LoadHistoryFromSelection(Session& session)
{
	//We don't want to keep capturing if we're trying to look at a historical waveform. That would be a bit silly.
	session.StopTrigger();

	//Go over each scope in the session and load the relevant history
	//We do this rather than just looping over the scopes in the history so that we can handle missing data.
	auto scopes = session.GetScopes();
	for(auto scope : scopes)
	{
		//Scope is not in history! Must have been added recently
		//Set all channels' data to null
		if(m_selectedPoint->m_history.find(scope) == m_selectedPoint->m_history.end() )
		{
			for(size_t i=0; i<scope->GetChannelCount(); i++)
			{
				auto chan = scope->GetChannel(i);
				for(size_t j=0; j<chan->GetStreamCount(); j++)
				{
					chan->Detach(j);
					chan->SetData(nullptr, j);
				}
			}
		}

		//Scope is in history. Load our saved waveform data
		else
		{
			LogTrace("Loading saved history\n");
			auto hist = m_selectedPoint->m_history[scope];
			for(auto it : hist)
			{
				auto stream = it.first;
				stream.m_channel->Detach(stream.m_stream);
				stream.m_channel->SetData(it.second, stream.m_stream);
			}
		}
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
