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
	, m_updatingSecondary(false)
{
	get_vbox()->pack_start(m_grid, Gtk::PACK_EXPAND_WIDGET);

	//TODO: hide input selector if we only have one input?
	//TODO: have some means of refreshing channel list when a channel is renamed
	//TODO: hide illegal channels (digital probes on Tek MSO)? Means we can't use row number as channel number
	if(meter->GetMeterChannelCount() >= 2)
	{
		m_grid.attach(m_inputLabel, 0, 0, 1, 1);
			m_inputLabel.set_text("Input Select");
			m_grid.attach_next_to(m_inputBox, m_inputLabel, Gtk::POS_RIGHT, 1, 1);
				for(int i=0; i<meter->GetMeterChannelCount(); i++)
					m_inputBox.append(meter->GetMeterChannelName(i));
	}

	m_grid.attach(m_primaryFrame, 0, 1, 2, 1);
		m_primaryFrame.set_label("Primary Measurement");
		m_primaryFrame.add(m_primaryGrid);

		m_primaryGrid.attach(m_typeLabel, 0, 0, 1, 1);
			m_typeLabel.set_text("Mode");
		m_primaryGrid.attach(m_typeBox, 1, 0, 1, 1);

		m_primaryGrid.attach(m_valueLabel, 0, 1, 1, 1);
			m_valueLabel.set_text("Value");
		m_primaryGrid.attach(m_valueBox, 1, 1, 1, 1);
			m_valueBox.override_font(Pango::FontDescription("monospace bold 20"));

		m_primaryGrid.attach(m_graph, 0, 2, 2, 1);

	m_grid.attach(m_secondaryFrame, 0, 2, 2, 1);
		m_secondaryFrame.set_label("Secondary Measurement");
		m_secondaryFrame.add(m_secondaryGrid);

		m_secondaryGrid.attach(m_secondaryTypeLabel, 0, 0, 1, 1);
			m_secondaryTypeLabel.set_text("Mode");
		m_secondaryGrid.attach(m_secondaryTypeBox, 1, 0, 1, 1);

		m_secondaryGrid.attach(m_secondaryValueLabel, 0, 1, 1, 1);
			m_secondaryValueLabel.set_text("Value");
		m_secondaryGrid.attach(m_secondaryValueBox, 1, 1, 1, 1);
			m_secondaryValueBox.override_font(Pango::FontDescription("monospace bold 20"));

		m_secondaryGrid.attach(m_secondaryGraph, 0, 2, 2, 1);

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

	m_secondaryGraph.set_size_request(600, 100);
	m_secondaryGraph.m_units = "V";
	m_secondaryGraph.m_series.push_back(&m_secondaryGraphData);
	m_secondaryGraph.m_seriesName = "data";
	m_secondaryGraph.m_axisColor = Gdk::Color("#ffffff");
	m_secondaryGraph.m_backgroundColor = Gdk::Color("#101010");
	m_secondaryGraph.m_drawLegend = false;
	m_secondaryGraph.m_series.push_back(&m_secondaryGraphData);
	m_secondaryGraphData.m_color = Gdk::Color("#ff0000");

	//Default values
	m_graph.m_minScale = 0;
	m_graph.m_maxScale = 1;
	m_graph.m_scaleBump = 0.1;
	m_graph.m_sigfigs = 3;

	m_secondaryGraph.m_minScale = 0;
	m_secondaryGraph.m_maxScale = 1;
	m_secondaryGraph.m_scaleBump = 0.1;
	m_secondaryGraph.m_sigfigs = 3;

	m_secondaryGraph.m_minRedline = -FLT_MAX;
	m_secondaryGraph.m_maxRedline = FLT_MAX;

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
	m_inputBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnInputChanged));
	m_typeBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnModeChanged));
	m_secondaryTypeBox.signal_changed().connect(sigc::mem_fun(*this, &MultimeterDialog::OnSecondaryModeChanged));

	//Enable the meter on the first channel by default
	m_inputBox.set_active_text(meter->GetMeterChannelName(meter->GetCurrentMeterChannel()));

	//Set up a timer for pulling updates
	//TODO: make update rate configurable
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &MultimeterDialog::OnTimer), 1000);

	m_minval = FLT_MAX;
	m_maxval = -FLT_MAX;

	m_secminval = FLT_MAX;
	m_secmaxval = -FLT_MAX;
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

	m_secminval = FLT_MAX;
	m_secmaxval = -FLT_MAX;

	auto mode = m_modemap[m_typeBox.get_active_text()];
	m_meter->SetMeterMode(mode);

	RefreshSecondaryModeList();
}

bool MultimeterDialog::OnTimer()
{
	//Update text display
	double value = m_meter->GetMeterValue();
	m_valueBox.set_text(m_meter->GetMeterUnit().PrettyPrint(value, m_meter->GetMeterDigits()));

	//Add new value to the graph and trim to fit
	double now = GetTime();
	auto series = m_graphData.GetSeries("data");
	series->push_back(GraphPoint(now, value));
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

	//No secondary measurement? Nothing to do
	if(m_meter->GetSecondaryMeterMode() == Multimeter::NONE)
		m_secondaryValueBox.set_text("");

	//Process secondary measurements
	else
	{
		//Update text display
		value = m_meter->GetSecondaryMeterValue();
		m_secondaryValueBox.set_text(m_meter->GetSecondaryMeterUnit().PrettyPrint(value, m_meter->GetMeterDigits()));

		//Add new value to the graph and trim to fit
		auto secseries = m_secondaryGraphData.GetSeries("data");
		secseries->push_back(GraphPoint(now, value));
		while(secseries->size() > max_points)
			secseries->pop_front();

		//Update graph limits
		m_secminval = min(m_secminval, value);
		m_secmaxval = max(m_secmaxval, value);

		m_secondaryGraph.m_minScale = m_secminval;
		m_secondaryGraph.m_maxScale = m_secmaxval;
		range = abs(m_secmaxval - m_secminval);
		if(range > 500000)
			m_secondaryGraph.m_scaleBump = 250000;
		else if(range > 50000)
			m_secondaryGraph.m_scaleBump = 25000;
		else if(range > 5000)
			m_secondaryGraph.m_scaleBump = 2500;
		else if(range > 500)
			m_secondaryGraph.m_scaleBump = 250;
		else if(range > 50)
			m_secondaryGraph.m_scaleBump = 25;
		else if(range > 5)
			m_secondaryGraph.m_scaleBump = 2.5;
		else if(range >= 0.5)
			m_secondaryGraph.m_scaleBump = 0.25;
		else if(range > 0.05)
			m_secondaryGraph.m_scaleBump = 0.1;
		else
			m_secondaryGraph.m_scaleBump = 0.025;
	}

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

	m_secondaryValueBox.set_text("");

	//Select the active stuff
	m_secondaryTypeBox.set_active_text(m_revmodemap[m_meter->GetSecondaryMeterMode()]);

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

	m_secminval = FLT_MAX;
	m_secmaxval = -FLT_MAX;

	auto mode = m_secmodemap[m_secondaryTypeBox.get_active_text()];
	m_meter->SetSecondaryMeterMode(mode);
}
