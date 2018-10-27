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
	@brief Implementation of main application window class
 */

#include "glscopeclient.h"
#include "../scopehal/Instrument.h"
#include "OscilloscopeWindow.h"
//#include "../scopehal/AnalogRenderer.h"
//#include "ProtocolDecoderDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
OscilloscopeWindow::OscilloscopeWindow(Oscilloscope* scope, std::string host, int port)
	/*: m_btnStart(Gtk::Stock::YES)
	, m_view(scope, this)
	, m_scope(scope)*/
{
	//Set title
	char title[256];
	/*snprintf(title, sizeof(title), "Oscilloscope: %s:%d (%s %s, serial %s)",
		host.c_str(),
		port,
		scope->GetVendor().c_str(),
		scope->GetName().c_str(),
		scope->GetSerial().c_str()
		);*/
	snprintf(title, sizeof(title), "FIXME");
	set_title(title);

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Add widgets
	CreateWidgets();

	//Done adding widgets
	show_all();

	//Set the update timer (100 Hz
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &OscilloscopeWindow::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, 10);

	/*
	//Set up display time scale
	m_timescale = 0;
	m_waiting = false;

	//Try triggering immediately. This lets us download an initial waveform right away.
	//It's also necessary to do this to initialize some other subsystems like the DMM.
	OnStart();
	*/
}

/**
	@brief Application cleanup
 */
OscilloscopeWindow::~OscilloscopeWindow()
{
}

/**
	@brief Helper function for creating widgets and setting up signal handlers
 */
void OscilloscopeWindow::CreateWidgets()
{
	//Set up window hierarchy
	add(m_vbox);
		m_vbox.pack_start(m_toolbar, Gtk::PACK_SHRINK);
			//m_toolbar.append(m_btnStart, sigc::mem_fun(*this, &OscilloscopeWindow::OnStart));
			//	m_btnStart.set_tooltip_text("Start capture");
		m_vbox.pack_start(m_waveformArea);

	//Set dimensions
	//m_viewscroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	//Set up message handlers
	//m_viewscroller.get_hadjustment()->signal_value_changed().connect(sigc::mem_fun(*this, &OscilloscopeWindow::OnScopeScroll));
	//m_viewscroller.get_vadjustment()->signal_value_changed().connect(sigc::mem_fun(*this, &OscilloscopeWindow::OnScopeScroll));
	//m_viewscroller.get_hadjustment()->set_step_increment(50);

	//Refresh the views
	//Need to refresh main view first so we have renderers to reference in the channel list
	//m_view.Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

bool OscilloscopeWindow::OnTimer(int /*timer*/)
{
	/*
	m_statprogress.set_fraction(0);

	static int i = 0;
	i ++;
	i %= 10;

	if(m_waiting)
	{
		//m_statprogress.set_text("Ready");
		string str = "Ready";
		for(int j=0; j<i; j++)
			str += ".";
		m_statprogress.set_text(str);

		//TODO: poll channel status and time/div etc and update our in-memory representation
		//(in case the user enabled a channel etc with hardware buttons)

		//Poll the trigger status of the scope
		Oscilloscope::TriggerMode status = m_scope->PollTrigger();
		if(status > Oscilloscope::TRIGGER_MODE_COUNT)
		{
			//Invalid value, skip it
			return true;
		}

		//If not TRIGGERED, do nothing
		if(status != Oscilloscope::TRIGGER_MODE_TRIGGERED)
			return true;

		double dt = GetTime() - m_tArm;
		LogDebug("Triggered (trigger was armed for %.2f ms)\n", dt * 1000);

		m_statprogress.set_text("Triggered");

		//Triggered - get the data from each channel
		double start = GetTime();
		m_scope->AcquireData(sigc::mem_fun(*this, &OscilloscopeWindow::OnCaptureProgressUpdate));
		dt = GetTime() - start;
		LogDebug("    Capture downloaded in %.2f ms\n", dt * 1000);

		//Set to a sane zoom if this is our first capture
		//otherwise keep time scale unchanged
		if(m_timescale == 0)
			OnZoomFit();

		//Refresh display
		m_view.SetSizeDirty();
		m_view.queue_draw();

		m_waiting = false;

		//TODO: if in continuous mode, trigger again
		//TODO: have settings for this
		//OnStart();
	}
	else
		m_statprogress.set_text("Stopped");
	*/
	//false to stop timer
	return true;
}

/*
void OscilloscopeWindow::OnZoomOut()
{
	//Get center of current view
	float fract = m_viewscroller.get_hadjustment()->get_value() / m_viewscroller.get_hadjustment()->get_upper();

	//Change zoom
	m_timescale /= 1.5;
	OnZoomChanged();

	//Dispatch the draw events
	while(Gtk::Main::events_pending())
		Gtk::Main::iteration();

	//Re-scroll
	m_viewscroller.get_hadjustment()->set_value(fract * m_viewscroller.get_hadjustment()->get_upper());
}

void OscilloscopeWindow::OnZoomIn()
{
	//Get center of current view
	float fract = m_viewscroller.get_hadjustment()->get_value() / m_viewscroller.get_hadjustment()->get_upper();

	//Change zoom
	m_timescale *= 1.5;
	OnZoomChanged();

	//Dispatch the draw events
	while(Gtk::Main::events_pending())
		Gtk::Main::iteration();

	//Re-scroll
	m_viewscroller.get_hadjustment()->set_value(fract * m_viewscroller.get_hadjustment()->get_upper());
}

void OscilloscopeWindow::OnZoomFit()
{
	if( (m_scope->GetChannelCount() != 0) && (m_scope->GetChannel(0) != NULL) && (m_scope->GetChannel(0)->GetData() != NULL))
	{
		CaptureChannelBase* capture = m_scope->GetChannel(0)->GetData();
		int64_t capture_len = capture->m_timescale * capture->GetEndTime();
		m_timescale = static_cast<float>(m_viewscroller.get_width()) / capture_len;
	}

	OnZoomChanged();
}

void OscilloscopeWindow::OnZoomChanged()
{
	for(size_t i=0; i<m_scope->GetChannelCount(); i++)
		m_scope->GetChannel(i)->m_timescale = m_timescale;

	m_view.SetSizeDirty();
	m_view.queue_draw();
}

int OscilloscopeWindow::OnCaptureProgressUpdate(float progress)
{
	m_statprogress.set_fraction(progress);

	//Dispatch pending gtk events (such as draw calls)
	while(Gtk::Main::events_pending())
		Gtk::Main::iteration();

	return 0;
}

void OscilloscopeWindow::OnStart()
{
	//TODO: get triggers
	//Load trigger conditions from sidebar
	//m_channelview.UpdateTriggers();

	//Start the capture
	m_tArm = GetTime();
	m_scope->StartSingleTrigger();
	m_waiting = true;

	//Print to stdout so scripts know we're ready
	//LogDebug("Ready\n");
	//fflush(stdout);
}

*/
