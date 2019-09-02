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
	@brief Top-level window for the application
 */

#ifndef MainWindow_h
#define MainWindow_h

#include "../scopehal/PowerSupply.h"

class ChannelRow
{
public:
	ChannelRow(PowerSupply* psu, int chan);

	Gtk::Frame* GetFrame()
	{ return m_frame; }

	void OnTimer();

protected:
	Gtk::Frame* m_frame;
	Gtk::Label* m_actualVoltageLabel;
	Gtk::Label* m_actualCurrentLabel;

	Graph* m_currentGraph;
	Graph* m_voltageGraph;
	Graphable m_channelData;

	PowerSupply* m_psu;
	int m_chan;

	void FormatVoltage(char* str, size_t len, double v);
	void FormatCurrent(char* str, size_t len, double i);
};

/**
	@brief Main application window class for a power supply
 */
class MainWindow	: public Gtk::Window
{
public:
	MainWindow(std::vector<PowerSupply*> psus);
	~MainWindow();

	size_t GetPSUCount()
	{ return m_psus.size(); }

	PowerSupply* GetPSU(size_t i)
	{ return m_psus[i]; }

protected:
	void OnQuit();

	//Initialization
	void CreateWidgets();

	//Widgets
	Gtk::VBox m_vbox;
		Gtk::MenuBar m_menu;
			Gtk::MenuItem m_fileMenuItem;
				Gtk::Menu m_fileMenu;
		/*
		Gtk::HBox m_toolbox;
			Gtk::Toolbar m_toolbar;
		*/

	bool OnTimer(int timer);

protected:

	//Our PSU connections
	std::vector<PowerSupply*> m_psus;

	std::vector<ChannelRow*> m_rows;
};

#endif
