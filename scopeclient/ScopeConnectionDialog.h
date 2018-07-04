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
	@brief Declaration of ScopeConnectionDialog
 */

#ifndef ScopeConnectionDialog_h
#define ScopeConnectionDialog_h

#include "../scopehal/ChannelRenderer.h"
#include "../scopehal/Oscilloscope.h"
#include "../scopehal/RedTinLogicAnalyzer.h"

class ScopeConnectionDialog : public Gtk::Dialog
{
public:
	ScopeConnectionDialog(std::string hostname, unsigned short port);
	virtual ~ScopeConnectionDialog();

	Oscilloscope* DetachScope();
	//NameServer* DetachNameServer();

protected:

	Gtk::HBox m_hostbox;
		Gtk::Label m_hostlabel;
		Gtk::Entry m_hostentry;
	Gtk::HBox m_portbox;
		Gtk::Label m_portlabel;
		Gtk::Entry m_portentry;
		Gtk::Button m_connectButton;
	Gtk::ProgressBar m_nameprogress;
	Gtk::ListViewText m_hostlist;

	void OnConnect();

	//RedTinLogicAnalyzer* m_scope;
	//NameServer* m_namesrvr;
};

#endif
