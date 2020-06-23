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
 
#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#endif

#include "glscopeclient.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "../scopehal/SiglentSCPIOscilloscope.h"
#include "../scopehal/AgilentOscilloscope.h"
#include "../scopehal/LeCroyOscilloscope.h"
#include "../scopehal/RigolOscilloscope.h"
#include "../scopehal/RohdeSchwarzOscilloscope.h"
#include "../scopehal/AntikernelLogicAnalyzer.h"
#include "InstrumentConnectionDialog.h"
#include <libgen.h>
#include <omp.h>
#include <chrono>
#include <iostream>
#include <thread>

#include "PreferenceManager.h"

using namespace std;

//for color selection
int g_numDecodes = 0;

ScopeApp* g_app = NULL;

void help();

void help()
{
	fprintf(stderr,
			"glscopeclient [general options] [logger options] [filename|scope]\n"
			"\n"
			"  [general options]:\n"
			"    --help      : this message...\n"
			"    --nodata    : when loading a .scopesession from the command line, only load instrument/UI settings\n"
			"                  (default is to load waveform data too)\n"
			"    --nodigital : only display analog channels at startup\n"
			"                  (default is to display digital channels too)\n"
			"    --reconnect : when loading a .scopesession from the command line, reconnect to the instrument\n"
			"                  (default is to do offline analysis)\n"
			"    --retrigger : when loading a .scopesession from the command line, start triggering immediately\n"
			"                  (default is to be paused)\n"
			"    --version   : print version number. (not yet implemented)\n"
			"\n"
			"  [logger options]:\n"
			"    levels: ERROR, WARNING, NOTICE, VERBOSE, DEBUG\n"
			"    --quiet|-q                    : reduce logging level by one step\n"
			"    --verbose                     : set logging level to VERBOSE\n"
			"    --debug                       : set logging level to DEBUG\n"
			"    --trace <classname>|          : name of class with tracing messages. (Only relevant when logging level is DEBUG.)\n"
			"            <classname::function>\n"
			"    --logfile|-l <filename>       : output log messages to file\n"
			"    --logfile-lines|-L <filename> : output log messages to file, with line buffering\n"
			"    --stdout-only                 : writes errors/warnings to stdout instead of stderr\n"
			"\n"
			"  [filename|scope]:\n"
			"    filename : path to a .scopesession to load on startup\n"
			"    scope    : <scope name>:<scope driver>:<transport protocol>[:<transport arguments]\n"
			"\n"
			"  Examples:\n"
			"    glscopeclient --debug myscope:siglent:lxi:192.166.1.123\n"
			"    glscopeclient --debug --trace SCPITMCTransport myscope:siglent:usbtmc:/dev/usbtmc0\n"
			"    glscopeclient --reconnect --retrigger foobar.scopesession\n"
			"\n"
	);
}

int main(int argc, char* argv[])
{
    PreferenceManager mgr{ "test.yml" };
    mgr.SavePreferences();


	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	vector<string> scopes;
	string fileToLoad;
	bool reconnect = false;
	bool nodata = false;
	bool retrigger = false;
	bool nodigital = false;
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if(s == "--help")
		{
			help();
			return 0;
		}
		else if(s == "--version")
		{
			//not implemented
			//ShowVersion();
			return 0;
		}
		else if(s == "--reconnect")
			reconnect = true;
		else if(s == "--nodata")
			nodata = true;
		else if(s == "--retrigger")
			retrigger = true;
		else if(s == "--nodigital")
			nodigital = true;
		else if(s[0] == '-')
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}

		//Not a flag. Either a connection string or a save file name.
		else
		{
			//If there's a colon after the first few characters, it's a connection string
			//(Windows may have drive letter colons early on)
			auto colon = s.rfind(":");
			if( (colon != string::npos) && (colon > 1) )
				scopes.push_back(s);

			//Otherwise assume it's a save file
			else
				fileToLoad = s;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Change to the binary's directory so we can use relative paths for external resources
	//FIXME: portability warning: this only works on Linux
#ifdef _WIN32
	// Retrieve the file name of the current process image
	TCHAR binPath[MAX_PATH];
	
	if( GetModuleFileName(NULL, binPath, MAX_PATH) == 0 )
	{
		LogError("Error: GetModuleFileName() failed.\n");
		return 1;
	}
	
	// Remove file name from path
	if( !PathRemoveFileSpec(binPath) )
	{
		LogError("Error: PathRemoveFileSpec() failed.\n");
		return 1;
	}
	
	// Set it as current working directory
	if( SetCurrentDirectory(binPath) == 0 )
	{
		LogError("Error: SetCurrentDirectory() failed.\n");
		return 1;
	}

#else
	char binDir[1024];
	ssize_t readlinkReturn = readlink("/proc/self/exe", binDir, (sizeof(binDir) - 1) );
	if ( readlinkReturn < 0 )
	{
		//FIXME: add errno output
		LogError("Error: readlink() failed.\n");
		return 1;
	}
	else if ( readlinkReturn == 0 )
	{
		LogError("Error: readlink() returned 0.\n");
		return 1;
	}
	else if ( (unsigned) readlinkReturn > (sizeof(binDir) - 1) )
	{
		LogError("Error: readlink() returned a path larger than our buffer.\n");
		return 1;
	}
	else
	{
		//Null terminate result
		binDir[readlinkReturn - 1] = 0;

		//Change to our binary's directory
		if ( chdir(dirname(binDir)) != 0 )
		{
			//FIXME: add errno output
			LogError("Error: chdir() failed.\n");
			return 1;
		}
	}
#endif

	g_app = new ScopeApp;

	//Initialize object creation tables for predefined libraries
	TransportStaticInit();
	DriverStaticInit();
	ScopeProtocolStaticInit();

	//Initialize object creation tables for plugins
	InitializePlugins();

	//If there are no scopes and we're not loading a file, show the dialog to connect.
	//TODO: support multi-scope connection
	if(scopes.empty() && fileToLoad.empty())
	{
		InstrumentConnectionDialog dlg;
		if(dlg.run() != Gtk::RESPONSE_OK)
			return 0;

		scopes.push_back(dlg.GetConnectionString());
	}

	//Connect to the scope(s)
	for(auto s : scopes)
	{
		//Scope format: name:driver:transport:args
		char nick[128];
		char driver[128];
		char trans[128];
		char args[128];
		if(4 != sscanf(s.c_str(), "%127[^:]:%127[^:]:%127[^:]:%127s", nick, driver, trans, args))
		{
			LogError("Invalid scope string %s\n", s.c_str());
			continue;
		}

		//Create the transport
		SCPITransport* transport = SCPITransport::CreateTransport(trans, args);
		if(transport == NULL)
			continue;

		//Check if the transport failed to initialize
		if(!transport->IsConnected())
		{
			Gtk::MessageDialog dlg(
				string("Failed to connect to instrument using connection string ") + s,
				false,
				Gtk::MESSAGE_ERROR,
				Gtk::BUTTONS_OK,
				true);
			dlg.run();

			continue;
		}

		//Create the scope
		Oscilloscope* scope = Oscilloscope::CreateOscilloscope(driver, transport);
		if(scope == NULL)
			continue;

		//All good, hook it up
		scope->m_nickname = nick;
		g_app->m_scopes.push_back(scope);
	}

	g_app->run(fileToLoad, reconnect, nodata, retrigger, nodigital);
	delete g_app;
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

	omp_set_num_threads(8);

	double tlast = GetTime();
	size_t npolls = 0;
	double dt = 0;
	while(!g_app->IsTerminating())
	{
		size_t npending = scope->GetPendingWaveformCount();

		//If the queue is too big, stop grabbing data
		if(npending > 100)
		{
			LogTrace("Queue is too big, sleeping\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			tlast = GetTime();
			continue;
		}

		//If the queue is more than 5 sec long, wait for a while before polling any more.
		//We've gotten ahead of the UI!
		if(npending*dt > 5)
		{
			LogTrace("Capture thread got 5 sec ahead of UI, sleeping\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			tlast = GetTime();
			continue;
		}

		//If trigger isn't armed, don't even bother polling for a while.
		if(!scope->IsTriggerArmed())
		{
			LogTrace("Scope isn't armed, sleeping\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			tlast = GetTime();
			continue;
		}

		auto stat = scope->PollTrigger();

		if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
		{
			//Collect the data, fail if that doesn't work
			if(!scope->AcquireData(true))
			{
				tlast = GetTime();
				continue;
			}

			//Measure how long the acquisition took
			double now = GetTime();
			dt = now - tlast;
			tlast = now;
			//LogDebug("Triggered, dt = %.3f ms (npolls = %zu)\n",
			//	dt*1000, npolls);

			npolls = 0;

			continue;
		}

		//Wait 1ms before polling again, so the UI thread has a chance to grab the mutex
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		npolls ++;
	}
}
