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
	@brief Program entry point
 */
#include "ngscopeclient.h"
#include "MainWindow.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "../scopeexports/scopeexports.h"

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

	//Set up logging
	g_guiLog = new GuiLogSink(console_verbosity);
	g_log_sinks.push_back(make_unique<ColoredSTDLogSink>(console_verbosity));
	g_log_sinks.push_back(unique_ptr<GuiLogSink>(g_guiLog));

	//Complain if the OpenMP wait policy isn't set right
	const char* policy = getenv("OMP_WAIT_POLICY");
	#ifndef _WIN32
		bool need_relaunch = false;
	#endif
	if((policy == NULL) || (strcmp(policy, "PASSIVE") != 0) )
	{
		#ifdef _WIN32
			LogWarning("glscopeclient works best with the OMP_WAIT_POLICY environment variable set to PASSIVE\n");
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

	//Initialize object creation tables for predefined libraries
	if(!VulkanInit())
		return 1;
	TransportStaticInit();
	DriverStaticInit();
	ScopeProtocolStaticInit();
	ScopeExportStaticInit();
	InitializePlugins();

	//Initialize ImGui
	IMGUI_CHECKVERSION();
	LogDebug("Using ImGui version %s\n", IMGUI_VERSION);
	auto ctx = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	//Don't serialize UI config for now
	//TODO: serialize to scopesession or something? https://github.com/ocornut/imgui/issues/4294
	io.IniFilename = nullptr;

	//Set up appearance settings
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.Colors[ImGuiCol_WindowBg].w = 1.0f;

	{
		//Make the top level window
		vk::raii::Queue queue(*g_vkComputeDevice, g_renderQueueType, AllocateVulkanRenderQueue());
		g_mainWindow = make_unique<MainWindow>(queue);

		//Main event loop
		while(!glfwWindowShouldClose(g_mainWindow->GetWindow()))
		{
			//poll and return immediately
			glfwPollEvents();

			//block until we have something to do, then process events
			//glfwWaitEvents();

			//Draw the main window
			g_mainWindow->Render();
		}

		g_vkComputeDevice->waitIdle();
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext(ctx);
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
