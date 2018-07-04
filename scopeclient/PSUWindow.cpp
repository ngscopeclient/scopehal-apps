/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2017 Andrew D. Zonenberg                                                                          *
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

#include "scopeclient.h"
#include "PSUWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
PSUWindow::PSUWindow(PowerSupply* psu, std::string host, int port)
	: m_psu(psu)
	, m_hostname(host)
{
	//Set title
	char title[256];
	snprintf(title, sizeof(title), "Power supply: %s:%d (%s %s, serial %s)",
		host.c_str(),
		port,
		psu->GetVendor().c_str(),
		psu->GetName().c_str(),
		psu->GetSerial().c_str()
		);
	set_title(title);

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(640, 240);

	//Add widgets
	CreateWidgets();

	//Done adding widgets
	show_all();

	//Set the update timer
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &PSUWindow::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, 500);
}

/**
	@brief Application cleanup
 */
PSUWindow::~PSUWindow()
{
	for(auto x : m_voltageData)
		delete x;
	m_voltageData.clear();

	for(auto x : m_currentData)
		delete x;
	m_currentData.clear();
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void PSUWindow::CreateWidgets()
{
	//Set up window hierarchy
	add(m_vbox);
		m_vbox.pack_start(m_masterEnableHbox, Gtk::PACK_SHRINK);
			m_masterEnableHbox.pack_start(m_masterEnableLabel, Gtk::PACK_SHRINK);
				m_masterEnableLabel.set_label("Master");
				m_masterEnableLabel.set_halign(Gtk::ALIGN_START);
				m_masterEnableLabel.set_size_request(150, -1);
				m_masterEnableLabel.override_font(Pango::FontDescription("sans bold 24"));
			m_masterEnableHbox.pack_start(m_masterEnableButton, Gtk::PACK_SHRINK);
				m_masterEnableButton.override_font(Pango::FontDescription("sans bold 24"));
				m_masterEnableButton.set_active(m_psu->GetMasterPowerEnable());
				m_masterEnableButton.set_halign(Gtk::ALIGN_START);
			m_masterEnableHbox.pack_start(m_revertButton, Gtk::PACK_EXPAND_WIDGET);
				m_revertButton.override_font(Pango::FontDescription("sans bold 16"));
				m_revertButton.set_halign(Gtk::ALIGN_END);
				m_revertButton.set_label("Revert");
				m_revertButton.set_image_from_icon_name("gtk-clear");
			m_masterEnableHbox.pack_start(m_commitButton, Gtk::PACK_SHRINK);
				m_commitButton.override_font(Pango::FontDescription("sans bold 16"));
				m_commitButton.set_halign(Gtk::ALIGN_END);
				m_commitButton.set_label("Commit");
				m_commitButton.set_image_from_icon_name("gtk-execute");
	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
	{
		//Create boxes
		m_hseps.push_back(Gtk::HSeparator());
		m_vhboxes.push_back(Gtk::HBox());
		m_channelLabelHboxes.push_back(Gtk::HBox());
		m_vmhboxes.push_back(Gtk::HBox());
		m_chanhboxes.push_back(Gtk::HBox());
		m_voltboxes.push_back(Gtk::VBox());
		m_currboxes.push_back(Gtk::VBox());
		m_ihboxes.push_back(Gtk::HBox());
		m_imhboxes.push_back(Gtk::HBox());

		//Create and set up labels and controls
		m_channelLabels.push_back(Gtk::Label());
			m_channelLabels[i].set_text(m_psu->GetPowerChannelName(i));
			m_channelLabels[i].set_halign(Gtk::ALIGN_START);
			m_channelLabels[i].override_font(Pango::FontDescription("sans bold 24"));
			m_channelLabels[i].set_size_request(150, -1);
		m_channelEnableButtons.push_back(Gtk::ToggleButton());
			m_channelEnableButtons[i].override_font(Pango::FontDescription("sans bold 16"));
			m_channelEnableButtons[i].set_halign(Gtk::ALIGN_START);
		m_voltageLabels.push_back(Gtk::Label());
			m_voltageLabels[i].set_text("Voltage (nominal)");
			m_voltageLabels[i].set_size_request(150, -1);
		m_voltageEntries.push_back(Gtk::Entry());
			m_voltageEntries[i].set_width_chars(6);
			m_voltageEntries[i].override_font(Pango::FontDescription("monospace bold 32"));
		m_mvoltageLabels.push_back(Gtk::Label());
			m_mvoltageLabels[i].set_text("Voltage (measured)");
			m_mvoltageLabels[i].set_size_request(150, -1);
		m_voltageValueLabels.push_back(Gtk::Label());
			m_voltageValueLabels[i].set_alignment(0, 0.5);
			m_voltageValueLabels[i].override_font(Pango::FontDescription("monospace bold 32"));
			m_voltageValueLabels[i].set_text("---");
		m_currentLabels.push_back(Gtk::Label());
			m_currentLabels[i].set_text("Current (nominal)");
			m_currentLabels[i].set_size_request(150, -1);
		m_currentEntries.push_back(Gtk::Entry());
			m_currentEntries[i].set_width_chars(6);
			m_currentEntries[i].override_font(Pango::FontDescription("monospace bold 32"));
		m_mcurrentLabels.push_back(Gtk::Label());
			m_mcurrentLabels[i].set_text("Current (measured)");
			m_mcurrentLabels[i].set_size_request(150, -1);
		m_currentValueLabels.push_back(Gtk::Label());
			m_currentValueLabels[i].set_alignment(0, 0.5);
			m_currentValueLabels[i].override_font(Pango::FontDescription("monospace bold 32"));
			m_currentValueLabels[i].set_text("---");
		m_channelStatusLabels.push_back(Gtk::Label());
			m_channelStatusLabels[i].set_halign(Gtk::ALIGN_END);
			m_channelStatusLabels[i].set_text("--");
			m_channelStatusLabels[i].override_font(Pango::FontDescription("sans bold 24"));

		m_voltboxes[i].set_size_request(500, -1);
		m_currboxes[i].set_size_request(500, -1);

		m_hseps[i].set_size_request(-1, 15);

		//Pack stuff
		m_vbox.pack_start(m_hseps[i], Gtk::PACK_EXPAND_WIDGET);
		m_vbox.pack_start(m_channelLabelHboxes[i], Gtk::PACK_SHRINK);
			m_channelLabelHboxes[i].pack_start(m_channelLabels[i], Gtk::PACK_SHRINK);
			m_channelLabelHboxes[i].pack_start(m_channelEnableButtons[i], Gtk::PACK_SHRINK);
			m_channelLabelHboxes[i].pack_start(m_channelStatusLabels[i], Gtk::PACK_EXPAND_WIDGET);
		m_vbox.pack_start(m_chanhboxes[i], Gtk::PACK_SHRINK);
			m_chanhboxes[i].pack_start(m_voltboxes[i]);
				m_voltboxes[i].pack_start(m_vhboxes[i], Gtk::PACK_SHRINK);
					m_vhboxes[i].pack_start(m_voltageLabels[i], Gtk::PACK_SHRINK);
					m_vhboxes[i].pack_start(m_voltageEntries[i]);
				m_voltboxes[i].pack_start(m_vmhboxes[i], Gtk::PACK_SHRINK);
					m_vmhboxes[i].pack_start(m_mvoltageLabels[i], Gtk::PACK_SHRINK);
					m_vmhboxes[i].pack_start(m_voltageValueLabels[i], Gtk::PACK_SHRINK);
			m_chanhboxes[i].pack_start(m_currboxes[i]);
				m_currboxes[i].pack_start(m_ihboxes[i], Gtk::PACK_SHRINK);
					m_ihboxes[i].pack_start(m_currentLabels[i], Gtk::PACK_SHRINK);
					m_ihboxes[i].pack_start(m_currentEntries[i]);
				m_currboxes[i].pack_start(m_imhboxes[i], Gtk::PACK_SHRINK);
					m_imhboxes[i].pack_start(m_mcurrentLabels[i], Gtk::PACK_SHRINK);
					m_imhboxes[i].pack_start(m_currentValueLabels[i], Gtk::PACK_SHRINK);

		//Event handlers
		m_channelEnableButtons[i].property_active().signal_changed().connect(
			sigc::bind<int>(sigc::mem_fun(*this, &PSUWindow::OnChannelEnableChanged), i));

		m_voltageEntries[i].signal_changed().connect(
			sigc::bind<int>(sigc::mem_fun(*this, &PSUWindow::OnChannelVoltageChanged), i));
		m_currentEntries[i].signal_changed().connect(
			sigc::bind<int>(sigc::mem_fun(*this, &PSUWindow::OnChannelCurrentChanged), i));
	}
	m_vbox.pack_start(m_voltageFrame, Gtk::PACK_SHRINK);
		m_voltageFrame.set_label_widget(m_voltageFrameLabel);
			m_voltageFrameLabel.set_markup("<b>Output Voltage</b>");
			m_voltageFrame.set_shadow_type(Gtk::SHADOW_NONE);
		m_voltageFrame.add(m_voltageGraph);
			m_voltageGraph.m_minScale = 0;
			m_voltageGraph.m_maxScale = 1;
			m_voltageGraph.m_scaleBump = 1;
			m_voltageGraph.m_minRedline = -1;
			m_voltageGraph.m_maxRedline = 100;
			m_voltageGraph.m_units = "V";
			m_voltageGraph.m_yAxisTitle = "";
			m_voltageGraph.set_size_request(100, 200);
			m_voltageGraph.m_seriesName = "voltage";
			for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
			{
				char str[16];
				snprintf(str, sizeof(str), "CH%d", i+1);
				m_voltageData.push_back(new Graphable(str));
				m_voltageGraph.m_series.push_back(m_voltageData[i]);
				m_voltageData[i]->m_color.set(GetColor(i));
			}

	m_vbox.pack_start(m_currentFrame, Gtk::PACK_SHRINK);
		m_currentFrame.set_label_widget(m_currentFrameLabel);
			m_currentFrameLabel.set_markup("<b>Output Current</b>");
			m_currentFrame.set_shadow_type(Gtk::SHADOW_NONE);
		m_currentFrame.add(m_currentGraph);
			m_currentGraph.m_minScale = 0;
			m_currentGraph.m_maxScale = 1;
			m_currentGraph.m_scaleBump = 0.1;
			m_currentGraph.m_minRedline = -1;
			m_currentGraph.m_maxRedline = 100;
			m_currentGraph.m_units = "A";
			m_currentGraph.m_yAxisTitle = "";
			m_currentGraph.set_size_request(100, 200);
			m_currentGraph.m_seriesName = "current";
			for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
			{
				char str[16];
				snprintf(str, sizeof(str), "CH%d", i+1);
				m_currentData.push_back(new Graphable(str));
				m_currentGraph.m_series.push_back(m_currentData[i]);
				m_currentData[i]->m_color.set(GetColor(i));
			}

	//Revert changes (clear background and load all "nominal" text boxes with the right values
	OnRevertChanges();

	//Event handlers
	m_masterEnableButton.property_active().signal_changed().connect(
		sigc::mem_fun(*this, &PSUWindow::OnMasterEnableChanged));
	m_commitButton.signal_clicked().connect(
		sigc::mem_fun(*this, &PSUWindow::OnCommitChanges));
	m_revertButton.signal_clicked().connect(
		sigc::mem_fun(*this, &PSUWindow::OnRevertChanges));

	show_all();
}

string PSUWindow::GetColor(int i)
{
	//from colorbrewer2.org
	const char* g_colorTable[10]=
	{
		"#A6CEE3",
		"#1F78B4",
		"#B2DF8A",
		"#33A02C",
		"#FB9A99",
		"#E31A1C",
		"#FDBF6F",
		"#FF7F00",
		"#CAB2D6",
		"#6A3D9A"
	};

	//Special-case colors for azonenberg's lab
	//TODO: make some kind of config file for this
	if(m_hostname.find("left") != string::npos)
	{
		if(i == 0)
			return "#c0c020";
		else
			return "#a06060";
	}
	else if(m_hostname.find("right") != string::npos)
	{
		if(i == 0)
			return "#8080ff";
		else
			return "#80ff80";
	}

	return g_colorTable[i];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

void PSUWindow::OnMasterEnableChanged()
{
	m_psu->SetMasterPowerEnable(m_masterEnableButton.get_active());
}

void PSUWindow::OnCommitChanges()
{
	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
	{
		m_psu->SetPowerVoltage(i, atof(m_voltageEntries[i].get_text().c_str()));
		m_psu->SetPowerCurrent(i, atof(m_currentEntries[i].get_text().c_str()));
	}

	//reload text boxes with proper formatting
	OnRevertChanges();
}

void PSUWindow::OnRevertChanges()
{
	char tmp[128];
	float vmax = 0;
	float imax = 0;

	for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
	{
		float v = m_psu->GetPowerVoltageNominal(i);
		float c = m_psu->GetPowerCurrentNominal(i);

		//Rescale graph to fit this channel
		vmax = max(v, vmax);
		imax = max(c, imax);

		snprintf(tmp, sizeof(tmp), "%7.3f", v);
		m_voltageEntries[i].set_text(tmp);

		snprintf(tmp, sizeof(tmp), "%6.3f", c);
		m_currentEntries[i].set_text(tmp);

		//clear to white
		m_voltageEntries[i].override_background_color(Gdk::RGBA("#ffffff"));
		m_currentEntries[i].override_background_color(Gdk::RGBA("#ffffff"));
	}

	vmax = ceil(vmax);

	m_voltageGraph.m_maxScale = vmax + 1;

	//Pick scale ranges for current more intelligently
	m_currentGraph.m_maxRedline = imax;
	m_currentGraph.m_minRedline = -1;
	if(imax > 1)
	{
		m_currentGraph.m_maxScale = ceil(imax) + 0.25;
		m_currentGraph.m_scaleBump = 0.25;

		m_currentGraph.m_units = "A";
		m_currentGraph.m_unitScale = 1;
	}
	else if(imax > 0.25)
	{
		m_currentGraph.m_maxScale = imax + 0.1;
		m_currentGraph.m_scaleBump = 0.1;

		m_currentGraph.m_units = "mA";
		m_currentGraph.m_unitScale = 1000;
	}
	else
	{
		m_currentGraph.m_maxScale = imax + 0.05;
		m_currentGraph.m_scaleBump = 0.025;

		m_currentGraph.m_units = "mA";
		m_currentGraph.m_unitScale = 1000;
	}
}

void PSUWindow::OnChannelVoltageChanged(int i)
{
	//make yellow to indicate uncommitted changes
	m_voltageEntries[i].override_background_color(Gdk::RGBA("#ffffa0"));
}

void PSUWindow::OnChannelCurrentChanged(int i)
{
	//make yellow to indicate uncommitted changes
	m_currentEntries[i].override_background_color(Gdk::RGBA("#ffffa0"));
}

void PSUWindow::OnChannelEnableChanged(int i)
{
	m_psu->SetPowerChannelActive(i, m_channelEnableButtons[i].get_active());
}

void PSUWindow::on_show()
{
	Gtk::Window::on_show();
}

void PSUWindow::on_hide()
{
	Gtk::Window::on_hide();
}

bool PSUWindow::OnTimer(int /*timer*/)
{
	try
	{
		//Master enable
		m_masterEnableButton.set_active(m_psu->GetMasterPowerEnable());

		char tmp[128];
		for(int i=0; i<m_psu->GetPowerChannelCount(); i++)
		{
			//Channel voltage
			double v = m_psu->GetPowerVoltageActual(i);
			if(fabs(v) < 1)
				snprintf(tmp, sizeof(tmp), "%5.1f   mV", v * 1000);
			else
				snprintf(tmp, sizeof(tmp), "%7.3f  V", v);
			m_voltageValueLabels[i].set_text(tmp);

			//Update voltage graph with the new data
			auto vseries = m_voltageData[i]->GetSeries("voltage");
			vseries->push_back(GraphPoint(GetTime(), v));
			while(vseries->size() > 500)
				vseries->pop_front();

			//Channel current
			double c = m_psu->GetPowerCurrentActual(i);
			if(fabs(c) < 1)
				snprintf(tmp, sizeof(tmp), "%4.1f  mA", c * 1000);
			else
				snprintf(tmp, sizeof(tmp), "%6.3f A", c);
			m_currentValueLabels[i].set_text(tmp);

			//Update current graph with the new data
			auto iseries = m_currentData[i]->GetSeries("current");
			iseries->push_back(GraphPoint(GetTime(), c));
			while(iseries->size() > 500)
				iseries->pop_front();

			//Channel enable
			bool enabled = m_psu->GetPowerChannelActive(i);
			m_channelEnableButtons[i].set_active(enabled);

			//Channel status
			if(!enabled)
			{
				m_channelStatusLabels[i].set_label("--");
				m_channelStatusLabels[i].override_color(Gdk::RGBA("#000000"));
			}

			else if(m_psu->IsPowerConstantCurrent(i))
			{
				m_channelStatusLabels[i].set_label("CC");
				m_channelStatusLabels[i].override_color(Gdk::RGBA("#ff0000"));
			}

			else
			{
				m_channelStatusLabels[i].set_label("CV");
				m_channelStatusLabels[i].override_color(Gdk::RGBA("#00a000"));
			}
		}
	}

	catch(const JtagException& ex)
	{
		printf("%s\n", ex.GetDescription().c_str());
	}

	//false to stop timer
	return true;
}
