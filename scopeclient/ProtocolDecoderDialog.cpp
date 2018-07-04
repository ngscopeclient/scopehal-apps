/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2017 Andrew D. Zonenberg                                                                          *
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
#include "scopeclient.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/Multimeter.h"
#include "ProtocolDecoderDialog.h"

/*
#include "../scopeprotocols/RPCDecoder.h"
#include "../scopeprotocols/RPCNameserverDecoder.h"
#include "../scopeprotocols/SchmittTriggerDecoder.h"
#include "../scopeprotocols/UARTDecoder.h"
#include "../scopehal/StateDecoder.h"
*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolDecoderDialog::ProtocolDecoderDialog(MainWindow* /*parent*/, Oscilloscope* scope/*, NameServer& namesrvr*/)
	: Gtk::Dialog(Glib::ustring("Protocol decode"), true)
	, m_scope(scope)
	//, m_namesrvr(namesrvr)
{
	set_size_request(480, 240);
	set_title("Protocol decode");

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	get_vbox()->pack_start(m_decoderbox, Gtk::PACK_SHRINK);
		m_decoderbox.pack_start(m_decoderlabel, Gtk::PACK_SHRINK);
			m_decoderlabel.set_text("Protocol");
			m_decoderlabel.set_width_chars(16);
			m_decoderlabel.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
		m_decoderbox.pack_start(m_decoderlist);
			/*
			std::vector<std::string> protocols;
			ProtocolDecoder::EnumProtocols(protocols);
			for(size_t i=0; i<protocols.size(); i++)
				m_decoderlist.append(protocols[i]);
			m_decoderlist.set_active(-1);
			*/
	get_vbox()->pack_start(m_namebox, Gtk::PACK_SHRINK);
		m_namebox.pack_start(m_namelabel, Gtk::PACK_SHRINK);
			m_namelabel.set_text("Label");
			m_namelabel.set_width_chars(16);
			m_namelabel.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
		m_namebox.pack_start(m_nameentry);
			m_nameentry.set_text("ProtocolDecoder");
	get_vbox()->pack_start(m_hsep, Gtk::PACK_SHRINK);
	get_vbox()->pack_start(m_body);
	get_vbox()->pack_start(m_hsep2, Gtk::PACK_SHRINK);
	get_vbox()->pack_start(m_parambody);

	m_decoderlist.signal_changed().connect(sigc::mem_fun(*this, &ProtocolDecoderDialog::OnDecoderSelected));

	show_all();

	//m_decoder = NULL;
}

ProtocolDecoderDialog::~ProtocolDecoderDialog()
{
	ClearBodyRows();
	/*
	if(m_decoder)
	{
		delete m_decoder;
		m_decoder = NULL;
	}
	*/
}

/**
	@brief Clears all of the per-decoder GUI data out in preparation for destruction or a new decoder
 */
void ProtocolDecoderDialog::ClearBodyRows()
{
	for(size_t i=0; i<m_bodyrows.size(); i++)
	{
		m_body.remove(m_bodyrows[i]->m_box);
		delete m_bodyrows[i];
	}
	m_bodyrows.clear();

	for(size_t i=0; i<m_paramrows.size(); i++)
	{
		m_parambody.remove(m_paramrows[i]->m_box);
		delete m_paramrows[i];
	}
	m_paramrows.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Detaches the current decoder (if any) from this instance and returns a pointer to it
 */
/*
ProtocolDecoder* ProtocolDecoderDialog::Detach()
{
	FillSignals();

	ProtocolDecoder* ret = m_decoder;
	ret->m_displayname = m_nameentry.get_text();
	m_decoder = NULL;

	ClearBodyRows();
	return ret;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

void ProtocolDecoderDialog::OnDecoderSelected()
{
	/*
	//Clear out the old decoder
	if(m_decoder)
	{
		delete m_decoder;
		m_decoder = NULL;
	}
	ClearBodyRows();

	//TODO: Serial number or something for unique naming?

	//Create the new decoder
	m_decoder = ProtocolDecoder::CreateDecoder(
		m_decoderlist.get_active_text(),
		"ProtocolDecoder", //TODO: hardware name?
		"#a0ffff",
		m_namesrvr);

	//Enumerate the list of signals it expects
	if(m_decoder != NULL)
	{
		for(size_t i=0; i<m_decoder->GetInputCount(); i++)
		{
			//Create the GUI row
			ProtocolDecoderGuiRow* row = new ProtocolDecoderGuiRow;
			row->m_label.set_text(m_decoder->GetInputName(i));

			//TODO: Add callback for dropdown selects
			//TODO: need a way to enumerate signals by name

			//Enumerate all signals and add those passing the filter
			for(size_t j=0; j<m_scope->GetChannelCount(); j++)
			{
				OscilloscopeChannel* chan = m_scope->GetChannel(j);
				if(m_decoder->ValidateChannel(i, chan))
					row->m_cbox.append(chan->m_displayname);
			}
			row->m_cbox.signal_changed().connect(sigc::mem_fun(*this, &ProtocolDecoderDialog::OnInputSelected));

			//Add to list
			m_bodyrows.push_back(row);
			m_body.pack_start(row->m_box, Gtk::PACK_SHRINK);
		}

		for(ProtocolDecoder::ParameterMapType::iterator it=m_decoder->GetParamBegin(); it != m_decoder->GetParamEnd(); ++it)
		{
			std::string name = it->first;
			ProtocolDecoderParameter& param = it->second;

			//Create the GUI row
			ProtocolDecoderGuiRowEntry* row = new ProtocolDecoderGuiRowEntry;
			row->m_label.set_markup(name);
			row->m_label.set_use_markup();
			row->m_entry.set_text(param.ToString());
			row->m_entry.signal_changed().connect(sigc::mem_fun(*this, &ProtocolDecoderDialog::OnInputSelected));

			//Add to list
			m_paramrows.push_back(row);
			m_parambody.pack_start(row->m_box, Gtk::PACK_SHRINK);
		}
	}
	*/
	show_all();
}

void ProtocolDecoderDialog::OnInputSelected()
{
	//We don't know which input box changed
	//Just redo all of them
	FillSignals();
}

void ProtocolDecoderDialog::FillSignals()
{
	/*
	try
	{
		//Make a map of signal names
		std::map<std::string, OscilloscopeChannel*> sigmap;
		for(size_t i=0; i<m_scope->GetChannelCount(); i++)
		{
			OscilloscopeChannel* chan = m_scope->GetChannel(i);

			//If it's a state decoder, pull its first input instead
			StateDecoder* dec = dynamic_cast<StateDecoder*>(chan);
			if(dec != NULL)
				chan = dec->GetInput(0);

			sigmap[chan->m_displayname] = chan;
		}

		//Hook up the inputs by name
		for(size_t i=0; i<m_bodyrows.size(); i++)
		{
			std::string sig = m_bodyrows[i]->m_cbox.get_active_text();
			if(sigmap.find(sig) == sigmap.end())
				m_decoder->SetInput(i, NULL);
			else
			{
				m_decoder->SetInput(i, sigmap[sig]);
				m_decoder->m_timescale = sigmap[sig]->m_timescale;
			}
		}

		//Hook up to parameters
		for(size_t i=0; i<m_paramrows.size(); i++)
		{
			std::string name = m_paramrows[i]->m_label.get_label();
			ProtocolDecoderParameter& param = m_decoder->GetParameter(name);
			std::string value = m_paramrows[i]->m_entry.get_text();
			param.ParseString(value);
		}
	}
	catch(const JtagException& ex)
	{
		printf("%s\n", ex.GetDescription().c_str());
		//exit(1);
	}
	*/
}
