/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg                                                                          *
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
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ngscopeclient.h"
#include "ngscopeclient-version.h"
#include "MainWindow.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "imgui_internal.h"

using namespace std;

unique_ptr<MainWindow> g_mainWindow;

GuiLogSink* g_guiLog;

#ifndef _WIN32
void Relaunch(int argc, char* argv[]);
#endif

static void print_help(FILE* stream)
{
	fprintf(stream,
		"usage: ngscopeclient [option...] [session | instrument...]\n"
		"\n"
		"ngscopeclient is a test and measurement remote control and analysis suite\n"
		"\n"
		"General options:\n"
		"  --version    print the application version and exit\n"
		"  --help, -h   print this help and exit\n"
		"\n"
		"Logging options:\n"
		"  -q, --quiet  make logging one level quieter (can be repeated)\n"
		"  --verbose    emit more detailed logs that might be useful to end users\n"
		"  --debug      emit very detailed logs only useful to developers\n"
		"  --trace <channel>\n"
		"      emit maximally detailed logs for the given channel\n"
		"  -l, --logfile <file>\n"
		"      write log entries to the specified file\n"
		"  -L, --logfile-lines <file>\n"
		"      write log entries to the specified file with line buffering\n"
		"  --stdout-only\n"
		"      only write logs to stdout (normally warning and above go to stderr)\n"
		"\n"
		"Session files:\n"
		"  If you wish to resume a prior session, pass the path to a session file\n"
		"  saved from the graphical interface as the sole non-option argument.\n"
		"  The file name _must_ end in '.scopesession'.\n"
		"\n"
		"Instrument connection strings:\n"
		"  When starting a new session, you may provide one or more instrument\n"
		"  connection strings as arguments, which will be added to the session.\n"
		"  Connection strings are not accepted when resuming an existing session.\n"
		"\n"
		"For full documentation, see https://ngscopeclient.org\n"
	);
}

int main(int argc, char* argv[])
{
	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Windows needs special console handling!
	#ifdef _WIN32
		bool attachConsoleFailed = false;
		bool getConsoleWindowFailed = false;
		//If we have a parent process console, we were probably run from a powershell/cmd.exe session.
		//If we had one, we need to attach to it (since as a Win32 subsystem application we aren't connected by default)
		//Failing here indicates we were run from explorer, and thus should not be spawning a console window
		//(we just log to the GuiLogSink instead)
		if(!AttachConsole(ATTACH_PARENT_PROCESS))
		{
			attachConsoleFailed = true;
		}

		//Once we've attached to the console (if we had one), make sure we had a window for it
		else if(GetConsoleWindow() == NULL)
			getConsoleWindowFailed = true;

		//If we get here, we were run from a Windows shell session and should log to that console
		else
		{
			//We're using the existing parent process console.
			//Reopen stdio streams so they point to it
			freopen("CON", "w", stdout);
			freopen("CON", "w", stderr);
			freopen("CON", "r", stdin);
		}
	#endif

	string sessionToOpen;
	bool noMaximize = false;
	vector<string> instrumentConnectionStrings;
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if (s == "--version")
		{
			fprintf(stdout, "ngscopeclient %s\n", NGSCOPECLIENT_VERSION);
			return 0;
		}

		if (s == "--help" || s == "-h")
		{
			print_help(stdout);
			return 0;
		}

		if (s == "--no-maximize" || s == "-nm")
		{
			noMaximize = true;
			continue;
		}

		//Other switch (unrecognized)
		if(s.find('-') == 0)
		{
			//Don't know what it is
			fprintf(stderr, "ngscopeclient: unrecognized option '%s'\n", s.c_str());
			fprintf(stderr, "Try 'ngscopeclient --help' for more information.\n");
			return 1;
		}

		//If it ends in .scopesession assume it's a session file
		if(s.find(".scopesession") != string::npos)
			sessionToOpen = s;

		//Assume it's an instrument
		else
			instrumentConnectionStrings.push_back(s);
	}

	//Set up logging to the GUI
	g_guiLog = new GuiLogSink(console_verbosity);
	g_log_sinks.push_back(unique_ptr<GuiLogSink>(g_guiLog));

	//Windows needs special console handling!
	#ifdef _WIN32

		//If we have a parent process console, we were probably run from a powershell/cmd.exe session.
		//If we had one, we need to attach to it (since as a Win32 subsystem application we aren't connected by default)
		//Failing here indicates we were run from explorer, and thus should not be spawning a console window
		//(we just log to the GuiLogSink instead)
		if(attachConsoleFailed)
		{
			LogNotice(
				"Startup: skipping stdout log sink since not run from a console "
				"(AttachConsole reports parent process has no console)\n");
		}

		//Once we've attached to the console (if we had one), make sure we had a window for it
		else if(getConsoleWindowFailed)
			LogNotice("Startup: skipping stdout log sink since not run from a console (no console window)\n");

		//If we get here, we were run from a Windows shell session and should log to that console
		else
		{
	#endif

			//Creating the log sink is done on all platforms, windows and otherwise
			g_log_sinks.push_back(make_unique<ColoredSTDLogSink>(console_verbosity));

	#ifdef _WIN32
			LogNotice("Startup: run from a console, keeping stdout log sink attached\n");
		}
	#endif

	//Can't load a session and reconnect to an instrument, has to be one or the other
	if( !sessionToOpen.empty() && !instrumentConnectionStrings.empty())
	{
		LogError("Cannot load a session and connect to an instrument simultaneously\n");
		return 1;
	}

	//Complain if the OpenMP wait policy isn't set right
	const char* policy = getenv("OMP_WAIT_POLICY");
	#ifndef _WIN32
		bool need_relaunch = false;
	#endif
	if((policy == NULL) || (strcmp(policy, "PASSIVE") != 0) )
	{
		#ifdef _WIN32
			LogWarning("ngscopeclient works best with the OMP_WAIT_POLICY environment variable set to PASSIVE\n");
		#else
			LogDebug("OMP_WAIT_POLICY not set to PASSIVE\n");
			setenv("OMP_WAIT_POLICY", "PASSIVE", true);

			need_relaunch = true;
		#endif
	}

	//Note if asan is active
	#ifdef __SANITIZE_ADDRESS__
	LogDebug("Compiled with AddressSanitizer\n");
	#endif

	#ifndef _WIN32
		if(need_relaunch)
		{
			LogDebug("Re-exec'ing with correct environment\n");
			Relaunch(argc, argv);
		}
	#endif

	//Make locale handling thread safe on Windows
	#ifdef _WIN32
	_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
	Unit::SetDefaultLocale();
	#endif

	//Initialize object creation tables for predefined libraries
	if(!VulkanInit())
		return 1;
	TransportStaticInit();
	DriverStaticInit();
	ScopeProtocolStaticInit();
	InitializePlugins();

	{
		//Make the top level window
		shared_ptr<QueueHandle> queue(g_vkQueueManager->GetRenderQueue("g_mainWindow.render"));
		g_mainWindow = make_unique<MainWindow>(queue,noMaximize);

		auto& session = g_mainWindow->GetSession();

		//Load a session on startup if requested
		if(!sessionToOpen.empty())
			g_mainWindow->SetStartupSession(sessionToOpen);

		//Render the main window once, so it can initialize a new empty session before we connect any instruments
		glfwPollEvents();
		g_mainWindow->Render();

		//Initialize the session with the requested arguments
		for(auto s : instrumentConnectionStrings)
		{
			LogTrace("Setup: connecting to %s\n", s.c_str());
			LogIndenter li;

			char name[128];
			char driver[128];
			char transport[128];
			char args[256];
			sscanf(s.c_str(), "%127[^:]:%127[^:]:%127[^:]:%255s", name, driver, transport, args);

			//Try to connect
			auto ptransport = SCPITransport::CreateTransport(transport, args);
			if(ptransport == nullptr)
			{
				LogError("Failed to create transport of type \"%s\"\n", transport);
				return 1;
			}
			if(!ptransport->IsConnected())
			{
				delete ptransport;
				LogError("Failed to connect to \"%s\"\n", args);
				return 1;
			}

			session.CreateAndAddInstrument(driver, ptransport, name);
		}

		//Main event loop
		while(!glfwWindowShouldClose(g_mainWindow->GetWindow()))
		{
			//Check which event loop model to use
			if(session.GetPreferences().GetEnumRaw("Power.Events.event_driven_ui") == 1)
				glfwWaitEventsTimeout(session.GetPreferences().GetReal("Power.Events.polling_timeout") / FS_PER_SECOND);
			else
				glfwPollEvents();

			//Draw the main window
			g_mainWindow->Render();
		}

		session.ClearBackgroundThreads();
	}

	//Done, clean up
	g_mainWindow = nullptr;
	ScopehalStaticCleanup();
	return 0;
}

#ifndef _WIN32
void Relaunch(int argc, char* argv[])
{
	//make a copy of arguments since argv[] does not have to be null terminated, but execvp requires that
	vector<char*> args;
	for(int i=0; i<argc; i++)
		args.push_back(argv[i]);
	args.push_back(NULL);

	//Launch ourself with the new environment
	execvp(argv[0], &args[0]);
}
#endif

/**
	@brief Helper function for right justified text in a table
 */
void RightJustifiedText(const string& str)
{
	//Getting column width is a pain, we have to use some nonpublic APIs here
	auto rect = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), ImGui::TableGetColumnIndex());

	float delta = rect.GetWidth() -
		(ImGui::CalcTextSize(str.c_str()).x + ImGui::GetScrollX() + 2*ImGui::GetStyle().ItemSpacing.x);
	if(delta < 0)
		delta = 0;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + delta);
	ImGui::TextUnformatted(str.c_str());
}

/**
	@brief Check if two rectangles intersect
 */
bool RectIntersect(ImVec2 posA, ImVec2 sizeA, ImVec2 posB, ImVec2 sizeB)
{
	//Enlarge hitboxes by a small margin to keep spacing between nodes
	float margin = 5;
	posA.x -= margin;
	posA.y -= margin;
	posB.x -= margin;
	posB.y -= margin;
	sizeA.x += 2*margin;
	sizeA.y += 2*margin;
	sizeB.x += 2*margin;
	sizeB.y += 2*margin;

	//A completely above B? No intersection
	if( (posA.y + sizeA.y) < posB.y)
		return false;

	//B completely above A? No intersection
	if( (posB.y + sizeB.y) < posA.y)
		return false;

	//A completely left of B? No intersection
	if( (posA.x + sizeA.x) < posB.x)
		return false;

	//B completely left of A? No intersection
	if( (posB.x + sizeB.x) < posA.x)
		return false;

	//If we get here, they overlap
	return true;
}

/**
	@brief Check if a rectangle is completely within the other one

	A is outer, B is inner
 */
bool RectContains(ImVec2 posA, ImVec2 sizeA, ImVec2 posB, ImVec2 sizeB)
{
	//Top left of B must be in A
	ImVec2 brA (posA.x + sizeA.x, posA.y + sizeA.y);
	if( (posB.x < posA.x) || (posB.x >= brA.x) )
		return false;
	if( (posB.y < posA.y) || (posB.y >= brA.y) )
		return false;

	//Bottom right of B must be in A
	ImVec2 brB (posB.x + sizeB.x, posB.y + sizeB.y);
	if( (brB.x < posA.x) || (brB.x >= brA.x) )
		return false;
	if( (brB.y < posA.y) || (brB.y >= brA.y) )
		return false;

	//Contianed if we get here
	return true;
}
