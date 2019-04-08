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
	@brief  Implementation of ProtocolAnalyzerWindow
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "ProtocolAnalyzerWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolAnalyzerColumns

ProtocolAnalyzerColumns::ProtocolAnalyzerColumns(PacketDecoder* decoder)
{
	add(m_timestamp);

	auto headers = decoder->GetHeaders();
	for(size_t i=0; i<headers.size(); i++)
	{
		m_headers.push_back(Gtk::TreeModelColumn<Glib::ustring>());
		add(m_headers[m_headers.size()-1]);
	}

	add(m_data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolAnalyzerWindow::ProtocolAnalyzerWindow(string title, OscilloscopeWindow* parent, PacketDecoder* decoder)
	: Gtk::Dialog(title, *parent, false)
	, m_decoder(decoder)
	, m_columns(decoder)
{
	m_decoder->AddRef();

	m_startTime = 0;

	set_size_request(1024, 600);

	//Set up the tree view
	m_model = Gtk::TreeStore::create(m_columns);
	m_tree.set_model(m_model);

	//Add the columns
	m_tree.append_column("Time", m_columns.m_timestamp);
	auto headers = decoder->GetHeaders();
	for(size_t i=0; i<headers.size(); i++)
		m_tree.append_column(headers[i], m_columns.m_headers[i]);
	m_tree.append_column("Data", m_columns.m_data);

	//Set up the widgets
	get_vbox()->pack_start(m_scroller, Gtk::PACK_EXPAND_WIDGET);
		m_scroller.add(m_tree);
		m_scroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	show_all();
}

ProtocolAnalyzerWindow::~ProtocolAnalyzerWindow()
{
	m_decoder->Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ProtocolAnalyzerWindow::OnWaveformDataReady()
{
	auto packets = m_decoder->GetPackets();
	if(packets.empty())
		return;

	auto headers = m_decoder->GetHeaders();

	for(auto p : packets)
	{
		//If this is our first packet, use it as the zero reference for time later on
		if(m_model->children().empty())
			m_startTime = p->m_start;

		//Convert timestamp to relative.
		//Note that LeCroy scopes seem to wrap the timestamp every minute.
		//If we see a negative time, add a minute as a workaround. This will give incorrect results if
		//more than a minute has passed since the last packet was seen.
		//TODO: use PC clock to correct for that
		double reltime = p->m_start - m_startTime;
		if(reltime < 0)
		{
			reltime += 60;
			m_startTime -= 60;
		}

		//Format timestamp
		auto row = *m_model->append();
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%.10f", reltime);
		row[m_columns.m_timestamp] = tmp;

		//Just copy headers without any processing
		for(size_t i=0; i<headers.size(); i++)
			row[m_columns.m_headers[i]] = p->m_headers[headers[i]];

		//Convert data to hex
		string sdata;
		for(auto b : p->m_data)
		{
			char t[4];
			snprintf(t, sizeof(t), "%02x ", b);
			sdata += t;
		}
		row[m_columns.m_data] = sdata;
	}

	//auto scroll to bottom
	auto adj = m_scroller.get_vadjustment();
	adj->set_value(adj->get_upper());
}
