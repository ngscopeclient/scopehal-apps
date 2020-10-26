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
	m_grid.attach_next_to(m_valueLabel, m_typeLabel, Gtk::POS_BOTTOM, 1, 1);
		m_valueLabel.set_text("Value");
		m_grid.attach_next_to(m_valueBox, m_valueLabel, Gtk::POS_RIGHT, 1, 1);
		m_valueBox.override_font(Pango::FontDescription("monospace bold 20"));
	get_vbox()->pack_end(m_graph, Gtk::PACK_EXPAND_WIDGET);

	//Graph setup
	m_graph.set_size_request(600, 100);
	m_graph.m_units = "V";
	m_graph.m_series.push_back(&m_graphData);
	m_graph.m_seriesName = "data";
	m_graph.m_axisColor = Gdk::Color("#ffffff");
	m_graph.m_backgroundColor = Gdk::Color("#101010");
	m_graph.m_drawLegend = false;
	m_graph.m_series.push_back(&m_graphData);
	m_graphData.m_color = Gdk::Color("#ff0000");

	//Default values
	m_graph.m_minScale = 0;
	m_graph.m_maxScale = 1;
	m_graph.m_scaleBump = 0.1;
	m_graph.m_sigfigs = 3;

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

	//Set up a timer for pulling updates
	//TODO: make update rate configurable
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &MultimeterDialog::OnTimer), 1000);

	m_minval = FLT_MAX;
	m_maxval = -FLT_MAX;
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
	m_minval = FLT_MAX;
	m_maxval = -FLT_MAX;

	auto nchan = m_inputBox.get_active_row_number();
	m_meter->SetCurrentMeterChannel(nchan);

	m_graphData.m_name = m_meter->GetMeterChannelName(nchan);
}

void MultimeterDialog::OnModeChanged()
{
	m_minval = FLT_MAX;
	m_maxval = -FLT_MAX;

	auto mode = m_modemap[m_typeBox.get_active_text()];
	m_meter->SetMeterMode(mode);
}

bool MultimeterDialog::OnTimer()
{
	//Update text display
	double value = m_meter->GetMeterValue();
	m_valueBox.set_text(m_meter->GetMeterUnit().PrettyPrint(value, m_meter->GetMeterDigits()));

	//Add new value to the graph and trim to fit
	auto series = m_graphData.GetSeries("data");
	series->push_back(GraphPoint(GetTime(), value));
	const int max_points = 4096;
	while(series->size() > max_points)
		series->pop_front();

	//Update graph limits
	m_minval = min(m_minval, value);
	m_maxval = max(m_maxval, value);

	m_graph.m_minScale = m_minval;
	m_graph.m_maxScale = m_maxval;
	double range = abs(m_maxval - m_minval);
	if(range > 5)
		m_graph.m_scaleBump = 2.5;
	else if(range >= 0.5)
		m_graph.m_scaleBump = 0.25;
	else if(range > 0.05)
		m_graph.m_scaleBump = 0.1;
	else
		m_graph.m_scaleBump = 0.025;


	return true;
}
