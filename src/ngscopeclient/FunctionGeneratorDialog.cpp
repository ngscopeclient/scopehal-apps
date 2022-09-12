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
	@brief Implementation of FunctionGeneratorDialog
 */

#include "ngscopeclient.h"
#include "FunctionGeneratorDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FunctionGeneratorDialog::FunctionGeneratorDialog(SCPIFunctionGenerator* generator, Session* session)
	: Dialog(string("Function Generator: ") + generator->m_nickname, ImVec2(500, 400))
	, m_session(session)
	, m_generator(generator)
{
	Unit hz(Unit::UNIT_HZ);

	for(int i=0; i<m_generator->GetFunctionChannelCount(); i++)
	{
		FunctionGeneratorChannelUIState state;
		state.m_outputEnabled = m_generator->GetFunctionChannelActive(i);

		state.m_amplitude = m_generator->GetFunctionChannelAmplitude(i);
		state.m_committedAmplitude = state.m_amplitude;

		state.m_offset = m_generator->GetFunctionChannelOffset(i);
		state.m_committedOffset = state.m_offset;

		state.m_dutyCycle = m_generator->GetFunctionChannelDutyCycle(i);

		state.m_frequency = hz.PrettyPrint(m_generator->GetFunctionChannelFrequency(i));
		state.m_committedFrequency = state.m_frequency;

		m_uiState.push_back(state);
	}
}

FunctionGeneratorDialog::~FunctionGeneratorDialog()
{
	m_session->RemoveFunctionGenerator(m_generator);
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

	for(int i=0; i<m_generator->GetFunctionChannelCount(); i++)
		DoChannel(i);

	return true;
}

/**
	@brief Run the UI for a single channel
 */
void FunctionGeneratorDialog::DoChannel(int i)
{
	auto chname = m_generator->GetFunctionChannelName(i);

	float valueWidth = 100;

	if(ImGui::CollapsingHeader(chname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID(chname.c_str());

		if(ImGui::Checkbox("Output Enable", &m_uiState[i].m_outputEnabled))
			m_generator->SetFunctionChannelActive(i, m_uiState[i].m_outputEnabled);

		//Amplitude and offset are potentially damaging operations
		//Require the user to commit changes before they take effect
		ImGui::SetNextItemWidth(valueWidth);
		if(FloatInputWithApplyButton("Amplitude", m_uiState[i].m_amplitude, m_uiState[i].m_committedAmplitude))
			m_generator->SetFunctionChannelAmplitude(i, m_uiState[i].m_amplitude);

		ImGui::SetNextItemWidth(valueWidth);
		if(FloatInputWithApplyButton("Offset", m_uiState[i].m_offset, m_uiState[i].m_committedOffset))
			m_generator->SetFunctionChannelOffset(i, m_uiState[i].m_offset);

		ImGui::SetNextItemWidth(valueWidth);
		if(ImGui::InputFloat("Duty Cycle", &m_uiState[i].m_dutyCycle))
			m_generator->SetFunctionChannelDutyCycle(i, m_uiState[i].m_dutyCycle);

		//Frequency needs some extra logic for unit conversion
		ImGui::SetNextItemWidth(valueWidth);
		if(TextInputWithApplyButton("Frequency", m_uiState[i].m_frequency, m_uiState[i].m_committedFrequency))
		{
			Unit hz(Unit::UNIT_HZ);
			float f = hz.ParseString(m_uiState[i].m_frequency);
			m_generator->SetFunctionChannelFrequency(i, f);
			m_uiState[i].m_frequency = hz.PrettyPrint(f);
			m_uiState[i].m_committedFrequency = m_uiState[i].m_frequency;
		}

		ImGui::PopID();
	}

	//Push config for dedicated generators
	if(dynamic_cast<Oscilloscope*>(m_generator) == nullptr)
		m_generator->GetTransport()->FlushCommandQueue();
}
