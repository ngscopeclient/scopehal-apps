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
	@brief Implementation of TimebasePropertiesDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "TimebasePropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TimebasePropertiesPage

void TimebasePropertiesPage::AddWidgets()
{
	m_grid.set_margin_left(10);
	m_grid.set_margin_right(10);
	m_grid.set_column_spacing(10);

	m_grid.attach(m_sampleRateLabel, 0, 0);
		m_sampleRateLabel.set_text("Sample rate");
	m_grid.attach_next_to(m_sampleRateBox, m_sampleRateLabel, Gtk::POS_RIGHT);
	m_grid.attach_next_to(m_memoryDepthLabel, m_sampleRateLabel, Gtk::POS_BOTTOM);
		m_memoryDepthLabel.set_text("Memory depth");
	m_grid.attach_next_to(m_memoryDepthBox, m_memoryDepthLabel, Gtk::POS_RIGHT);

	//Set up sample rate box
	//TODO: interleaving support etc
	Unit unit(Unit::UNIT_SAMPLERATE);
	auto rates = m_scope->GetSampleRatesNonInterleaved();
	for(auto rate : rates)
		m_sampleRateBox.append(unit.PrettyPrint(rate));
	m_sampleRateBox.set_active_text(unit.PrettyPrint(m_scope->GetSampleRate()));

	//Set up memory depth box
	unit = Unit(Unit::UNIT_SAMPLEDEPTH);
	auto depths = m_scope->GetSampleDepthsNonInterleaved();
	for(auto depth : depths)
		m_memoryDepthBox.append(unit.PrettyPrint(depth));
	m_memoryDepthBox.set_active_text(unit.PrettyPrint(m_scope->GetSampleDepth()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TimebasePropertiesDialog::TimebasePropertiesDialog(
	OscilloscopeWindow* parent,
	const vector<Oscilloscope*>& scopes)
	: Gtk::Dialog("Timebase Properties", *parent, Gtk::DIALOG_MODAL)
	, m_scopes(scopes)
{
	add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);

	get_vbox()->pack_start(m_tabs, Gtk::PACK_EXPAND_WIDGET);

	for(auto scope : scopes)
	{
		TimebasePropertiesPage* page = new TimebasePropertiesPage(scope);
		m_tabs.append_page(page->m_grid, scope->m_nickname);
		page->AddWidgets();
	}

	/*
	get_vbox()->pack_start(m_channelNameBox, Gtk::PACK_SHRINK);
		m_channelNameBox.pack_start(m_channelNameLabel, Gtk::PACK_SHRINK);
		m_channelNameLabel.set_text("Timebase");
		m_channelNameBox.pack_start(m_channelNameEntry, Gtk::PACK_EXPAND_WIDGET);
		m_channelNameLabel.set_size_request(150, 1);
		m_channelNameLabel.set_halign(Gtk::ALIGN_START);
		m_channelNameEntry.set_text(chan->GetHwname());
		m_channelNameEntry.set_halign(Gtk::ALIGN_START);

	get_vbox()->pack_start(m_channelDisplayNameBox, Gtk::PACK_SHRINK);
			m_channelDisplayNameBox.pack_start(m_channelDisplayNameLabel, Gtk::PACK_SHRINK);
			m_channelDisplayNameLabel.set_text("Display name");
			m_channelDisplayNameBox.pack_start(m_channelDisplayNameEntry, Gtk::PACK_EXPAND_WIDGET);
			m_channelDisplayNameLabel.set_size_request(150, 1);
			m_channelDisplayNameLabel.set_halign(Gtk::ALIGN_START);
			m_channelDisplayNameEntry.set_text(chan->m_displayname);
	*/
	show_all();
}

TimebasePropertiesDialog::~TimebasePropertiesDialog()
{
	for(auto it : m_pages)
		delete it.second;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void TimebasePropertiesDialog::ConfigureTimebase()
{
	//m_chan->m_displayname = m_channelDisplayNameEntry.get_text();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
