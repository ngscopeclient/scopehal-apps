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
	@brief Program entry point
 */

#include "psuclient.h"
#include "MainWindow.h"
#include "../scopehal/RohdeSchwarzHMC804xPowerSupply.h"

using namespace std;

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
};

PSUApp::~PSUApp()
{

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
	delete m_window;
	m_window = NULL;
}

/**
	@brief Create windows for each instrument
 */
void PSUApp::on_activate()
{
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
		char args[128];
		if(3 != sscanf(s.c_str(), "%127[^:]:%127[^:]:%127s", nick, api, args))
		{
			LogError("Invalid PSU string %s\n", s.c_str());
			continue;
		}

		string sapi(api);

		//Connect to the scope
		if(sapi == "rs_hmc8")
		{
			auto psu = new RohdeSchwarzHMC804xPowerSupply(new SCPISocketTransport(args));
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

