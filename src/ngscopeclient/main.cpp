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
#include "MainWindow.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "imgui_internal.h"

using namespace std;

unique_ptr<MainWindow> g_mainWindow;

GuiLogSink* g_guiLog;

#ifndef _WIN32
void Relaunch(int argc, char* argv[]);
#endif

int main(int argc, char* argv[])
{
	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		//TODO: other arguments

	}

	//Set up logging to the GUI
	g_guiLog = new GuiLogSink(console_verbosity);
	g_log_sinks.push_back(unique_ptr<GuiLogSink>(g_guiLog));

	//If on Windows, and not run from a console
	//remove the stdout log sink that would otherwise spawn a console on startup
	#ifdef _WIN32
		if(GetConsoleWindow()) == NULL)
			LogNotice("Startup: skipping stdout log sink since not run from a console (no console window)\n");
		else if(getenv("PROMPT") != nullptr)
			LogNotice("Startup: skipping stdout log sink since not run from a console (no prompt)\n");
		else
		{
	#endif
			g_log_sinks.push_back(make_unique<ColoredSTDLogSink>(console_verbosity));
	#ifdef _WIN32
			LogNotice("Startup: run from a console, keeping stdout log sink attached\n");
		}
	#endif

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
		g_mainWindow = make_unique<MainWindow>(queue);

		//Main event loop
		auto& session = g_mainWindow->GetSession();
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
