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
	@brief Implementation of LogViewerDialog
 */

#include "ngscopeclient.h"
#include "MainWindow.h"
#include "LogViewerDialog.h"

using namespace std;

extern GuiLogSink* g_guiLog;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

LogViewerDialog::LogViewerDialog(MainWindow* parent)
	: Dialog("Log Viewer", "Log Viewer", ImVec2(500, 300))
	, m_parent(parent)
{
}

LogViewerDialog::~LogViewerDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool LogViewerDialog::DoRender()
{
	auto errColor = m_parent->GetColorPref("Appearance.Log Viewer.error_color");
	auto warningColor = m_parent->GetColorPref("Appearance.Log Viewer.warning_color");
	auto baseColor = m_parent->GetColorPref("Appearance.Graphs.bottom_color");

	ImGui::BeginChild("scrollview", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	auto font = m_parent->GetFontPref("Appearance.General.console_font");
	ImGui::PushFont(font.first, font.second);
	auto& lines = g_guiLog->GetLines();

	float width = ImGui::GetFontSize();
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit;
	if(ImGui::BeginTable("table", 3, flags))
	{
		//TODO: use ImGuiListClipper

		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 10*width);
		ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 0.0f);
		ImGui::TableHeadersRow();

		for(auto& line : lines)
		{
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			switch(line.m_sev)
			{
				case Severity::ERROR:
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, errColor);
					break;

				case Severity::WARNING:
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, warningColor);
					break;

				default:
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, baseColor);
					break;
			}

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(line.m_timestamp.PrettyPrint().c_str());

			ImGui::TableSetColumnIndex(1);
			switch(line.m_sev)
			{
				//no need for fatal, we abort before we can see it

				case Severity::ERROR:
					ImGui::TextUnformatted("Error");
					break;

				case Severity::WARNING:
					ImGui::TextUnformatted("Warning");
					break;

				case Severity::NOTICE:
					ImGui::TextUnformatted("Notice");
					break;

				case Severity::VERBOSE:
					ImGui::TextUnformatted("Verbose");
					break;

				case Severity::DEBUG:
				default:
					ImGui::TextUnformatted("Debug");
					break;
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(line.m_msg.c_str());
		}

		ImGui::EndTable();
	}

	ImGui::PopFont();

	if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();

	return true;
}
