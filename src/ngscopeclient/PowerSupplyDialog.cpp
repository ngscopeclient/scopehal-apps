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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PowerSupplyDialog::PowerSupplyDialog(SCPIPowerSupply* psu, shared_ptr<PowerSupplyState> state)
	: Dialog(string("Power Supply: ") + psu->m_nickname, ImVec2(500, 400))
	, m_psu(psu)
	, m_state(state)
{
	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
		m_channelUIState.push_back(PowerSupplyChannelUIState(m_psu, i));
}

PowerSupplyDialog::~PowerSupplyDialog()
{
	LogWarning("Power supply dialog closed, need to disconnect from PSU and remove from session\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool PowerSupplyDialog::DoRender()
{
	//Top level settings
	if(ImGui::CollapsingHeader("Global", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool on = true;
		ImGui::Checkbox("Output Enable", &on);
	}

	//Per channel settings
	float valueWidth = 200;
	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
	{
		//Add new historical sample data
		m_channelUIState[i].AddHistory(m_state->m_channelVoltage[i].load(), m_state->m_channelCurrent[i].load() );

		if(ImGui::CollapsingHeader(m_psu->GetPowerChannelName(i).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Output Enable", &m_channelUIState[i].m_outputEnabled);

			//Advanced features (not available with all PSUs)
			if(ImGui::TreeNode("Advanced"))
			{
				ImGui::Checkbox("Overcurrent Shutdown", &m_channelUIState[i].m_overcurrentShutdownEnabled);

				ImGui::Checkbox("Soft Start", &m_channelUIState[i].m_softStartEnabled);

				ImGui::TreePop();
			}

			//Set points for channels
			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
			if(ImGui::TreeNode("Set Points"))
			{
				ImGui::SetNextItemWidth(valueWidth);
				ImGui::InputFloat("V", &m_channelUIState[i].m_setVoltage);
				ImGui::SameLine();
				if(ImGui::Button("Apply"))
				{

				}

				ImGui::SetNextItemWidth(valueWidth);
				ImGui::InputFloat("A", &m_channelUIState[i].m_setCurrent);
				ImGui::SameLine();
				if(ImGui::Button("Apply"))
				{

				}

				ImGui::TreePop();
			}

			//Actual values of channels
			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
			if(ImGui::TreeNode("Measured"))
			{
				ImGui::BeginDisabled();
					float f = m_state->m_channelVoltage[i].load();
					ImGui::SetNextItemWidth(valueWidth);
					ImGui::InputFloat("V", &f);

					f = m_state->m_channelCurrent[i].load();
					ImGui::SetNextItemWidth(valueWidth);
					ImGui::InputFloat("A", &f);
				ImGui::EndDisabled();

				ImGui::TreePop();
			}

			//Historical voltage/current graph
			if(ImGui::TreeNode("Trends"))
			{
				/*
				ImGui::PlotLines(
					"Voltage",
					&m_channelUIState[i].m_voltageHistory[0],
					m_channelUIState[i].m_voltageHistory.size(),
					0,
					nullptr,
					FLT_MAX,
					FLT_MAX,
					ImVec2(300, 100)
					);

				ImGui::PlotLines(
					"Current",
					&m_channelUIState[i].m_currentHistory[0],
					m_channelUIState[i].m_currentHistory.size(),
					0,
					nullptr,
					FLT_MAX,
					FLT_MAX,
					ImVec2(300, 100)
					);
				*/
				ImGui::TreePop();
			}
		}
	}

	return true;
}
