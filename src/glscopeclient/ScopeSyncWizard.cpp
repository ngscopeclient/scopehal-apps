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

	auto primary = parent->GetScope(0);
	auto secondary = parent->GetScope(nscope);

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
		string("Touch a probe from ") + primary->m_nickname + " and another probe from " +
			secondary->m_nickname + " to the reference point.\n"
		);

	m_grid.attach_next_to(m_primaryChannelLabel, m_label, Gtk::POS_BOTTOM, 1, 1);
		m_primaryChannelLabel.set_text("Primary channel");
		m_primaryChannelLabel.set_halign(Gtk::ALIGN_START);
	m_grid.attach_next_to(m_primaryChannelBox, m_primaryChannelLabel, Gtk::POS_RIGHT, 1, 1);
		for(size_t i=0; i<primary->GetChannelCount(); i++)
		{
			//For now, we can only use analog channels to deskew
			auto chan = primary->GetChannel(i);
			if(chan->GetType(0) != Stream::STREAM_TYPE_ANALOG)
				continue;

			//Add to the box
			m_primaryChannelBox.append(chan->GetDisplayName());
			m_primaryChannels[chan->GetDisplayName()] = chan;
		}

	m_grid.attach_next_to(m_secondaryChannelLabel, m_primaryChannelLabel, Gtk::POS_BOTTOM, 1, 1);
		m_secondaryChannelLabel.set_text("Secondary channel");
		m_secondaryChannelLabel.set_halign(Gtk::ALIGN_START);
	m_grid.attach_next_to(m_secondaryChannelBox, m_secondaryChannelLabel, Gtk::POS_RIGHT, 1, 1);
		for(size_t i=0; i<secondary->GetChannelCount(); i++)
		{
			//For now, we can only use analog channels to deskew
			auto chan = secondary->GetChannel(i);
			if(chan->GetType(0) != Stream::STREAM_TYPE_ANALOG)
				continue;

			//Add to the box
			m_secondaryChannelBox.append(chan->GetDisplayName());
			m_secondaryChannels[chan->GetDisplayName()] = chan;
		}
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
	, m_activeSetupPage(NULL)
	, m_activeSecondaryPage(NULL)
	, m_bestCorrelationOffset(0)
	, m_bestCorrelation(0)
	, m_primaryWaveform(0)
	, m_secondaryWaveform(0)
	, m_delta(0)
	, m_maxSkewSamples(0)
	, m_numAverages(10)
	, m_shuttingDown(false)
	, m_waitingForWaveform(false)
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

	//Last page
	append_page(m_donePage);
		set_page_type(m_donePage, Gtk::ASSISTANT_PAGE_CONFIRM);
		m_donePage.attach(m_doneLabel, 0, 0, 1, 1);
			set_page_title(m_donePage, "Complete");
			m_doneLabel.set_markup(
				string("Instrument synchronization successfully completed!\n") +
				string("\n") +
				string("The sync wizard may be re-run at any time to tune if necessary.\n")
				);

	//Mark the first page as complete, so we can move on
	set_page_complete(m_welcomePage);

	show_all();
}

ScopeSyncWizard::~ScopeSyncWizard()
{
	m_shuttingDown = true;

	for(auto d : m_deskewSetupPages)
		delete d;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void ScopeSyncWizard::on_cancel()
{
	hide();
}

void ScopeSyncWizard::on_apply()
{
	hide();
	m_parent->OnSyncComplete();
}

void ScopeSyncWizard::on_prepare(Gtk::Widget* page)
{
	//Seems to be called during the destructor, avoid spurious events here
	if(m_shuttingDown)
		return;

	if(page == &m_primaryProgressPage)
	{
		ConfigurePrimaryScope(m_parent->GetScope(0));
		for(auto p : m_deskewProgressPages)
			ConfigureSecondaryScope(p->GetScope());
	}

	if(page == &m_donePage)
		set_page_complete(*page);

	//Mark setup pages complete immediately
	for(auto p : m_deskewSetupPages)
	{
		if(page == &p->m_grid)
		{
			m_activeSetupPage = p;
			set_page_complete(*page);
		}
	}

	//Process deskew stuff
	for(auto p : m_deskewProgressPages)
	{
		if(page == &p->m_grid)
		{
			m_activeSecondaryPage = p;
			ActivateSecondaryScope(p);
		}
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

void ScopeSyncWizard::ConfigureSecondaryScope(Oscilloscope* scope)
{
	//Set trigger to external
	auto trig = new EdgeTrigger(scope);
	trig->SetInput(0, StreamDescriptor(scope->GetExternalTrigger(), 0));
	trig->SetType(EdgeTrigger::EDGE_RISING);
	trig->SetLevel(0.25);	//hard coded 250 mV threshold for now
	scope->SetTrigger(trig);

	//Set reference clock to external
	m_primaryProgressBar.set_text("Configure secondary reference clock");
	scope->SetUseExternalRefclk(true);

	//Set the trigger offset to the same as the primary
	m_primaryProgressBar.set_text("Configure secondary trigger offset");
	scope->SetTriggerOffset(m_parent->GetScope(0)->GetTriggerOffset());

	m_parent->m_scopeDeskewCal[scope] = 0;
}

void ScopeSyncWizard::ActivateSecondaryScope(ScopeSyncDeskewProgressPage* page)
{
	page->m_progressBar.set_fraction(0);

	//Arm trigger and acquire a waveform
	RequestWaveform();

	//Clean out stats
	m_averageSkews.clear();
}

void ScopeSyncWizard::OnWaveformDataReady()
{
	//no longer waiting for timeout
	m_timeoutConnection.disconnect();

	//We must have active pages (sanity check)
	if(!m_activeSecondaryPage || !m_activeSetupPage)
		return;

	//We must have selected channels
	auto pri = m_activeSetupPage->GetPrimaryChannel();
	auto sec = m_activeSetupPage->GetSecondaryChannel();
	if(!pri || !sec)
		return;

	//Verify we have data to work with
	auto pw = dynamic_cast<AnalogWaveform*>(pri->GetData(0));
	auto sw = dynamic_cast<AnalogWaveform*>(sec->GetData(0));
	if(!pw || !sw)
		return;

	//Good, not waiting
	m_waitingForWaveform = false;

	//Set up state
	m_bestCorrelation = -999999;
	m_bestCorrelationOffset = 0;
	m_primaryWaveform = pw;
	m_secondaryWaveform = sw;

	//Max allowed skew between instruments is 5K points for now (arbitrary limit)
	m_maxSkewSamples = static_cast<int64_t>(pw->m_offsets.size() / 2);
	m_maxSkewSamples = min(m_maxSkewSamples, static_cast<int64_t>(5000LL));

	m_delta = - m_maxSkewSamples;

	//Set the timer
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &ScopeSyncWizard::OnTimer), 10);
}

bool ScopeSyncWizard::OnTimer()
{
	//Calculate cross-correlation between the primary and secondary waveforms at up to +/- half the waveform length
	int64_t len = m_primaryWaveform->m_offsets.size();
	size_t slen = m_secondaryWaveform->m_offsets.size();

	int64_t samplesPerBlock = 5000;
	int64_t blockEnd = min(m_delta + samplesPerBlock, len/2);
	blockEnd = min(blockEnd, m_maxSkewSamples);

	//Update the progress bar
	float blockfrac = m_averageSkews.size() * 1.0f / m_numAverages;
	int64_t blockpos = m_delta + m_maxSkewSamples;
	float infrac = blockpos * 1.0f / (2*m_maxSkewSamples);
	float frac = blockfrac + infrac/m_numAverages;
	float progress = (frac * 0.9) + 0.1;
	m_activeSecondaryPage->m_progressBar.set_text("Cross-correlate skew reference waveform");
	m_activeSecondaryPage->m_progressBar.set_fraction(progress);

	std::mutex cmutex;
	#pragma omp parallel for
	for(int64_t d = m_delta; d < blockEnd; d ++)
	{
		//Convert delta from samples of the primary waveform to femtoseconds
		int64_t deltaFs = m_primaryWaveform->m_timescale * d;

		//Loop over samples in the primary waveform
		//TODO: Can we AVX this?
		ssize_t samplesProcessed = 0;
		size_t isecondary = 0;
		double correlation = 0;
		for(size_t i=0; i<(size_t)len; i++)
		{
			//Timestamp of this sample, in fs
			int64_t start = m_primaryWaveform->m_offsets[i] * m_primaryWaveform->m_timescale +
				m_primaryWaveform->m_triggerPhase;

			//Target timestamp in the secondary waveform
			int64_t target = start + deltaFs;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			//Skip secondary samples if the current secondary sample ends before the primary sample starts
			bool done = false;
			while( (((m_secondaryWaveform->m_offsets[isecondary] + m_secondaryWaveform->m_durations[isecondary]) *
						m_secondaryWaveform->m_timescale) + m_secondaryWaveform->m_triggerPhase) < target)
			{
				isecondary ++;

				//If off the end of the waveform, stop
				if(isecondary >= slen)
				{
					done = true;
					break;
				}
			}
			if(done)
				break;

			//Do the actual cross-correlation
			correlation += m_primaryWaveform->m_samples[i] * m_secondaryWaveform->m_samples[isecondary];
			samplesProcessed ++;
		}

		double normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}
	m_delta = blockEnd;

	//Need more data to go on
	if(m_delta < m_maxSkewSamples)
		return true;

	//Collect the skew from this round
	auto scope = m_activeSecondaryPage->GetScope();
	int64_t skew = m_bestCorrelationOffset * m_primaryWaveform->m_timescale;
	Unit fs(Unit::UNIT_FS);
	LogTrace("Best correlation = %f (delta = %ld / %s)\n",
		m_bestCorrelation, m_bestCorrelationOffset, fs.PrettyPrint(skew).c_str());
	m_averageSkews.push_back(skew);

	//Do we have additional averages to collect?
	if(m_averageSkews.size() < m_numAverages)
	{
		char tmp[128];
		snprintf(
			tmp,
			sizeof(tmp),
			"Acquire skew reference waveform (%zu/%zu)",
			m_averageSkews.size()+1,
			m_numAverages);
		m_activeSecondaryPage->m_progressBar.set_text(tmp);

		RequestWaveform();
		return false;
	}

	//Last iteration
	else
	{
		set_page_complete(m_activeSecondaryPage->m_grid);
		m_activeSecondaryPage->m_progressBar.set_fraction(1);
		m_activeSecondaryPage->m_progressBar.set_text("Done");

		//Calculate median skew
		sort(m_averageSkews.begin(), m_averageSkews.end());
		skew = (m_averageSkews[4] + m_averageSkews[5]) / 2;

		//Figure out where we want the secondary to go
		LogTrace("Median skew: %s\n", fs.PrettyPrint(skew).c_str());
		m_parent->m_scopeDeskewCal[scope] = skew;
	}

	return false;
}

bool ScopeSyncWizard::OnWaveformTimeout()
{
	if(!m_waitingForWaveform)
		return false;

	LogWarning("Timed out waiting for waveform, retriggering...\n");
	m_parent->OnStop();
	RequestWaveform();
	return false;
}

/**
	@brief Request a new waveform and set a timeout in case the scope doesn't trigger in time
 */
void ScopeSyncWizard::RequestWaveform()
{
	m_parent->ArmTrigger(OscilloscopeWindow::TRIGGER_TYPE_SINGLE);
	m_waitingForWaveform = true;
	m_timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &ScopeSyncWizard::OnWaveformTimeout), 5000);
}
