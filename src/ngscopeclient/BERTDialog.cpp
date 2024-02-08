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

#include <cinttypes>

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
	RefreshFromHardware();
}

BERTDialog::~BERTDialog()
{
	m_session->RemoveBERT(m_bert);
}

void BERTDialog::RefreshFromHardware()
{
	m_txPattern = m_bert->GetGlobalCustomPattern();
	m_txPatternText = to_string_hex(m_txPattern);

	Unit sa(Unit::UNIT_SAMPLEDEPTH);
	m_integrationLength = m_bert->GetBERIntegrationLength();
	m_committedIntegrationLength = m_integrationLength;
	m_integrationLengthText = sa.PrettyPrint(m_integrationLength);

	//Transmit pattern
	m_refclkIndex = m_bert->GetRefclkOutMux();
	m_refclkNames = m_bert->GetRefclkOutMuxNames();

	auto currentRate = m_bert->GetDataRate();
	m_dataRateIndex = 0;
	m_dataRates = m_bert->GetAvailableDataRates();
	Unit bps(Unit::UNIT_BITRATE);
	m_dataRateNames.clear();
	for(size_t i=0; i<m_dataRates.size(); i++)
	{
		auto rate = m_dataRates[i];
		if(rate == currentRate)
			m_dataRateIndex = i;

		m_dataRateNames.push_back(bps.PrettyPrint(rate));
	}

	m_refclkFrequency = m_bert->GetRefclkOutFrequency();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool BERTDialog::DoRender()
{
	float width = 10 * ImGui::GetFontSize();

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

	//Global pattern generator settings
	if(!m_bert->IsCustomPatternPerChannel())
	{
		if(ImGui::CollapsingHeader("Pattern Generator"))
		{
			ImGui::SetNextItemWidth(width);
			if(ImGui::InputText("Custom Pattern", &m_txPatternText))
			{
				sscanf(m_txPatternText.c_str(), "%" PRIx64, &m_txPattern);
				m_bert->SetGlobalCustomPattern(m_txPattern);
				m_refclkFrequency = m_bert->GetRefclkOutFrequency();
			}

			HelpMarker(to_string(m_bert->GetCustomPatternLength()) +
				" -bit pattern sent by all channels in custom-pattern mode.\n"
				"\n"
				"Note that this includes the reference clock output on the ML4039, if\n"
				"configured in SERDES mode.");
		}
	}

	//Timebase settings
	if(ImGui::CollapsingHeader("Timebase"))
	{
		ImGui::SetNextItemWidth(width);
		if(Dialog::Combo("Clock Out", m_refclkNames, m_refclkIndex))
		{
			m_bert->SetRefclkOutMux(m_refclkIndex);

			//Need to refresh custom pattern here
			//because ML4039 sets this to 0xaaaa if we select SERDES mode on clock out
			m_txPattern = m_bert->GetGlobalCustomPattern();
			m_txPatternText = to_string_hex(m_txPattern);

			m_refclkFrequency = m_bert->GetRefclkOutFrequency();
		}
		HelpMarker("Select which clock to output from the reference clock output port");

		ImGui::SetNextItemWidth(width);
		ImGui::BeginDisabled();
		Unit hz(Unit::UNIT_HZ);
		string srate = hz.PrettyPrint(m_refclkFrequency);
		ImGui::InputText("Clock Out Frequency", &srate);
		ImGui::EndDisabled();
		HelpMarker("Calculated frequency of the reference clock output");

		ImGui::SetNextItemWidth(width);
		ImGui::BeginDisabled();
		srate = hz.PrettyPrint(m_bert->GetRefclkInFrequency());
		ImGui::InputText("Clock In Frequency", &srate);
		ImGui::EndDisabled();
		HelpMarker("Required frequency for external reference clock");

		ImGui::SetNextItemWidth(width);
		const char* items[2] =
		{
			"Internal",
			"External"
		};
		int iext = m_bert->GetUseExternalRefclk() ? 1 : 0;
		if(ImGui::Combo("Clock Source", &iext, items, 2))
			m_bert->SetUseExternalRefclk(iext == 1);

		ImGui::SetNextItemWidth(width);
		if(Dialog::Combo("Data Rate", m_dataRateNames, m_dataRateIndex))
		{
			m_bert->SetDataRate(m_dataRates[m_dataRateIndex]);

			//Reload refclk mux setting names
			m_refclkNames = m_bert->GetRefclkOutMuxNames();
			m_refclkFrequency = m_bert->GetRefclkOutFrequency();
		}
		HelpMarker("PHY signaling rate for all transmit and receive ports");

		ImGui::SetNextItemWidth(width);
		Unit sa(Unit::UNIT_SAMPLEDEPTH);
		if(UnitInputWithImplicitApply(
			"Integration Length",
			m_integrationLengthText,
			m_committedIntegrationLength,
			sa))
		{
			m_integrationLength = m_committedIntegrationLength;
			m_bert->SetBERIntegrationLength(m_integrationLength);
		}
		HelpMarker(
			"Number of UIs to sample for each BER measurement.\n\n"
			"Larger integration periods lead to slower update rates, but\n"
			"give better resolution at low BER values."
			);
	}

	return true;
}
