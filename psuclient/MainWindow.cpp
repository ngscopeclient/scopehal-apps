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

////////////////////////////////////////////////////////////////////////////////////////////////////
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

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Add widgets
	CreateWidgets();
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
		m_vbox.pack_start(m_menu, Gtk::PACK_SHRINK);
			m_menu.append(m_fileMenuItem);
				m_fileMenuItem.set_label("File");
				m_fileMenuItem.set_submenu(m_fileMenu);
					auto item = Gtk::manage(new Gtk::MenuItem("Quit", false));
					item->signal_activate().connect(
						sigc::mem_fun(*this, &MainWindow::OnQuit));
					m_fileMenu.append(*item);

		/*
		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
		m_statusbar.pack_end(m_triggerConfigLabel, Gtk::PACK_SHRINK);
		m_triggerConfigLabel.set_size_request(75, 1);
		*/

	//Process all of the channels
	for(auto psu : m_psus)
	{
		for(int i=0; i<psu->GetPowerChannelCount(); i++)
		{
			//Create the top level frame for all of our control widgets
			char name[256];
			snprintf(name, sizeof(name), "%s %s", psu->m_nickname.c_str(), psu->GetPowerChannelName(i).c_str());

			auto frame = Gtk::manage(new Gtk::Frame);
			frame->set_label(name);

			m_vbox.pack_start(*frame, Gtk::PACK_SHRINK);

			//Horizontal box with controls on the left and load graph on the right
			auto hbox = Gtk::manage(new Gtk::HBox);
			frame->add(*hbox);

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
			auto aventry = Gtk::manage(new Gtk::Entry);
			avvbox->pack_start(*aventry, Gtk::PACK_SHRINK);

			auto avibox = Gtk::manage(new Gtk::HBox);
			avbox->pack_start(*avibox, Gtk::PACK_SHRINK);
			auto ailabel = Gtk::manage(new Gtk::Label);
			ailabel->set_label("Current");
			ailabel->set_size_request(75, 1);
			avibox->pack_start(*ailabel, Gtk::PACK_SHRINK);
			auto aientry = Gtk::manage(new Gtk::Entry);
			avibox->pack_start(*aientry, Gtk::PACK_SHRINK);

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
			auto tventry = Gtk::manage(new Gtk::Entry);
			tvvbox->pack_start(*tventry, Gtk::PACK_SHRINK);

			auto tvibox = Gtk::manage(new Gtk::HBox);
			tvbox->pack_start(*tvibox, Gtk::PACK_SHRINK);
			auto tilabel = Gtk::manage(new Gtk::Label);
			tilabel->set_label("Current");
			tilabel->set_size_request(75, 1);
			tvibox->pack_start(*tilabel, Gtk::PACK_SHRINK);
			auto tientry = Gtk::manage(new Gtk::Entry);
			tvibox->pack_start(*tientry, Gtk::PACK_SHRINK);

			//Miscellaneous settings box
			auto sframe = Gtk::manage(new Gtk::Frame);
			sframe->set_label("Settings");
			hbox->pack_start(*sframe, Gtk::PACK_SHRINK);

			auto sbox = Gtk::manage(new Gtk::VBox);
			sframe->add(*sbox);

			//Checkboxes for settings
			auto sstart = Gtk::manage(new Gtk::CheckButton);
			sstart->set_label("Soft start");
			sbox->pack_start(*sstart, Gtk::PACK_SHRINK);

			auto ocbox = Gtk::manage(new Gtk::HBox);
			sbox->pack_start(*ocbox, Gtk::PACK_SHRINK);
			auto oclabel = Gtk::manage(new Gtk::Label);
			oclabel->set_text("Overcurrent mode");
			ocbox->pack_start(*oclabel, Gtk::PACK_SHRINK);
			auto occombo = Gtk::manage(new Gtk::ComboBoxText);
			occombo->append("Current limit");
			occombo->append("Shut down");
			ocbox->pack_start(*occombo, Gtk::PACK_SHRINK);
			oclabel->set_size_request(125, 1);

			auto pebox = Gtk::manage(new Gtk::HBox);
			sbox->pack_start(*pebox, Gtk::PACK_SHRINK);
			auto pelabel = Gtk::manage(new Gtk::Label);
			pelabel->set_text("Power");
			pebox->pack_start(*pelabel, Gtk::PACK_SHRINK);

			pelabel->set_size_request(125, 1);
			auto pswitch = Gtk::manage(new Gtk::Switch);
			pebox->pack_start(*pswitch, Gtk::PACK_SHRINK);

			//Vertical box for graphs
			auto gbox = Gtk::manage(new Gtk::VBox);
			hbox->pack_start(*gbox, Gtk::PACK_SHRINK);

			//Graphs for I/V
			auto vgraph = Gtk::manage(new Graph);
			vgraph->set_size_request(600, 100);
			vgraph->m_units = "V";
			vgraph->m_minScale = 0;
			vgraph->m_maxScale = 5;
			vgraph->m_scaleBump = 1;
			gbox->pack_start(*vgraph, Gtk::PACK_SHRINK);

			auto igraph = Gtk::manage(new Graph);
			igraph->set_size_request(600, 100);
			igraph->m_units = "A";
			igraph->m_minScale = 0;
			igraph->m_maxScale = 5;
			igraph->m_scaleBump = 1;
			gbox->pack_start(*igraph, Gtk::PACK_SHRINK);
		}
	}

	//Done adding widgets
	show_all();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

void MainWindow::OnQuit()
{
	close();
}
