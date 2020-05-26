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
	@brief Implementation of InstrumentConnectionDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "InstrumentConnectionDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

InstrumentConnectionDialog::InstrumentConnectionDialog()
	: Gtk::Dialog("Connect To Instrument", Gtk::DIALOG_MODAL)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);

	m_grid.set_margin_left(10);
	m_grid.set_margin_right(10);
	m_grid.set_column_spacing(10);

	m_grid.attach(m_nicknameLabel, 0, 0, 1, 1);
		m_nicknameLabel.set_text("Nickname");
	m_grid.attach_next_to(m_nicknameEntry, m_nicknameLabel, Gtk::POS_RIGHT, 1, 1);

	m_grid.attach_next_to(m_driverLabel, m_nicknameLabel, Gtk::POS_BOTTOM, 1, 1);
		m_driverLabel.set_text("Driver");
	m_grid.attach_next_to(m_driverBox, m_driverLabel, Gtk::POS_RIGHT, 1, 1);

	vector<string> drivers;
	Oscilloscope::EnumDrivers(drivers);
	for(auto d : drivers)
		m_driverBox.append(d);

	m_grid.attach_next_to(m_transportLabel, m_driverLabel, Gtk::POS_BOTTOM, 1, 1);
		m_transportLabel.set_text("Transport");
	m_grid.attach_next_to(m_transportBox, m_transportLabel, Gtk::POS_RIGHT, 1, 1);

	vector<string> transports;
	SCPITransport::EnumTransports(transports);
	for(auto t : transports)
		m_transportBox.append(t);

	m_grid.attach_next_to(m_pathLabel, m_transportLabel, Gtk::POS_BOTTOM, 1, 1);
		m_pathLabel.set_text("Path");
	m_grid.attach_next_to(m_pathEntry, m_pathLabel, Gtk::POS_RIGHT, 1, 1);

	m_pathEntry.set_size_request(250, 1);

	show_all();
}

InstrumentConnectionDialog::~InstrumentConnectionDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

string InstrumentConnectionDialog::GetConnectionString()
{
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "%s:%s:%s:%s",
		m_nicknameEntry.get_text().c_str(),
		m_driverBox.get_active_text().c_str(),
		m_transportBox.get_active_text().c_str(),
		m_pathEntry.get_text().c_str());
	return tmp;
}
