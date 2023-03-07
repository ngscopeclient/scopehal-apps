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

	//Set up initial empty state
	m_channelUIState.resize(m_load->GetChannelCount());

	//Asynchronously load rest of the state
	for(size_t i=0; i<m_load->GetChannelCount(); i++)
	{
		//Add placeholders for non-power channels
		//TODO: can we avoid spawning a thread here pointlessly?
		if( (m_load->GetInstrumentTypesForChannel(i) & Instrument::INST_LOAD) == 0)
			m_futureUIState.push_back(async(launch::async, [load, i]{ LoadChannelUIState dummy; return dummy; }));

		//Actual power channels get async load
		else
			m_futureUIState.push_back(async(launch::async, [load, i]{ return LoadChannelUIState(load, i); }));
	}
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

	//Grab asynchronously loaded channel state if it's ready
	if(m_futureUIState.size())
	{
		bool allDone = true;
		for(size_t i=0; i<m_futureUIState.size(); i++)
		{
			//Already loaded? No action needed
			//if(m_channelUIState[i].m_setVoltage != "")
			//	continue;

			//Not ready? Keep waiting
			if(m_futureUIState[i].wait_for(0s) != future_status::ready)
			{
				allDone = false;
				continue;
			}

			//Ready, process it
			m_channelUIState[i] = m_futureUIState[i].get();
		}

		if(allDone)
			m_futureUIState.clear();
	}

	//Channel information
	for(size_t i=0; i<m_load->GetChannelCount(); i++)
	{
		//Skip non-load channels
		if( (m_load->GetInstrumentTypesForChannel(i) & Instrument::INST_LOAD) == 0)
			continue;

		if(ImGui::CollapsingHeader(m_channelNames[i].c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(m_channelNames[i].c_str());
				ChannelSettings(i);
			ImGui::PopID();
		}
	}

	return true;
}

/**
	@brief Run settings for a single channel of the load
 */
void LoadDialog::ChannelSettings(size_t channel)
{
	float valueWidth = 100;
	Unit volts(Unit::UNIT_VOLTS);
	Unit amps(Unit::UNIT_AMPS);

	if(ImGui::Checkbox("Load Enable", &m_channelUIState[channel].m_loadEnabled))
		m_load->SetLoadActive(channel, m_channelUIState[channel].m_loadEnabled);

	//Actual values of channels
	ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
	if(ImGui::TreeNode("Measured"))
	{
		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			auto svolts = volts.PrettyPrint(m_state->m_channelVoltage[channel]);
			ImGui::InputText("Voltage###VMeasured", &svolts);
		ImGui::EndDisabled();

		/*if(!cc && m_channelUIState[i].m_outputEnabled && !shdn)
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
			ImGui::TextUnformatted("CV");
			ImGui::PopStyleColor();
			Tooltip("Channel is operating in constant-voltage mode");
		}*/
		HelpMarker("Measured voltage being sunk by the load");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			auto scurr = amps.PrettyPrint(m_state->m_channelCurrent[channel]);
			ImGui::InputText("Current###IMeasured", &scurr);
		ImGui::EndDisabled();

		/*
		if(cc && m_channelUIState[i].m_outputEnabled && !shdn)
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("CC");
			Tooltip("Channel is operating in constant-current mode");
			ImGui::PopStyleColor();
		}
		*/

		HelpMarker("Measured current being sunk by the load");

		ImGui::TreePop();
	}
}
