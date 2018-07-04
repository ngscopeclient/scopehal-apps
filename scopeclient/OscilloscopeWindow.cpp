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

#include "scopeclient.h"
#include "../scopehal/Instrument.h"
#include "OscilloscopeWindow.h"
//#include "../scopehal/AnalogRenderer.h"
#include "ProtocolDecoderDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initializes the main window
 */
OscilloscopeWindow::OscilloscopeWindow(Oscilloscope* scope, std::string host, int port)
	: m_btnStart(Gtk::Stock::YES)
	, m_view(scope, this)
	, m_scope(scope)
{
	//Set title
	char title[256];
	snprintf(title, sizeof(title), "Oscilloscope: %s:%d (%s %s, serial %s)",
		host.c_str(),
		port,
		scope->GetVendor().c_str(),
		scope->GetName().c_str(),
		scope->GetSerial().c_str()
		);
	set_title(title);

	//Initial setup
	set_reallocate_redraws(true);
	set_default_size(1280, 800);

	//Add widgets
	CreateWidgets();

	//Done adding widgets
	show_all();

	//Set the update timer
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &OscilloscopeWindow::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, 5);

	//Set up display time scale
	m_timescale = 0;
	m_waiting = false;

	//Try triggering immediately. This lets us download an initial waveform right away.
	//It's also necessary to do this to initialize some other subsystems like the DMM.
	OnStart();
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
			m_toolbar.append(m_btnStart, sigc::mem_fun(*this, &OscilloscopeWindow::OnStart));
				m_btnStart.set_tooltip_text("Start capture");
		m_vbox.pack_start(m_viewscroller);
			m_viewscroller.add(m_view);
		m_vbox.pack_start(m_statusbar, Gtk::PACK_SHRINK);
			m_statusbar.set_size_request(-1,16);
			m_statusbar.pack_start(m_statprogress, Gtk::PACK_SHRINK);
			m_statprogress.set_size_request(200, -1);
			m_statprogress.set_fraction(0);
			m_statprogress.set_show_text();

	//Set dimensions
	m_viewscroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	//Set up message handlers
	//m_viewscroller.get_hadjustment()->signal_value_changed().connect(sigc::mem_fun(*this, &OscilloscopeWindow::OnScopeScroll));
	//m_viewscroller.get_vadjustment()->signal_value_changed().connect(sigc::mem_fun(*this, &OscilloscopeWindow::OnScopeScroll));
	m_viewscroller.get_hadjustment()->set_step_increment(50);

	//Refresh the views
	//Need to refresh main view first so we have renderers to reference in the channel list
	m_view.Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Message handlers

bool OscilloscopeWindow::OnTimer(int /*timer*/)
{
	try
	{
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
	}

	catch(const JtagException& ex)
	{
		printf("%s\n", ex.GetDescription().c_str());
	}

	//false to stop timer
	return true;
}

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
	try
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
	catch(const JtagException& ex)
	{
		LogError("%s\n", ex.GetDescription().c_str());
	}
}

/*
void ChannelListView::UpdateTriggers()
{
	//Clear out old triggers
	m_parent->GetScope()->ResetTriggerConditions();

	//Loop over our child nodes
	Gtk::TreeNodeChildren children = m_model->children();
	for(Gtk::TreeNodeChildren::iterator it=children.begin(); it != children.end(); ++it)
	{
		//std::string name = it->get_value(m_columns.name);
		OscilloscopeChannel* chan = it->get_value(m_columns.chan);
		std::string val = it->get_value(m_columns.value);

		//Protocol decoders are evaluated host side and can't be used for triggering - skip them
		//if(dynamic_cast<ProtocolDecoder*>(chan) != NULL)
		//	continue;

		//Should be a digital channel (analog stuff not supported yet)
		if(chan->GetType() == OscilloscopeChannel::CHANNEL_TYPE_DIGITAL)
		{
			//Initialize trigger vector

			//If string is empty, mark as don't care
			std::vector<Oscilloscope::TriggerType> triggerbits;
			if(val == "")
			{
				for(int i=0; i<chan->GetWidth(); i++)
					triggerbits.push_back(Oscilloscope::TRIGGER_TYPE_DONTCARE);
			}

			//otherwise parse Verilog-format values
			else
			{
				//Default high bits to low
				for(int i=0; i<chan->GetWidth(); i++)
					triggerbits.push_back(Oscilloscope::TRIGGER_TYPE_LOW);

				const char* vstr = val.c_str();
				const char* quote = strstr(vstr, "'");
				char valbuf[64] = {0};

				int base = 10;

				//Ignore the length, just use the base
				if(quote != NULL)
				{
					//Parse it
					char cbase;
					sscanf(quote, "'%c%63s", &cbase, valbuf);
					vstr = valbuf;

					if(cbase == 'h')
						base = 16;
					else if(cbase == 'b')
						base = 2;
					//default to decimal
				}

				//Parse it
				switch(base)
				{
					//decimal
					case 10:
					{
						if(chan->GetWidth() > 32)
						{
							throw JtagExceptionWrapper(
								"Decimal values for channels >32 bits not supported",
								"");
						}

						unsigned int val = atoi(vstr);
						for(int i=0; i<chan->GetWidth(); i++)
						{
							triggerbits[chan->GetWidth() - 1 - i] =
								(val & 1) ? Oscilloscope::TRIGGER_TYPE_HIGH : Oscilloscope::TRIGGER_TYPE_LOW;
							val >>= 1;
						}
					}
					break;

					//hex
					case 16:
					{
						//Go right to left
						int w = chan->GetWidth();
						int nbit = w-1;
						for(int i=strlen(vstr)-1; i>=0; i--, nbit -= 4)
						{
							if(nbit <= 0)
								break;

							//Is it an X? Don't care
							if(tolower(vstr[i]) == 'x')
							{
								if(nbit > 2)
									triggerbits[nbit-3] = Oscilloscope::TRIGGER_TYPE_DONTCARE;
								if(nbit > 1)
									triggerbits[nbit-2] = Oscilloscope::TRIGGER_TYPE_DONTCARE;
								if(nbit > 0)
									triggerbits[nbit-1] = Oscilloscope::TRIGGER_TYPE_DONTCARE;
								triggerbits[nbit] = Oscilloscope::TRIGGER_TYPE_DONTCARE;
							}

							//No, hex - convert this character to binary
							else
							{
								int x;
								char cbuf[2] = {vstr[i], 0};
								sscanf(cbuf, "%1x", &x);
								if(nbit > 2)
									triggerbits[nbit - 3] = (x & 8) ? Oscilloscope::TRIGGER_TYPE_HIGH : Oscilloscope::TRIGGER_TYPE_LOW;
								if(nbit > 1)
									triggerbits[nbit - 2] = (x & 4) ? Oscilloscope::TRIGGER_TYPE_HIGH : Oscilloscope::TRIGGER_TYPE_LOW;
								if(nbit > 0)
									triggerbits[nbit - 1] = (x & 2) ? Oscilloscope::TRIGGER_TYPE_HIGH : Oscilloscope::TRIGGER_TYPE_LOW;
								triggerbits[nbit] = (x & 1) ? Oscilloscope::TRIGGER_TYPE_HIGH : Oscilloscope::TRIGGER_TYPE_LOW;
							}
						}
					}
					break;

					//binary
					case 2:
					{
						//Right to left, one bit at a time
						int w = chan->GetWidth();
						int nbit = w-1;
						for(int i=strlen(vstr)-1; i>=0; i--, nbit --)
						{
							if(nbit <= 0)
								break;

							if(tolower(vstr[i]) == 'x')
								triggerbits[nbit] = Oscilloscope::TRIGGER_TYPE_DONTCARE;
							else if(vstr[i] == '1')
								triggerbits[nbit] = Oscilloscope::TRIGGER_TYPE_HIGH;
							else
								triggerbits[nbit] = Oscilloscope::TRIGGER_TYPE_LOW;
						}
					}
					break;
				}
			}

			//Feed to the scope
			m_parent->GetScope()->SetTriggerForChannel(chan, triggerbits);
		}

		//Unknown channel type
		else
		{
			LogError("Unknown channel type - maybe analog? Not supported\n");
		}
	}
}
*/
