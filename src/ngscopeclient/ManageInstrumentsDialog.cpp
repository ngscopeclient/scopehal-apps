/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
#include "ManageInstrumentsDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ManageInstrumentsDialog::ManageInstrumentsDialog(Session& session)
	: Dialog("Manage Instruments", "Manage Instruments", ImVec2(1000, 300))
	, m_session(session)
	, m_selection(nullptr)
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
		ImGui::TextUnformatted(
			"All instruments in a trigger group are synchronized and trigger in lock-step.\n"
			"The root instrument of a trigger group must have a trigger-out port.\n"
			"All instruments in a trigger group should be connected to a common reference clock to avoid skew");

		if(ImGui::BeginTable("groups", 7, flags))
		{
			TriggerGroupsTable();
			ImGui::EndTable();
		}

		//Garbage collect trigger groups that have nothing in them
		m_session.GarbageCollectTriggerGroups();
	}

	if(ImGui::CollapsingHeader("All Instruments", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if(ImGui::BeginTable("alltable", 7, flags))
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
	ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_WidthFixed, 6*width);
	ImGui::TableSetupColumn("Make", ImGuiTableColumnFlags_WidthFixed, 9*width);
	ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 15*width);
	ImGui::TableSetupColumn("Transport", ImGuiTableColumnFlags_WidthFixed, 4*width);
	ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 25*width);
	ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthFixed, 8*width);
	ImGui::TableHeadersRow();

	auto& groups = m_session.GetTriggerGroups();
	for(auto& group : groups)
	{
		//If we get here, we just deleted the last scope in the group
		//but it won't be GC'd until the end of the frame
		if(group->empty())
			continue;

		auto firstScope = dynamic_cast<SCPIOscilloscope*>(group->m_primary);
		if(!firstScope)
			LogFatal("don't know what to do with non-SCPI oscilloscopes\n");

		ImGui::PushID(firstScope);
		ImGui::TableNextRow(ImGuiTableRowFlags_None);
		ImGui::TableSetColumnIndex(0);

		//Display the node for the root of the trigger group
		bool rootOpen = ImGui::TreeNodeEx(
			firstScope->m_nickname.c_str(),
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen );

		//Help tooltip
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
			ImGui::TextUnformatted(
				"Drag to the root of a trigger group to add this instrument to the group.\n"
				"Drag to an ungrouped instrument to create a new group under it.\n"
				"Drag an instrment to the root of its current group to make it the primary.\n");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		//Allow dropping
		if(ImGui::BeginDragDropTarget())
		{
			auto payload = ImGui::AcceptDragDropPayload("TriggerGroup", 0);
			if( (payload != nullptr) && (payload->DataSize == sizeof(TriggerGroupDragDescriptor)) )
			{
				auto desc = reinterpret_cast<TriggerGroupDragDescriptor*>(payload->Data);

				//Dropping from a different group
				if(desc->m_group != group.get())
				{
					//Add it as a secondary of us
					group->m_secondaries.push_back(desc->m_scope);

					//Remove from the existing group
					desc->m_group->RemoveScope(desc->m_scope);
				}

				//Drop from a child of this group
				else
					group->MakePrimary(desc->m_scope);
			}

			ImGui::EndDragDropTarget();
		}

		//Allow dragging
		if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			TriggerGroupDragDescriptor desc(group.get(), firstScope);
			ImGui::SetDragDropPayload(
				"TriggerGroup",
				&desc,
				sizeof(desc));
			ImGui::TextUnformatted(firstScope->m_nickname.c_str());
			ImGui::EndDragDropSource();
		}

		if(ImGui::TableSetColumnIndex(1))
			ImGui::TextUnformatted(firstScope->GetVendor().c_str());
		if(ImGui::TableSetColumnIndex(2))
			ImGui::TextUnformatted(firstScope->GetName().c_str());
		if(ImGui::TableSetColumnIndex(3))
			ImGui::TextUnformatted(firstScope->GetTransportName().c_str());
		if(ImGui::TableSetColumnIndex(4))
			ImGui::TextUnformatted(firstScope->GetTransportConnectionString().c_str());
		if(ImGui::TableSetColumnIndex(5))
			ImGui::TextUnformatted(firstScope->GetSerial().c_str());

		//then put all other scopes under it
		if(rootOpen)
		{
			for(size_t i=0; i<group->m_secondaries.size(); i++)
			{
				auto scope = dynamic_cast<SCPIOscilloscope*>(group->m_secondaries[i]);
				if(!scope)
					continue;

				ImGui::PushID(scope);
				ImGui::TableNextRow(ImGuiTableRowFlags_None);
				ImGui::TableSetColumnIndex(0);
				ImGui::TreeNodeEx(
					scope->m_nickname.c_str(),
					ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanFullWidth);

				//Allow dragging
				if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
				{
					TriggerGroupDragDescriptor desc(group.get(), scope);
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
					ImGui::TextUnformatted(scope->GetTransportName().c_str());
				if(ImGui::TableSetColumnIndex(4))
					ImGui::TextUnformatted(scope->GetTransportConnectionString().c_str());
				if(ImGui::TableSetColumnIndex(5))
					ImGui::TextUnformatted(scope->GetSerial().c_str());
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

			//Make it primary of the new group
			m_session.MakeNewTriggerGroup(desc->m_scope);

			//Remove from the existing group
			desc->m_group->RemoveScope(desc->m_scope);
		}

		ImGui::EndDragDropTarget();
	}

	ImGui::PopID();
}

void ManageInstrumentsDialog::AllInstrumentsTable()
{
	auto insts = m_session.GetSCPIInstruments();
	float width = ImGui::GetFontSize();
	ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
	ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_WidthFixed, 6*width);
	ImGui::TableSetupColumn("Make", ImGuiTableColumnFlags_WidthFixed, 9*width);
	ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 15*width);
	ImGui::TableSetupColumn("Transport", ImGuiTableColumnFlags_WidthFixed, 4*width);
	ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 25*width);
	ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthFixed, 8*width);
	ImGui::TableSetupColumn("Features", ImGuiTableColumnFlags_WidthFixed, 10*width);
	ImGui::TableHeadersRow();

	for(auto inst : insts)
	{
		auto itype = inst->GetInstrumentTypes();
		bool rowIsSelected = (m_selection == inst);
		ImGui::PushID(inst);
		ImGui::TableNextRow(ImGuiTableRowFlags_None);
		ImGui::TableSetColumnIndex(0);
		if(ImGui::Selectable(
				inst->m_nickname.c_str(),
				rowIsSelected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
				ImVec2(0, 0)))
		{
			m_selection = inst;
			rowIsSelected = true;
		}
		if(ImGui::TableSetColumnIndex(1))
			ImGui::TextUnformatted(inst->GetVendor().c_str());
		if(ImGui::TableSetColumnIndex(2))
			ImGui::TextUnformatted(inst->GetName().c_str());
		if(ImGui::TableSetColumnIndex(3))
			ImGui::TextUnformatted(inst->GetTransportName().c_str());
		if(ImGui::TableSetColumnIndex(4))
			ImGui::TextUnformatted(inst->GetTransportConnectionString().c_str());
		if(ImGui::TableSetColumnIndex(5))
			ImGui::TextUnformatted(inst->GetSerial().c_str());
		if(ImGui::TableSetColumnIndex(6))
		{
			string types = "";

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

			ImGui::TextUnformatted(types.c_str());
		}
		ImGui::PopID();
	}
}
