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
	@brief Declaration of MultimeterDialog
 */

#ifndef MultimeterDialog_h
#define MultimeterDialog_h

#include "../../lib/scopeprotocols/MultimeterTrendFilter.h"

class OscilloscopeWindow;

/**
	@brief Dialog for interacting with a Multimeter (which may or may not be part of an Oscilloscope)
 */
class MultimeterDialog	: public Gtk::Dialog
{
public:
	MultimeterDialog(Multimeter* meter, OscilloscopeWindow* parent);
	virtual ~MultimeterDialog();

protected:
	enum UpdateRate
	{
		UPDATE_1HZ,
		UPDATE_2HZ,
		UPDATE_5HZ
	};

	virtual void on_show();
	virtual void on_hide();

	void OnInputChanged();
	void OnModeChanged();
	void OnSecondaryModeChanged();
	void OnTimerIntervalChanged()
	{ m_timerIntervalChanged = true; }

	void RefreshSecondaryModeList();

	void AddMode(Multimeter::MeasurementTypes type, const std::string& label);
	void AddSecondaryMode(Multimeter::MeasurementTypes type, const std::string& label);

	std::map<std::string, Multimeter::MeasurementTypes> m_modemap;
	std::map<Multimeter::MeasurementTypes, std::string> m_revmodemap;

	std::map<std::string, Multimeter::MeasurementTypes> m_secmodemap;
	std::map<Multimeter::MeasurementTypes, std::string> m_revsecmodemap;

	bool OnTimer();

	Multimeter* m_meter;

	Gtk::Grid m_grid;
		Gtk::Label						m_inputLabel;
			Gtk::ComboBoxText			m_inputBox;
		Gtk::Label						m_rateLabel;
			Gtk::ComboBoxText			m_rateBox;
		Gtk::Frame						m_primaryFrame;
			Gtk::Grid					m_primaryGrid;
				Gtk::Label				m_typeLabel;
					Gtk::ComboBoxText	m_typeBox;
				Gtk::Label				m_valueLabel;
					Gtk::Label			m_valueBox;

		Gtk::Frame						m_secondaryFrame;
			Gtk::Grid					m_secondaryGrid;
				Gtk::Label				m_secondaryTypeLabel;
					Gtk::ComboBoxText	m_secondaryTypeBox;
				Gtk::Label				m_secondaryValueLabel;
					Gtk::Label			m_secondaryValueBox;

	bool m_updatingSecondary;

	MultimeterTrendFilter* m_trendFilter;
	OscilloscopeWindow* m_parent;

	bool m_timerIntervalChanged;
};

#endif
