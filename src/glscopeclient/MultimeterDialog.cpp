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
	@brief Implementation of MultimeterDialog
 */
#include "glscopeclient.h"
#include "MultimeterDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MultimeterDialog::MultimeterDialog(Multimeter* meter)
	: Gtk::Dialog( string("Multimeter: ") + meter->m_nickname )
	, m_meter(meter)
{
	get_vbox()->add(m_grid);

	//TODO: hide input selector if we only have one input?
	//TODO: have some means of refreshing channel list when a channel is renamed
	//TODO: hide illegal channels (digital probes on Tek MSO)? Means we can't use row number as channel number
	m_grid.attach(m_inputLabel, 0, 0, 1, 1);
		m_inputLabel.set_text("Input Select");
	m_grid.attach_next_to(m_inputBox, m_inputLabel, Gtk::POS_RIGHT, 1, 1);
		for(int i=0; i<meter->GetMeterChannelCount(); i++)
			m_inputBox.append(meter->GetMeterChannelName(i));
	m_grid.attach_next_to(m_typeLabel, m_inputLabel, Gtk::POS_BOTTOM, 1, 1);
		m_typeLabel.set_text("Measurement Type");
	m_grid.attach_next_to(m_typeBox, m_typeLabel, Gtk::POS_RIGHT, 1, 1);

	AddMode(Multimeter::DC_VOLTAGE, "DC Voltage");
	AddMode(Multimeter::DC_RMS_AMPLITUDE, "RMS Amplitude (DC coupled)");
	AddMode(Multimeter::AC_RMS_AMPLITUDE, "RMS Amplitude (AC coupled)");
	AddMode(Multimeter::FREQUENCY, "Frequency");
	AddMode(Multimeter::DC_CURRENT, "DC Current");
	AddMode(Multimeter::AC_CURRENT, "AC Current");
	AddMode(Multimeter::TEMPERATURE, "Temperature");

	//Put meter in DC voltage mode by default
	//TODO: load selected mode instead
	//TODO: what happens if meter doesn't support DC voltage?
	m_typeBox.set_active_text("DC Voltage");
	meter->SetMeterMode(Multimeter::DC_VOLTAGE);

	//Event handlers
	m_inputBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnInputChanged));
	m_typeBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnModeChanged));

	//Enable the meter on the first channel by default
	m_inputBox.set_active_text(meter->GetMeterChannelName(0));
	meter->SetCurrentMeterChannel(0);
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
	m_meter->SetCurrentMeterChannel(m_inputBox.get_active_row_number());
}

void MultimeterDialog::OnModeChanged()
{
	auto mode = m_modemap[m_typeBox.get_active_text()];
	m_meter->SetMeterMode(mode);
}
