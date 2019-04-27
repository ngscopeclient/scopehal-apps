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

#include "glscopeclient.h"
#include "OscilloscopeWindow.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "../scopemeasurements/scopemeasurements.h"
#include "../scopehal/LeCroyVICPOscilloscope.h"
#include <thread>

using namespace std;

//for color selection
int g_numDecodes = 0;

bool g_terminating = false;

void ScopeThread(Oscilloscope* scope);

/**
	@brief The main application class
 */
class ScopeApp : public Gtk::Application
{
public:
	ScopeApp()
	 : Gtk::Application()
	 , m_window(NULL)
	{}

	virtual ~ScopeApp();

	static Glib::RefPtr<ScopeApp> create()
	{
		return Glib::RefPtr<ScopeApp>(new ScopeApp);
	}

	vector<Oscilloscope*> m_scopes;

	virtual void run();

protected:
	OscilloscopeWindow* m_window;

	virtual void on_activate();

	vector<thread*> m_threads;
};

ScopeApp::~ScopeApp()
{
	for(auto t : m_threads)
	{
		t->join();
		delete t;
	}
}

void ScopeApp::run()
{
	register_application();
	on_activate();

	while(true)
	{
		//Poll the scope to see if we have any new data
		m_window->PollScopes();

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
void ScopeApp::on_activate()
{
	//Start the scope threads
	for(auto scope : m_scopes)
		m_threads.push_back(new thread(ScopeThread, scope));

	//Test application
	m_window = new OscilloscopeWindow(m_scopes);
	add_window(*m_window);
	m_window->present();
}

int main(int argc, char* argv[])
{
	auto app = ScopeApp::create();

	//FIXME: proper way to locate shaders etc
	chdir("/nfs4/home/azonenberg/code/scopehal-cmake/src/glscopeclient/");

	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	vector<string> scopes;
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
			scopes.push_back(s);
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Initialize the protocol decoder and measurement libraries
	ScopeProtocolStaticInit();
	ScopeMeasurementStaticInit();

	//Connect to the scope(s)
	for(auto s : scopes)
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
				LogError("Invalid scope string %s\n", s.c_str());
				continue;
			}
		}

		string sapi(api);

		//Connect to the scope
		if(sapi == "lecroy_vicp")
		{
			//default port if not specified
			if(port == 0)
				port = 1861;

			auto scope = new LeCroyVICPOscilloscope(host, port);
			scope->m_nickname = nick;
			app->m_scopes.push_back(scope);
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

void ScopeThread(Oscilloscope* scope)
{
	#ifndef _WIN32
	pthread_setname_np(pthread_self(), "ScopeThread");
	#endif

	while(!g_terminating)
	{
		//If the queue is too big, stop grabbing data
		if(scope->GetPendingWaveformCount() > 5000)
		{
			usleep(50 * 1000);
			continue;
		}

		//If the scope isn't triggering, hold off for a bit
		if(!scope->IsTriggerArmed())
		{
			usleep(50 * 1000);
			continue;
		}

		if(scope->PollTrigger() == Oscilloscope::TRIGGER_MODE_TRIGGERED)
		{
			scope->AcquireData(true);
			LogDebug("Now have %zu pending waveforms\n", scope->GetPendingWaveformCount());
		}
	}
}
