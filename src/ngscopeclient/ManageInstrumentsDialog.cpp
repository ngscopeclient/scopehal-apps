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
	: Dialog("Manage Instruments", "Manage Instruments", ImVec2(1000, 150))
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
	ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit;

	float width = ImGui::GetFontSize();
	if(ImGui::BeginTable("table", 7, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_WidthFixed, 6*width);
		ImGui::TableSetupColumn("Make", ImGuiTableColumnFlags_WidthFixed, 9*width);
		ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 15*width);
		ImGui::TableSetupColumn("Transport", ImGuiTableColumnFlags_WidthFixed, 4*width);
		ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed, 25*width);
		ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthFixed, 8*width);
		ImGui::TableSetupColumn("Features", ImGuiTableColumnFlags_WidthFixed, 10*width);
		ImGui::TableHeadersRow();

		auto insts = m_session.GetSCPIInstruments();
		for(auto inst : insts)
		{
			ImGui::PushID(inst);

			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			//Nickname
			bool rowIsSelected = (m_selection == inst);
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

			//Transport
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

				auto itype = inst->GetInstrumentTypes();
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

		ImGui::EndTable();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
