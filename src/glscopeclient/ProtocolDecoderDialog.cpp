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
	@brief Implementation of ProtocolDecoderDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "ProtocolDecoderDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolDecoderDialog::ProtocolDecoderDialog(
	OscilloscopeWindow* parent,
	ProtocolDecoder* decoder,
	OscilloscopeChannel* chan)
	: Gtk::Dialog(decoder->GetProtocolDisplayName(), *parent, Gtk::DIALOG_MODAL)
	, m_decoder(decoder)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	//hide close button to force user to pick OK or cancel
	set_deletable(false);

	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);
		m_grid.attach(m_channelDisplayNameLabel, 0, 0, 1, 1);
			m_channelDisplayNameLabel.set_text("Display name");
		m_grid.attach_next_to(m_channelDisplayNameEntry, m_channelDisplayNameLabel, Gtk::POS_RIGHT, 1, 1);
			m_channelDisplayNameLabel.set_halign(Gtk::ALIGN_START);
			m_channelDisplayNameEntry.set_text(decoder->m_displayname);

		m_grid.attach_next_to(m_channelColorLabel, m_channelDisplayNameLabel, Gtk::POS_BOTTOM, 1, 1);
			m_channelColorLabel.set_text("Waveform color");
			m_channelColorLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_channelColorButton, m_channelColorLabel, Gtk::POS_RIGHT, 1, 1);
			m_channelColorButton.set_color(Gdk::Color(decoder->m_displaycolor));

	Gtk::Widget* last_label = &m_channelColorLabel;
	for(size_t i=0; i<decoder->GetInputCount(); i++)
	{
		//Add the row
		auto row = new ChannelSelectorRow;
		m_grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
		m_grid.attach_next_to(row->m_chans, row->m_label, Gtk::POS_RIGHT, 1, 1);
		m_rows.push_back(row);
		last_label = &row->m_label;

		//Label is just the channel name
		row->m_label.set_label(decoder->GetInputName(i));

		//always allow not connecting an input
		row->m_chans.append("NULL");
		row->m_chanptrs["NULL"] = NULL;

		//Handle null inputs
		OscilloscopeChannel* din = decoder->GetInput(i);
		if(din == NULL)
			row->m_chans.set_active_text("NULL");

		//Fill the channel list with all channels that are legal to use here
		for(size_t j=0; j<parent->GetScopeCount(); j++)
		{
			Oscilloscope* scope = parent->GetScope(j);
			for(size_t k=0; k<scope->GetChannelCount(); k++)
			{
				auto c = scope->GetChannel(k);
				if(decoder->ValidateChannel(i, c))
				{
					row->m_chans.append(c->m_displayname);
					row->m_chanptrs[c->m_displayname] = c;
					if( (c == chan && i==0) || (c == din) )
						row->m_chans.set_active_text(c->m_displayname);
				}
			}
		}

		//Add protocol decoders
		auto decodes = ProtocolDecoder::EnumDecodes();
		for(auto d : decodes)
		{
			if(decoder->ValidateChannel(i, d))
			{
				row->m_chans.append(d->m_displayname);
				row->m_chanptrs[d->m_displayname] = d;
				if( (d == chan && i==0) || (d == din) )
					row->m_chans.set_active_text(d->m_displayname);
			}
		}
	}

	//Add parameters
	for(auto it = decoder->GetParamBegin(); it != decoder->GetParamEnd(); it ++)
	{
		auto row = new ParameterRow;
		m_grid.attach_next_to(row->m_label, *last_label, Gtk::POS_BOTTOM, 1, 1);
		m_grid.attach_next_to(row->m_entry, row->m_label, Gtk::POS_RIGHT, 1, 1);
		last_label = &row->m_label;
		m_prows.push_back(row);

		row->m_label.set_label(it->first);

		//Set initial value
		row->m_entry.set_text(it->second.ToString());
	}
	show_all();
}

ProtocolDecoderDialog::~ProtocolDecoderDialog()
{
	for(auto r : m_rows)
		delete r;
	for(auto r : m_prows)
		delete r;
	m_rows.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void ProtocolDecoderDialog::ConfigureDecoder()
{
	//See if we're using the default name
	string old_name = m_decoder->m_displayname;
	bool default_name = (m_decoder->GetHwname() == old_name);

	for(size_t i=0; i<m_rows.size(); i++)
	{
		auto chname = m_rows[i]->m_chans.get_active_text();
		m_decoder->SetInput(i, m_rows[i]->m_chanptrs[chname]);
	}

	for(size_t i=0; i<m_prows.size(); i++)
	{
		m_decoder->GetParameter(m_prows[i]->m_label.get_label()).ParseString(
			m_prows[i]->m_entry.get_text());
	}

	m_decoder->m_displaycolor = m_channelColorButton.get_color().to_string();

	//Set the name of the decoder based on the input channels etc.
	//If the user specified a new name, use that.
	//But if they left the old autogenerated name, update appropriately.
	m_decoder->SetDefaultName();
	auto dname = m_channelDisplayNameEntry.get_text();
	if( (dname != "") && (!default_name || (old_name != dname)) )
		m_decoder->m_displayname = dname;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
