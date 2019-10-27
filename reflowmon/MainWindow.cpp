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

#include "reflowmon.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/Graph.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
MainWindow::MainWindow(Multimeter* dmm)
	: m_dmm(dmm)
{
	//Set title
	string title = "Reflow Monitor: ";
	char tt[256];
	snprintf(tt, sizeof(tt), "%s (%s %s, serial %s)",
		dmm->m_nickname.c_str(),
		dmm->GetVendor().c_str(),
		dmm->GetName().c_str(),
		dmm->GetSerial().c_str()
		);
	title += tt;
	set_title(title);

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
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void MainWindow::CreateWidgets()
{
	//Set up window hierarchy
	add(m_vbox);
		m_tempFrame.set_label("Temperature");
		m_vbox.pack_start(m_tempFrame, Gtk::PACK_SHRINK);
			m_tempFrame.add(m_tempBox);
				m_tempBox.pack_start(m_tempGraph, Gtk::PACK_SHRINK);
					m_tempGraph.set_size_request(1000, 300);
					m_tempGraph.m_units = "C";
					m_tempGraph.m_minScale = 0;
					m_tempGraph.m_maxScale = 270;
					m_tempGraph.m_scaleBump = 20;
					m_tempGraph.m_maxRedline = 219;
					m_tempGraph.m_series.push_back(&m_tempData);
					m_tempGraph.m_seriesName = "temp";
					m_tempGraph.m_timeScale = 1.5;
					m_tempGraph.m_timeTick = 30;
					m_tempGraph.m_drawLegend = false;
					m_tempData.m_color = Gdk::Color("#0000ff");
				m_tempBox.pack_start(m_tempLabelBox, Gtk::PACK_SHRINK);
					m_tempLabelBox.pack_start(m_tempLabel, Gtk::PACK_EXPAND_WIDGET);
						m_tempLabel.override_font(Pango::FontDescription("monospace bold 20"));
					m_tempLabelBox.pack_start(m_talLabel, Gtk::PACK_EXPAND_WIDGET);
						m_talLabel.override_font(Pango::FontDescription("monospace bold 20"));
						m_talLabel.set_label("TAL: 0 s");

		m_rateFrame.set_label("Ramp Rate");
		m_vbox.pack_start(m_rateFrame, Gtk::PACK_SHRINK);
			m_rateFrame.add(m_rateBox);
				m_rateBox.pack_start(m_rateGraph, Gtk::PACK_SHRINK);
					m_rateGraph.set_size_request(1000, 300);
					m_rateGraph.m_units = "C/s";
					m_rateGraph.m_minScale = -4;
					m_rateGraph.m_maxScale = 4;
					m_rateGraph.m_scaleBump = 1;
					m_rateGraph.m_series.push_back(&m_rateData);
					m_rateGraph.m_seriesName = "rate";
					m_rateGraph.m_timeScale = 1.5;
					m_rateGraph.m_timeTick = 30;
					m_rateGraph.m_drawLegend = false;
					m_rateGraph.m_minRedline = -999;
					m_rateData.m_color = Gdk::Color("#0000ff");
				m_rateBox.pack_start(m_rateLabel, Gtk::PACK_SHRINK);
					m_rateLabel.override_font(Pango::FontDescription("monospace bold 20"));

	//Done adding widgets
	show_all();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

bool MainWindow::OnTimer(int /*timer*/)
{
	double t = GetTime();
	double temp = m_dmm->GetTemperature();

	m_tempData.GetSeries("temp")->push_back(GraphPoint(t, temp));

	char str[32];
	snprintf(str, sizeof(str), "%.1f C ", temp);
	m_tempLabel.set_label(str);

	//TODO: make melting point configurable
	if(temp > 219)
		m_tal ++;
	snprintf(str, sizeof(str), "TAL: %d s ", m_tal);
	m_talLabel.set_label(str);

	//Save historical temp data to calculate ramp rate
	m_tempBuffer.push_back(temp);
	while(m_tempBuffer.size() > 5)
		m_tempBuffer.erase(m_tempBuffer.begin());

	float rate = (temp - m_tempBuffer[0]) / 5;

	m_rateData.GetSeries("rate")->push_back(GraphPoint(t, rate));

	snprintf(str, sizeof(str), "%.2f C/s ", rate);
	m_rateLabel.set_label(str);

	return true;
}
