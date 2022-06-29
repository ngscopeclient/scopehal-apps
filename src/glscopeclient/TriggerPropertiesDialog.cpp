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
	@brief Implementation of TriggerPropertiesDialog
 */
#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "TriggerPropertiesDialog.h"
#include "../scopehal/CDRTrigger.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TriggerPropertiesDialog::TriggerPropertiesDialog(
	OscilloscopeWindow* parent,
	Oscilloscope* scope)
	: Gtk::Dialog(string("Trigger properties"), *parent)
	, m_scope(scope)
	, m_parent(parent)
{
	char buf[128];

	get_vbox()->pack_start(m_grid, Gtk::PACK_SHRINK);

	//Scope information
	m_grid.attach(m_scopeNameLabel, 0, 0, 1, 1);
		m_scopeNameLabel.set_text("Scope");
			m_scopeNameLabel.set_halign(Gtk::ALIGN_START);
			m_scopeNameLabel.set_size_request(100, 1);
		m_grid.attach_next_to(m_scopeNameEntry, m_scopeNameLabel, Gtk::POS_RIGHT, 1, 1);
			m_scopeNameEntry.set_halign(Gtk::ALIGN_START);
			snprintf(buf, sizeof(buf), "%s (%s, serial %s)",
				scope->m_nickname.c_str(),
				scope->GetName().c_str(),
				scope->GetSerial().c_str());
			m_scopeNameEntry.set_text(buf);

	//List of trigger types
	m_grid.attach_next_to(m_triggerTypeLabel, m_scopeNameLabel, Gtk::POS_BOTTOM, 1, 1);
		m_triggerTypeLabel.set_text("Trigger Type");
		m_grid.attach_next_to(m_triggerTypeBox, m_triggerTypeLabel, Gtk::POS_RIGHT, 1, 1);
			auto types = scope->GetTriggerTypes();
			for(auto t : types)
				m_triggerTypeBox.append(t);
			auto trig = scope->GetTrigger();
			if(trig)
				m_triggerTypeBox.set_active_text(trig->GetTriggerDisplayName());
			m_triggerTypeBox.signal_changed().connect(
				sigc::mem_fun(*this, &TriggerPropertiesDialog::OnTriggerTypeChanged));
			m_triggerTypeBox.set_hexpand(true);

	//Trigger horizontal offset
	Unit fs(Unit::UNIT_FS);
	m_grid.attach_next_to(m_triggerOffsetLabel, m_triggerTypeLabel, Gtk::POS_BOTTOM, 1, 1);
		m_triggerOffsetLabel.set_text("Trigger Offset");
		m_grid.attach_next_to(m_triggerOffsetEntry, m_triggerOffsetLabel, Gtk::POS_RIGHT, 1, 1);
			auto offset = m_scope->GetTriggerOffset();
			m_triggerOffsetEntry.set_text(fs.PrettyPrint(offset));
			m_triggerOffsetEntry.set_hexpand(true);

	//Actual content
	get_vbox()->pack_start(m_contentGrid, Gtk::PACK_SHRINK);
	if(trig)
		AddRows(trig);

	show_all();
}

TriggerPropertiesDialog::~TriggerPropertiesDialog()
{
	Clear();
}

void TriggerPropertiesDialog::Clear()
{
	m_pllLockConnection.disconnect();

	auto children = m_contentGrid.get_children();
	for(auto c : children)
		m_contentGrid.remove(*c);

	for(auto r : m_rows)
		delete r;
	for(auto it : m_prows)
		delete it.second;
	m_rows.clear();
	m_prows.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Output

void TriggerPropertiesDialog::ConfigureTrigger()
{
	//Get the existing trigger
	auto trig = m_scope->GetTrigger();

	//Hook up the input(s)
	//TODO: is this now done live?
	FilterDialog::ConfigureInputs(trig, m_rows);

	//Also, set the trigger offset
	//TODO: do this (and nothing else) in event handler when offset is changed
	Unit fs(Unit::UNIT_FS);
	int64_t oldoff = m_scope->GetTriggerOffset();
	m_scope->SetTriggerOffset(fs.ParseString(m_triggerOffsetEntry.get_text()));
	int64_t newoff = m_scope->GetTriggerOffset();
	m_parent->OnTriggerOffsetChanged(m_scope, oldoff, newoff);

	//Push changes to the scope
	m_scope->PushTrigger();

	//Redraw everything
	m_parent->RefreshAllViews();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void TriggerPropertiesDialog::OnTriggerTypeChanged()
{
	//Remove the old trigger stuff
	Clear();

	//See what type the new trigger is
	auto type = m_triggerTypeBox.get_active_text();

	//If it's the same trigger type currently set on the scope, load UI with those settings
	auto current_trig = m_scope->GetTrigger();
	if(current_trig == NULL)
		return;

	if(current_trig->GetTriggerDisplayName() == type)
		AddRows(current_trig);

	//Nope, create a new trigger
	else
	{
		auto trig = Trigger::CreateTrigger(type, m_scope);

		//Copy level and first input from the current trigger
		trig->SetLevel(current_trig->GetLevel());
		trig->SetInput(0, current_trig->GetInput(0));

		AddRows(trig);
		m_scope->SetTrigger(trig);
	}
}

void TriggerPropertiesDialog::AddRows(Trigger* trig)
{
	//Add inputs
	for(size_t i=0; i<trig->GetInputCount(); i++)
	{
		//Add the row
		auto row = new ChannelSelectorRow;
		m_contentGrid.attach(row->m_label, 0, i, 1, 1);
		m_contentGrid.attach_next_to(row->m_chans, row->m_label, Gtk::POS_RIGHT, 1, 1);
		m_rows.push_back(row);

		auto cur_in = trig->GetInput(i);

		//Label is just the channel name
		row->m_label.set_label(trig->GetInputName(i));

		//Fill the channel list with all channels that are legal to use here.
		//They must be from the current instrument, so don't bother checking others.
		//TODO: multiple streams
		for(size_t k=0; k<m_scope->GetChannelCount(); k++)
		{
			auto chan = m_scope->GetChannel(k);

			//Hide channels we can't enable due to interleave conflicts etc
			//Trigger channel can't be enabled for display, but is always a legal source
			if( !m_scope->CanEnableChannel(k) && (chan->GetType(0) != Stream::STREAM_TYPE_TRIGGER) )
				continue;

			auto c = StreamDescriptor(chan, 0);
			if(trig->ValidateChannel(i, c))
			{
				auto name = c.m_channel->GetDisplayName();
				row->m_chans.append(name);
				row->m_chanptrs[name] = c;

				if(c == cur_in)
					row->m_chans.set_active_text(name);
			}
		}

	}

	auto ct = dynamic_cast<CDRTrigger*>(trig);

	//Add parameters
	for(auto it = trig->GetParamBegin(); it != trig->GetParamEnd(); it ++)
	{
		auto row = FilterDialog::CreateRow(
			m_contentGrid,
			it->first,
			it->second,
			m_prows.size() + trig->GetInputCount(),
			NULL,
			trig);
		m_prows[it->first] = row;

		//Special case: add CDR autobaud button and lock status
		if(ct && ct->IsAutomaticBitRateCalculationAvailable() && (it->first == ct->GetBitRateName()))
		{
			//Autobaud button
			auto srow = dynamic_cast<ParameterRowString*>(row);
			auto button = Gtk::manage(new Gtk::Button);
			button->set_label("Auto");
			button->signal_clicked().connect(sigc::mem_fun(*ct, &CDRTrigger::CalculateBitRate));
			srow->m_contentbox.attach_next_to(*button, srow->m_entry, Gtk::POS_RIGHT);

			auto lock = Gtk::manage(new Gtk::Label);
			srow->m_contentbox.attach_next_to(*lock, *button, Gtk::POS_RIGHT);

			lock->set_margin_left(10);
			lock->set_margin_right(10);

			m_pllLockConnection.disconnect();
			m_pllLockConnection = Glib::signal_timeout().connect(
				sigc::bind(sigc::mem_fun(*this, &TriggerPropertiesDialog::OnCDRLockTimer), ct, lock), 1000);
		}

		it->second.signal_changed().connect(sigc::mem_fun(*this, &TriggerPropertiesDialog::ConfigureTrigger));
	}

	m_contentGrid.show_all();
}

bool TriggerPropertiesDialog::OnCDRLockTimer(CDRTrigger* trig, Gtk::Label* label)
{
	if(trig->IsCDRLocked())
		label->set_label("[Locked]");
	else
		label->set_label("[Unlocked]");

	return true;
}
