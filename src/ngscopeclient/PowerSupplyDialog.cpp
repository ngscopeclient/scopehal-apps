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
	@brief Implementation of PowerSupplyDialog
 */

#include "ngscopeclient.h"
#include "PowerSupplyDialog.h"

using namespace std;
using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PowerSupplyDialog::PowerSupplyDialog(SCPIPowerSupply* psu, shared_ptr<PowerSupplyState> state, Session* session)
	: Dialog(
		string("Power Supply: ") + psu->m_nickname,
		string("Power Supply: ") + psu->m_nickname,
		ImVec2(500, 400))
	, m_session(session)
	, m_masterEnable(psu->GetMasterPowerEnable())
	, m_tstart(GetTime())
	, m_psu(psu)
	, m_state(state)
{
	AsyncLoadState();
}

PowerSupplyDialog::~PowerSupplyDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void PowerSupplyDialog::RefreshFromHardware()
{
	//pull settings again
	AsyncLoadState();
}

void PowerSupplyDialog::AsyncLoadState()
{
	//Clear existing state (if any) and allocate space for new state
	m_channelUIState.clear();
	m_channelUIState.resize(m_psu->GetChannelCount());

	//Do the async load
	m_futureUIState.clear();
	SCPIPowerSupply* psu = m_psu;
	for(size_t i=0; i<m_psu->GetChannelCount(); i++)
	{
		//Add placeholders for non-power channels
		//TODO: can we avoid spawning a thread here pointlessly?
		if( (m_psu->GetInstrumentTypesForChannel(i) & Instrument::INST_PSU) == 0)
			m_futureUIState.push_back(async(launch::async, [/*psu, i*/]{ PowerSupplyChannelUIState dummy; return dummy; }));

		//Actual power channels get async load
		else
			m_futureUIState.push_back(async(launch::async, [psu, i]{ return PowerSupplyChannelUIState(psu, i); }));
	}
}

bool PowerSupplyDialog::DoRender()
{
	//Device information
	if(ImGui::CollapsingHeader("Info"))
	{
		ImGui::BeginDisabled();

			auto name = m_psu->GetName();
			auto vendor = m_psu->GetVendor();
			auto serial = m_psu->GetSerial();
			auto driver = m_psu->GetDriverName();
			auto transport = m_psu->GetTransport();
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

	//Top level settings
	if(m_psu->SupportsMasterOutputSwitching())
	{
		if(ImGui::CollapsingHeader("Global", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if(ImGui::Checkbox("Output Enable", &m_masterEnable))
				m_psu->SetMasterPowerEnable(m_masterEnable);

			HelpMarker(
				"Top level output enable, gating all outputs from the PSU.\n"
				"\n"
				"This acts as a second switch in series with the per-channel output enables.");
		}
	}

	//Grab asynchronously loaded channel state if it's ready
	if(m_futureUIState.size())
	{
		bool allDone = true;
		for(size_t i=0; i<m_futureUIState.size(); i++)
		{
			//Already loaded? No action needed
			if(m_channelUIState[i].m_setVoltage != "")
				continue;

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

	auto t = GetTime() - m_tstart;

	//Per channel settings
	for(size_t i=0; i<m_psu->GetChannelCount(); i++)
	{
		//Skip non-power channels
		if( (m_psu->GetInstrumentTypesForChannel(i) & Instrument::INST_PSU) == 0)
			continue;

		ChannelSettings(i, m_state->m_channelVoltage[i].load(), m_state->m_channelCurrent[i].load(), t);
	}

	return true;
}

/**
	@brief A single channel's settings

	@param i		Channel index
	@param v		Most recently observed voltage
	@param a		Most recently observed current
	@param etime	Elapsed time for animation
 */
void PowerSupplyDialog::ChannelSettings(int i, float v, float a, float etime)
{
	float valueWidth = 100;

	auto chname = m_psu->GetChannel(i)->GetDisplayName();

	Unit volts(Unit::UNIT_VOLTS);
	Unit amps(Unit::UNIT_AMPS);
	Unit fs(Unit::UNIT_FS);

	if(ImGui::CollapsingHeader(chname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID(chname.c_str());

		bool shdn = m_state->m_channelFuseTripped[i].load();
		bool cc = m_state->m_channelConstantCurrent[i].load();

		if(m_psu->SupportsIndividualOutputSwitching())
		{
			if(ImGui::Checkbox("Output Enable", &m_channelUIState[i].m_outputEnabled))
				m_psu->SetPowerChannelActive(i, m_channelUIState[i].m_outputEnabled);
			if(shdn)
			{
				//TODO: preference for configuring this?
				float alpha = fabs(sin(etime*M_PI))*0.5 + 0.5;

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1*alpha, 0, 0, 1*alpha));
				ImGui::TextUnformatted("Overload shutdown");
				ImGui::PopStyleColor();
				Tooltip(
					"Overcurrent shutdown has been triggered.\n\n"
					"Clear the fault on your load, then turn the output off and on again to reset."
					);
			}
			HelpMarker("Turns power from this channel on or off");
		}

		//Advanced features (not available with all PSUs)
		bool ocp = m_psu->SupportsOvercurrentShutdown();
		bool ss = m_psu->SupportsSoftStart();
		if(ocp || ss)
		{
			if(ImGui::TreeNode("Advanced"))
			{
				if(ocp)
				{
					if(ImGui::Checkbox("Overcurrent Shutdown", &m_channelUIState[i].m_overcurrentShutdownEnabled))
						m_psu->SetPowerOvercurrentShutdownEnabled(i, m_channelUIState[i].m_overcurrentShutdownEnabled);
					HelpMarker(
						"When enabled, the channel will shut down on overcurrent rather than switching to constant current mode.\n"
						"\n"
						"Once the overcurrent shutdown has been activated, the channel must be disabled and re-enabled to "
						"restore power to the load.");
				}

				if(ss)
				{
					if(ImGui::Checkbox("Soft Start", &m_channelUIState[i].m_softStartEnabled))
						m_psu->SetSoftStartEnabled(i, m_channelUIState[i].m_softStartEnabled);

					HelpMarker(
						"Deliberately limit the rise time of the output in order to reduce inrush current when driving "
						"capacitive loads.");

					ImGui::SetNextItemWidth(valueWidth);
					if(UnitInputWithExplicitApply(
						"Ramp time", m_channelUIState[i].m_setSSRamp, m_channelUIState[i].m_committedSSRamp, fs))
					{
						m_psu->SetSoftStartRampTime(i, m_channelUIState[i].m_committedSSRamp);
					}
					HelpMarker(
						"Transition time between off and on state when using soft start\n\n"
						"Changes are not pushed to hardware until you click Apply.\n\n"
						"CAUTION: Some instruments (e.g. R&S HMC804x) will turn off the output\n"
						"when changing the ramp time."
						);
				}

				ImGui::TreePop();
			}
		}

		if(m_psu->SupportsVoltageCurrentControl(i))
		{
			//Set points for channels
			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
			if(ImGui::TreeNode("Set Points"))
			{
				ImGui::SetNextItemWidth(valueWidth);
				if(UnitInputWithExplicitApply(
					"Voltage", m_channelUIState[i].m_setVoltage, m_channelUIState[i].m_committedSetVoltage, volts))
				{
					m_psu->SetPowerVoltage(i, m_channelUIState[i].m_committedSetVoltage);
				}
				HelpMarker("Target voltage to be supplied to the load.\n\nChanges are not pushed to hardware until you click Apply.");

				ImGui::SetNextItemWidth(valueWidth);
				if(UnitInputWithExplicitApply(
					"Current", m_channelUIState[i].m_setCurrent, m_channelUIState[i].m_committedSetCurrent, amps))
				{
					m_psu->SetPowerCurrent(i, m_channelUIState[i].m_committedSetCurrent);
				}
				HelpMarker("Maximum current to be supplied to the load.\n\nChanges are not pushed to hardware until you click Apply.");

				ImGui::TreePop();
			}

			//Actual values of channels
			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
			if(ImGui::TreeNode("Measured"))
			{
				ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(valueWidth);
					auto svolts = volts.PrettyPrint(v);
					ImGui::InputText("Voltage###VMeasured", &svolts);
				ImGui::EndDisabled();

				if(!cc && m_channelUIState[i].m_outputEnabled && !shdn)
				{
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
					ImGui::TextUnformatted("CV");
					ImGui::PopStyleColor();
					Tooltip("Channel is operating in constant-voltage mode");
				}
				HelpMarker("Measured voltage being output by the supply");

				ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(valueWidth);
					auto scurr = amps.PrettyPrint(a);
					ImGui::InputText("Current###IMeasured", &scurr);
				ImGui::EndDisabled();

				if(cc && m_channelUIState[i].m_outputEnabled && !shdn)
				{
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
					ImGui::TextUnformatted("CC");
					Tooltip("Channel is operating in constant-current mode");
					ImGui::PopStyleColor();
				}

				HelpMarker("Measured current being output by the supply");

				ImGui::TreePop();
			}
		}

		ImGui::PopID();
	}
}
