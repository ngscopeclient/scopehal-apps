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
	@brief Program entry point
 */

#include "psuclient.h"
#include "MainWindow.h"
#include "../scopehal/RohdeSchwarzHMC804xPowerSupply.h"
#include <thread>

using namespace std;

bool g_terminating = false;

void PSUThread(PowerSupply* psu);

/**
	@brief The main application class
 */
class PSUApp : public Gtk::Application
{
public:
	PSUApp()
	 : Gtk::Application()
	 , m_window(NULL)
	{}

	virtual ~PSUApp();

	static Glib::RefPtr<PSUApp> create()
	{
		return Glib::RefPtr<PSUApp>(new PSUApp);
	}

	vector<PowerSupply*> m_psus;

	virtual void run();

protected:
	MainWindow* m_window;

	virtual void on_activate();

	vector<thread*> m_threads;
};

PSUApp::~PSUApp()
{
	for(auto t : m_threads)
	{
		t->join();
		delete t;
	}
}

void PSUApp::run()
{
	register_application();
	on_activate();

	while(true)
	{
		//Poll the scope to see if we have any new data
		//m_window->PollScopes();

		//Dispatch events if we have any
		while(Gtk::Main::events_pending())
			Gtk::Main::iteration();

		//Stop if the main window got closed
		if(!m_window->is_visible())
			break;
	}

	g_terminating = true;

	delete m_window;
	m_window = NULL;
}

/**
	@brief Create windows for each instrument
 */
void PSUApp::on_activate()
{
	//Start the scope threads
	for(auto psu : m_psus)
		m_threads.push_back(new thread(PSUThread, psu));

	//Test application
	m_window = new MainWindow(m_psus);
	add_window(*m_window);
	m_window->present();
}

int main(int argc, char* argv[])
{
	auto app = PSUApp::create();

	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	vector<string> psus;
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if(s == "--help")
		{
			//not implemented
			return 0;
		}
		else if(s == "--version")
		{
			//not implemented
			//ShowVersion();
			return 0;
		}
		else if(s[0] == '-')
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
		else
			psus.push_back(s);
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Connect to the PSU(s)
	for(auto s : psus)
	{
		//Scope format: name:api:host[:port]
		char nick[128];
		char api[128];
		char host[128];
		int port = 0;
		if(4 != sscanf(s.c_str(), "%127[^:]:%127[^:]:%127[^:]:%d", nick, api, host, &port))
		{
			if(3 != sscanf(s.c_str(), "%127[^:]:%127[^:]:%127[^:]", nick, api, host))
			{
				LogError("Invalid PSU string %s\n", s.c_str());
				continue;
			}
		}

		string sapi(api);

		//Connect to the scope
		if(sapi == "rs_hmc8")
		{
			//default port if not specified
			if(port == 0)
				port = 5025;

			auto psu = new RohdeSchwarzHMC804xPowerSupply(new SCPISocketTransport(host, port));
			psu->m_nickname = nick;
			app->m_psus.push_back(psu);
		}
		else
		{
			LogError("Unrecognized API \"%s\", use --help\n", api);
			return 1;
		}
	}

	app->run();
	return 0;
}

double GetTime()
{
#ifdef _WIN32
	uint64_t tm;
	static uint64_t freq = 0;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&tm));
	double ret = tm;
	if(freq == 0)
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
	return ret / freq;
#else
	timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	double d = static_cast<double>(t.tv_nsec) / 1E9f;
	d += t.tv_sec;
	return d;
#endif
}

void PSUThread(PowerSupply* psu)
{
	#ifndef _WIN32
	pthread_setname_np(pthread_self(), "PSUThread");
	#endif

	/*
	uint32_t delay_us = 1000;
	double tlast = GetTime();
	size_t npolls = 0;
	uint32_t delay_max = 500 * 1000;
	uint32_t delay_min = 250;
	double dt = 0;
	while(!g_terminating)
	{
		size_t npending = scope->GetPendingWaveformCount();

		//LogDebug("delay = %.2f ms, pending=%zu\n", delay_us * 0.001f, npending );

		//If the queue is too big, stop grabbing data
		if(npending > 5000)
		{
			usleep(50 * 1000);
			tlast = GetTime();
			continue;
		}

		//If the queue is more than 5 sec long, wait for a while before polling any more.
		//We've gotten ahead of the UI!
		if(npending*dt > 5)
		{
			//LogDebug("Capture thread got 5 sec ahead of UI, pausing\n");
			usleep(50 * 1000);
			tlast = GetTime();
			continue;
		}

		//If trigger isn't armed, don't even bother polling for a while.
		if(!scope->IsTriggerArmed())
		{
			usleep(50 * 1000);
			tlast = GetTime();
			continue;
		}

		auto stat = scope->PollTrigger();

		if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
		{
			scope->AcquireData(true);

			//Measure how long the acquisition took
			double now = GetTime();
			dt = now - tlast;
			tlast = now;
			//LogDebug("Triggered, dt = %.3f ms (npolls = %zu, delay_ms = %.2f)\n",
			//	dt*1000, npolls, delay_us * 0.001f);

			//Adjust polling interval so that we poll a handful of times between triggers
			if(npolls > 5)
			{
				delay_us *= 1.5;

				//Don't increase poll interval beyond 500ms. If we hit that point the scope is either insanely slow,
				//or they're targeting some kind of intermittent signal. Don't add more lag on top of that!
				if(delay_us > delay_max)
					delay_us = delay_max;
			}
			if(npolls < 2)
			{
				delay_us /= 1.5;
				if(delay_us < delay_min)
					delay_us = delay_min;
			}

			//If we have a really high trigger latency (super low bandwidth link?)
			//then force the delay to be a bit higher so we have time for other threads to get to the scope
			if(dt > 2000)
			{
				if(delay_us < 5000)
					delay_us = 5000;
			}

			npolls = 0;

			continue;
		}
		npolls ++;

		//We didn't trigger. Wait a while before the next time we poll to avoid hammering slower hardware.
		usleep(delay_us);

		//If we've polled a ton of times and the delay is tiny, do a big step increase
		if(npolls > 50)
		{
			//LogDebug("Super laggy scope, bumping polling interval\n");
			delay_us *= 10;
			npolls = 0;
		}
	}
	*/
}
