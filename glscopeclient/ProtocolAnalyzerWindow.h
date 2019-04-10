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
	@brief  Declaration of ProtocolAnalyzerWindow
 */
#ifndef ProtocolAnalyzerWindow_h
#define ProtocolAnalyzerWindow_h

class OscilloscopeWindow;

#include "../../lib/scopehal/PacketDecoder.h"

class ProtocolAnalyzerColumns : public Gtk::TreeModel::ColumnRecord
{
public:
	ProtocolAnalyzerColumns(PacketDecoder* decoder);

	Gtk::TreeModelColumn<Glib::ustring>					m_timestamp;
	std::vector< Gtk::TreeModelColumn<Glib::ustring> >	m_headers;
	Gtk::TreeModelColumn<Glib::ustring>					m_data;
};

class ProtocolAnalyzerWindow : public Gtk::Dialog
{
public:
	ProtocolAnalyzerWindow(std::string title, OscilloscopeWindow* parent, PacketDecoder* decoder);
	~ProtocolAnalyzerWindow();

	void OnWaveformDataReady();

protected:
	PacketDecoder* m_decoder;

	Gtk::ScrolledWindow m_scroller;
		Gtk::TreeView m_tree;
	Glib::RefPtr<Gtk::TreeStore> m_model;
	ProtocolAnalyzerColumns m_columns;
};

#endif
