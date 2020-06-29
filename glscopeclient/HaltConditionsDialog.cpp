/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of HaltConditionsDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "HaltConditionsDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HaltConditionsDialog::HaltConditionsDialog(
	OscilloscopeWindow* parent)
	: Gtk::Dialog("Halt Conditions", *parent)
	, m_parent(parent)
{
	char buf[128];

	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);
		m_grid.attach(m_haltEnabledButton, 0, 0, 1, 1);
			m_haltEnabledButton.set_label("Halt Enabled");

		m_grid.attach_next_to(m_channelNameLabel, m_haltEnabledButton, Gtk::POS_BOTTOM, 1, 1);
			m_channelNameLabel.set_label("Halt when");
		m_grid.attach_next_to(m_channelNameBox, m_channelNameLabel, Gtk::POS_RIGHT, 1, 1);
		m_grid.attach_next_to(m_operatorBox, m_channelNameBox, Gtk::POS_RIGHT, 1, 1);
			m_operatorBox.append("==");
			m_operatorBox.append("!=");
		m_grid.attach_next_to(m_targetEntry, m_operatorBox, Gtk::POS_RIGHT, 1, 1);

	show_all();
}

HaltConditionsDialog::~HaltConditionsDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void HaltConditionsDialog::RefreshChannels()
{
	//TODO: save previous state

	m_channelNameBox.remove_all();
	m_chanptrs.clear();

	//Populate channel list
	/*
	for(size_t j=0; j<m_parent->GetScopeCount(); j++)
	{
		auto scope = m_parent->GetScope(j);
		for(size_t k=0; k<scope->GetChannelCount(); k++)
		{
			auto c = scope->GetChannel(k);

			m_channelNameBox.append(c->m_displayname);
			m_chanptrs[c->m_displayname] = c;
		}
	}
	*/

	//For now, only allow conditional triggering on complex decodes
	auto decodes = ProtocolDecoder::EnumDecodes();
	for(auto d : decodes)
	{
		if(d->GetType() != OscilloscopeChannel::CHANNEL_TYPE_COMPLEX)
			continue;
		m_channelNameBox.append(d->m_displayname);
		m_chanptrs[d->m_displayname] = d;
	}
}

/**
	@brief Check if we should halt the trigger
 */
bool HaltConditionsDialog::ShouldHalt()
{
	//If conditional halt is not enabled, no sense checking conditions
	if(!m_haltEnabledButton.get_active())
		return false;

	//Get the channel we're looking at
	auto chan = m_chanptrs[m_channelNameBox.get_active_text()];
	auto decode = dynamic_cast<ProtocolDecoder*>(chan);
	if(decode == NULL)
		return false;

	//Don't check if no data to look at
	auto data = decode->GetData();
	if(data->m_offsets.empty())
		return false;

	//TODO: support more than just == / !=
	bool match_equal = (m_operatorBox.get_active_text() == "==");

	//Loop over the decode and see if anything matches
	size_t len = data->m_offsets.size();
	auto text = m_targetEntry.get_text();
	for(size_t i=0; i<len; i++)
	{
		auto target = decode->GetText(i);
		if(match_equal)
		{
			if(target == text)
				return true;
		}
		else
		{
			if(target != text)
				return true;
		}
	}

	return false;
}
