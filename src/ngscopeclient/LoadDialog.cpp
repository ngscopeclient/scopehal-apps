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
	@brief Implementation of LoadDialog
 */

#include "ngscopeclient.h"
#include "LoadDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

LoadDialog::LoadDialog(SCPILoad* load, shared_ptr<LoadState> state, Session* session)
	: Dialog(string("Load: ") + load->m_nickname, ImVec2(500, 400))
	, m_session(session)
	, m_tstart(GetTime())
	, m_load(load)
	, m_state(state)
{
	//Inputs
	for(size_t i=0; i<m_load->GetChannelCount(); i++)
		m_channelNames.push_back(m_load->GetChannel(i)->GetDisplayName());
}

LoadDialog::~LoadDialog()
{
	m_session->RemoveLoad(m_load);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool LoadDialog::DoRender()
{
	//Device information
	if(ImGui::CollapsingHeader("Info"))
	{
		ImGui::BeginDisabled();

			auto name = m_load->GetName();
			auto vendor = m_load->GetVendor();
			auto serial = m_load->GetSerial();
			auto driver = m_load->GetDriverName();
			auto transport = m_load->GetTransport();
			auto tname = transport->GetName();
			auto tstring = transport->GetConnectionString();

			ImGui::InputText("Make", &vendor[0], vendor.size());
			ImGui::InputText("Model", &name[0], name.size());
			ImGui::InputText("Serial", &serial[0], serial.size());
			ImGui::InputText("Driver", &driver[0], driver.size());
			ImGui::InputText("Transport", &tname[0], tname.size());
			ImGui::InputText("Path", &tstring[0], tstring.size());

		ImGui::EndDisabled();
	}
/*
	//Save history
	auto pri = m_state->m_primaryMeasurement.load();
	auto sec = m_state->m_secondaryMeasurement.load();
	bool firstUpdateDone = m_state->m_firstUpdateDone.load();
	bool hasSecondary = m_load->GetSecondaryMeterMode() != Load::NONE;

	float valueWidth = 100;
	auto primaryMode = m_load->ModeToText(m_load->GetMeterMode());
	auto secondaryMode = m_load->ModeToText(m_load->GetSecondaryMeterMode());

	if(ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if(ImGui::Checkbox("Autorange", &m_autorange))
			m_load->SetMeterAutoRange(m_autorange);
		HelpMarker("Enables automatic selection of load scale ranges.");

		//Channel selector (hide if we have only one channel)
		if(m_load->GetChannelCount() > 1)
		{
			if(Combo("Channel", m_channelNames, m_selectedChannel))
				m_load->SetCurrentMeterChannel(m_selectedChannel);

			HelpMarker("Select which input channel is being monitored.");
		}

		//Primary operating mode selector
		if(Combo("Mode", m_primaryModeNames, m_primaryModeSelector))
			OnPrimaryModeChanged();
		HelpMarker("Select the type of measurement to make.");

		//Secondary operating mode selector
		if(m_secondaryModeNames.empty())
			ImGui::BeginDisabled();
		if(Combo("Secondary Mode", m_secondaryModeNames, m_secondaryModeSelector))
			m_load->SetSecondaryMeterMode(m_secondaryModes[m_secondaryModeSelector]);
		if(m_secondaryModeNames.empty())
			ImGui::EndDisabled();

		HelpMarker(
			"Select auxiliary measurement mode, if supported.\n\n"
			"The set of available auxiliary measurements depends on the current primary measurement mode.");
	}

	if(ImGui::CollapsingHeader("Measurements", ImGuiTreeNodeFlags_DefaultOpen))
	{
		string spri;
		string ssec;

		//Hide values until we get first readings back from the load
		if(firstUpdateDone)
		{
			spri = m_load->GetMeterUnit().PrettyPrint(pri, m_load->GetMeterDigits());
			if(hasSecondary)
				ssec = m_load->GetSecondaryMeterUnit().PrettyPrint(sec, m_load->GetMeterDigits());
		}

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			ImGui::InputText(primaryMode.c_str(), &spri[0], spri.size());
		ImGui::EndDisabled();
		HelpMarker("Most recent value for the primary measurement");

		if(hasSecondary)
		{
			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(valueWidth);
				ImGui::InputText(secondaryMode.c_str(), &ssec[0], ssec.size());
			ImGui::EndDisabled();
			HelpMarker("Most recent value for the secondary measurement");
		}
	}
*/
	return true;
}
