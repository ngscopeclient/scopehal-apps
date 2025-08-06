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
	, m_displayedSeverity(5)
	, m_severityFilter(Severity::DEBUG)
	, m_lastLine(0)
{
	m_severities.push_back("Fatal");
	m_severities.push_back("Error");
	m_severities.push_back("Warning");
	m_severities.push_back("Notice");
	m_severities.push_back("Verbose");
	m_severities.push_back("Debug");
	m_severities.push_back("Trace");
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

	if(ImGui::CollapsingHeader("Settings"))
	{
		if(Combo("###Severity", m_severities, m_displayedSeverity))
			m_severityFilter = static_cast<Severity>(m_displayedSeverity + 1);

		float width = ImGui::GetFontSize();
		static ImGuiTableFlags flags =
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingFixedFit;
		if(ImGui::BeginTable("filters", 2, flags, ImVec2(0, 7*ImGui::GetFontSize())))
		{
			ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
			ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 10*width);
			ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch, 0.0f);
			ImGui::TableHeadersRow();

			for(auto filter : g_trace_filters)
			{
				ImGui::TableNextRow(ImGuiTableRowFlags_None);
				ImGui::TableSetColumnIndex(0);

				//Split the filter on the :: if any
				if(filter.find("::") != string::npos)
				{
					size_t icolon = filter.find(':');
					string cname = filter.substr(0, icolon);
					if(cname == "")
						cname = "[global]";
					string label = cname + "###" + filter;

					//Class name
					if(ImGui::Selectable(label.c_str(),
						(filter == m_selectedFilter), ImGuiSelectableFlags_SpanAllColumns))
					{
						m_selectedFilter = filter;
					}

					//Function name
					ImGui::TableSetColumnIndex(1);
					string fname = filter.substr(icolon + 2);
					ImGui::Text(fname.c_str());
				}

				//no it's just a class name
				else
				{
					if(ImGui::Selectable(filter.c_str(),
						(filter == m_selectedFilter), ImGuiSelectableFlags_SpanAllColumns))
					{
						m_selectedFilter = filter;
					}
				}
			}

			ImGui::EndTable();
		}

		ImGui::InputText("Filter", &m_traceFilter);
		ImGui::SameLine();
		if(ImGui::Button("+"))
		{
			g_trace_filters.emplace(m_traceFilter);
			m_traceFilter = "";
		}
		ImGui::SameLine();
		if(ImGui::Button("-"))
			g_trace_filters.erase(m_selectedFilter);
	}

	auto font = m_parent->GetFontPref("Appearance.General.console_font");
	ImGui::PushFont(font.first, font.second);
	auto& lines = g_guiLog->GetLines();

	float width = ImGui::GetFontSize();
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollX |
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

		for(size_t i=0; i<lines.size(); i++)
		{
			auto& line = lines[i];

			//Hide anything that doesn't pass our filter
			if(line.m_sev > m_severityFilter)
				continue;

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
					ImGui::TextUnformatted("Debug");
					break;

				case Severity::TRACE:
					ImGui::TextUnformatted("Trace");
					break;
				default:
					break;
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(line.m_msg.c_str());

			//Autoscroll when new messages arrive
			if(m_lastLine < i)
			{
				m_lastLine = i;
				ImGui::SetScrollHereY(1.0f);
			}
		}

		ImGui::EndTable();
	}

	ImGui::PopFont();

	return true;
}
