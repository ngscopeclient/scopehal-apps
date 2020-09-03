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
	@brief  Implementation of ProtocolAnalyzerWindow
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "ProtocolAnalyzerWindow.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolAnalyzerColumns

ProtocolAnalyzerColumns::ProtocolAnalyzerColumns(PacketDecoder* decoder)
{
	add(m_timestamp);
	add(m_capturekey);
	add(m_offset);

	auto headers = decoder->GetHeaders();
	for(size_t i=0; i<headers.size(); i++)
	{
		m_headers.push_back(Gtk::TreeModelColumn<Glib::ustring>());
		add(m_headers[m_headers.size()-1]);
	}

	add(m_image);
	add(m_data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolAnalyzerWindow::ProtocolAnalyzerWindow(
	string title,
	OscilloscopeWindow* parent,
	PacketDecoder* decoder,
	WaveformArea* area)
	: Gtk::Dialog(title, *parent)
	, m_parent(parent)
	, m_decoder(decoder)
	, m_area(area)
	, m_columns(decoder)
	, m_updating(false)
{
	set_skip_taskbar_hint();
	set_type_hint(Gdk::WINDOW_TYPE_HINT_TOOLBAR);

	m_decoder->AddRef();

	set_default_size(1024, 600);

	//Set up the tree view
	m_model = Gtk::TreeStore::create(m_columns);
	m_tree.set_model(m_model);

	//Add the columns
	m_tree.append_column("Time", m_columns.m_timestamp);
	auto headers = decoder->GetHeaders();
	for(size_t i=0; i<headers.size(); i++)
		m_tree.append_column(headers[i], m_columns.m_headers[i]);

	if(decoder->GetShowImageColumn())
		m_tree.append_column("Image", m_columns.m_image);

	if(decoder->GetShowDataColumn())
		m_tree.append_column("Data", m_columns.m_data);

	m_tree.get_selection()->signal_changed().connect(
		sigc::mem_fun(*this, &ProtocolAnalyzerWindow::OnSelectionChanged));

	//Set up the widgets
	get_vbox()->pack_start(m_scroller, Gtk::PACK_EXPAND_WIDGET);
		m_scroller.add(m_tree);
			m_tree.get_selection()->set_mode(Gtk::SELECTION_BROWSE);
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
	auto data = m_decoder->GetData(0);
	auto packets = m_decoder->GetPackets();
	if(packets.empty())
		return;

	auto headers = m_decoder->GetHeaders();

	m_updating = true;

	for(auto p : packets)
	{
		//Need a bit of math in case the capture is >1 second long
		time_t capstart = data->m_startTimestamp;
		int64_t ps = data->m_startPicoseconds + p->m_offset;
		const int64_t seconds_per_ps = 1000ll * 1000ll * 1000ll * 1000ll;
		if(ps > seconds_per_ps)
		{
			capstart += (ps / seconds_per_ps);
			ps %= seconds_per_ps;
		}

		//Format timestamp
		char tmp[128];
		strftime(tmp, sizeof(tmp), "%H:%M:%S.", localtime(&capstart));
		string stime = tmp;
		snprintf(tmp, sizeof(tmp), "%010zu", ps / 100);	//round to nearest 100ps for display
		stime += tmp;

		//Create the row
		auto row = *m_model->append();
		row[m_columns.m_timestamp] = stime;
		row[m_columns.m_capturekey] = TimePoint(data->m_startTimestamp, data->m_startPicoseconds);
		row[m_columns.m_offset] = p->m_offset;

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

		//Add the image for video packets
		auto vp = dynamic_cast<VideoScanlinePacket*>(p);
		if(vp != NULL)
		{
			size_t rowsize = p->m_data.size();
			size_t width = rowsize / 3;
			size_t height = 24;

			Glib::RefPtr<Gdk::Pixbuf> image = Gdk::Pixbuf::create(
				Gdk::COLORSPACE_RGB,
				false,
				8,
				width,
				height);

			//Make a 2D image
			uint8_t* pixels = image->get_pixels();
			size_t bcount = rowsize * height;
			for(size_t y=0; y<height; y++)
				memcpy(pixels + y*rowsize, &p->m_data[0], rowsize);

			row[m_columns.m_image] = image;
		}

		//Select the newly added row
		m_tree.get_selection()->select(row);
	}

	//auto scroll to bottom
	auto adj = m_scroller.get_vadjustment();
	adj->set_value(adj->get_upper());

	m_updating = false;
}

void ProtocolAnalyzerWindow::OnSelectionChanged()
{
	//If we're updating with a new waveform we're already on the newest waveform.
	//No need to refresh anything.
	if(m_updating)
		return;

	auto sel = m_tree.get_selection();
	if(sel->count_selected_rows() == 0)
		return;
	auto row = *sel->get_selected();

	//Select the waveform
	m_parent->JumpToHistory(row[m_columns.m_capturekey]);

	//Set the offset of the decoder's group
	m_area->CenterTimestamp(row[m_columns.m_offset]);
	m_area->m_group->m_frame.queue_draw();
}

void ProtocolAnalyzerWindow::RemoveHistory(TimePoint timestamp)
{
	//This always happens from the start of time, so just remove from the beginning of our list
	//until we have nothing that matches.
	auto children = m_model->children();
	while(!children.empty())
	{
		//Stop if the timestamp is before our first point
		auto it = children.begin();
		TimePoint reftime = (*it)[m_columns.m_capturekey];
		if(timestamp.first < reftime.first)
			break;
		if( (timestamp.first == reftime.first) && (timestamp.second <= reftime.second) )
			break;

		//Remove it
		m_model->erase(it);
	}
}
