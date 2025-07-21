/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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
	\addtogroup dialogs

	@file
	@author Andrew D. Zonenberg
	@brief Implementation of AboutDialog
 */

#include "ngscopeclient.h"
#include "AboutDialog.h"
#include "MainWindow.h"
#include <ngscopeclient-version.h>
#include <imgui_markdown.h>
#include <vkFFT.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

AboutDialog::AboutDialog(MainWindow* parent)
	: Dialog("About ngscopeclient", to_string_hex(reinterpret_cast<uintptr_t>(this)), ImVec2(600, 400))
	, m_parent(parent)
{
	//this file is currently maintained by hand and updated for each release
	//TODO: make a script for this using https://api.github.com/repos/ngscopeclient/scopehal-apps/contributors
	m_authorsMarkdown = ReadDataFile("md/authors.md");

	m_licenseMarkdown = ReadDataFile("md/licenses.md");
}

AboutDialog::~AboutDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool AboutDialog::DoRender()
{
	pair<ImFont*, float> headings[] =
	{
		m_parent->GetFontPref("Appearance.Markdown.heading_1_font"),
		m_parent->GetFontPref("Appearance.Markdown.heading_2_font"),
		m_parent->GetFontPref("Appearance.Markdown.heading_3_font")
	};

	ImGui::MarkdownConfig mdConfig
	{
		nullptr,	//linkCallback
		nullptr,	//tooltipCallback
		nullptr,	//imageCallback
		"",			//linkIcon (not used)
		{
			{ headings[0].first, headings[0].second, true },
			{ headings[1].first, headings[1].second, true },
			{ headings[2].first, headings[2].second, false }
		},
		nullptr		//userData
	};

	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
	if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
	{
		if(ImGui::BeginTabItem("Versions"))
		{
			int nver = VkFFTGetVersion();
			string vkfftVersion =
				to_string(nver / 10000) + "." +
				to_string( (nver / 100) % 100) +  "." +
				to_string( nver % 100);

			string str =
				string("  * ngscopeclient ") + NGSCOPECLIENT_VERSION + "\n" +
				"  * libscopehal " + ScopehalGetVersion() + "\n" +
				"  * Dear ImGui " + IMGUI_VERSION + "\n"
				"  * VkFFT " + vkfftVersion + "\n"
				"  * Vulkan SDK " + to_string(VK_HEADER_VERSION) + "\n"
				;
			ImGui::Markdown( str.c_str(), str.length(), mdConfig );

			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("Licenses"))
		{
			ImGui::Markdown(m_licenseMarkdown.c_str(), m_licenseMarkdown.length(), mdConfig );
			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("Authors"))
		{
			ImGui::Markdown(m_authorsMarkdown.c_str(), m_authorsMarkdown.length(), mdConfig );
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
