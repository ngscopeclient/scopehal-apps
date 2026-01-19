/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of ManageInstrumentsDialog
 */

#include "ngscopeclient.h"
#include "../scopehal/MockOscilloscope.h"
#include "ManageInstrumentsDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ManageInstrumentsDialog::ManageInstrumentsDialog(Session& session, MainWindow* parent)
	: Dialog("Manage Instruments", "Manage Instruments", ImVec2(1024, 300))
	, m_session(session)
	, m_parent(parent)
	//, m_selection(nullptr)
{
}

ManageInstrumentsDialog::~ManageInstrumentsDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ManageInstrumentsDialog::DoRender()
{
	auto scopes = m_session.GetScopes();

	ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit |
		ImGuiTableFlags_NoKeepColumnsVisible;

	//TODO: should VNAs really be considered scopes?

	if(ImGui::CollapsingHeader("Trigger Groups", ImGuiTreeNodeFlags_DefaultOpen) && (scopes.size() != 0))
	{
		HelpMarker(
			"All instruments in a trigger group are synchronized and trigger in lock-step.\n"
			"The root instrument of a trigger group must have a trigger-out port.\n"
			"All instruments in a trigger group should be connected to a common reference clock to avoid skew.");

		if(ImGui::BeginTable("groups", 6, flags))
		{
			TriggerGroupsTable();
			ImGui::EndTable();
		}

		//Garbage collect trigger groups that have nothing in them
		m_session.GarbageCollectTriggerGroups();
	}

	if(ImGui::CollapsingHeader("All Instruments", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if(ImGui::BeginTable("alltable", 8, flags))
		{
			AllInstrumentsTable();
			ImGui::EndTable();
		}
	}

	return true;
}

void ManageInstrumentsDialog::TriggerGroupsTable()
{
	float width = ImGui::GetFontSize();
	ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
	ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_WidthFixed, 12*width);
	ImGui::TableSetupColumn("Make", ImGuiTableColumnFlags_WidthFixed, 9*width);
	ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 15*width);
	ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthFixed, 8*width);
	ImGui::TableSetupColumn("Skew", ImGuiTableColumnFlags_WidthFixed, 8*width);
	ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 8*width);
	ImGui::TableHeadersRow();

	Unit fs(Unit::UNIT_FS);

	auto groups = m_session.GetTriggerGroups();
	for(auto group : groups)
	{
		//If we get here, we just deleted the last scope in the group
		//but it won't be GC'd until the end of the frame
		if(group->empty())
			continue;

		//If we have no scopes but are not empty, we're a trend-only group
		//Show a dummy root node
		bool rootOpen = false;
		bool rootIsMock = false;
		shared_ptr<SCPIOscilloscope> firstScope = nullptr;
		shared_ptr<MockOscilloscope> mockScope = nullptr;

		if(!group->HasScopes())
		{
			ImGui::PushID(this);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);
			ImGui::TableSetColumnIndex(0);

			//Display the node for the root of the trigger group
			rootOpen = ImGui::TreeNodeEx(
				group->GetDescription().c_str(),
				ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen );
		}

		//Normal root node
		else
		{
			//If the first scope is a mock scope, show it differently
			firstScope = dynamic_pointer_cast<SCPIOscilloscope>(group->m_primary);
			mockScope = dynamic_pointer_cast<MockOscilloscope>(group->m_primary);

			if(mockScope)
			{
				ImGui::PushID(mockScope.get());
				ImGui::TableNextRow(ImGuiTableRowFlags_None);
				ImGui::TableSetColumnIndex(0);

				//Display the node for the root of the trigger group
				rootOpen = ImGui::TreeNodeEx(
					group->GetDescription().c_str(),
					ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen );

				rootIsMock = true;
			}
			else if(!firstScope)
				LogFatal("group has secondary but no primary, shouldn't be possible\n");
			else
			{
				ImGui::PushID(firstScope.get());
				ImGui::TableNextRow(ImGuiTableRowFlags_None);
				ImGui::TableSetColumnIndex(0);

				//Display the node for the root of the trigger group
				rootOpen = ImGui::TreeNodeEx(
					group->GetDescription().c_str(),
					ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen );
			}
		}

		//Help tooltip
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
			ImGui::TextUnformatted(
				"Drag to the root of a trigger group to add this instrument to the group.\n"
				"Drag to an ungrouped instrument to create a new group under it.\n"
				"Drag an instrument to the root of its current group to make it the primary.\n");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		if(!rootIsMock)
		{
			//Allow dropping
			if(ImGui::BeginDragDropTarget())
			{
				auto payload = ImGui::AcceptDragDropPayload("TriggerGroup", 0);
				if( (payload != nullptr) && (payload->DataSize == sizeof(TriggerGroupDragDescriptor)) )
				{
					auto desc = reinterpret_cast<TriggerGroupDragDescriptor*>(payload->Data);

					//Stop the trigger if rearranging trigger groups
					m_session.StopTrigger();

					//Dropping from a different group
					if(desc->m_group != group.get())
					{
						if(desc->m_scope)
						{
							group->AddSecondary(desc->m_scope);
							desc->m_group->RemoveScope(desc->m_scope);
						}
						else
						{
							group->AddFilter(desc->m_filter);
							desc->m_group->RemoveFilter(desc->m_filter);
						}
					}

					//Drop from a child of this group
					else
					{
						if(desc->m_scope)
							group->MakePrimary(desc->m_scope);
						//no hierarchy for filters so do nothing
					}
				}

				ImGui::EndDragDropTarget();
			}

			//Allow dragging
			if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				TriggerGroupDragDescriptor desc(group.get(), firstScope, nullptr);
				ImGui::SetDragDropPayload(
					"TriggerGroup",
					&desc,
					sizeof(desc));
				ImGui::TextUnformatted(firstScope->m_nickname.c_str());
				ImGui::EndDragDropSource();
			}
		}

		if(firstScope)
		{
			if(ImGui::TableSetColumnIndex(1))
				ImGui::TextUnformatted(firstScope->GetVendor().c_str());
			if(ImGui::TableSetColumnIndex(2))
				ImGui::TextUnformatted(firstScope->GetName().c_str());
			if(ImGui::TableSetColumnIndex(3))
				ImGui::TextUnformatted(firstScope->GetSerial().c_str());
		}

		else if(mockScope)
		{
			if(ImGui::TableSetColumnIndex(1))
				ImGui::TextUnformatted(mockScope->GetVendor().c_str());
			if(ImGui::TableSetColumnIndex(2))
				ImGui::TextUnformatted(mockScope->GetName().c_str());
			if(ImGui::TableSetColumnIndex(3))
				ImGui::TextUnformatted(mockScope->GetSerial().c_str());
		}

		//then put all other nodes under it
		if(rootOpen)
		{
			for(size_t i=0; i<group->m_secondaries.size(); i++)
			{
				auto scope = dynamic_pointer_cast<SCPIOscilloscope>(group->m_secondaries[i]);
				if(!scope)
					continue;

				ImGui::PushID(scope.get());
				ImGui::TableNextRow(ImGuiTableRowFlags_None);
				ImGui::TableSetColumnIndex(0);
				ImGui::TreeNodeEx(
					scope->m_nickname.c_str(),
					ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanFullWidth);

				//Allow dragging
				if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
				{
					TriggerGroupDragDescriptor desc(group.get(), scope, nullptr);
					ImGui::SetDragDropPayload(
						"TriggerGroup",
						&desc,
						sizeof(desc));
					ImGui::TextUnformatted(scope->m_nickname.c_str());
					ImGui::EndDragDropSource();
				}

				if(ImGui::TableSetColumnIndex(1))
					ImGui::TextUnformatted(scope->GetVendor().c_str());
				if(ImGui::TableSetColumnIndex(2))
					ImGui::TextUnformatted(scope->GetName().c_str());
				if(ImGui::TableSetColumnIndex(3))
					ImGui::TextUnformatted(scope->GetSerial().c_str());
				if(ImGui::TableSetColumnIndex(4))
					ImGui::TextUnformatted(fs.PrettyPrint(m_session.GetDeskew(scope)).c_str());
				if(ImGui::TableSetColumnIndex(5))
				{
					if(ImGui::Button("Deskew"))
						m_parent->ShowSyncWizard(group, scope);
				}
				ImGui::PopID();
			}

			for(size_t i=0; i<group->m_filters.size(); i++)
			{
				auto f = group->m_filters[i];

				ImGui::PushID(f);
				ImGui::TableNextRow(ImGuiTableRowFlags_None);
				ImGui::TableSetColumnIndex(0);
				ImGui::TreeNodeEx(
					f->GetDisplayName().c_str(),
					ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanFullWidth);

				//Allow dragging
				if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
				{
					TriggerGroupDragDescriptor desc(group.get(), nullptr, f);
					ImGui::SetDragDropPayload(
						"TriggerGroup",
						&desc,
						sizeof(desc));
					ImGui::TextUnformatted(f->GetDisplayName().c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopID();
			}
		}

		if(rootOpen)
			ImGui::TreePop();
		ImGui::PopID();
	}

	//Create an extra dummy row to drop children into to make a new group
	RowForNewGroup();
}

void ManageInstrumentsDialog::RowForNewGroup()
{
	ImGui::PushID("NewGroup");
	ImGui::TableNextRow(ImGuiTableRowFlags_None);
	ImGui::TableSetColumnIndex(0);

	ImGui::Selectable("New Group", false, ImGuiSelectableFlags_Disabled);

	//Allow dropping
	if(ImGui::BeginDragDropTarget())
	{
		auto payload = ImGui::AcceptDragDropPayload("TriggerGroup", 0);
		if( (payload != nullptr) && (payload->DataSize == sizeof(TriggerGroupDragDescriptor)) )
		{
			auto desc = reinterpret_cast<TriggerGroupDragDescriptor*>(payload->Data);

			//Are we dragging a scope?
			if(desc->m_scope)
			{
				//Make it primary of the new group
				m_session.MakeNewTriggerGroup(desc->m_scope);

				//Remove from the existing group
				desc->m_group->RemoveScope(desc->m_scope);
			}

			//Or is it a filter?
			else if(desc->m_filter)
			{
				m_session.MakeNewTriggerGroup(desc->m_filter);
				desc->m_group->RemoveFilter(desc->m_filter);
			}
		}

		ImGui::EndDragDropTarget();
	}

	ImGui::PopID();
}

void ManageInstrumentsDialog::AllInstrumentsTable()
{
	auto& prefs = m_session.GetPreferences();
	auto insts = m_session.GetInstruments();
	float width = ImGui::GetFontSize();
	ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
	ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_WidthFixed, 12*width);
	ImGui::TableSetupColumn("Make", ImGuiTableColumnFlags_WidthFixed, 9*width);
	ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 12*width);
	ImGui::TableSetupColumn("Transport", ImGuiTableColumnFlags_WidthFixed, 4*width);
	ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 15*width);
	ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 5*width);
	ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthFixed, 7*width);
	ImGui::TableSetupColumn("Features", ImGuiTableColumnFlags_WidthFixed, 10*width);
	ImGui::TableHeadersRow();

	size_t instNumber = insts.size();
	size_t instIndex = 0;
	m_instrumentCurrentNames.resize(instNumber);
	m_instrumentCommittedNames.resize(instNumber);
	m_instrumentCurrentPaths.resize(instNumber);
	m_instrumentCommittedPaths.resize(instNumber);

	for(auto inst : insts)
	{
		auto itype = inst->GetInstrumentTypes();
		//bool rowIsSelected = (m_selection == inst);
		ImGui::PushID(inst.get());
		ImGui::TableNextRow(ImGuiTableRowFlags_None);
		if(ImGui::TableSetColumnIndex(0))
		{
			if(m_instrumentCommittedNames[instIndex].empty()) m_instrumentCommittedNames[instIndex]=inst->m_nickname;
			if(m_instrumentCurrentNames[instIndex].empty()) m_instrumentCurrentNames[instIndex]=inst->m_nickname;
			ImGui::SetNextItemWidth(12*width);
			if(TextInputWithExplicitApply("",m_instrumentCurrentNames[instIndex],m_instrumentCommittedNames[instIndex]))
			{
				string oldName = inst->m_nickname;
				inst->m_nickname = m_instrumentCommittedNames[instIndex];
				auto si = dynamic_pointer_cast<SCPIInstrument>(inst);
				if(si)
					m_parent->RenameRecentInstrument(si,oldName);
			}
		}
		if(ImGui::TableSetColumnIndex(1))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(inst->GetVendor().c_str());
		}
		/*ImGui::TableSetColumnIndex(1);
		ImGui::AlignTextToFramePadding();
		if(ImGui::Selectable(
				inst->GetVendor().c_str(),
				rowIsSelected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
				ImVec2(0, 0)))
		{
			m_selection = dynamic_pointer_cast<SCPIInstrument>(inst);
			rowIsSelected = true;
		}*/
		if(ImGui::TableSetColumnIndex(2))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(inst->GetName().c_str());
		}
		if(ImGui::TableSetColumnIndex(3))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(inst->GetTransportName().c_str());
		}
		if(ImGui::TableSetColumnIndex(4))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(inst->GetTransportConnectionString().c_str());
		}
		if(ImGui::TableSetColumnIndex(5))
		{
			ImGui::AlignTextToFramePadding();
			renderBadge(0,
						inst->IsOffline() ? ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_offline_badge_color")) : ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color")),
						inst->IsOffline() ? "Offline" : "Online");
		}
		if(ImGui::TableSetColumnIndex(6))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(inst->GetSerial().c_str());
		}
		if(ImGui::TableSetColumnIndex(7))
		{
			string types = "";

			if(dynamic_pointer_cast<MockOscilloscope>(inst) != nullptr)
				types += "offline ";

			if(itype & Instrument::INST_OSCILLOSCOPE)
				types += "oscilloscope ";
			if(itype & Instrument::INST_DMM)
				types += "multimeter ";
			if(itype & Instrument::INST_PSU)
				types += "powersupply ";
			if(itype & Instrument::INST_FUNCTION)
				types += "funcgen ";
			if(itype & Instrument::INST_RF_GEN)
				types += "rfgen ";
			if(itype & Instrument::INST_LOAD)
				types += "load ";
			if(itype & Instrument::INST_BERT)
				types += "bert ";

			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(types.c_str());
		}
		ImGui::PopID();
		instIndex++;
	}
}
