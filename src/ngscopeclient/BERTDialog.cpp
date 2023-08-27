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
	@brief Implementation of BERTDialog
 */

#include "ngscopeclient.h"
#include "BERTDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

BERTDialog::BERTDialog(SCPIBERT* bert, shared_ptr<BERTState> state, Session* session)
	: Dialog(
		string("BERT: ") + bert->m_nickname,
		string("BERT: ") + bert->m_nickname,
		ImVec2(500, 400))
	, m_session(session)
	, m_tstart(GetTime())
	, m_bert(bert)
	, m_state(state)
{
	//Create UI state for each channel
	for(size_t i=0; i<m_bert->GetChannelCount(); i++)
	{
		m_channelNames.push_back(m_bert->GetChannel(i)->GetDisplayName());
		m_channelUIState.push_back(BERTChannelUIState(bert, i));
	}
}

BERTDialog::~BERTDialog()
{
	m_session->RemoveBERT(m_bert);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool BERTDialog::DoRender()
{
	//Device information
	if(ImGui::CollapsingHeader("Info"))
	{
		ImGui::BeginDisabled();

			auto name = m_bert->GetName();
			auto vendor = m_bert->GetVendor();
			auto serial = m_bert->GetSerial();
			auto driver = m_bert->GetDriverName();
			auto transport = m_bert->GetTransport();
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

	//Timebase settings
	if(ImGui::CollapsingHeader("Timebase"))
	{

	}

	//Channel information
	for(size_t i=0; i<m_bert->GetChannelCount(); i++)
	{
		//Skip non-load channels
		if( (m_bert->GetInstrumentTypesForChannel(i) & Instrument::INST_BERT) == 0)
			continue;

		if(dynamic_cast<BERTInputChannel*>(m_bert->GetChannel(i)))
		{
			if(ImGui::CollapsingHeader(m_channelNames[i].c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushID(m_channelNames[i].c_str());
					RxChannelSettings(i);
				ImGui::PopID();
			}
		}
	}

	return true;
}

void BERTDialog::RxChannelSettings(size_t channel)
{
	auto& uistate = m_channelUIState[channel];

	float valueWidth = 150;
	ImGui::SetNextItemWidth(valueWidth);
	if(Dialog::Combo("Pattern", uistate.m_patternNames, uistate.m_patternIndex))
		m_bert->SetRxPattern(channel, uistate.m_patternValues[uistate.m_patternIndex]);

	ImGui::SetNextItemWidth(valueWidth);
	if(ImGui::Checkbox("Invert", &m_channelUIState[channel].m_invert))
		m_bert->SetRxInvert(channel, m_channelUIState[channel].m_invert);
}
