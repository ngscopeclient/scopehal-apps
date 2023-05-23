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
	@brief Implementation of MeasurementsDialog
 */

#include "ngscopeclient.h"
#include "MeasurementsDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MeasurementsDialog::MeasurementsDialog(Session& session)
	: Dialog("Measurements", "Measurements", ImVec2(300, 400))
	, m_session(session)
{

}

MeasurementsDialog::~MeasurementsDialog()
{
	for(auto s : m_streams)
	{
		auto ochan = dynamic_cast<OscilloscopeChannel*>(s.m_channel);
		if(ochan)
			ochan->Release();
	}
	m_streams.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool MeasurementsDialog::DoRender()
{
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit;

	float width = ImGui::GetFontSize();

	int ncols = 2;	//TODO: add statistics
	bool deleteRow = false;
	size_t rowToDelete = 0;
	if(ImGui::BeginTable("table", ncols, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 15*width);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 10*width);
		//TODO: statistics
		ImGui::TableHeadersRow();

		for(size_t i=0; i<m_streams.size(); i++)
		{
			auto s = m_streams[i];
			auto name = s.GetName();
			ImGui::TableNextRow(ImGuiTableRowFlags_None);
			ImGui::PushID(name.c_str());

			ImGui::TableSetColumnIndex(0);
			ImGui::Selectable(name.c_str(), false);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::MenuItem("Delete"))
				{
					deleteRow = true;
					rowToDelete = i;
				}

				ImGui::EndPopup();
			}

			ImGui::TableSetColumnIndex(1);
			auto value = s.GetYAxisUnits().PrettyPrint(s.GetScalarValue());
			ImGui::TextUnformatted(value.c_str());

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if(deleteRow)
		RemoveStream(rowToDelete);

	return true;
}

void MeasurementsDialog::RemoveStream(size_t i)
{
	auto ochan = dynamic_cast<OscilloscopeChannel*>(m_streams[i].m_channel);
	if(ochan)
		ochan->Release();
	m_streams.erase(m_streams.begin() + i);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

void MeasurementsDialog::AddStream(StreamDescriptor stream)
{
	//TODO: search for duplicates?
	m_streams.push_back(stream);

	auto ochan = dynamic_cast<OscilloscopeChannel*>(stream.m_channel);
	if(ochan)
		ochan->AddRef();
}
