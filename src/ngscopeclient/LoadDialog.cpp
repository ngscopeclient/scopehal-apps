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
	: Dialog(
		string("Load: ") + load->m_nickname,
		string("Load: ") + load->m_nickname,
		ImVec2(500, 400))
	, m_session(session)
	, m_tstart(GetTime())
	, m_load(load)
	, m_state(state)
{
	//Inputs
	for(size_t i=0; i<m_load->GetChannelCount(); i++)
		m_channelNames.push_back(m_load->GetChannel(i)->GetDisplayName());

	RefreshFromHardware();
}

LoadDialog::~LoadDialog()
{
	m_session->RemoveLoad(m_load);
}

void LoadDialog::RefreshFromHardware()
{
	m_channelUIState.clear();
	m_futureUIState.clear();

	//Set up initial empty UI state
	m_channelUIState.resize(m_load->GetChannelCount());

	//Asynchronously load rest of the state
	auto load = m_load;
	for(size_t i=0; i<m_load->GetChannelCount(); i++)
	{
		//Add placeholders for non-power channels
		//TODO: can we avoid spawning a thread here pointlessly?
		if( (m_load->GetInstrumentTypesForChannel(i) & Instrument::INST_LOAD) == 0)
			m_futureUIState.push_back(async(launch::async, [/*load, i*/]{ LoadChannelUIState dummy; return dummy; }));

		//Actual power channels get async load
		else
			m_futureUIState.push_back(async(launch::async, [load, i]{ return LoadChannelUIState(load, i); }));
	}
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
	float valueWidth = 150;
	Unit volts(Unit::UNIT_VOLTS);
	Unit amps(Unit::UNIT_AMPS);
	Unit watts(Unit::UNIT_WATTS);
	Unit ohms(Unit::UNIT_OHMS);

	if(ImGui::Checkbox("Load Enable", &m_channelUIState[channel].m_loadEnabled))
		m_load->SetLoadActive(channel, m_channelUIState[channel].m_loadEnabled);

	ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
	if(ImGui::TreeNode("Configuration"))
	{
		ImGui::SetNextItemWidth(valueWidth);
		if(Dialog::Combo("Voltage Range",
			m_channelUIState[channel].m_voltageRangeNames,
			m_channelUIState[channel].m_voltageRangeIndex))
		{
			m_load->SetLoadVoltageRange(channel, m_channelUIState[channel].m_voltageRangeIndex);
		}
		HelpMarker("Maximum operating voltage for the load");

		ImGui::SetNextItemWidth(valueWidth);
		if(Dialog::Combo("Current Range",
			m_channelUIState[channel].m_currentRangeNames,
			m_channelUIState[channel].m_currentRangeIndex))
		{
			m_load->SetLoadCurrentRange(channel, m_channelUIState[channel].m_currentRangeIndex);
		}
		HelpMarker("Maximum operating current for the load");

		const char* modes[4] =
		{
			"Constant current",
			"Constant voltage",
			"Constant resistance",
			"Constant power"
		};

		ImGui::SetNextItemWidth(valueWidth);
		if(ImGui::Combo("Mode", (int*)&m_channelUIState[channel].m_mode, modes, 4))
		{
			//Turn the load off before changing mode, to avoid accidental overloading of the DUT
			m_load->SetLoadActive(channel, false);
			m_channelUIState[channel].m_loadEnabled = false;

			m_load->SetLoadMode(channel, m_channelUIState[channel].m_mode);

			//Refresh set point with hardware config for the new mode
			m_channelUIState[channel].RefreshSetPoint();
		}
		HelpMarker("Operating mode for the control loop");

		//Update set point text if it's been changed via the filter graph
		if(m_channelUIState[channel].m_committedSetPoint != m_load->GetLoadSetPoint(channel))
			m_channelUIState[channel].RefreshSetPoint();

		//Set point
		ImGui::SetNextItemWidth(valueWidth);
		bool applySetPoint = false;
		switch(m_load->GetLoadMode(channel))
		{
			case Load::MODE_CONSTANT_CURRENT:
				applySetPoint = UnitInputWithExplicitApply(
					"Current",
					m_channelUIState[channel].m_setPoint,
					m_channelUIState[channel].m_committedSetPoint,
					amps);
				break;

			case Load::MODE_CONSTANT_VOLTAGE:
				applySetPoint = UnitInputWithExplicitApply(
					"Voltage",
					m_channelUIState[channel].m_setPoint,
					m_channelUIState[channel].m_committedSetPoint,
					volts);
				break;

			case Load::MODE_CONSTANT_RESISTANCE:
				applySetPoint = UnitInputWithExplicitApply(
					"Resistance",
					m_channelUIState[channel].m_setPoint,
					m_channelUIState[channel].m_committedSetPoint,
					ohms);
				break;

			case Load::MODE_CONSTANT_POWER:
				applySetPoint = UnitInputWithExplicitApply(
					"Power",
					m_channelUIState[channel].m_setPoint,
					m_channelUIState[channel].m_committedSetPoint,
					watts);
				break;

			default:
				break;
		}
		if(applySetPoint)
			m_load->SetLoadSetPoint(channel, m_channelUIState[channel].m_committedSetPoint);

		HelpMarker("Set point for the load.\n\nChanges are not pushed to hardware until you click Apply.");

		ImGui::TreePop();
	}

	//Actual values of channels
	ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
	if(ImGui::TreeNode("Measured"))
	{
		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			auto svolts = volts.PrettyPrint(m_state->m_channelVoltage[channel]);
			ImGui::InputText("Voltage###VMeasured", &svolts);
		ImGui::EndDisabled();

		HelpMarker("Measured voltage being sunk by the load");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			auto scurr = amps.PrettyPrint(m_state->m_channelCurrent[channel]);
			ImGui::InputText("Current###IMeasured", &scurr);
		ImGui::EndDisabled();

		HelpMarker("Measured current being sunk by the load");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			auto pcurr = watts.PrettyPrint(m_state->m_channelVoltage[channel] * m_state->m_channelCurrent[channel]);
			ImGui::InputText("Power###PCalc", &pcurr);
		ImGui::EndDisabled();

		HelpMarker("Measured power being sunk by the load");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(valueWidth);
			auto rcurr = ohms.PrettyPrint(m_state->m_channelVoltage[channel] / m_state->m_channelCurrent[channel]);
			ImGui::InputText("Resistance###RCalc", &rcurr);
		ImGui::EndDisabled();

		HelpMarker("Equivalent resistance of the load");

		ImGui::TreePop();
	}
}
