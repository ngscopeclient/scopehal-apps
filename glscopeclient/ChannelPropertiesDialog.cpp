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
	@brief Implementation of ChannelPropertiesDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "ChannelPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ChannelPropertiesDialog::ChannelPropertiesDialog(
	OscilloscopeWindow* parent,
	OscilloscopeChannel* chan)
	: Gtk::Dialog(string("Channel properties"), *parent, Gtk::DIALOG_MODAL)
	, m_chan(chan)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	char buf[128];

	get_vbox()->pack_start(m_scopeNameBox, Gtk::PACK_SHRINK);
		m_scopeNameBox.pack_start(m_scopeNameLabel, Gtk::PACK_SHRINK);
		m_scopeNameLabel.set_text("Scope");
		m_scopeNameBox.pack_start(m_scopeNameEntry, Gtk::PACK_EXPAND_WIDGET);
		m_scopeNameLabel.set_size_request(150, 1);
		m_scopeNameLabel.set_halign(Gtk::ALIGN_START);
		m_scopeNameEntry.set_halign(Gtk::ALIGN_START);
		snprintf(buf, sizeof(buf), "%s (%s, serial %s)",
			chan->GetScope()->m_nickname.c_str(),
			chan->GetScope()->GetName().c_str(),
			chan->GetScope()->GetSerial().c_str());
		m_scopeNameEntry.set_text(buf);

	get_vbox()->pack_start(m_channelNameBox, Gtk::PACK_SHRINK);
		m_channelNameBox.pack_start(m_channelNameLabel, Gtk::PACK_SHRINK);
		m_channelNameLabel.set_text("Channel");
		m_channelNameBox.pack_start(m_channelNameEntry, Gtk::PACK_EXPAND_WIDGET);
		m_channelNameLabel.set_size_request(150, 1);
		m_channelNameLabel.set_halign(Gtk::ALIGN_START);
		m_channelNameEntry.set_text(chan->GetHwname());
		m_channelNameEntry.set_halign(Gtk::ALIGN_START);

	get_vbox()->pack_start(m_channelDisplayNameBox, Gtk::PACK_SHRINK);
			m_channelDisplayNameBox.pack_start(m_channelDisplayNameLabel, Gtk::PACK_SHRINK);
			m_channelDisplayNameLabel.set_text("Display name");
			m_channelDisplayNameBox.pack_start(m_channelDisplayNameEntry, Gtk::PACK_EXPAND_WIDGET);
			m_channelDisplayNameLabel.set_size_request(150, 1);
			m_channelDisplayNameLabel.set_halign(Gtk::ALIGN_START);
			m_channelDisplayNameEntry.set_text(chan->m_displayname);

	show_all();
}

ChannelPropertiesDialog::~ChannelPropertiesDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void ChannelPropertiesDialog::ConfigureChannel()
{
	m_chan->m_displayname = m_channelDisplayNameEntry.get_text();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
