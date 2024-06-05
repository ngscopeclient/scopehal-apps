/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of FunctionGeneratorDialog
 */

#include "ngscopeclient.h"
#include "FunctionGeneratorDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FunctionGeneratorDialog::FunctionGeneratorDialog(shared_ptr<SCPIFunctionGenerator> generator, Session* session)
	: Dialog(
		string("Function Generator: ") + generator->m_nickname,
		string("Function Generator: ") + generator->m_nickname,
		ImVec2(400, 350))
	, m_session(session)
	, m_generator(generator)
{
	Unit hz(Unit::UNIT_HZ);
	Unit percent(Unit::UNIT_PERCENT);
	Unit volts(Unit::UNIT_VOLTS);
	Unit fs(Unit::UNIT_FS);

	size_t n = m_generator->GetChannelCount();
	for(size_t i=0; i<n; i++)
	{
		FunctionGeneratorChannelUIState state;
		if(0 == (m_generator->GetInstrumentTypesForChannel(i) & Instrument::INST_FUNCTION))
		{
			//Add dummy placeholder (never used)
			m_uiState.push_back(state);
			continue;
		}

		state.m_outputEnabled = m_generator->GetFunctionChannelActive(i);

		state.m_committedAmplitude = m_generator->GetFunctionChannelAmplitude(i);
		state.m_amplitude = volts.PrettyPrint(state.m_committedAmplitude);

		state.m_committedOffset = m_generator->GetFunctionChannelOffset(i);
		state.m_offset = volts.PrettyPrint(state.m_committedOffset);

		state.m_committedDutyCycle = m_generator->GetFunctionChannelDutyCycle(i);
		state.m_dutyCycle = percent.PrettyPrint(state.m_committedDutyCycle);

		state.m_committedFrequency = m_generator->GetFunctionChannelFrequency(i);
		state.m_frequency = hz.PrettyPrint(state.m_committedFrequency);

		state.m_committedRiseTime = m_generator->GetFunctionChannelRiseTime(i);
		state.m_riseTime = fs.PrettyPrint(state.m_committedRiseTime);

		state.m_committedFallTime = m_generator->GetFunctionChannelFallTime(i);
		state.m_fallTime = fs.PrettyPrint(state.m_committedFallTime);

		//Convert waveform shape to list box index
		state.m_waveShapes = m_generator->GetAvailableWaveformShapes(i);
		state.m_shapeIndex = 0;
		auto shape = m_generator->GetFunctionChannelShape(i);
		for(size_t j=0; j<state.m_waveShapes.size(); j++)
		{
			if(shape == state.m_waveShapes[j])
				state.m_shapeIndex = j;
			state.m_waveShapeNames.push_back(m_generator->GetNameOfShape(state.m_waveShapes[j]));
		}

		if(m_generator->GetFunctionChannelOutputImpedance(i) == FunctionGenerator::IMPEDANCE_50_OHM)
			state.m_impedanceIndex = 1;
		else
			state.m_impedanceIndex = 0;

		m_uiState.push_back(state);
	}

	m_impedances.push_back(FunctionGenerator::IMPEDANCE_HIGH_Z);
	m_impedances.push_back(FunctionGenerator::IMPEDANCE_50_OHM);
	m_impedanceNames.push_back("High-Z");
	m_impedanceNames.push_back("50Ω");
}

FunctionGeneratorDialog::~FunctionGeneratorDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool FunctionGeneratorDialog::DoRender()
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

	size_t n = m_generator->GetChannelCount();
	for(size_t i=0; i<n; i++)
	{
		if(0 == (m_generator->GetInstrumentTypesForChannel(i) & Instrument::INST_FUNCTION))
			continue;

		DoChannel(i);
	}

	return true;
}

/**
	@brief Run the UI for a single channel
 */
void FunctionGeneratorDialog::DoChannel(size_t i)
{
	auto chname = m_generator->GetChannel(i)->GetDisplayName();

	float valueWidth = 200;

	Unit pct(Unit::UNIT_PERCENT);
	Unit hz(Unit::UNIT_HZ);
	Unit volts(Unit::UNIT_VOLTS);
	Unit fs(Unit::UNIT_FS);

	if(ImGui::CollapsingHeader(chname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		//Check for updates (value changed instrument side since last commit)
		//TODO: move to background thread? or just rely on clientside caching to make it fast?
		auto freq = m_generator->GetFunctionChannelFrequency(i);
		if(freq != m_uiState[i].m_committedFrequency)
		{
			m_uiState[i].m_committedFrequency = freq;
			m_uiState[i].m_frequency = hz.PrettyPrint(freq);
		}

		ImGui::PushID(chname.c_str());

		if(ImGui::Checkbox("Output Enable", &m_uiState[i].m_outputEnabled))
			m_generator->SetFunctionChannelActive(i, m_uiState[i].m_outputEnabled);
		HelpMarker("Turns the output signal from this channel on or off");

		if(m_generator->HasFunctionImpedanceControls(i))
		{
			ImGui::SetNextItemWidth(valueWidth);
			if(Combo("Output Impedance", m_impedanceNames, m_uiState[i].m_impedanceIndex))
			{
				auto& state = m_uiState[i];
				m_generator->SetFunctionChannelOutputImpedance(i, m_impedances[state.m_impedanceIndex]);

				//Refresh amplitude and offset when changing impedance
				state.m_committedAmplitude = m_generator->GetFunctionChannelAmplitude(i);
				state.m_amplitude = volts.PrettyPrint(state.m_committedAmplitude);
				state.m_committedOffset = m_generator->GetFunctionChannelOffset(i);
				state.m_offset = volts.PrettyPrint(state.m_committedOffset);
			}
			HelpMarker(
				"Select the expected load impedance.\n\n"
				"If set incorrectly, amplitude and offset will be inaccurate due to reflections.");
		}

		//Amplitude and offset are potentially damaging operations
		//Require the user to explicitly commit changes before they take effect
		ImGui::SetNextItemWidth(valueWidth);
		if(UnitInputWithExplicitApply("Amplitude", m_uiState[i].m_amplitude, m_uiState[i].m_committedAmplitude, volts))
			m_generator->SetFunctionChannelAmplitude(i, m_uiState[i].m_committedAmplitude);
		HelpMarker("Peak-to-peak amplitude of the generated waveform");

		ImGui::SetNextItemWidth(valueWidth);
		if(UnitInputWithExplicitApply("Offset", m_uiState[i].m_offset, m_uiState[i].m_committedOffset, volts))
			m_generator->SetFunctionChannelOffset(i, m_uiState[i].m_committedOffset);
		HelpMarker("DC offset for the waveform above (positive) or below (negative) ground");

		//All other settings apply when user presses enter or focus is lost
		ImGui::SetNextItemWidth(valueWidth);
		if(Combo("Waveform", m_uiState[i].m_waveShapeNames, m_uiState[i].m_shapeIndex))
			m_generator->SetFunctionChannelShape(i, m_uiState[i].m_waveShapes[m_uiState[i].m_shapeIndex]);
		HelpMarker("Select the type of waveform to generate");

		ImGui::SetNextItemWidth(valueWidth);
		if(UnitInputWithImplicitApply("Frequency", m_uiState[i].m_frequency, m_uiState[i].m_committedFrequency, hz))
			m_generator->SetFunctionChannelFrequency(i, m_uiState[i].m_committedFrequency);

		//Duty cycle controls are not available in all generators
		if(m_generator->HasFunctionDutyCycleControls(i))
		{
			auto waveformType = m_uiState[i].m_waveShapes[m_uiState[i].m_shapeIndex];
			bool hasDutyCycle = false;
			switch(waveformType)
			{
				case FunctionGenerator::SHAPE_PULSE:
				case FunctionGenerator::SHAPE_SQUARE:
				case FunctionGenerator::SHAPE_PRBS_NONSTANDARD:
					hasDutyCycle = true;
					break;

				default:
					hasDutyCycle = false;
			}
			ImGui::SetNextItemWidth(valueWidth);
			if(!hasDutyCycle)
				ImGui::BeginDisabled();
			if(UnitInputWithImplicitApply("Duty Cycle", m_uiState[i].m_dutyCycle, m_uiState[i].m_committedDutyCycle, pct))
				m_generator->SetFunctionChannelDutyCycle(i, m_uiState[i].m_committedDutyCycle);
			if(!hasDutyCycle)
				ImGui::EndDisabled();
			HelpMarker("Duty cycle of the waveform, in percent. Not applicable to all waveform types.");
		}

		//Rise and fall time controls are not present in all generators
		//TODO: not all waveforms make sense to have rise/fall times etiher
		if(m_generator->HasFunctionRiseFallTimeControls(i))
		{
			ImGui::SetNextItemWidth(valueWidth);
			if(UnitInputWithImplicitApply("Rise Time", m_uiState[i].m_riseTime, m_uiState[i].m_committedRiseTime, fs))
				m_generator->SetFunctionChannelRiseTime(i, m_uiState[i].m_committedRiseTime);

			ImGui::SetNextItemWidth(valueWidth);
			if(UnitInputWithImplicitApply("Fall Time", m_uiState[i].m_fallTime, m_uiState[i].m_committedFallTime, fs))
				m_generator->SetFunctionChannelFallTime(i, m_uiState[i].m_committedFallTime);
		}

		ImGui::PopID();
	}

	//Push config for dedicated generators
	if(dynamic_pointer_cast<Oscilloscope>(m_generator) == nullptr)
		m_generator->GetTransport()->FlushCommandQueue();
}
