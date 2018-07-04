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
	@brief Program entry point
 */

#include "scopeclient.h"
#include "OscilloscopeWindow.h"
#include "DMMWindow.h"
#include "PSUWindow.h"
#include "ScopeConnectionDialog.h"
//#include "../scopehal/NetworkedOscilloscope.h"
#include "../scopehal/RedTinLogicAnalyzer.h"
#include "../scopehal/LeCroyVICPOscilloscope.h"
#include "../scopehal/RohdeSchwarzHMC804xPowerSupply.h"
#include "../scopeprotocols/scopeprotocols.h"

using namespace std;

class InstrumentInfo
{
public:
	Instrument* m_inst;
	string m_server;
	unsigned short m_port;

	InstrumentInfo(Instrument* o, string s, unsigned short p)
		: m_inst(o)
		, m_server(s)
		, m_port(p)
	{}
};

/**
	@brief The main application class
 */
class ScopeApp : public Gtk::Application
{
public:
	ScopeApp()
	 : Gtk::Application()
	{}

	virtual ~ScopeApp();

	static Glib::RefPtr<ScopeApp> create()
	{
		return Glib::RefPtr<ScopeApp>(new ScopeApp);
	}

	vector<InstrumentInfo> m_instruments;

protected:
	vector<Gtk::Window*> m_windows;

	virtual void on_activate();
};

ScopeApp::~ScopeApp()
{
	for(auto w : m_windows)
		delete w;
	for(auto i : m_instruments)
		delete i.m_inst;
}

/**
	@brief Create windows for each instrument
 */
void ScopeApp::on_activate()
{
	for(auto i : m_instruments)
	{
		auto features = i.m_inst->GetInstrumentTypes();

		//Add UI for the oscilloscope
		if(features & Instrument::INST_OSCILLOSCOPE)
		{
			auto w = new OscilloscopeWindow(dynamic_cast<Oscilloscope*>(i.m_inst), i.m_server, i.m_port);
			m_windows.push_back(w);
			add_window(*w);
			w->present();
		}

		//Add UI for the DMM
		if(features & Instrument::INST_DMM)
		{
			auto w = new DMMWindow(dynamic_cast<Multimeter*>(i.m_inst), i.m_server, i.m_port);
			m_windows.push_back(w);
			add_window(*w);
			w->present();
		}

		//Add UI for the power supply
		if(features & Instrument::INST_PSU)
		{
			auto w = new PSUWindow(dynamic_cast<PowerSupply*>(i.m_inst), i.m_server, i.m_port);
			m_windows.push_back(w);
			add_window(*w);
			w->present();
		}
	}
}

int main(int argc, char* argv[])
{
	int exit_code = 0;
	try
	{
		auto app = ScopeApp::create();

		//Global settings
		unsigned short port = 0;
		string server = "";
		string api = "redtin_uart";
		//bool scripted = false;
		string scopename = "";
		string tty = "/dev/ttyUSB0";

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
			//else if(s == "--scripted")
			//	scripted = true;
			else if(s == "--scopename")
				scopename = argv[++i];
			else if(s == "--tty")
				tty = argv[++i];
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
		/*if(api == "scoped")
			scope = new NetworkedOscilloscope(server, port);
		else if(api == "redtin")
		{
			//Not scripting? Normal dialog process
			if(!scripted)
			{
				ScopeConnectionDialog dlg(server, port);
				if(Gtk::RESPONSE_OK != dlg.run())
					return 0;

				namesrvr = dlg.DetachNameServer();
				scope = dlg.DetachScope();
			}
			else
			{
				RedTinLogicAnalyzer* la = new RedTinLogicAnalyzer(server, port);
				namesrvr = new NameServer(&la->m_iface);
				la->Connect(scopename);
				scope = la;
			}
		}*/
		if(api == "redtin_uart")
			app->m_instruments.push_back(InstrumentInfo(new RedTinLogicAnalyzer(tty, 115200), server, port));
		else if(api == "lecroy_vicp")
		{
			//default port if not specified
			if(port == 0)
				port = 1861;

			app->m_instruments.push_back(InstrumentInfo(new LeCroyVICPOscilloscope(server, port), server, port));
		}
		else if(api == "rohdeschwarz_psu")
		{
			//default port if not specified
			if(port == 0)
				port = 5025;

			app->m_instruments.push_back(
				InstrumentInfo(new RohdeSchwarzHMC804xPowerSupply(server, port), server, port));
		}
		else
		{
			LogError("Unrecognized API \"%s\", use --help\n", api.c_str());
			return 1;
		}

		//and run the app
		app->run();
	}
	catch(const JtagException& ex)
	{
		LogError("%s\n", ex.GetDescription().c_str());
		exit_code = 1;
	}

	return exit_code;
}
