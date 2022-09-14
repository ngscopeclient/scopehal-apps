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
// RFGeneratorChannelUIState

RFGeneratorChannelUIState::RFGeneratorChannelUIState(SCPIRFSignalGenerator* generator, int channel)
	: m_outputEnabled(generator->GetChannelOutputEnable(channel))
	, m_committedLevel(generator->GetChannelOutputPower(channel))
	, m_committedFrequency(generator->GetChannelCenterFrequency(channel))
	, m_committedSweepStart(generator->GetSweepStartFrequency(channel))
	, m_committedSweepStop(generator->GetSweepStopFrequency(channel))
	, m_committedSweepStartLevel(generator->GetSweepStartLevel(channel))
	, m_committedSweepStopLevel(generator->GetSweepStopLevel(channel))
	, m_committedSweepDwellTime(generator->GetSweepDwellTime(channel))
	, m_committedSweepPoints(generator->GetSweepPoints(channel))
	{
		Unit dbm(Unit::UNIT_DBM);
		Unit hz(Unit::UNIT_HZ);
		Unit fs(Unit::UNIT_FS);

		m_level = dbm.PrettyPrint(m_committedLevel);
		m_frequency = hz.PrettyPrint(m_committedFrequency);
		m_sweepStart = hz.PrettyPrint(m_committedSweepStart);
		m_sweepStop = hz.PrettyPrint(m_committedSweepStop);
		m_sweepStartLevel = dbm.PrettyPrint(m_committedSweepStartLevel);
		m_sweepStopLevel = dbm.PrettyPrint(m_committedSweepStopLevel);
		m_sweepDwellTime = fs.PrettyPrint(m_committedSweepDwellTime);

		m_sweepPoints = m_committedSweepPoints;

		//TODO: enumeration API?
		m_sweepShapeNames.push_back("Sawtooth");
		m_sweepShapeNames.push_back("Triangle");
		m_sweepShapes.push_back(RFSignalGenerator::SWEEP_SHAPE_SAWTOOTH);
		m_sweepShapes.push_back(RFSignalGenerator::SWEEP_SHAPE_TRIANGLE);
		if(generator->GetSweepShape(channel) == RFSignalGenerator::SWEEP_SHAPE_TRIANGLE)
			m_sweepShape = 1;
		else
			m_sweepShape = 0;

		//TODO: enumeration API?
		m_sweepSpaceNames.push_back("Linear");
		m_sweepSpaceNames.push_back("Logarithmic");
		m_sweepSpaceTypes.push_back(RFSignalGenerator::SWEEP_SPACING_LINEAR);
		m_sweepSpaceTypes.push_back(RFSignalGenerator::SWEEP_SPACING_LOG);
		if(generator->GetSweepSpacing(channel) == RFSignalGenerator::SWEEP_SPACING_LOG)
			m_sweepSpacing = 1;
		else
			m_sweepSpacing = 0;

		//TODO: enumeration API?
		m_sweepTypeNames.push_back("None");
		m_sweepTypeNames.push_back("Frequency");
		m_sweepTypeNames.push_back("Level");
		m_sweepTypeNames.push_back("Frequency + Level");
		m_sweepTypes.push_back(RFSignalGenerator::SWEEP_TYPE_NONE);
		m_sweepTypes.push_back(RFSignalGenerator::SWEEP_TYPE_FREQ);
		m_sweepTypes.push_back(RFSignalGenerator::SWEEP_TYPE_LEVEL);
		m_sweepTypes.push_back(RFSignalGenerator::SWEEP_TYPE_FREQ_LEVEL);
		auto type = generator->GetSweepType(channel);
		switch(type)
		{
			case RFSignalGenerator::SWEEP_TYPE_NONE:
				m_sweepType = 0;
				break;

			case RFSignalGenerator::SWEEP_TYPE_FREQ:
				m_sweepType = 1;
				break;

			case RFSignalGenerator::SWEEP_TYPE_LEVEL:
				m_sweepType = 2;
				break;

			case RFSignalGenerator::SWEEP_TYPE_FREQ_LEVEL:
				m_sweepType = 3;
				break;
		}

		//TODO: enumeration API?
		m_sweepDirectionNames.push_back("Forward");
		m_sweepDirectionNames.push_back("Reverse");
		m_sweepDirections.push_back(RFSignalGenerator::SWEEP_DIR_FWD);
		m_sweepDirections.push_back(RFSignalGenerator::SWEEP_DIR_REV);
		if(generator->GetSweepDirection(channel) == RFSignalGenerator::SWEEP_DIR_REV)
			m_sweepDirection = 1;
		else
			m_sweepDirection = 0;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

RFGeneratorDialog::RFGeneratorDialog(
	SCPIRFSignalGenerator* generator,
	shared_ptr<RFSignalGeneratorState> state,
	Session* session)
	: Dialog(string("RF Generator: ") + generator->m_nickname, ImVec2(400, 350))
	, m_session(session)
	, m_state(state)
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

		string f;
		string p;
		if(m_state->m_firstUpdateDone)
		{
			f = hz.PrettyPrint(m_state->m_channelFrequency[i]);
			p = dbm.PrettyPrint(m_state->m_channelLevel[i]);
		}

		bool sweepingPower = m_uiState[i].m_sweepType >= 2;
		bool sweepingFrequency = (m_uiState[i].m_sweepType == 1) || (m_uiState[i].m_sweepType == 3);

		if(sweepingPower)
		{
			ImGui::BeginDisabled();
			ImGui::InputText("Level", &p);
			ImGui::EndDisabled();

			HelpMarker(
				"Power level of the generated waveform.\n\n"
				"This value cannot be changed when doing a power sweep. Change levels under sweep settings.");
		}
		else
		{
			//Power level is a potentially damaging operation
			//Require the user to explicitly commit changes before it takes effect
			if(UnitInputWithExplicitApply("Level", m_uiState[i].m_level, m_uiState[i].m_committedLevel, dbm))
				m_generator->SetChannelOutputPower(i, m_uiState[i].m_committedLevel);
			HelpMarker("Power level of the generated waveform");
		}

		if(sweepingFrequency)
		{
			ImGui::BeginDisabled();
			ImGui::InputText("Frequency", &f);
			ImGui::EndDisabled();

			HelpMarker(
				"Carrier frequency of the generated waveform.\n\n"
				"This value cannot be changed when doing a frequency sweep. Change frequency under sweep settings.");
		}
		else
		{
			if(UnitInputWithImplicitApply("Frequency", m_uiState[i].m_frequency, m_uiState[i].m_committedFrequency, hz))
				m_generator->SetChannelCenterFrequency(i, m_uiState[i].m_committedFrequency);

			HelpMarker("Carrier frequency of the generated waveform.");
		}

		if(m_generator->IsSweepAvailable(i))
		{
			if(ImGui::TreeNode("Sweep"))
			{
				ImGui::PushID("Sweep");

				if(Combo("Mode", m_uiState[i].m_sweepTypeNames, m_uiState[i].m_sweepType))
					m_generator->SetSweepType(i, m_uiState[i].m_sweepTypes[m_uiState[i].m_sweepType]);
				HelpMarker("Choose whether to sweep frequency, power, both, or neither.");

				if(UnitInputWithImplicitApply("Dwell Time",
					m_uiState[i].m_sweepDwellTime, m_uiState[i].m_committedSweepDwellTime, fs))
				{
					m_generator->SetSweepDwellTime(i, m_uiState[i].m_committedSweepDwellTime);
				}
				HelpMarker("Time to stay at each frequency before moving to the next.");

				if(IntInputWithImplicitApply("Points", m_uiState[i].m_sweepPoints, m_uiState[i].m_committedSweepPoints))
					m_generator->SetSweepPoints(i, m_uiState[i].m_committedSweepPoints);
				HelpMarker("Number of steps in the sweep.");

				if(Combo("Shape", m_uiState[i].m_sweepShapeNames, m_uiState[i].m_sweepShape))
					m_generator->SetSweepShape(i, m_uiState[i].m_sweepShapes[m_uiState[i].m_sweepShape]);
				HelpMarker("Select the shape of the sweep waveform (triangle or sawtooth).");

				if(Combo("Spacing", m_uiState[i].m_sweepSpaceNames, m_uiState[i].m_sweepSpacing))
					m_generator->SetSweepSpacing(i, m_uiState[i].m_sweepSpaceTypes[m_uiState[i].m_sweepSpacing]);
				HelpMarker("Specify how to divide the sweep range into points (linear or logarithmic spacing).");

				if(Combo("Direction", m_uiState[i].m_sweepDirectionNames, m_uiState[i].m_sweepDirection))
					m_generator->SetSweepDirection(i, m_uiState[i].m_sweepDirections[m_uiState[i].m_sweepDirection]);
				HelpMarker("Allows the direction of the sweep to be reversed.");

				if(UnitInputWithImplicitApply("Start Frequency",
					m_uiState[i].m_sweepStart, m_uiState[i].m_committedSweepStart, hz))
				{
					m_generator->SetSweepStartFrequency(i, m_uiState[i].m_committedSweepStart);
				}
				HelpMarker("Initial value for frequency sweeps. Ignored if not sweeping frequency.");

				if(UnitInputWithExplicitApply("Start Level",
					m_uiState[i].m_sweepStartLevel, m_uiState[i].m_committedSweepStartLevel, dbm))
				{
					m_generator->SetSweepStartLevel(i, m_uiState[i].m_committedSweepStartLevel);
				}
				HelpMarker("Initial value for power sweeps. Ignored if not sweeping power.");

				if(UnitInputWithImplicitApply("Stop Frequency",
					m_uiState[i].m_sweepStop, m_uiState[i].m_committedSweepStop, hz))
				{
					m_generator->SetSweepStopFrequency(i, m_uiState[i].m_committedSweepStop);
				}
				HelpMarker("Ending value for frequency sweeps. Ignored if not sweeping frequency.");

				if(UnitInputWithExplicitApply("Stop Level",
					m_uiState[i].m_sweepStopLevel, m_uiState[i].m_committedSweepStopLevel, dbm))
				{
					m_generator->SetSweepStopLevel(i, m_uiState[i].m_committedSweepStopLevel);
				}
				HelpMarker("Ending value for power sweeps. Ignored if not sweeping power.");

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
}
