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
	@brief Implementation of ScopeSyncWizard
 */
#include "glscopeclient.h"
#include "ScopeSyncWizard.h"
#include "OscilloscopeWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ScopeSyncDeskewSetupPage

ScopeSyncDeskewSetupPage::ScopeSyncDeskewSetupPage(OscilloscopeWindow* parent, size_t nscope)
	: m_parent(parent)
	, m_nscope(nscope)
{
	m_grid.attach(m_label, 0, 0, 1, 1);

	m_label.set_markup(
		string("Select a signal on the DUT to use as a skew reference. This signal should have minimal autocorrelation,\n") +
		string("and should contain at least one fast edge visible with the current trigger settings.\n") +
		string("\n") +
		string("Examples of good reference signals: \n") +
		string("* A single fast edge\n") +
		string("* Pseudorandom bit sequences\n") +
		string("* RAM DQ pins\n") +
		string("* 64/66b coded serial links\n") +
		string("\n") +
		string("Examples of bad reference signals: \n") +
		string("* Power rails\n") +
		string("* Clocks\n") +
		string("* 8B/10B coded serial links\n") +
		string("\n") +
		string("Touch a probe from ") + parent->GetScope(0)->m_nickname + " and another probe from " +
			parent->GetScope(nscope)->m_nickname + " to the reference point.\n"
		);

	m_grid.attach_next_to(m_primaryChannelLabel, m_label, Gtk::POS_BOTTOM, 1, 1);
		m_primaryChannelLabel.set_text("Primary channel");
		m_primaryChannelLabel.set_halign(Gtk::ALIGN_START);
	m_grid.attach_next_to(m_primaryChannelBox, m_primaryChannelLabel, Gtk::POS_RIGHT, 1, 1);
		//TODO: fill

	m_grid.attach_next_to(m_secondaryChannelLabel, m_primaryChannelLabel, Gtk::POS_BOTTOM, 1, 1);
		m_secondaryChannelLabel.set_text("Secondary channel");
		m_secondaryChannelLabel.set_halign(Gtk::ALIGN_START);
	m_grid.attach_next_to(m_secondaryChannelBox, m_secondaryChannelLabel, Gtk::POS_RIGHT, 1, 1);
		//TODO: fill
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ScopeSyncDeskewProgressPage

ScopeSyncDeskewProgressPage::ScopeSyncDeskewProgressPage(OscilloscopeWindow* parent, size_t nscope)
	: m_parent(parent)
	, m_nscope(nscope)
{
	m_grid.attach(m_progressBar, 0, 0, 1, 1);
		m_progressBar.set_show_text(true);
		m_progressBar.set_size_request(300, 16);
}

Oscilloscope* ScopeSyncDeskewProgressPage::GetScope()
{
	return m_parent->GetScope(m_nscope);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ScopeSyncWizard::ScopeSyncWizard(OscilloscopeWindow* parent)
	: m_parent(parent)
	, m_activeSecondaryPage(NULL)
{
	set_transient_for(*parent);

	//Welcome / hardware setup
	append_page(m_welcomePage);
		set_page_type(m_welcomePage, Gtk::ASSISTANT_PAGE_INTRO);
		set_page_title(m_welcomePage, "Hardware Setup");
		m_welcomePage.attach(m_welcomeLabel, 0, 0, 1, 1);
			m_welcomeLabel.set_markup(
				string("Before instrument synchronization can begin, the hardware must be properly connected.\n") +
				string("\n") +
				string("1) The instrument \"") + parent->GetScope(0)->m_nickname + "\" is selected as primary.\n" +
				string("2) Connect a common reference clock to all instruments\n") +
				string("3) Connect the trigger output on the primary instrument to the external trigger on each secondary.\n")
				);

	//Configure primary instrument
	append_page(m_primaryProgressPage);
		set_page_type(m_primaryProgressPage, Gtk::ASSISTANT_PAGE_PROGRESS);
		set_page_title(m_primaryProgressPage, string("Configure ") + m_parent->GetScope(0)->m_nickname);
		m_primaryProgressPage.attach(m_primaryProgressBar, 0, 0, 1, 1);
		m_primaryProgressBar.set_show_text(true);
		m_primaryProgressBar.set_size_request(300, 16);

	//Deskew path for each instrument
	for(size_t i=1; i<m_parent->GetScopeCount(); i++)
	{
		auto setpage = new ScopeSyncDeskewSetupPage(m_parent, i);
		m_deskewSetupPages.push_back(setpage);
		append_page(setpage->m_grid);
		set_page_type(setpage->m_grid, Gtk::ASSISTANT_PAGE_CONTENT);
		set_page_title(setpage->m_grid, string("Configure ") + m_parent->GetScope(i)->m_nickname);

		auto progpage = new ScopeSyncDeskewProgressPage(m_parent, i);
		m_deskewProgressPages.push_back(progpage);
		append_page(progpage->m_grid);
		set_page_type(progpage->m_grid, Gtk::ASSISTANT_PAGE_PROGRESS);
		set_page_title(progpage->m_grid, string("Deskew ") + m_parent->GetScope(i)->m_nickname);
	}

	//Mark the first page as complete, so we can move on
	set_page_complete(m_welcomePage);

	show_all();
}

ScopeSyncWizard::~ScopeSyncWizard()
{
	for(auto d : m_deskewSetupPages)
		delete d;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ScopeSyncWizard::on_cancel()
{
	hide();
}

void ScopeSyncWizard::on_prepare(Gtk::Widget* page)
{
	if(page == &m_primaryProgressPage)
		ConfigurePrimaryScope(m_parent->GetScope(0));

	//Mark setup pages complete immediately
	for(auto p : m_deskewSetupPages)
	{
		if(page == &p->m_grid)
			set_page_complete(*page);
	}

	for(auto p : m_deskewProgressPages)
	{
		if(page == &p->m_grid)
			ConfigureSecondaryScope(p, p->GetScope());
	}
}

void ScopeSyncWizard::ConfigurePrimaryScope(Oscilloscope* scope)
{
	m_primaryProgressBar.set_fraction(0);

	//Don't touch reference source on the master, it might be slaved to a GPSDO or something
	m_primaryProgressBar.set_text("Configure clock source");
	m_primaryProgressBar.set_fraction(25);

	//This is where we'd enable the reference out on the master if this is configurable.
	//On LeCroy hardware, it always appears to be enabled.
	m_primaryProgressBar.set_text("Enable reference clock out");
	m_primaryProgressBar.set_fraction(50);

	//Enable the trigger-out signal.
	//Some instruments may have a shared auxiliary output we need to configure for this function.
	//On others, this may do nothing.
	m_primaryProgressBar.set_text("Enable trigger out");
	m_primaryProgressBar.set_fraction(75);
	scope->EnableTriggerOutput();

	//Done
	m_primaryProgressBar.set_text("Done");
	m_primaryProgressBar.set_fraction(1);
	queue_draw();
	set_page_complete(m_primaryProgressPage);
}

void ScopeSyncWizard::ConfigureSecondaryScope(ScopeSyncDeskewProgressPage* page, Oscilloscope* scope)
{
	m_activeSecondaryPage = page;

	page->m_progressBar.set_fraction(0);

	//Set trigger to external
	page->m_progressBar.set_text("Configure trigger source");
	page->m_progressBar.set_fraction(10);
	scope->SetTriggerChannelIndex(scope->GetExternalTrigger()->GetIndex());

	//Set reference clock to external
	page->m_progressBar.set_text("Configure reference clock");
	page->m_progressBar.set_fraction(20);
	scope->SetUseExternalRefclk(true);

	//Arm trigger and acquire a waveform
	page->m_progressBar.set_text("Acquire skew reference waveform");
	page->m_progressBar.set_fraction(30);
	m_parent->ArmTrigger(true);
}

void ScopeSyncWizard::OnWaveformDataReady()
{
	if(!m_activeSecondaryPage)
		return;

	m_activeSecondaryPage->m_progressBar.set_text("Cross-correlate skew reference waveform");
	m_activeSecondaryPage->m_progressBar.set_fraction(40);
}
