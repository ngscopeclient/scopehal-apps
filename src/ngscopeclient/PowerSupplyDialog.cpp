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
	@brief Implementation of PowerSupplyDialog
 */

#include "ngscopeclient.h"
#include "PowerSupplyDialog.h"

using namespace std;
using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PowerSupplyDialog::PowerSupplyDialog(SCPIPowerSupply* psu, shared_ptr<PowerSupplyState> state, Session* session)
	: Dialog(string("Power Supply: ") + psu->m_nickname, ImVec2(500, 400))
	, m_session(session)
	, m_masterEnable(psu->GetMasterPowerEnable())
	, m_tstart(GetTime())
	, m_historyDepth(60)
	, m_psu(psu)
	, m_state(state)
{
	//Set up initial empty state
	m_channelUIState.resize(m_psu->GetPowerChannelCount());

	//Asynchronously load rest of the state
	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
		m_futureUIState.push_back(async(launch::async, [psu, i]{ return PowerSupplyChannelUIState(psu, i); }));
}

PowerSupplyDialog::~PowerSupplyDialog()
{
	m_session->RemovePowerSupply(m_psu);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

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
	if(ImGui::CollapsingHeader("Global", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if(ImGui::Checkbox("Output Enable", &m_masterEnable))
			m_psu->SetMasterPowerEnable(m_masterEnable);

		HelpMarker(
			"Top level output enable, gating all outputs from the PSU.\n"
			"\n"
			"This acts as a second switch in series with the per-channel output enables.");
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
	bool firstUpdateDone = m_state->m_firstUpdateDone.load();

	//Per channel settings
	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
	{
		float v = m_state->m_channelVoltage[i].load();
		float a = m_state->m_channelCurrent[i].load();

		//Update history
		if(firstUpdateDone)
		{
			m_channelUIState[i].m_voltageHistory.AddPoint(t, v);
			m_channelUIState[i].m_currentHistory.AddPoint(t, a);
		}
		m_channelUIState[i].m_voltageHistory.Span = m_historyDepth;
		m_channelUIState[i].m_currentHistory.Span = m_historyDepth;

		ChannelSettings(i, v, a, t);
	}

	//Combined trend plot for all channels
	if(ImGui::CollapsingHeader("Trends"))
		CombinedTrendPlot(t);

	return true;
}

/**
	@brief A single channel's settings

	@param i		Channel index
	@param v		Most recently observed voltage
	@param a		Most recently observed current
	@param etime	Elapsed time for plotting
 */
void PowerSupplyDialog::ChannelSettings(int i, float v, float a, float etime)
{
	float valueWidth = 100;

	auto chname = m_psu->GetPowerChannelName(i);

	Unit volts(Unit::UNIT_VOLTS);
	Unit amps(Unit::UNIT_AMPS);

	if(ImGui::CollapsingHeader(chname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID(chname.c_str());

		bool shdn = m_state->m_channelFuseTripped[i].load();
		bool cc = m_state->m_channelConstantCurrent[i].load();

		if(ImGui::Checkbox("Output Enable", &m_channelUIState[i].m_outputEnabled))
			m_psu->SetPowerChannelActive(i, m_channelUIState[i].m_outputEnabled);
		if(shdn)
		{
			//TODO: preference for configuring this?
			float alpha = fabs(sin(etime*M_PI))*0.5 + 0.5;

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1*alpha, 0, 0, 1*alpha));
			ImGui::Text("Overload shutdown");
			ImGui::PopStyleColor();
			Tooltip(
				"Overcurrent shutdown has been triggered.\n\n"
				"Clear the fault on your load, then turn the output off and on again to reset."
				);
		}
		HelpMarker("Turns power from this channel on or off");

		//Advanced features (not available with all PSUs)
		if(ImGui::TreeNode("Advanced"))
		{
			if(ImGui::Checkbox("Overcurrent Shutdown", &m_channelUIState[i].m_overcurrentShutdownEnabled))
				m_psu->SetPowerOvercurrentShutdownEnabled(i, m_channelUIState[i].m_overcurrentShutdownEnabled);
			HelpMarker(
				"When enabled, the channel will shut down on overcurrent rather than switching to constant current mode.\n"
				"\n"
				"Once the overcurrent shutdown has been activated, the channel must be disabled and re-enabled to "
				"restore power to the load.");

			if(ImGui::Checkbox("Soft Start", &m_channelUIState[i].m_softStartEnabled))
				m_psu->SetSoftStartEnabled(i, m_channelUIState[i].m_softStartEnabled);

			HelpMarker(
				"Deliberately limit the rise time of the output in order to reduce inrush current when driving "
				"capacitive loads.\n");

			ImGui::TreePop();
		}

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
				m_psu->SetPowerVoltage(i, m_channelUIState[i].m_committedSetCurrent);
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
				ImGui::Text("CV");
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
				ImGui::Text("CC");
				Tooltip("Channel is operating in constant-current mode");
				ImGui::PopStyleColor();
			}

			HelpMarker("Measured current being output by the supply");

			ImGui::TreePop();
		}

		//Historical voltage/current graph
		if(ImGui::TreeNode("Trends"))
		{
			auto csize = ImGui::GetContentRegionAvail();

			if(ImPlot::BeginPlot("Voltage History", ImVec2(csize.x, 200), ImPlotFlags_NoLegend) )
			{
				ImPlot::SetupAxisLimits(ImAxis_X1, etime - m_historyDepth, etime, ImGuiCond_Always);

				auto& hist = m_channelUIState[i].m_voltageHistory;
				ImPlot::PlotLine(
					chname.c_str(),
					&hist.Data[0].x,
					&hist.Data[0].y,
					hist.Data.size(),
					0,
					0,
					2*sizeof(float));

				ImPlot::EndPlot();
			}

			if(ImPlot::BeginPlot("Current History", ImVec2(csize.x, 200), ImPlotFlags_NoLegend) )
			{
				ImPlot::SetupAxisLimits(ImAxis_X1, etime - m_historyDepth, etime, ImGuiCond_Always);

				auto& hist = m_channelUIState[i].m_currentHistory;

				ImPlot::PlotLine(
					chname.c_str(),
					&hist.Data[0].x,
					&hist.Data[0].y,
					hist.Data.size(),
					0,
					0,
					2*sizeof(float));

				ImPlot::EndPlot();
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}

/**
	@brief Combined trend plots for all channels
 */
void PowerSupplyDialog::CombinedTrendPlot(float etime)
{
	auto csize = ImGui::GetContentRegionAvail();

	if(ImPlot::BeginPlot("Voltage History", ImVec2(csize.x, 200)) )
	{
		ImPlot::SetupAxisLimits(ImAxis_X1, etime - m_historyDepth, etime, ImGuiCond_Always);

		for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
		{
			auto chname = m_psu->GetPowerChannelName(i);
			auto& hist = m_channelUIState[i].m_voltageHistory;
			ImPlot::PlotLine(
				chname.c_str(),
				&hist.Data[0].x,
				&hist.Data[0].y,
				hist.Data.size(),
				0,
				0,
				2*sizeof(float));
		}

		ImPlot::EndPlot();
	}

	if(ImPlot::BeginPlot("Current History", ImVec2(csize.x, 200)) )
	{
		ImPlot::SetupAxisLimits(ImAxis_X1, etime - m_historyDepth, etime, ImGuiCond_Always);

		for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
		{
			auto chname = m_psu->GetPowerChannelName(i);
			auto& hist = m_channelUIState[i].m_currentHistory;
			ImPlot::PlotLine(
				chname.c_str(),
				&hist.Data[0].x,
				&hist.Data[0].y,
				hist.Data.size(),
				0,
				0,
				2*sizeof(float));
		}

		ImPlot::EndPlot();
	}
}
