/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of SCPIConsoleDialog
 */
#include "glscopeclient.h"
#include "SCPIConsoleDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SCPIConsoleDialog::SCPIConsoleDialog(SCPIDevice* device)
	: Gtk::Dialog("")
	, m_device(device)
	, m_results(1)
{
	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);

	auto inst = dynamic_cast<Instrument*>(device);
	if(inst != nullptr)
		set_title(string("SCPI Console: ") + inst->m_nickname);

	m_grid.attach(m_results, 0, 0, 2, 1);
	m_grid.attach(m_commandBox, 0, 1, 1, 1);
		m_grid.attach(m_submitButton, 1, 1, 1, 1);

	m_submitButton.set_label("Send");
	m_submitButton.set_can_default();
	set_default(m_submitButton);

	m_grid.set_hexpand(true);
	m_grid.set_vexpand(true);
	m_results.set_hexpand(true);
	m_results.set_vexpand(true);
	m_results.set_headers_visible(false);

	//minimum initial size
	m_results.set_size_request(450, 100);

	m_commandBox.set_hexpand(true);

	show_all();

	m_submitButton.signal_clicked().connect(sigc::mem_fun(*this, &SCPIConsoleDialog::OnSend));
	m_commandBox.signal_activate().connect(sigc::mem_fun(*this, &SCPIConsoleDialog::OnSend));
}

SCPIConsoleDialog::~SCPIConsoleDialog()
{

}

void SCPIConsoleDialog::OnSend()
{
	auto command = m_commandBox.get_text();
	m_commandBox.set_text("");

	m_results.append(command);

	//Command, not a query - no response
	auto transport = m_device->GetTransport();
	if(command.find('?') == string::npos)
		transport->SendCommandImmediate(command);

	//Query, we have a response
	else
		m_results.append(Trim(transport->SendCommandImmediateWithReply(command)));
}
