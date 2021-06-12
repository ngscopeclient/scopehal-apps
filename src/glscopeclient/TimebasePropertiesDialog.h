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
	@brief Dialog for configuring trigger properties
 */

#ifndef TimebasePropertiesDialog_h
#define TimebasePropertiesDialog_h

class TimebasePropertiesPage
{
public:
	TimebasePropertiesPage(Oscilloscope* scope, OscilloscopeWindow* parent)
	: m_scope(scope)
	, m_parent(parent)
	, m_initializing(false)
	{}

	Oscilloscope* m_scope;

	Gtk::HBox m_box;
		Gtk::StackSidebar m_sidebar;
		Gtk::Stack m_stack;
			Gtk::Grid m_tgrid;
				Gtk::Label			m_sampleRateLabel;
					Gtk::ComboBoxText	m_sampleRateBox;
				Gtk::Label			m_memoryDepthLabel;
					Gtk::ComboBoxText	m_memoryDepthBox;
				Gtk::Label			m_sampleModeLabel;
					Gtk::ComboBoxText	m_sampleModeBox;
				Gtk::Label			m_interleaveLabel;
					Gtk::Switch			m_interleaveSwitch;
			Gtk::Grid m_fgrid;
				Gtk::Label			m_spanLabel;
					Gtk::Entry			m_spanEntry;
				Gtk::Label			m_rbwLabel;
					Gtk::Entry			m_rbwEntry;

	void RefreshSampleRates(bool interleaving);
	void RefreshSampleDepths(bool interleaving);
	void RefreshSampleModes();

	void AddWidgets();

	void OnDepthChanged();
	void OnRateChanged();
	void OnSpanChanged();
	void OnRBWChanged();
	void OnModeChanged();
	bool OnInterleaveSwitchChanged(bool state);

protected:
	OscilloscopeWindow* m_parent;
	bool m_initializing;
};

/**
	@brief Dialog for configuring trigger settings for a scope
 */
class TimebasePropertiesDialog	: public Gtk::Dialog
{
public:
	TimebasePropertiesDialog(OscilloscopeWindow* parent, const std::vector<Oscilloscope*>& scopes);
	virtual ~TimebasePropertiesDialog();

protected:
	Gtk::Notebook m_tabs;
		std::map<Oscilloscope*, TimebasePropertiesPage*> m_pages;

	const std::vector<Oscilloscope*>& m_scopes;
};

#endif
