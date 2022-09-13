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
	@brief Implementation of RFGeneratorDialog
 */

#include "ngscopeclient.h"
#include "RFGeneratorDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

RFGeneratorDialog::RFGeneratorDialog(SCPIRFSignalGenerator* generator, Session* session)
	: Dialog(string("RF Generator: ") + generator->m_nickname, ImVec2(400, 350))
	, m_session(session)
	, m_generator(generator)
{
	Unit hz(Unit::UNIT_HZ);
	Unit dbm(Unit::UNIT_DBM);

	double start = GetTime();

	for(int i=0; i<m_generator->GetChannelCount(); i++)
		m_uiState.push_back(RFGeneratorChannelUIState(m_generator, i));

	LogDebug("Intial UI state loaded in %.2f ms\n", (GetTime() - start) * 1000);
}

RFGeneratorDialog::~RFGeneratorDialog()
{
	m_session->RemoveRFGenerator(m_generator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool RFGeneratorDialog::DoRender()
{
	//Device information
	if(ImGui::CollapsingHeader("Info"))
	{
		ImGui::BeginDisabled();

			auto name = m_generator->GetName();
			auto vendor = m_generator->GetVendor();
			auto serial = m_generator->GetSerial();
			auto driver = m_generator->GetDriverName();
			auto transport = m_generator->GetTransport();
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

	for(int i=0; i<m_generator->GetChannelCount(); i++)
		DoChannel(i);

	return true;
}

/**
	@brief Run the UI for a single channel
 */
void RFGeneratorDialog::DoChannel(int i)
{
	auto chname = m_generator->GetChannelName(i);

	Unit fs(Unit::UNIT_FS);
	Unit hz(Unit::UNIT_HZ);
	Unit dbm(Unit::UNIT_DBM);

	if(ImGui::CollapsingHeader(chname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID(chname.c_str());

		if(ImGui::Checkbox("Output Enable", &m_uiState[i].m_outputEnabled))
			m_generator->SetChannelOutputEnable(i, m_uiState[i].m_outputEnabled);
		HelpMarker("Turns the RF signal from this channel on or off");

		//Power level is a potentially damaging operation
		//Require the user to explicitly commit changes before it takes effect
		if(UnitInputWithExplicitApply("Level", m_uiState[i].m_level, m_uiState[i].m_committedLevel, dbm))
			m_generator->SetChannelOutputPower(i, m_uiState[i].m_committedLevel);
		HelpMarker("Power level of the generated waveform");

		if(UnitInputWithImplicitApply("Frequency", m_uiState[i].m_frequency, m_uiState[i].m_committedFrequency, hz))
			m_generator->SetChannelCenterFrequency(i, m_uiState[i].m_committedFrequency);

		if(m_generator->IsSweepAvailable(i))
		{
			if(ImGui::TreeNode("Sweep"))
			{
				ImGui::PushID("Sweep");

				if(UnitInputWithImplicitApply("Dwell Time",
					m_uiState[i].m_sweepDwellTime, m_uiState[i].m_committedSweepDwellTime, fs))
				{
					m_generator->SetSweepDwellTime(i, m_uiState[i].m_committedSweepDwellTime);
				}

				if(UnitInputWithImplicitApply("Start Frequency",
					m_uiState[i].m_sweepStart, m_uiState[i].m_committedSweepStart, hz))
				{
					m_generator->SetSweepStartFrequency(i, m_uiState[i].m_committedSweepStart);
				}

				if(UnitInputWithExplicitApply("Start Level",
					m_uiState[i].m_sweepStartLevel, m_uiState[i].m_committedSweepStartLevel, dbm))
				{
					m_generator->SetSweepStartLevel(i, m_uiState[i].m_committedSweepStartLevel);
				}

				if(UnitInputWithImplicitApply("Stop Frequency",
					m_uiState[i].m_sweepStop, m_uiState[i].m_committedSweepStop, hz))
				{
					m_generator->SetSweepStopFrequency(i, m_uiState[i].m_committedSweepStop);
				}

				if(UnitInputWithExplicitApply("Stop Level",
					m_uiState[i].m_sweepStopLevel, m_uiState[i].m_committedSweepStopLevel, hz))
				{
					m_generator->SetSweepStopLevel(i, m_uiState[i].m_committedSweepStopLevel);
				}

				ImGui::PopID();
				ImGui::TreePop();
			}
		}

		if(ImGui::TreeNode("Analog Modulation"))
		{
			ImGui::TreePop();
		}

		if(m_generator->IsVectorModulationAvailable(i))
		{
			if(ImGui::TreeNode("Vector Modulation"))
			{
				ImGui::TreePop();
			}
		}

		ImGui::PopID();
	}

	//Push any pending traffic to hardware
	m_generator->GetTransport()->FlushCommandQueue();
}
