/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolDecoderDialog::ProtocolDecoderDialog(
	OscilloscopeWindow* parent,
	ProtocolDecoder* decoder,
	OscilloscopeChannel* chan)
	: Gtk::Dialog("Protocol Decode", *parent, Gtk::DIALOG_MODAL)
	, m_decoder(decoder)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	for(size_t i=0; i<decoder->GetInputCount(); i++)
	{
		//Add the row
		auto row = new ChannelSelectorRow;
		get_vbox()->pack_start(row->m_box, Gtk::PACK_SHRINK);
		m_rows.push_back(row);

		//Label is just the channel name
		row->m_label.set_label(decoder->GetInputName(i));

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
					if(c == chan)
						row->m_chans.set_active_text(c->m_displayname);
				}
			}
		}
	}
	show_all();
}

ProtocolDecoderDialog::~ProtocolDecoderDialog()
{
	for(auto r : m_rows)
		delete r;
	m_rows.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void ProtocolDecoderDialog::ConfigureDecoder()
{
	for(size_t i=0; i<m_rows.size(); i++)
	{
		auto chname = m_rows[i]->m_chans.get_active_text();
		m_decoder->SetInput(i, m_rows[i]->m_chanptrs[chname]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
