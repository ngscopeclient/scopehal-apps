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
	@brief Implementation of ChannelPropertiesDialog
 */

#include "ngscopeclient.h"
#include "ChannelPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ChannelPropertiesDialog::ChannelPropertiesDialog(OscilloscopeChannel* chan)
	: Dialog(string("Channel properties: ") + chan->GetHwname(), ImVec2(300, 400))
	, m_channel(chan)
{
	m_committedDisplayName = m_channel->GetDisplayName();
	m_displayName = m_committedDisplayName;

	//Vertical settings are per stream
	size_t nstreams = m_channel->GetStreamCount();
	m_committedOffset.resize(nstreams);
	m_offset.resize(nstreams);
	m_committedRange.resize(nstreams);
	m_range.resize(nstreams);
	for(size_t i = 0; i<nstreams; i++)
	{
		auto unit = m_channel->GetYAxisUnits(i);

		m_committedOffset[i] = chan->GetOffset(i);
		m_offset[i] = unit.PrettyPrint(m_committedOffset[i]);

		m_committedRange[i] = chan->GetVoltageRange(i);
		m_range[i] = unit.PrettyPrint(m_committedRange[i]);
	}

	//Hardware acquisition settings if this is a scope channel
	auto scope = m_channel->GetScope();
	if(scope)
	{
		auto nchan = m_channel->GetIndex();
		m_committedAttenuation = scope->GetChannelAttenuation(nchan);
		m_attenuation = to_string(m_committedAttenuation);

		m_coupling = 0;
		auto curCoup = m_channel->GetCoupling();
		m_couplings = scope->GetAvailableCouplings(nchan);
		for(size_t i=0; i<m_couplings.size(); i++)
		{
			auto c = m_couplings[i];

			switch(c)
			{
				case OscilloscopeChannel::COUPLE_DC_50:
					m_couplingNames.push_back("DC 50Ω");
					break;

				case OscilloscopeChannel::COUPLE_AC_50:
					m_couplingNames.push_back("AC 50Ω");
					break;

				case OscilloscopeChannel::COUPLE_DC_1M:
					m_couplingNames.push_back("DC 1MΩ");
					break;

				case OscilloscopeChannel::COUPLE_AC_1M:
					m_couplingNames.push_back("AC 1MΩ");
					break;

				case OscilloscopeChannel::COUPLE_GND:
					m_couplingNames.push_back("Ground");
					break;

				default:
					m_couplingNames.push_back("Invalid");
					break;
			}

			if(c == curCoup)
				m_coupling = i;
		}
	}
	else
	{
		m_committedAttenuation = 1;
		m_attenuation = "";
	}
}

ChannelPropertiesDialog::~ChannelPropertiesDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ChannelPropertiesDialog::DoRender()
{
	//TODO: handle stream count changing

	auto scope = m_channel->GetScope();
	if(ImGui::CollapsingHeader("Info"))
	{
		ImGui::BeginDisabled();

			//Scope info
			if(scope)
			{
				auto nickname = scope->m_nickname;
				auto index = to_string(m_channel->GetIndex());
				ImGui::InputText("Instrument", &nickname);
				ImGui::InputText("Channel", &index);
			}

			//TODO: filter info

		ImGui::EndDisabled();
	}

	if(ImGui::CollapsingHeader("Display"))
	{
		if(TextInputWithImplicitApply("Nickname", m_displayName, m_committedDisplayName))
			m_channel->SetDisplayName(m_committedDisplayName);

		//TODO: color
	}

	//Input settings only make sense if we have an attached scope
	auto nstreams = m_channel->GetStreamCount();
	if(scope)
	{
		if(ImGui::CollapsingHeader("Input"))
		{
			//Attenuation
			Unit counts(Unit::UNIT_COUNTS);
			if(UnitInputWithImplicitApply("Attenuation", m_attenuation, m_committedAttenuation, counts))
			{
				scope->SetChannelAttenuation(m_channel->GetIndex(), m_committedAttenuation);

				//Update offset and range when attenuation is changed
				for(size_t i = 0; i<nstreams; i++)
				{
					auto unit = m_channel->GetYAxisUnits(i);

					m_committedOffset[i] = m_channel->GetOffset(i);
					m_offset[i] = unit.PrettyPrint(m_committedOffset[i]);

					m_committedRange[i] = m_channel->GetVoltageRange(i);
					m_range[i] = unit.PrettyPrint(m_committedRange[i]);
				}
			}

			//Only show coupling box if the instrument has configurable coupling
			if(m_couplings.size() > 1)
			{
				if(Combo("Coupling", m_couplingNames, m_coupling))
					m_channel->SetCoupling(m_couplings[m_coupling]);
			}
		}
	}

	//Vertical settings are per stream
	for(size_t i = 0; i<nstreams; i++)
	{
		string streamname = "Vertical";
		if(nstreams > 1)
			streamname = m_channel->GetStreamName(i);

		if(ImGui::CollapsingHeader(streamname.c_str()))
		{
			auto unit = m_channel->GetYAxisUnits(i);

			//If no change to offset in dialog, update our input value when we change offset outside the dialog
			auto off = m_channel->GetOffset(i);
			auto soff = unit.PrettyPrint(m_committedOffset[i]);
			if( (m_committedOffset[i] != off) && (soff == m_offset[i]) )
			{
				m_offset[i] = unit.PrettyPrint(off);
				m_committedOffset[i] = off;
			}
			if(UnitInputWithExplicitApply("Offset", m_offset[i], m_committedOffset[i], unit))
				m_channel->SetOffset(m_committedOffset[i], i);

			//Same for range
			auto range = m_channel->GetVoltageRange(i);
			auto srange = unit.PrettyPrint(m_committedRange[i]);
			if( (m_committedRange[i] != range) && (srange == m_range[i]) )
			{
				m_range[i] = unit.PrettyPrint(range);
				m_committedRange[i] = range;
			}
			if(UnitInputWithExplicitApply("Range", m_range[i], m_committedRange[i], unit))
				m_channel->SetVoltageRange(m_committedRange[i], i);
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
