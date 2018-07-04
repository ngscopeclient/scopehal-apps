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
	@brief Declaration of ProtocolDecoderDialog
 */

#ifndef ProtocolDecoderDialog_h
#define ProtocolDecoderDialog_h

#include "../scopehal/Oscilloscope.h"
//#include "../scopehal/ProtocolDecoder.h"

#include <glibmm/main.h>

class MainWindow;

class ProtocolDecoderGuiRow
{
public:
	ProtocolDecoderGuiRow()
	{
		m_box.pack_start(m_label, Gtk::PACK_SHRINK);
			m_label.set_width_chars(16);
			m_label.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
		m_box.pack_start(m_cbox);
	}

	Gtk::HBox m_box;
		Gtk::Label m_label;
		Gtk::ComboBoxText m_cbox;
};

class ProtocolDecoderGuiRowEntry
{
public:
	ProtocolDecoderGuiRowEntry()
	{
		m_box.pack_start(m_label, Gtk::PACK_SHRINK);
			m_label.set_width_chars(16);
			m_label.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
		m_box.pack_start(m_entry);
	}

	Gtk::HBox m_box;
		Gtk::Label m_label;
		Gtk::Entry m_entry;
};

class ProtocolDecoderDialog : public Gtk::Dialog
{
public:
	ProtocolDecoderDialog(MainWindow* parent, Oscilloscope* scope/*, NameServer& namesrvr*/);
	virtual ~ProtocolDecoderDialog();

	//ProtocolDecoder* Detach();

protected:
	Oscilloscope* m_scope;

	//Static content that doesn't change
	Gtk::HBox m_decoderbox;
		Gtk::Label m_decoderlabel;
		Gtk::ComboBoxText m_decoderlist;
	Gtk::HBox m_namebox;
		Gtk::Label m_namelabel;
		Gtk::Entry m_nameentry;
	Gtk::HSeparator m_hsep;

	//Stuff here depends on the specific decoder we selected
	Gtk::VBox m_body;
	Gtk::HSeparator m_hsep2;
	Gtk::VBox m_parambody;

	void OnDecoderSelected();
	void OnInputSelected();
	void FillSignals();

	void ClearBodyRows();

	//ProtocolDecoder* m_decoder;

	//NameServer& m_namesrvr;

	std::vector<ProtocolDecoderGuiRow*> m_bodyrows;
	std::vector<ProtocolDecoderGuiRowEntry*> m_paramrows;
};

#endif
