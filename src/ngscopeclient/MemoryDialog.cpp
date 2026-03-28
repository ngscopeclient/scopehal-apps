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
	@brief Implementation of MemoryDialog
 */

#include "ngscopeclient.h"
#include "MemoryDialog.h"
#include "Session.h"
#include "MainWindow.h"
#include "imgui_internal.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comparison operator for sorting objects by size

class AcceleratorBufferSizeGreater
{
public:
	bool operator()(const AcceleratorBufferBase* const& lhs, const AcceleratorBufferBase* const& rhs)
	{
		size_t ourSize = lhs->capacity() * lhs->GetElementSize();
		size_t theirSize = rhs->capacity() * rhs->GetElementSize();

		return ourSize > theirSize;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MemoryDialog::MemoryDialog(Session* session, MainWindow* parent)
	: Dialog("Memory Analysis", "Memory", ImVec2(800, 600), session, parent)
	, m_selection(nullptr)
{

}

MemoryDialog::~MemoryDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool MemoryDialog::DoRender()
{
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit;

	float width = ImGui::GetFontSize();

	//auto& prefs = m_session->GetPreferences();

	//TODO: summary of how many objects and how big

	if(ImGui::BeginTable("table", 10, flags))
	{
		Unit bytes(Unit::UNIT_BYTES);
		Unit pct(Unit::UNIT_PERCENT);

		auto highOverheadColor = ColorFromString("#800000");
		auto someOverheadColor = ColorFromString("#808000");
		auto scratchBufferColor = ColorFromString("#404040");

		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 8*width);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 7*width);
		ImGui::TableSetupColumn("Size (elements)", ImGuiTableColumnFlags_WidthFixed, 7*width);
		ImGui::TableSetupColumn("Size (bytes)", ImGuiTableColumnFlags_WidthFixed, 7*width);
		ImGui::TableSetupColumn("Capacity (elements)", ImGuiTableColumnFlags_WidthFixed, 7*width);
		ImGui::TableSetupColumn("Capacity (bytes)", ImGuiTableColumnFlags_WidthFixed, 7*width);
		ImGui::TableSetupColumn("Overhead (elements)", ImGuiTableColumnFlags_WidthFixed, 12*width);
		ImGui::TableSetupColumn("Overhead (bytes)", ImGuiTableColumnFlags_WidthFixed, 12*width);
		ImGui::TableSetupColumn("Overhead (%)", ImGuiTableColumnFlags_WidthFixed, 5*width);

		ImGui::TableHeadersRow();

		//TODO: separate general monospace font?
		auto font = m_parent->GetFontPref("Appearance.General.console_font");
		ImGui::PushFont(font.first, font.second);

		//This section needs to lock the list of AcceleratorBuffer's
		//so we don't have stuff changing or invalidated under our nose
		{
			lock_guard<recursive_mutex> lock(AcceleratorBufferBase::GetMutex());
			auto& set = AcceleratorBufferBase::GetObjects();

			//Make a list sorted by the total actual memory of the object
			vector<AcceleratorBufferBase*> sortedList;
			sortedList.reserve(set.size());
			for(auto p : set)
				sortedList.push_back(p);

			//Sort the list
			sort(sortedList.begin(), sortedList.end(), AcceleratorBufferSizeGreater());

			//And actually display them
			for(auto p : sortedList)
			{
				auto size = p->size();
				auto sizeBytes = size * p->GetElementSize();

				auto cap = p->capacity();
				auto capBytes = cap * p->GetElementSize();

				auto overhead = cap - size;
				auto overheadBytes = overhead * p->GetElementSize();
				auto overheadPct = (size == 0) ? 0 : (overhead * 1.0 / size);

				//Don't show empty buffers for now
				if(cap == 0)
					continue;

				ImGui::PushID(p);
				ImGui::TableNextRow(ImGuiTableRowFlags_None);

				//Scratch buffers get their own color, independent of overhead
				//(overhead is not a useful metric here since a buffer can pass through many hands)
				auto name = p->GetName();
				if(name.find("ScratchBufferManager") == 0)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, scratchBufferColor);

				//Color rows with high overhead
				else if(overheadPct > 1)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, highOverheadColor);
				else if(overheadPct > 0.1)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, someOverheadColor);

				//Object address
				char selid[128];
				snprintf(selid, sizeof(selid), "%p", reinterpret_cast<void*>(p));
				ImGui::TableSetColumnIndex(0);
				bool rowIsSelected = (m_selection == p);
				if(ImGui::Selectable(
					selid,
					rowIsSelected,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
					ImVec2(0, 0)))
				{
					m_selection = p;
				}

				//Name
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(name.c_str());

				//Type
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(p->GetType().c_str());

				//Size
				ImGui::TableSetColumnIndex(3);
				ImGui::TextAligned(1.0, -FLT_MIN, "%zu", size);

				ImGui::TableSetColumnIndex(4);
				ImGui::TextAligned(1.0, -FLT_MIN, "%s", bytes.PrettyPrintTabular(sizeBytes, 5, 3).c_str());

				//Capacity
				ImGui::TableSetColumnIndex(5);
				ImGui::TextAligned(1.0, -FLT_MIN, "%zu", cap);

				ImGui::TableSetColumnIndex(6);
				ImGui::TextAligned(1.0, -FLT_MIN, "%s", bytes.PrettyPrintTabular(capBytes, 5, 3).c_str());

				//Overhead
				ImGui::TableSetColumnIndex(7);
				ImGui::TextAligned(1.0, -FLT_MIN, "%zu", overhead);

				ImGui::TableSetColumnIndex(8);
				ImGui::TextAligned(1.0, -FLT_MIN, "%s", bytes.PrettyPrintTabular(overheadBytes, 5, 3).c_str());

				ImGui::TableSetColumnIndex(9);
				ImGui::TextAligned(1.0, -FLT_MIN, "%s", pct.PrettyPrintTabular(overheadPct, 4, 1).c_str());

				ImGui::PopID();
			}
		}

		ImGui::PopFont();

		ImGui::EndTable();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
