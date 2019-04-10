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
	show_all();
}

HistoryWindow::~HistoryWindow()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

bool HistoryWindow::on_delete_event(GdkEventAny* /*ignored*/)
{
	m_parent->HideHistory();
	return true;
}
