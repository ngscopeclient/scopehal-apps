/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of main application class
 */

#include "glscopeclient.h"
#include "../scopehal/MockOscilloscope.h"

using namespace std;

ScopeApp::~ScopeApp()
{
	ShutDownSession();
}

void ScopeApp::run(
	vector<Oscilloscope*> scopes,
	vector<string> filesToLoad,
	bool reconnect,
	bool nodata,
	bool retrigger)
{
	register_application();

	//Get the system configured locale for numbers, then set the default back to "C"
	//so we get . as decimal point for all interchange.
	//All numbers printed for display use the Unit class which will use the user's requested locale.
	Unit::SetLocale(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "C");

	//Initialize locale configuration to per-thread on Windows
#ifdef _WIN32
	_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
#endif

	m_window = new OscilloscopeWindow(scopes);
	add_window(*m_window);

	//Handle file loads specified on the command line
	for(auto f : filesToLoad)
	{
		//Ignore blank files (should never happen)
		if(f.empty())
			continue;

		//Can only load one scopesession
		else if (f.find(".scopesession") != string::npos)
		{
			m_window->DoFileOpen(f, !nodata, reconnect);
			break;
		}

		//For any external file format, create an import filter
		Filter* filter = NULL;
		string color = GetDefaultChannelColor(g_numDecodes ++);

		if(f.find(".wav") != string::npos)
		{
			filter = Filter::CreateFilter("WAV Import", color);
			filter->GetParameter("WAV File").SetFileName(f);
		}

		else if(f.find(".bin") != string::npos)
		{
			filter = Filter::CreateFilter("BIN Import", color);
			filter->GetParameter("BIN File").SetFileName(f);
		}

		//Complex I/Q: user probably will have to override format specifics later
		else if(f.find(".complex") != string::npos)
		{
			filter = Filter::CreateFilter("Complex Import", color);
			filter->GetParameter("Complex File").SetFileName(f);
		}

		else if(f.find(".csv") != string::npos)
		{
			filter = Filter::CreateFilter("CSV Import", color);
			filter->GetParameter("CSV File").SetFileName(f);
		}

		else if(f.find(".vcd") != string::npos)
		{
			filter = Filter::CreateFilter("VCD Import", color);
			filter->GetParameter("VCD File").SetFileName(f);
		}

		else if(f.find(".wfm") != string::npos)
		{
			filter = Filter::CreateFilter("WFM Import", color);
			filter->GetParameter("WFM File").SetFileName(f);
		}

		else if( (f.find(".s") != string::npos) && (f[f.length()-1] == 'p') )
		{
			filter = Filter::CreateFilter("Touchstone Import", color);
			filter->GetParameter("Touchstone File").SetFileName(f);
		}

		else
		{
			LogError("Unrecognized file extension, ignoring %s\n", f.c_str());
			continue;
		}

		//Name the filter
		string base = BaseName(f);
		size_t dot = base.find('.');
		if(dot != string::npos)
			base = base.substr(0, dot);
		filter->SetDisplayName(base);

		//Add all of the streams
		for(size_t i=0; i<filter->GetStreamCount(); i++)
			m_window->OnAddChannel(StreamDescriptor(filter, i));
	}

	m_window->present();

	//If no scope threads are running already (from a file load), start them now
	if(m_threads.empty())
		StartScopeThreads(scopes);

	//If retriggering, start the trigger
	if(retrigger)
		m_window->OnStart();

	while(true)
	{
		Gtk::Main::iteration();

		//Stop if the main window got closed
		if(!m_window->is_visible())
			break;
	}

	m_terminating = true;

	delete m_window;
	m_window = NULL;
}

void ScopeApp::DispatchPendingEvents()
{
	while(Gtk::Main::events_pending())
		Gtk::Main::iteration();
}

/**
	@brief Shuts down the current session and disconnects from all instruments but don't close the window
 */
void ScopeApp::ShutDownSession()
{
	//Set terminating flag so all current ScopeThread's terminate
	m_terminating = true;

	//Wait for all threads to shut down and remove them
	for(auto t : m_threads)
	{
		t->join();
		delete t;
	}
	m_threads.clear();

	//Back to normal mode
	m_terminating = false;
}

void ScopeApp::StartScopeThreads(vector<Oscilloscope*> scopes)
{
	//Start the scope threads
	for(auto scope : scopes)
	{
		//Mock scopes can't trigger, so don't waste time polling them
		if(dynamic_cast<MockOscilloscope*>(scope) != NULL)
			continue;

		m_threads.push_back(new thread(ScopeThread, scope));
	}
}

/**
	@brief Connect to one or more scopes
 */
vector<Oscilloscope*> ScopeApp::ConnectToScopes(vector<string> scopes)
{
	vector<Oscilloscope*> ret;

	for(auto s : scopes)
	{
		//Scope format: name:driver:transport:args
		char nick[128];
		char driver[128];
		char trans[128];
		char args[128];
		if(4 != sscanf(s.c_str(), "%127[^:]:%127[^:]:%127[^:]:%127s", nick, driver, trans, args))
		{
			args[0] = '\0';
			if(3 != sscanf(s.c_str(), "%127[^:]:%127[^:]:%127[^:]", nick, driver, trans))
			{
				LogError("Invalid scope string %s\n", s.c_str());
				continue;
			}
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
		ret.push_back(scope);
	}

	return ret;
}
