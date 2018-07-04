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
	@brief Implementation of ScopeConnectionDialog
 */

#include "scopeclient.h"
#include "ScopeConnectionDialog.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ScopeConnectionDialog::ScopeConnectionDialog(std::string hostname, unsigned short port)
	: Gtk::Dialog(Glib::ustring("Connect to scope / LA"), true)
	, m_hostlist(2)
{
	set_size_request(480, 240);

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	get_vbox()->pack_start(m_hostbox, Gtk::PACK_SHRINK);
		m_hostbox.pack_start(m_hostlabel, Gtk::PACK_SHRINK);
			m_hostlabel.set_text("Hostname");
			m_hostlabel.set_width_chars(16);
			m_hostlabel.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
		m_hostbox.pack_start(m_hostentry);
			m_hostentry.set_text(hostname);

	get_vbox()->pack_start(m_portbox, Gtk::PACK_SHRINK);
		m_portbox.pack_start(m_portlabel, Gtk::PACK_SHRINK);
			m_portlabel.set_text("Port");
			m_portlabel.set_width_chars(16);
			m_portlabel.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
		m_portbox.pack_start(m_portentry);
			char portname[16];
			snprintf(portname, sizeof(portname), "%d", (int)port);
			m_portentry.set_text(portname);
			m_portentry.set_width_chars(6);
		m_portbox.pack_start(m_connectButton, Gtk::PACK_SHRINK);
			m_connectButton.set_label("Connect");
			m_connectButton.signal_clicked().connect(sigc::mem_fun(*this, &ScopeConnectionDialog::OnConnect));

	show_all();

	//m_scope = NULL;
	//m_namesrvr = NULL;
}

ScopeConnectionDialog::~ScopeConnectionDialog()
{
	/*
	if(m_namesrvr)
	{
		delete m_namesrvr;
		m_namesrvr = NULL;
	}

	if(m_scope)
	{
		delete m_scope;
		m_scope = NULL;
	}
	*/
}

void ScopeConnectionDialog::OnConnect()
{
	/*
	//Try to connect
	try
	{
		//Get the port number
		int portnum;
		sscanf(m_portentry.get_text().c_str(), "%5d", &portnum);

		//Try connecting
		m_scope = new RedTinLogicAnalyzer(m_hostentry.get_text(), portnum);

		//Connection successful - remove the old controls
		get_vbox()->remove(m_hostbox);
		get_vbox()->remove(m_portbox);
		add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

		//Add the progress bar
		get_vbox()->pack_start(m_nameprogress, Gtk::PACK_SHRINK);
		show_all();

		//Create the name server and dump the host table
		m_namesrvr = new NameServer(&m_scope->m_iface);
		for(int i=0; i<256; i++)
		{
			m_nameprogress.set_fraction(static_cast<float>(i) / 256);
			char name[256];
			snprintf(name, sizeof(name), "Loading hosts: %d / %d", i, 256);

			//Dispatch pending gtk events (such as draw calls)
			while(Gtk::Main::events_pending())
				Gtk::Main::iteration();

			//Do the real work
			m_namesrvr->LoadHostTableEntry(i, false);
		}

		//Done loading host info, remove the progress bar and add the list box
		get_vbox()->remove(m_nameprogress);
		get_vbox()->pack_start(m_hostlist);
			m_hostlist.set_column_title(0, "Name");
			m_hostlist.set_column_title(1, "Address");
		show_all();

		//Populate the list box
		bool found = false;
		for(NameServer::ForwardMapType::const_iterator it = m_namesrvr->cbegin(); it != m_namesrvr->cend(); ++it)
		{
			if(it->first.find("LA") == std::string::npos)
				continue;
			int nrow = m_hostlist.append(it->first);
			char adbuf[256];
			snprintf(adbuf, sizeof(adbuf), "%04x", it->second);
			m_hostlist.set_text(nrow, 1, adbuf);
			found = true;
		}

		//If nothing was found, display message and abort
		if(!found)
		{
			Gtk::MessageDialog dlg("No logic analyzer cores found", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
			dlg.run();
			exit(0);
		}

		//Select the first one
		Gtk::TreePath path("0");
		m_hostlist.get_selection()->select(path);
	}
	catch(const JtagException& ex)
	{
		printf("%s\n", ex.GetDescription().c_str());
		Gtk::MessageDialog dlg("Failed to connect to server", false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
		dlg.run();
	}
	*/
}

Oscilloscope* ScopeConnectionDialog::DetachScope()
{
	/*
	m_scope->Connect(m_hostlist.get_text(m_hostlist.get_selected()[0]));
	Oscilloscope* r = m_scope;
	m_scope = NULL;
	return r;
	*/
	return NULL;
}

/*
NameServer* ScopeConnectionDialog::DetachNameServer()
{
	NameServer* n = m_namesrvr;
	m_namesrvr = NULL;
	return n;
}
*/
