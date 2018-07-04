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
	@brief A top-level window containing the UI for a multimeter
 */
#ifndef DMMWindow_h
#define DMMWindow_h

/**
	@brief Main application window class for a multimeter
 */
class DMMWindow	: public Gtk::Window
{
public:
	DMMWindow(Multimeter* meter, std::string host, int port);
	~DMMWindow();

protected:

	//Initialization
	void CreateWidgets();

	//Widgets
	Gtk::HBox m_hbox;
		Gtk::VBox m_vbox;
			Gtk::HBox m_signalSourceBox;
				Gtk::Label m_signalSourceLabel;
				Gtk::ComboBoxText m_signalSourceSelector;
			Gtk::HBox m_measurementTypeBox;
				Gtk::Label m_measurementTypeLabel;
				Gtk::ComboBoxText m_measurementTypeSelector;
			Gtk::HBox m_autoRangeBox;
				Gtk::Label m_autoRangeLabel;
				Gtk::CheckButton m_autoRangeSelector;
		Gtk::VBox m_measurementBox;
			Gtk::Label m_voltageLabel;
			Gtk::Label m_vppLabel;
			Gtk::Label m_frequencyLabel;

	//Our instrument connection
	Multimeter* m_meter;

	//Status polling
	bool OnTimer(int timer);

	//UI handlers
	void OnSignalSourceChanged();
	void OnMeasurementTypeChanged();
	void OnAutoRangeChanged();

	virtual void on_show();
	virtual void on_hide();
};


#endif
