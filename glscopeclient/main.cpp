/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2018 Andrew D. Zonenberg                                                                          *
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
#include "../scopehal/LeCroyVICPOscilloscope.h"

using namespace std;

/**
	@brief The main application class
 */
class ScopeApp : public Gtk::Application
{
public:
	ScopeApp()
	 : Gtk::Application()
	 , m_scope(NULL)
	 , m_window(NULL)
	{}

	virtual ~ScopeApp();

	static Glib::RefPtr<ScopeApp> create()
	{
		return Glib::RefPtr<ScopeApp>(new ScopeApp);
	}

	Oscilloscope* m_scope;
	std::string m_host;

protected:
	Gtk::Window* m_window;

	virtual void on_activate();
};

ScopeApp::~ScopeApp()
{
	delete m_window;
}

/**
	@brief Create windows for each instrument
 */
void ScopeApp::on_activate()
{
	//Test application
	m_window = new OscilloscopeWindow(m_scope, m_host, 0);
	add_window(*m_window);
	m_window->present();
}

int main(int argc, char* argv[])
{
	auto app = ScopeApp::create();

	//FIXME: proper way to locate shaders etc
	chdir("/nfs4/home/azonenberg/code/scopehal-cmake/src/glscopeclient/");

	//Global settings
	unsigned short port = 0;
	string server = "";
	string api = "redtin_uart";

	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
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
		else if(s == "--port")
			port = atoi(argv[++i]);
		else if(s == "--server")
			server = argv[++i];
		else if(s == "--api")
			api = argv[++i];
		else if(s == "--version")
		{
			//not implemented
			//ShowVersion();
			return 0;
		}
		else
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Initialize the protocol decoder library
	ScopeProtocolStaticInit();

	//Connect to the server
	if(api == "lecroy_vicp")
	{
		//default port if not specified
		if(port == 0)
			port = 1861;

		app->m_scope = new LeCroyVICPOscilloscope(server, port);
		app->m_host = server;
	}
	else
	{
		LogError("Unrecognized API \"%s\", use --help\n", api.c_str());
		return 1;
	}

	//and run the app
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
