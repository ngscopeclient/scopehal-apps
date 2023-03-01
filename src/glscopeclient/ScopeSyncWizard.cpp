/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
#ifdef __x86_64__
#include <immintrin.h>
#endif

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ScopeSyncDeskewWelcomePage

ScopeSyncDeskewWelcomePage::ScopeSyncDeskewWelcomePage(OscilloscopeWindow* parent)
	: m_parent(parent)
{
	m_grid.attach(m_welcomeLabel, 0, 0, 1, 1);
		m_welcomeLabel.set_markup(
			string("Before instrument synchronization can begin, the hardware must be properly connected.\n") +
			string("\n") +
			string("1) The instrument \"") + parent->GetScope(0)->m_nickname + "\" is selected as primary.\n" +
			string("2) Connect a common reference clock to all instruments\n") +
			string("3) Connect the trigger output on the primary instrument to an input of each secondary.\n")
			);

	//Select trigger input for each scope
	m_grid.attach(m_triggerFrame, 0, 1, 1, 1);
	m_triggerFrame.set_label("Secondary trigger channel");
	m_triggerFrame.set_margin_top(20);
	m_triggerFrame.add(m_triggerGrid);
	size_t nscopes = parent->GetScopeCount();
	for(size_t i=1; i<nscopes; i++)
	{
		auto scope = parent->GetScope(i);

		//Trigger name
		auto label = Gtk::manage(new Gtk::Label(scope->m_nickname));
		label->set_margin_right(10);
		label->set_margin_left(10);
		m_triggerGrid.attach(*label, 0, i, 1, 1);

		//List of channels
		auto chanbox = Gtk::manage(new Gtk::ComboBoxText);
		m_triggerGrid.attach(*chanbox, 1, i, 1, 1);
		m_scopeNameBoxes[scope] = chanbox;

		//Skip sync box
		auto skip = Gtk::manage(new Gtk::CheckButton);
		skip->set_label("Skip sync");
		skip->set_margin_left(20);
		m_triggerGrid.attach(*skip, 2, i, 1, 1);
		m_skipBoxes[scope] = skip;

		//Populate channel list
		for(size_t j=0; j<scope->GetChannelCount(); j++)
			chanbox->append(scope->GetChannel(j)->GetDisplayName());
	}

	m_grid.show_all();
}

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
		string("* Long pseudorandom bit sequences (PRBS-31)\n") +
		string("* RAM DQ pins while performing heavy memory accesses\n") +
		string("* 64/66b coded serial links\n") +
		string("\n") +
		string("Examples of bad reference signals: \n") +
		string("* Power rails\n") +
		string("* Clocks\n") +
		string("* 8B/10B coded serial links\n") +
		string("* Short repeating patterns (PRBS-7)\n") +
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
			auto chan = primary->GetOscilloscopeChannel(i);
			if(chan == nullptr)
				continue;
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
			auto chan = secondary->GetOscilloscopeChannel(i);
			if(chan == nullptr)
				continue;
			if(chan->GetType(0) != Stream::STREAM_TYPE_ANALOG)
				continue;

			//Add to the box
			m_secondaryChannelBox.append(chan->GetDisplayName());
			m_secondaryChannels[chan->GetDisplayName()] = chan;
		}
}

Oscilloscope* ScopeSyncDeskewSetupPage::GetScope()
{
	return m_parent->GetScope(m_nscope);
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
	: m_welcomePage(parent)
	, m_parent(parent)
	, m_activeSetupPage(NULL)
	, m_activeSecondaryPage(NULL)
	, m_bestCorrelationOffset(0)
	, m_bestCorrelation(0)
	, m_primaryWaveform(0)
	, m_secondaryWaveform(0)
	, m_maxSkewSamples(0)
	, m_numAverages(10)
	, m_shuttingDown(false)
	, m_waitingForWaveform(false)
{
	set_transient_for(*parent);

	//Welcome / hardware setup
	append_page(m_welcomePage.m_grid);
		set_page_type(m_welcomePage.m_grid, Gtk::ASSISTANT_PAGE_INTRO);
		set_page_title(m_welcomePage.m_grid, "Hardware Setup");

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
	set_page_complete(m_welcomePage.m_grid);

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

			//If we're skipping this page, move past it
			if(m_welcomePage.m_skipBoxes[p->GetScope()]->get_active())
			{
				next_page();
				return;
			}
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
	next_page();
}

void ScopeSyncWizard::ConfigureSecondaryScope(Oscilloscope* scope)
{
	//Bypass if we're skipping sync
	if(m_welcomePage.m_skipBoxes[scope]->get_active())
		return;

	//Set trigger to external
	auto trig = new EdgeTrigger(scope);
	auto nscope = m_welcomePage.m_scopeNameBoxes[scope]->get_active_row_number();
	trig->SetInput(0, StreamDescriptor(scope->GetOscilloscopeChannel(nscope), 0));
	trig->SetType(EdgeTrigger::EDGE_RISING);
	trig->SetLevel(0.25);	//hard coded 250 mV threshold for now
	scope->SetTrigger(trig);

	//Set reference clock to external
	m_primaryProgressBar.set_text("Configure secondary reference clock");
	scope->SetUseExternalRefclk(true);

	m_parent->m_scopeDeskewCal[scope] = 0;
}

void ScopeSyncWizard::ActivateSecondaryScope(ScopeSyncDeskewProgressPage* page)
{
	//Bypass if we're skipping sync on this scope
	if(m_welcomePage.m_skipBoxes[page->GetScope()]->get_active())
	{
		set_page_complete(page->m_grid);
		next_page();
		return;
	}

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
	auto pw = pri->GetData(0);
	auto sw = sec->GetData(0);
	if(!pw || !sw)
		return;

	//Good, not waiting
	m_waitingForWaveform = false;

	//Set up state
	m_bestCorrelation = -999999;
	m_bestCorrelationOffset = 0;
	m_primaryWaveform = pw;
	m_secondaryWaveform = sw;

	//Max allowed skew between instruments is 10K points for now (arbitrary limit)
	m_maxSkewSamples = static_cast<int64_t>(pw->size() / 2);
	m_maxSkewSamples = min(m_maxSkewSamples, static_cast<int64_t>(10000LL));

	//Set the timer
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &ScopeSyncWizard::OnTimer), 1);
}

bool ScopeSyncWizard::OnTimer()
{
	auto upri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform);
	auto usec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform);

	auto spri = dynamic_cast<SparseAnalogWaveform*>(m_primaryWaveform);
	auto ssec = dynamic_cast<SparseAnalogWaveform*>(m_secondaryWaveform);

	//Optimized path (if both waveforms are dense packed)
	if(upri && usec)
	{
		//If sample rates are equal we can simplify things a lot
		if(m_primaryWaveform->m_timescale == m_secondaryWaveform->m_timescale)
		{
			#if defined(__x86_64__) && !defined(__clang__)
			if(g_hasAvx512F)
				DoProcessWaveformDensePackedEqualRateAVX512F();
			else
			#endif
				DoProcessWaveformDensePackedEqualRateGeneric();
		}

		//Also special-case 2:1 sample rate ratio (primary 2x speed of secondary)
		else if((m_primaryWaveform->m_timescale * 2) == m_secondaryWaveform->m_timescale)
		{
			#if defined(__x86_64__) && !defined(__clang__)
			if(g_hasAvx512F)
				DoProcessWaveformDensePackedDoubleRateAVX512F();
			else
			#endif
				DoProcessWaveformDensePackedDoubleRateGeneric();
		}

		//Unequal sample rates, more math needed
		else
			DoProcessWaveformDensePackedUnequalRate();
	}

	//Fallback path (if at least one waveform is not dense packed)
	else if(spri && ssec)
		DoProcessWaveformSparse();

	else
	{
		LogError("Mixed sparse and uniform waveforms not implemented\n");
		return false;
	}

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

		next_page();
	}

	return false;
}

void ScopeSyncWizard::DoProcessWaveformSparse()
{
	//Calculate cross-correlation between the primary and secondary waveforms at up to +/- half the waveform length
	int64_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	std::mutex cmutex;

	auto ppri = dynamic_cast<SparseAnalogWaveform*>(m_primaryWaveform);
	auto psec = dynamic_cast<SparseAnalogWaveform*>(m_secondaryWaveform);

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
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
			int64_t start = ppri->m_offsets[i] * m_primaryWaveform->m_timescale + m_primaryWaveform->m_triggerPhase;

			//Target timestamp in the secondary waveform
			int64_t target = start + deltaFs;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			//Skip secondary samples if the current secondary sample ends before the primary sample starts
			bool done = false;
			while( (((psec->m_offsets[isecondary] + psec->m_durations[isecondary]) *
						psec->m_timescale) + psec->m_triggerPhase) < target)
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
			correlation += ppri->m_samples[i] * psec->m_samples[isecondary];
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
}

void ScopeSyncWizard::DoProcessWaveformDensePackedDoubleRateGeneric()
{
	size_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	std::mutex cmutex;

	int64_t phaseshift = (m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase)
		/ m_primaryWaveform->m_timescale;

	float* ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform)->m_samples.GetCpuPointer();
	float* psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform)->m_samples.GetCpuPointer();

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Shift by relative trigger phase
		int64_t delta = d + phaseshift;

		size_t end = 2*(slen - delta);
		end = min(end, len);

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		double correlation = 0;
		for(size_t i=0; i<end; i++)
		{
			//If off the start of the waveform, skip it
			if(((int64_t)i + delta) < 0)
				continue;

			uint64_t utarget = ((i  + delta) / 2);

			//Do the actual cross-correlation
			correlation += ppri[i] * psec[utarget];
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
}

#if defined(__x86_64__) && !defined(__clang__)
__attribute__((target("avx512f")))
void ScopeSyncWizard::DoProcessWaveformDensePackedDoubleRateAVX512F()
{
	size_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	//Number of samples actually being processed
	//(in this application it's OK to truncate w/o a scalar implementation at the end)
	size_t len_rounded = len - (len % 32);
	size_t slen_rounded = slen - (slen % 32);

	std::mutex cmutex;

	int64_t phaseshift = (m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase)
		/ m_primaryWaveform->m_timescale;

	m_primaryWaveform->PrepareForCpuAccess();
	m_secondaryWaveform->PrepareForCpuAccess();
	float* ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform)->m_samples.GetCpuPointer();
	float* psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform)->m_samples.GetCpuPointer();

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Shift by relative trigger phase
		int64_t delta = d + phaseshift;

		size_t end = 2*(slen_rounded - delta);
		end = min(end, len_rounded);

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		__m512 vcorrelation = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		__m512i perm0 = _mm512_set_epi32(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
		__m512i perm1 = _mm512_set_epi32(15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8, 8);
		for(size_t i=0; i<end; i+= 32)
		{
			//If off the start of the waveform, skip it
			if(((int64_t)i + delta) < 0)
				continue;

			//Primary waveform is easy
			__m512 pri1 = _mm512_loadu_ps(ppri + i);
			__m512 pri2 = _mm512_loadu_ps(ppri + i + 16);

			uint64_t utarget = ((i  + delta) / 2);

			//Secondary waveform is more work since we have to shuffle the samples
			__m512 sec = _mm512_loadu_ps(psec + utarget);
			__m512 sec1 = _mm512_permutexvar_ps(perm0, sec);
			__m512 sec2 = _mm512_permutexvar_ps(perm1, sec);

			//Do the actual cross-correlation
			vcorrelation = _mm512_fmadd_ps(pri1, sec1, vcorrelation);
			vcorrelation = _mm512_fmadd_ps(pri2, sec2, vcorrelation);

			samplesProcessed += 32;
		}

		//Horizontal add the output
		//(outside the inner loop, no need to bother vectorizing this)
		float vec[16];
		_mm512_storeu_ps(vec, vcorrelation);
		float correlation = 0;
		for(int i=0; i<16; i++)
			correlation += vec[i];

		float normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}
}
#endif /* __x86_64__ && !__clang__*/

void ScopeSyncWizard::DoProcessWaveformDensePackedEqualRateGeneric()
{
	int64_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	std::mutex cmutex;

	int64_t phaseshift =
		(m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase) /
		m_primaryWaveform->m_timescale;

	float* ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform)->m_samples.GetCpuPointer();
	float* psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform)->m_samples.GetCpuPointer();

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Shift by relative trigger phase
		int64_t delta = d + phaseshift;

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		double correlation = 0;
		for(size_t i=0; i<(size_t)len; i++)
		{
			//Target timestamp in the secondary waveform
			int64_t target = i + delta;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			//If off the end of the waveform, stop
			uint64_t utarget = target;
			if(utarget >= slen)
				break;

			//Do the actual cross-correlation
			correlation += ppri[i] * psec[utarget];
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
}

#if defined(__x86_64__) && !defined(__clang__)
__attribute__((target("avx512f")))
void ScopeSyncWizard::DoProcessWaveformDensePackedEqualRateAVX512F()
{
	size_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	//Number of samples actually being processed
	//(in this application it's OK to truncate w/o a scalar implementation at the end)
	size_t len_rounded = len - (len % 16);
	size_t slen_rounded = slen - (slen % 16);

	std::mutex cmutex;

	int64_t phaseshift =
		(m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase) /
		m_primaryWaveform->m_timescale;

	m_primaryWaveform->PrepareForCpuAccess();
	m_secondaryWaveform->PrepareForCpuAccess();
	float* ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform)->m_samples.GetCpuPointer();
	float* psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform)->m_samples.GetCpuPointer();

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Shift by relative trigger phase
		int64_t delta = d + phaseshift;

		size_t end = slen_rounded - delta;
		end = min(end, len_rounded);

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		__m512 vcorrelation = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		for(size_t i=0; i<end; i += 16)
		{
			//If off the start of the waveform, skip it
			if((int64_t)i + delta < 0)
				continue;

			samplesProcessed += 16;

			//Do the actual cross-correlation
			__m512 pri = _mm512_loadu_ps(ppri + i);
			__m512 sec = _mm512_loadu_ps(psec + i + delta);
			vcorrelation = _mm512_fmadd_ps(pri, sec, vcorrelation);
		}

		//Horizontal add the output
		//(outside the inner loop, no need to bother vectorizing this)
		float vec[16];
		_mm512_storeu_ps(vec, vcorrelation);
		float correlation = 0;
		for(int i=0; i<16; i++)
			correlation += vec[i];

		float normalizedCorrelation = correlation / samplesProcessed;

		//Update correlation
		lock_guard<mutex> lock(cmutex);
		if(normalizedCorrelation > m_bestCorrelation)
		{
			m_bestCorrelation = normalizedCorrelation;
			m_bestCorrelationOffset = d;
		}
	}
}
#endif /* __x86_64__ && !__clang__*/

void ScopeSyncWizard::DoProcessWaveformDensePackedUnequalRate()
{
	int64_t len = m_primaryWaveform->size();
	size_t slen = m_secondaryWaveform->size();

	auto ppri = dynamic_cast<UniformAnalogWaveform*>(m_primaryWaveform);
	auto psec = dynamic_cast<UniformAnalogWaveform*>(m_secondaryWaveform);

	std::mutex cmutex;

	#pragma omp parallel for
	for(int64_t d = -m_maxSkewSamples; d < m_maxSkewSamples; d ++)
	{
		//Convert delta from samples of the primary waveform to femtoseconds
		int64_t deltaFs = m_primaryWaveform->m_timescale * d;

		//Shift by relative trigger phase
		deltaFs += (m_primaryWaveform->m_triggerPhase - m_secondaryWaveform->m_triggerPhase);

		//Loop over samples in the primary waveform
		ssize_t samplesProcessed = 0;
		size_t isecondary = 0;
		double correlation = 0;
		for(size_t i=0; i<(size_t)len; i++)
		{
			//Target timestamp in the secondary waveform
			int64_t target = i * m_primaryWaveform->m_timescale + deltaFs;

			//If off the start of the waveform, skip it
			if(target < 0)
				continue;

			uint64_t utarget = target;

			//Skip secondary samples if the current secondary sample ends before the primary sample starts
			bool done = false;
			while( ((isecondary + 1) *	m_secondaryWaveform->m_timescale) < utarget)
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
			correlation += ppri->m_samples[i] * psec->m_samples[isecondary];
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
