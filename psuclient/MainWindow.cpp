/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of main application window class
 */

#include "psuclient.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/Graph.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ChannelRow

ChannelRow::ChannelRow(PowerSupply* psu, int chan)
	: m_psu(psu)
	, m_chan(chan)
{
	//Create the top level frame for all of our control widgets
	char name[256];
	snprintf(name, sizeof(name), "%s %s", psu->m_nickname.c_str(), psu->GetPowerChannelName(chan).c_str());

	m_frame = Gtk::manage(new Gtk::Frame);
	m_frame->set_label(name);

	//Horizontal box with controls on the left and load graph on the right
	auto hbox = Gtk::manage(new Gtk::HBox);
	m_frame->add(*hbox);

	//Vertical box for I/V settings
	auto vbox = Gtk::manage(new Gtk::VBox);
	hbox->pack_start(*vbox, Gtk::PACK_SHRINK);

	//Voltage and current ACTUAL box
	auto aframe = Gtk::manage(new Gtk::Frame);
	aframe->set_label("Actual");
	vbox->pack_start(*aframe, Gtk::PACK_SHRINK);
	auto avbox = Gtk::manage(new Gtk::VBox);
	aframe->add(*avbox);

	auto avvbox = Gtk::manage(new Gtk::HBox);
	avbox->pack_start(*avvbox, Gtk::PACK_SHRINK);
	auto avlabel = Gtk::manage(new Gtk::Label);
	avlabel->set_label("Voltage");
	avlabel->set_size_request(75, 1);
	avvbox->pack_start(*avlabel, Gtk::PACK_SHRINK);
	m_actualVoltageLabel = Gtk::manage(new Gtk::Label);
	m_actualVoltageLabel->override_font(Pango::FontDescription("monospace bold 20"));
	avvbox->pack_start(*m_actualVoltageLabel, Gtk::PACK_SHRINK);

	auto avibox = Gtk::manage(new Gtk::HBox);
	avbox->pack_start(*avibox, Gtk::PACK_SHRINK);
	auto ailabel = Gtk::manage(new Gtk::Label);
	ailabel->set_label("Current");
	ailabel->set_size_request(75, 1);
	avibox->pack_start(*ailabel, Gtk::PACK_SHRINK);
	m_actualCurrentLabel = Gtk::manage(new Gtk::Label);
	m_actualCurrentLabel->override_font(Pango::FontDescription("monospace bold 20"));
	avibox->pack_start(*m_actualCurrentLabel, Gtk::PACK_SHRINK);

	//Voltage and current SET POINT box
	auto tframe = Gtk::manage(new Gtk::Frame);
	tframe->set_label("Target");
	vbox->pack_start(*tframe, Gtk::PACK_SHRINK);

	auto tvbox = Gtk::manage(new Gtk::VBox);
	tframe->add(*tvbox);

	auto tvvbox = Gtk::manage(new Gtk::HBox);
	tvbox->pack_start(*tvvbox, Gtk::PACK_SHRINK);
	auto tvlabel = Gtk::manage(new Gtk::Label);
	tvlabel->set_label("Voltage");
	tvlabel->set_size_request(75, 1);
	tvvbox->pack_start(*tvlabel, Gtk::PACK_SHRINK);
	m_setVoltageEntry = Gtk::manage(new Gtk::Entry);
	m_setVoltageEntry->override_font(Pango::FontDescription("monospace bold 20"));
	tvvbox->pack_start(*m_setVoltageEntry, Gtk::PACK_SHRINK);

	auto tvibox = Gtk::manage(new Gtk::HBox);
	tvbox->pack_start(*tvibox, Gtk::PACK_SHRINK);
	auto tilabel = Gtk::manage(new Gtk::Label);
	tilabel->set_label("Current");
	tilabel->set_size_request(75, 1);
	tvibox->pack_start(*tilabel, Gtk::PACK_SHRINK);
	m_setCurrentEntry = Gtk::manage(new Gtk::Entry);
	m_setCurrentEntry->override_font(Pango::FontDescription("monospace bold 20"));
	tvibox->pack_start(*m_setCurrentEntry, Gtk::PACK_SHRINK);

	//Miscellaneous settings box
	auto sframe = Gtk::manage(new Gtk::Frame);
	sframe->set_label("Settings");
	sframe->set_margin_left(5);
	sframe->set_margin_right(5);
	hbox->pack_start(*sframe, Gtk::PACK_SHRINK);

	auto sbox = Gtk::manage(new Gtk::VBox);
	sframe->add(*sbox);

	//Checkboxes for settings
	m_softStartModeButton = Gtk::manage(new Gtk::CheckButton);
	m_softStartModeButton->set_label("Soft start");
	sbox->pack_start(*m_softStartModeButton, Gtk::PACK_SHRINK);

	auto ocbox = Gtk::manage(new Gtk::HBox);
	sbox->pack_start(*ocbox, Gtk::PACK_SHRINK);
	auto oclabel = Gtk::manage(new Gtk::Label);
	oclabel->set_text("Overcurrent mode");
	ocbox->pack_start(*oclabel, Gtk::PACK_SHRINK);
	m_overcurrentModeBox = Gtk::manage(new Gtk::ComboBoxText);
	m_overcurrentModeBox->append("Current limit");
	m_overcurrentModeBox->append("Shut down");
	ocbox->pack_start(*m_overcurrentModeBox, Gtk::PACK_SHRINK);
	oclabel->set_size_request(125, 1);

	auto pebox = Gtk::manage(new Gtk::HBox);
	sbox->pack_start(*pebox, Gtk::PACK_SHRINK);
	auto pelabel = Gtk::manage(new Gtk::Label);
	pelabel->set_text("Power");
	pebox->pack_start(*pelabel, Gtk::PACK_SHRINK);

	pelabel->set_size_request(125, 1);
	m_powerSwitch = Gtk::manage(new Gtk::Switch);
	pebox->pack_start(*m_powerSwitch, Gtk::PACK_SHRINK);

	//Vertical box for graphs
	auto gbox = Gtk::manage(new Gtk::VBox);
	hbox->pack_start(*gbox, Gtk::PACK_SHRINK);

	//Graphs for I/V
	m_voltageGraph = Gtk::manage(new Graph);
	m_voltageGraph->set_size_request(600, 100);
	m_voltageGraph->m_units = "V";
	m_voltageGraph->m_minScale = 0;
	m_voltageGraph->m_maxScale = 6;
	m_voltageGraph->m_scaleBump = 1;
	m_voltageGraph->m_series.push_back(&m_channelData);
	m_voltageGraph->m_seriesName = "voltage";
	gbox->pack_start(*m_voltageGraph, Gtk::PACK_SHRINK);

	m_currentGraph = Gtk::manage(new Graph);
	m_currentGraph->set_size_request(600, 100);
	m_currentGraph->m_units = "A";
	m_currentGraph->m_minScale = 0;
	m_currentGraph->m_maxScale = 5;
	m_currentGraph->m_scaleBump = 1;
	m_currentGraph->m_series.push_back(&m_channelData);
	m_currentGraph->m_seriesName = "current";
	gbox->pack_start(*m_currentGraph, Gtk::PACK_SHRINK);

	m_channelData.m_color = Gdk::Color("#0000ff");

	//Refresh status of controls from the hardware.
	//For now we only do this once at startup and don't poll for changes later.
	char tmp[128];
	double v = m_psu->GetPowerVoltageNominal(m_chan);
	FormatVoltage(tmp, sizeof(tmp), v);
	m_setVoltageEntry->set_text(tmp);

	double i = m_psu->GetPowerCurrentNominal(m_chan);
	FormatCurrent(tmp, sizeof(tmp), i);
	m_setCurrentEntry->set_text(tmp);

	m_powerSwitch->set_state(m_psu->GetPowerChannelActive(m_chan));

	if(m_psu->GetPowerOvercurrentShutdownEnabled(m_chan))
		m_overcurrentModeBox->set_active_text("Shut down");
	else
		m_overcurrentModeBox->set_active_text("Current limit");

	m_softStartModeButton->set_active(m_psu->IsSoftStartEnabled(m_chan));

	//Connect signal handlers after initial values are loaded
	m_powerSwitch->property_active().signal_changed().connect(sigc::mem_fun(*this, &ChannelRow::OnPowerSwitch));

	SetGraphLimits();
}

void ChannelRow::SetGraphLimits()
{
	//Set max range for graphs to 10% beyond the nominal values
	double v = m_psu->GetPowerVoltageNominal(m_chan);
	double i = m_psu->GetPowerCurrentNominal(m_chan);
	m_voltageGraph->m_maxScale = v * 1.1;
	m_currentGraph->m_maxScale = i * 1.1;

	//Set redline at the current limit
	m_currentGraph->m_maxRedline = i;

	//Set step sizes appropriately
	if(v > 6)
		m_voltageGraph->m_scaleBump = 2;
	else
		m_voltageGraph->m_scaleBump = 1;

	if(i > 1)
		m_currentGraph->m_scaleBump = 1;
	else if(i > 0.1)
		m_currentGraph->m_scaleBump = 0.1;
	else
		m_currentGraph->m_scaleBump = 0.025;

	//Set units
	if(i > 2)
	{
		m_currentGraph->m_units = "A";
		m_currentGraph->m_unitScale = 1;
	}
	else
	{
		m_currentGraph->m_units = "mA";
		m_currentGraph->m_unitScale = 1000;
	}
}

void ChannelRow::OnTimer()
{
	//If channel is off, nothing to do
	if(m_psu->GetPowerChannelActive(m_chan) )
	{
		//Refresh status from the hardware
		char tmp[128];
		double v = m_psu->GetPowerVoltageActual(m_chan);
		FormatVoltage(tmp, sizeof(tmp), v);
		m_actualVoltageLabel->set_text(tmp);

		double i = m_psu->GetPowerCurrentActual(m_chan);
		FormatCurrent(tmp, sizeof(tmp), i);
		m_actualCurrentLabel->set_text(tmp);

		//Add the new data to the graph
		double t = GetTime();
		m_channelData.GetSeries("voltage")->push_back(GraphPoint(t, v));
		m_channelData.GetSeries("current")->push_back(GraphPoint(t, i));
	}

	else
	{
		m_actualVoltageLabel->set_text("");
		m_actualCurrentLabel->set_text("");
	}
}

void ChannelRow::FormatVoltage(char* str, size_t len, double v)
{
	if(v > 1)
		snprintf(str, len, "%.3f V", v);
	else
		snprintf(str, len, "%.2f mV", v * 1000);
}

void ChannelRow::FormatCurrent(char* str, size_t len, double i)
{
	if(i > 1)
		snprintf(str, len, "%.3f A", i);
	else if(i > 0.1)
		snprintf(str, len, "%.1f mA", i * 1000);
	else if(i > 0.01)
		snprintf(str, len, "%.2f mA", i * 1000);
	else
		snprintf(str, len, "%.3f mA", i * 1000);
}

void ChannelRow::OnPowerSwitch()
{
	m_psu->SetPowerChannelActive(m_chan, m_powerSwitch->get_state());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
MainWindow::MainWindow(vector<PowerSupply*> psus)
	: m_psus(psus)
{
	//Set title
	string title = "Power Supply: ";
	for(size_t i=0; i<psus.size(); i++)
	{
		auto psu = psus[i];

		char tt[256];
		snprintf(tt, sizeof(tt), "%s (%s %s, serial %s)",
			psu->m_nickname.c_str(),
			psu->GetVendor().c_str(),
			psu->GetName().c_str(),
			psu->GetSerial().c_str()
			);

		if(i > 0)
			title += ", ";
		title += tt;
	}
	set_title(title);

	for(auto p : psus)
	{
		//Master power off? We don't like that as the UI has no place for a master power switch.
		//If it's on, no action required.
		if(p->GetMasterPowerEnable())
			continue;

		//Master power is off. If we have any channels enabled, then turn them off to prevent glitches
		//when we enable the master.
		for(int i=0; i<p->GetPowerChannelCount(); i++)
		{
			if(p->GetPowerChannelActive(i))
				p->SetPowerChannelActive(i, false);
		}

		//Turn the master on so we can use individual channel switches to turn everything on and off.
		p->SetMasterPowerEnable(true);
	}

	//Initial setup
	set_reallocate_redraws(true);

	//Add widgets
	CreateWidgets();

	//Set the update timer
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &MainWindow::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, 1000);
}

/**
	@brief Application cleanup
 */
MainWindow::~MainWindow()
{
	for(auto row : m_rows)
		delete row;
	m_rows.clear();
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void MainWindow::CreateWidgets()
{
	//Set up window hierarchy
	add(m_vbox);
		/*
		m_vbox.pack_start(m_menu, Gtk::PACK_SHRINK);
			m_menu.append(m_fileMenuItem);
				m_fileMenuItem.set_label("File");
				m_fileMenuItem.set_submenu(m_fileMenu);
					auto item = Gtk::manage(new Gtk::MenuItem("Quit", false));
					item->signal_activate().connect(
						sigc::mem_fun(*this, &MainWindow::OnQuit));
					m_fileMenu.append(*item);
				*/

	//Process all of the channels
	for(auto psu : m_psus)
	{
		for(int i=0; i<psu->GetPowerChannelCount(); i++)
		{
			auto row = new ChannelRow(psu, i);
			m_rows.push_back(row);
			m_vbox.pack_start(*row->GetFrame(), Gtk::PACK_SHRINK);
		}
	}

	//Done adding widgets
	show_all();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

bool MainWindow::OnTimer(int /*timer*/)
{
	for(auto row : m_rows)
		row->OnTimer();

	return true;
}

void MainWindow::OnQuit()
{
	close();
}
