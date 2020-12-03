/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
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
	@brief Implementation of WaveformGroupPropertiesDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "WaveformGroupPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformGroupPropertiesDialog::WaveformGroupPropertiesDialog(
	OscilloscopeWindow* parent,
	WaveformGroup* group)
	: Gtk::Dialog(string("Waveform group properties"), *parent, Gtk::DIALOG_MODAL)
	, m_group(group)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);
		m_grid.attach(m_groupNameLabel, 0, 0, 1, 1);
			m_groupNameLabel.set_text("Name");
			m_groupNameLabel.set_halign(Gtk::ALIGN_START);
		m_grid.attach_next_to(m_groupNameEntry, m_groupNameLabel, Gtk::POS_RIGHT, 1, 1);
			m_groupNameEntry.set_halign(Gtk::ALIGN_START);
			m_groupNameEntry.set_text(group->m_realframe.get_label());

	show_all();
}

WaveformGroupPropertiesDialog::~WaveformGroupPropertiesDialog()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void WaveformGroupPropertiesDialog::ConfigureGroup()
{
	m_group->m_realframe.set_label(m_groupNameEntry.get_text());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
