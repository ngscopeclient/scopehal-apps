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
	@brief Dialog for configuring channel properties
 */

#ifndef ScopeSyncWizard_h
#define ScopeSyncWizard_h

class OscilloscopeWindow;

class ScopeSyncDeskewSetupPage
{
public:
	ScopeSyncDeskewSetupPage(OscilloscopeWindow* parent, size_t nscope);

	Gtk::Grid m_grid;
		Gtk::Label m_label;
		Gtk::Label m_primaryChannelLabel;
			Gtk::ComboBoxText m_primaryChannelBox;
		Gtk::Label m_secondaryChannelLabel;
			Gtk::ComboBoxText m_secondaryChannelBox;

protected:
	OscilloscopeWindow* m_parent;
	size_t m_nscope;
};

class ScopeSyncDeskewProgressPage
{
public:
	ScopeSyncDeskewProgressPage(OscilloscopeWindow* parent, size_t nscope);

	Gtk::Grid m_grid;
		Gtk::ProgressBar m_progressBar;

	Oscilloscope* GetScope();

	OscilloscopeWindow* m_parent;
	size_t m_nscope;
};

/**
	@brief Dialog for configuring a single scope channel
 */
class ScopeSyncWizard	: public Gtk::Assistant
{
public:
	ScopeSyncWizard(OscilloscopeWindow* parent);

	virtual ~ScopeSyncWizard();

	void OnWaveformDataReady();

protected:
	Gtk::Grid m_welcomePage;
		Gtk::Label m_welcomeLabel;
	Gtk::Grid m_primaryProgressPage;
		Gtk::ProgressBar m_primaryProgressBar;
	std::vector<ScopeSyncDeskewSetupPage*> m_deskewSetupPages;
	std::vector<ScopeSyncDeskewProgressPage*> m_deskewProgressPages;

	virtual void on_cancel();
	virtual void on_prepare(Gtk::Widget* page);

	void ConfigurePrimaryScope(Oscilloscope* scope);
	void ConfigureSecondaryScope(ScopeSyncDeskewProgressPage* page, Oscilloscope* scope);

	OscilloscopeWindow* m_parent;

	ScopeSyncDeskewProgressPage* m_activeSecondaryPage;
};

#endif
