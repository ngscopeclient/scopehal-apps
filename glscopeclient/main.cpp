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

#include "glscopeclient.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "../scopemeasurements/scopemeasurements.h"
#include "../scopehal/SiglentSCPIOscilloscope.h"
#include "../scopehal/AgilentOscilloscope.h"
#include "../scopehal/LeCroyOscilloscope.h"
#include "../scopehal/RigolOscilloscope.h"
#include "../scopehal/RohdeSchwarzOscilloscope.h"
#include "../scopehal/AntikernelLogicAnalyzer.h"
#include <libgen.h>

using namespace std;

//for color selection
int g_numDecodes = 0;

ScopeApp* g_app = NULL;

int main(int argc, char* argv[])
{
	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	vector<string> scopes;
	string fileToLoad;
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

	g_app = new ScopeApp;

	//Initialize object creation tables for predefined libraries
	TransportStaticInit();
	DriverStaticInit();
	ScopeProtocolStaticInit();
	ScopeMeasurementStaticInit();

	//Initialize object creation tables for plugins
	InitializePlugins();

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

		//Create the scope
		Oscilloscope* scope = Oscilloscope::CreateOscilloscope(driver, transport);
		if(scope == NULL)
			continue;

		//All good, hook it up
		scope->m_nickname = nick;
		g_app->m_scopes.push_back(scope);
	}

	g_app->run(fileToLoad);
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

	uint32_t delay_us = 1000;
	double tlast = GetTime();
	size_t npolls = 0;
	uint32_t delay_max = 500 * 1000;
	uint32_t delay_min = 250;
	double dt = 0;
	while(!g_app->IsTerminating())
	{
		size_t npending = scope->GetPendingWaveformCount();

		//LogDebug("delay = %.2f ms, pending=%zu\n", delay_us * 0.001f, npending );

		//If the queue is too big, stop grabbing data
		if(npending > 100)
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
}
