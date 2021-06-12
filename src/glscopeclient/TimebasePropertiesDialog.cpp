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
	m_initializing = true;

	//Stack setup
	m_box.pack_start(m_sidebar, Gtk::PACK_EXPAND_WIDGET);
	m_box.pack_start(m_stack, Gtk::PACK_EXPAND_WIDGET);
	m_sidebar.set_stack(m_stack);
	m_stack.set_homogeneous();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Time domain settings

	m_stack.add(m_tgrid, "Time Domain", "Time Domain");

	m_tgrid.set_margin_left(10);
	m_tgrid.set_margin_right(10);
	m_tgrid.set_column_spacing(10);
	m_tgrid.set_row_spacing(5);

	m_tgrid.attach(m_sampleRateLabel, 0, 0, 1, 1);
		m_sampleRateLabel.set_text("Sample Rate");
	m_tgrid.attach_next_to(m_sampleRateBox, m_sampleRateLabel, Gtk::POS_RIGHT, 1, 1);
	m_tgrid.attach_next_to(m_memoryDepthLabel, m_sampleRateLabel, Gtk::POS_BOTTOM, 1, 1);
		m_memoryDepthLabel.set_text("Memory Depth");
	m_tgrid.attach_next_to(m_memoryDepthBox, m_memoryDepthLabel, Gtk::POS_RIGHT, 1, 1);

	m_tgrid.attach_next_to(m_sampleModeLabel, m_memoryDepthLabel, Gtk::POS_BOTTOM, 1, 1);
		m_sampleModeLabel.set_text("Timebase Mode");
	m_tgrid.attach_next_to(m_sampleModeBox, m_sampleModeLabel, Gtk::POS_RIGHT, 1, 1);

	m_tgrid.attach_next_to(m_interleaveLabel, m_sampleModeLabel, Gtk::POS_BOTTOM, 1, 1);
		m_interleaveLabel.set_text("Channel Combining");
	m_tgrid.attach_next_to(m_interleaveSwitch, m_interleaveLabel, Gtk::POS_RIGHT, 1, 1);

	bool interleaving = m_scope->IsInterleaving();
	m_interleaveSwitch.set_state(interleaving);
	m_interleaveSwitch.set_sensitive(m_scope->CanInterleave());

	m_memoryDepthBox.signal_changed().connect(
		sigc::mem_fun(*this, &TimebasePropertiesPage::OnDepthChanged), false);
	m_sampleRateBox.signal_changed().connect(
		sigc::mem_fun(*this, &TimebasePropertiesPage::OnRateChanged), false);
	m_sampleModeBox.signal_changed().connect(
		sigc::mem_fun(*this, &TimebasePropertiesPage::OnModeChanged), false);
	m_interleaveSwitch.signal_state_set().connect(
		sigc::mem_fun(*this, &TimebasePropertiesPage::OnInterleaveSwitchChanged), false);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Frequency domain settings

	if(m_scope->HasFrequencyControls())
	{
		m_stack.add(m_fgrid, "Frequency Domain", "Frequency Domain");

		m_fgrid.set_margin_left(10);
		m_fgrid.set_margin_right(10);
		m_fgrid.set_column_spacing(10);
		m_fgrid.set_row_spacing(5);

		m_fgrid.attach(m_spanLabel, 0, 0, 1, 1);
		m_spanLabel.set_text("Span");
		m_fgrid.attach_next_to(m_spanEntry, m_spanLabel, Gtk::POS_RIGHT, 1, 1);
		m_spanEntry.set_text(Unit(Unit::UNIT_HZ).PrettyPrint(m_scope->GetSpan()));

		m_fgrid.attach_next_to(m_rbwLabel, m_spanLabel, Gtk::POS_BOTTOM, 1, 1);
		m_rbwLabel.set_text("RBW");
		m_fgrid.attach_next_to(m_rbwEntry, m_rbwLabel, Gtk::POS_RIGHT, 1, 1);
		m_rbwEntry.set_text(Unit(Unit::UNIT_HZ).PrettyPrint(m_scope->GetResolutionBandwidth()));

		m_spanEntry.signal_changed().connect(sigc::mem_fun(*this, &TimebasePropertiesPage::OnSpanChanged), false);
		m_rbwEntry.signal_changed().connect(sigc::mem_fun(*this, &TimebasePropertiesPage::OnRBWChanged), false);
	}

	m_initializing = false;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Done, pull stuff from the instrment

	RefreshSampleRates(interleaving);
	RefreshSampleDepths(interleaving);
	RefreshSampleModes();
}

void TimebasePropertiesPage::OnDepthChanged()
{
	//ignore spurious changes when populating box
	if(m_initializing)
		return;

	Unit depth(Unit::UNIT_SAMPLEDEPTH);

	//Make the change, preserving trigger offset which might otherwise be affected
	int64_t offset = m_scope->GetTriggerOffset();
	m_scope->SetSampleDepth(round(depth.ParseString(m_memoryDepthBox.get_active_text())));
	m_scope->SetTriggerOffset(offset);

	//Update channels menu in parent scope in case this change alters the set of available channels
	m_parent->RefreshChannelsMenu();

	//Available sampling modes might change as the memory depth changes
	RefreshSampleModes();
}

void TimebasePropertiesPage::OnRateChanged()
{
	//ignore spurious changes when populating box
	if(m_initializing)
		return;

	Unit rate(Unit::UNIT_SAMPLERATE);

	//Make the change, preserving trigger offset which might otherwise be affected
	int64_t offset = m_scope->GetTriggerOffset();
	m_scope->SetSampleRate(round(rate.ParseString(m_sampleRateBox.get_active_text())));
	m_scope->SetTriggerOffset(offset);

	//Update channels menu in parent scope in case this change alters the set of available channels
	m_parent->RefreshChannelsMenu();

	//Available sampling modes might change as the sample rate changes
	RefreshSampleModes();
}

/**
	@brief Handle the interleaving switch being hit
 */
bool TimebasePropertiesPage::OnInterleaveSwitchChanged(bool state)
{
	//Refresh the list of legal timebase/memory configurations
	RefreshSampleRates(state);
	RefreshSampleDepths(state);

	//Make the change, preserving trigger offset which might otherwise be affected
	int64_t offset = m_scope->GetTriggerOffset();
	m_scope->SetInterleaving(m_interleaveSwitch.get_state());
	m_scope->SetTriggerOffset(offset);

	//Update channels menu in parent scope in case this change alters the set of available channels
	m_parent->RefreshChannelsMenu();

	//run default handler
	return false;
}

void TimebasePropertiesPage::OnSpanChanged()
{
	//ignore spurious changes when populating box
	if(m_initializing)
		return;

	Unit hz(Unit::UNIT_HZ);
	m_scope->SetSpan(round(hz.ParseString(m_spanEntry.get_text())));
}

void TimebasePropertiesPage::OnRBWChanged()
{
	//ignore spurious changes when populating box
	if(m_initializing)
		return;

	Unit hz(Unit::UNIT_HZ);
	m_scope->SetResolutionBandwidth(round(hz.ParseString(m_rbwEntry.get_text())));
}

void TimebasePropertiesPage::OnModeChanged()
{
	//ignore spurious changes when populating box
	if(m_initializing)
		return;

	if(m_sampleModeBox.get_active_text() == "Equivalent time")
		m_scope->SetSamplingMode(Oscilloscope::EQUIVALENT_TIME);
	else
		m_scope->SetSamplingMode(Oscilloscope::REAL_TIME);

	//Equivalent and real time sampling normally have very different sets of available sample rate/depth
	bool interleaving = m_scope->IsInterleaving();
	RefreshSampleRates(interleaving);
	RefreshSampleDepths(interleaving);
}

/**
	@brief Update the list of sample modes
 */
void TimebasePropertiesPage::RefreshSampleModes()
{
	m_initializing = true;

	m_sampleModeBox.remove_all();

	//Populate the box
	int nmodes = 0;
	if(m_scope->IsSamplingModeAvailable(Oscilloscope::REAL_TIME))
	{
		m_sampleModeBox.append("Real time");
		nmodes ++;
	}
	if(m_scope->IsSamplingModeAvailable(Oscilloscope::EQUIVALENT_TIME))
	{
		m_sampleModeBox.append("Equivalent time");
		nmodes ++;
	}

	//Select the current contents
	switch(m_scope->GetSamplingMode())
	{
		case Oscilloscope::REAL_TIME:
			m_sampleModeBox.set_active_text("Real time");
			break;

		case Oscilloscope::EQUIVALENT_TIME:
			m_sampleModeBox.set_active_text("Equivalent time");
			break;
	}

	//Disable the box unless there's more than one option
	if(nmodes > 1)
		m_sampleModeBox.set_sensitive(true);
	else
		m_sampleModeBox.set_sensitive(false);

	m_initializing = false;
}

/**
	@brief Update the list of sample rates (these may change when, for example, interleaving is turned on/off)
 */
void TimebasePropertiesPage::RefreshSampleRates(bool interleaving)
{
	m_initializing = true;

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

	m_initializing = false;
}

/**
	@brief Update the list of sample depths (these may change when, for example, interleaving is turned on/off)
 */
void TimebasePropertiesPage::RefreshSampleDepths(bool interleaving)
{
	m_initializing = true;

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

	m_initializing = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TimebasePropertiesDialog::TimebasePropertiesDialog(
	OscilloscopeWindow* parent,
	const vector<Oscilloscope*>& scopes)
	: Gtk::Dialog("Timebase Properties", *parent)
	, m_scopes(scopes)
{
	get_vbox()->pack_start(m_tabs, Gtk::PACK_EXPAND_WIDGET);

	for(auto scope : scopes)
	{
		TimebasePropertiesPage* page = new TimebasePropertiesPage(scope, parent);
		m_tabs.append_page(page->m_box, scope->m_nickname);
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
