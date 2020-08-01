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
	m_grid.set_row_spacing(5);

	m_grid.attach(m_sampleRateLabel, 0, 0, 1, 1);
		m_sampleRateLabel.set_text("Sample Rate");
	m_grid.attach_next_to(m_sampleRateBox, m_sampleRateLabel, Gtk::POS_RIGHT, 1, 1);
	m_grid.attach_next_to(m_memoryDepthLabel, m_sampleRateLabel, Gtk::POS_BOTTOM, 1, 1);
		m_memoryDepthLabel.set_text("Memory Depth");
	m_grid.attach_next_to(m_memoryDepthBox, m_memoryDepthLabel, Gtk::POS_RIGHT, 1, 1);

	m_grid.attach_next_to(m_interleaveLabel, m_memoryDepthLabel, Gtk::POS_BOTTOM, 1, 1);
		m_interleaveLabel.set_text("Channel Combining");
	m_grid.attach_next_to(m_interleaveSwitch, m_interleaveLabel, Gtk::POS_RIGHT, 1, 1);

	bool interleaving = m_scope->IsInterleaving();
	m_interleaveSwitch.set_state(interleaving);
	m_interleaveSwitch.set_sensitive(m_scope->CanInterleave());

	m_interleaveSwitch.signal_state_set().connect(
		sigc::mem_fun(*this, &TimebasePropertiesPage::OnInterleaveSwitchChanged), false);

	RefreshSampleRates(interleaving);
	RefreshSampleDepths(interleaving);
}

/**
	@brief Update the list of sample rates (these may change when, for example, interleaving is turned on/off)
 */
void TimebasePropertiesPage::RefreshSampleRates(bool interleaving)
{
	m_sampleRateBox.remove_all();

	vector<uint64_t> rates;
	if(interleaving)
		rates = m_scope->GetSampleRatesInterleaved();
	else
		rates = m_scope->GetSampleRatesNonInterleaved();

	Unit unit(Unit::UNIT_SAMPLERATE);
	string last_rate;
	for(auto rate : rates)
	{
		last_rate = unit.PrettyPrint(rate);
		m_sampleRateBox.append(last_rate);
	}
	m_sampleRateBox.set_active_text(unit.PrettyPrint(m_scope->GetSampleRate()));

	//If no text was selected, select the highest valid rate
	if(m_sampleRateBox.get_active_text() == "")
		m_sampleRateBox.set_active_text(last_rate);
}

/**
	@brief Update the list of sample depths (these may change when, for example, interleaving is turned on/off)
 */
void TimebasePropertiesPage::RefreshSampleDepths(bool interleaving)
{
	m_memoryDepthBox.remove_all();

	vector<uint64_t> depths;
	if(interleaving)
		depths = m_scope->GetSampleDepthsInterleaved();
	else
		depths = m_scope->GetSampleDepthsNonInterleaved();

	Unit unit(Unit::UNIT_SAMPLEDEPTH);
	string last_depth;
	for(auto depth : depths)
	{
		last_depth = unit.PrettyPrint(depth);
		m_memoryDepthBox.append(last_depth);
	}

	m_memoryDepthBox.set_active_text(unit.PrettyPrint(m_scope->GetSampleDepth()));

	//If no text was selected, select the highest valid depth
	if(m_memoryDepthBox.get_active_text() == "")
		m_memoryDepthBox.set_active_text(last_depth);
}

/**
	@brief Handle the interleaving switch being hit
 */
bool TimebasePropertiesPage::OnInterleaveSwitchChanged(bool state)
{
	//Refresh the list of legal timebase/memory configurations
	RefreshSampleRates(state);
	RefreshSampleDepths(state);

	//run default handler
	return false;
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
		m_pages[scope] = page;
	}

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
	for(auto it : m_pages)
	{
		//Configure interleaving
		it.first->SetInterleaving(it.second->m_interleaveSwitch.get_state());

		//Figure out the requested sample rate
		char scale;
		long rate;
		sscanf(it.second->m_sampleRateBox.get_active_text().c_str(), "%ld %cS/s", &rate, &scale);
		uint64_t frate = rate;
		if(scale == 'k')
			frate *= 1000;
		else if(scale == 'M')
			frate *= 1000000;
		else if(scale == 'G')
			frate *= 1000000000;

		//Figure out the memory depth
		long depth;
		sscanf(it.second->m_memoryDepthBox.get_active_text().c_str(), "%ld %cS", &depth, &scale);
		uint64_t fdepth = depth;
		if(scale == 'k')
			fdepth *= 1000;
		else if(scale == 'M')
			fdepth *= 1000000;
		else if(scale == 'G')
			fdepth *= 1000000000;

		//Apply changes
		it.first->SetSampleDepth(fdepth);
		it.first->SetSampleRate(frate);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers
