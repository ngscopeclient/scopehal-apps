/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of MultimeterDialog
 */
#include "glscopeclient.h"
#include "MultimeterDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MultimeterDialog::MultimeterDialog(Multimeter* meter, OscilloscopeWindow* parent)
	: Gtk::Dialog( string("Multimeter: ") + meter->m_nickname )
	, m_meter(meter)
	, m_updatingSecondary(false)
	, m_parent(parent)
	, m_timerIntervalChanged(false)
{
	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);

	//TODO: hide input selector if we only have one input?
	//TODO: have some means of refreshing channel list when a channel is renamed
	//TODO: hide illegal channels (digital probes on Tek MSO)? Means we can't use row number as channel number
	if(meter->GetChannelCount() >= 2)
	{
		m_grid.attach(m_inputLabel, 0, 0, 1, 1);
			m_inputLabel.set_text("Input Select");
			m_grid.attach_next_to(m_inputBox, m_inputLabel, Gtk::POS_RIGHT, 1, 1);
				for(size_t i=0; i<meter->GetChannelCount(); i++)
					m_inputBox.append(meter->GetChannel(i)->GetDisplayName());
	}

	m_grid.attach(m_rateLabel, 0, 1, 1, 1);
		m_rateLabel.set_text("Update Rate");
	m_grid.attach(m_rateBox, 1, 1, 1, 1);
		m_rateBox.append("1 Hz");
		m_rateBox.append("2 Hz");
		m_rateBox.append("5 Hz");
		m_rateBox.set_active_text("1 Hz");

	m_grid.attach(m_primaryFrame, 0, 2, 2, 1);
		m_primaryFrame.set_label("Primary Measurement");
		m_primaryFrame.add(m_primaryGrid);

		m_primaryGrid.attach(m_typeLabel, 0, 0, 1, 1);
			m_typeLabel.set_text("Mode");
		m_primaryGrid.attach(m_typeBox, 1, 0, 1, 1);

		m_primaryGrid.attach(m_valueLabel, 0, 1, 1, 1);
			m_valueLabel.set_text("Value");
		m_primaryGrid.attach(m_valueBox, 1, 1, 1, 1);
			m_valueBox.override_font(Pango::FontDescription("monospace bold 20"));

	m_grid.attach(m_secondaryFrame, 0, 3, 2, 1);
		m_secondaryFrame.set_label("Secondary Measurement");
		m_secondaryFrame.add(m_secondaryGrid);

		m_secondaryGrid.attach(m_secondaryTypeLabel, 0, 0, 1, 1);
			m_secondaryTypeLabel.set_text("Mode");
		m_secondaryGrid.attach(m_secondaryTypeBox, 1, 0, 1, 1);

		m_secondaryGrid.attach(m_secondaryValueLabel, 0, 1, 1, 1);
			m_secondaryValueLabel.set_text("Value");
		m_secondaryGrid.attach(m_secondaryValueBox, 1, 1, 1, 1);
			m_secondaryValueBox.override_font(Pango::FontDescription("monospace bold 20"));

	//Allow resizing
	m_primaryFrame.set_hexpand(true);
	m_primaryFrame.set_vexpand(true);
	m_secondaryFrame.set_hexpand(true);
	m_secondaryFrame.set_vexpand(true);

	AddMode(Multimeter::DC_VOLTAGE, "DC Voltage");
	AddMode(Multimeter::DC_RMS_AMPLITUDE, "RMS Amplitude (DC coupled)");
	AddMode(Multimeter::AC_RMS_AMPLITUDE, "RMS Amplitude (AC coupled)");
	AddMode(Multimeter::FREQUENCY, "Frequency");
	AddMode(Multimeter::DC_CURRENT, "DC Current");
	AddMode(Multimeter::AC_CURRENT, "AC Current");
	AddMode(Multimeter::TEMPERATURE, "Temperature");

	//Get current meter modes
	m_typeBox.set_active_text(m_revmodemap[m_meter->GetMeterMode()]);
	RefreshSecondaryModeList();

	//Event handlers
	m_rateBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnTimerIntervalChanged));
	m_inputBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnInputChanged));
	m_typeBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnModeChanged));
	m_secondaryTypeBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnSecondaryModeChanged));

	//Enable the meter on the first channel by default
	m_inputBox.set_active_text(meter->GetChannel(meter->GetCurrentMeterChannel())->GetDisplayName());

	//Set up a timer for pulling updates
	//TODO: make update rate configurable
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &MultimeterDialog::OnTimer), 1000);
}

MultimeterDialog::~MultimeterDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void MultimeterDialog::AddMode(Multimeter::MeasurementTypes type, const std::string& label)
{
	if(m_meter->GetMeasurementTypes() & type)
	{
		m_typeBox.append(label);
		m_modemap[label] = type;

		m_revmodemap[type] = label;
	}
}

void MultimeterDialog::on_show()
{
	Gtk::Dialog::on_show();
	show_all();

	m_meter->StartMeter();
}

void MultimeterDialog::on_hide()
{
	Gtk::Dialog::on_hide();

	m_meter->StopMeter();
}

void MultimeterDialog::OnInputChanged()
{
	auto nchan = m_inputBox.get_active_row_number();
	m_meter->SetCurrentMeterChannel(nchan);
}

void MultimeterDialog::OnModeChanged()
{
	auto mode = m_modemap[m_typeBox.get_active_text()];
	m_meter->SetMeterMode(mode);

	RefreshSecondaryModeList();
}

bool MultimeterDialog::OnTimer()
{
	//TODO: pull values in a background thread as fast as we can to avoid bogging down the GUI thread?
	//How does this play with scope based meters that we don't want to spam?

	//Update text display
	double value = m_meter->GetMeterValue();
	m_valueBox.set_text(m_meter->GetMeterUnit().PrettyPrint(value, m_meter->GetMeterDigits()));

	//No secondary measurement? Nothing to do
	double secvalue = 0;
	if(m_meter->GetSecondaryMeterMode() == Multimeter::NONE)
		m_secondaryValueBox.set_text("");

	//Process secondary measurements
	else
	{
		secvalue = m_meter->GetSecondaryMeterValue();
		m_secondaryValueBox.set_text(m_meter->GetSecondaryMeterUnit().PrettyPrint(secvalue, m_meter->GetMeterDigits()));
	}

	//Reset timer if interval was changed
	if(m_timerIntervalChanged)
	{
		m_timerIntervalChanged = false;

		int interval = 1000;
		switch(m_rateBox.get_active_row_number())
		{
			case UPDATE_1HZ:
				interval = 1000;
				break;

			case UPDATE_2HZ:
				interval = 500;
				break;

			case UPDATE_5HZ:
				interval = 200;
				break;

			default:
				break;
		}
		Glib::signal_timeout().connect(sigc::mem_fun(*this, &MultimeterDialog::OnTimer), interval);
		return false;
	}
	else
		return true;
}

void MultimeterDialog::RefreshSecondaryModeList()
{
	m_updatingSecondary = true;

	m_secmodemap.clear();
	m_revsecmodemap.clear();
	m_secondaryTypeBox.remove_all();

	AddSecondaryMode(Multimeter::FREQUENCY, "Frequency");

	//No secondary is always a valid option
	m_secondaryTypeBox.append("None");
	m_secmodemap["None"] = Multimeter::NONE;
	m_revsecmodemap[Multimeter::NONE] = "None";

	//Select the active stuff
	auto mode = m_meter->GetSecondaryMeterMode();
	m_secondaryTypeBox.set_active_text(m_revmodemap[mode]);

	m_secondaryValueBox.set_text("");

	m_updatingSecondary = false;
}

void MultimeterDialog::AddSecondaryMode(Multimeter::MeasurementTypes type, const std::string& label)
{
	if(m_meter->GetSecondaryMeasurementTypes() & type)
	{
		m_secondaryTypeBox.append(label);
		m_secmodemap[label] = type;
		m_revsecmodemap[type] = label;
	}
}

void MultimeterDialog::OnSecondaryModeChanged()
{
	if(m_updatingSecondary)
		return;

	auto mode = m_secmodemap[m_secondaryTypeBox.get_active_text()];
	m_meter->SetSecondaryMeterMode(mode);
}
