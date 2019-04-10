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
	@brief  Implementation of HistoryWindow
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "HistoryWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HistoryColumns

HistoryColumns::HistoryColumns()
{
	add(m_timestamp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HistoryWindow::HistoryWindow(OscilloscopeWindow* parent)
	: m_parent(parent)
	, m_updating(false)
{
	set_title("History");

	set_default_size(320, 800);

	//Set up the tree view
	m_model = Gtk::TreeStore::create(m_columns);
	m_tree.set_model(m_model);

	//Add the columns
	m_tree.append_column("Time", m_columns.m_timestamp);

	//Set up the widgets
	add(m_vbox);
		m_vbox.pack_start(m_hbox, Gtk::PACK_SHRINK);
			m_hbox.pack_start(m_maxLabel, Gtk::PACK_SHRINK);
				m_maxLabel.set_label("Max waveforms");
			m_hbox.pack_start(m_maxBox, Gtk::PACK_EXPAND_WIDGET);
				m_maxBox.set_text("1000");
		m_vbox.pack_start(m_scroller, Gtk::PACK_EXPAND_WIDGET);
			m_scroller.add(m_tree);
			m_scroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
				m_tree.get_selection()->set_mode(Gtk::SELECTION_BROWSE);
	m_vbox.show_all();

	//not shown by default
	hide();
}

HistoryWindow::~HistoryWindow()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void HistoryWindow::OnWaveformDataReady(Oscilloscope* scope)
{
	//Use the timestamp from the first enabled channel
	OscilloscopeChannel* chan = NULL;
	CaptureChannelBase* data = NULL;
	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		chan = scope->GetChannel(i);
		if(chan->IsEnabled())
		{
			data = chan->GetData();
			break;
		}
	}

	//No channels at all? Nothing to do
	if(chan == NULL)
		return;

	m_updating = true;

	//Format timestamp
	char tmp[128];
	strftime(tmp, sizeof(tmp), "%H:%M:%S.", localtime(&data->m_startTimestamp));
	string stime = tmp;
	snprintf(tmp, sizeof(tmp), "%010zu", data->m_startPicoseconds / 100);	//round to nearest 100ps for display
	stime += tmp;

	//Create the row
	auto row = *m_model->append();
	row[m_columns.m_timestamp] = stime;

	//TODO: add actual info to the row

	//auto scroll to bottom
	auto adj = m_scroller.get_vadjustment();
	adj->set_value(adj->get_upper());

	//Select the newly added row
	m_tree.get_selection()->select(row);

	//Remove extra waveforms, if we have any
	string smax = m_maxBox.get_text();
	size_t nmax = atoi(smax.c_str());
	auto children = m_model->children();
	while(children.size() > nmax)
	{
		//TODO: delete any saved waveforms etc
		m_model->erase(children.begin());
	}

	m_updating = false;
}

bool HistoryWindow::on_delete_event(GdkEventAny* /*ignored*/)
{
	m_parent->HideHistory();
	return true;
}
