/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of CreateFilterBrowser
 */

#include "ngscopeclient.h"
#include "CreateFilterBrowser.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

CreateFilterBrowser::CreateFilterBrowser(Session& session, MainWindow* parent)
	: Dialog("Filter Palette", "Filter Palette", ImVec2(550, 400))
	, m_session(session)
	, m_parent(parent)
	, m_selectedCategoryIndex(0)
{
	m_categoryNames.push_back("All");
	m_categoryValues.push_back(Filter::CAT_COUNT);

	m_categoryNames.push_back("Bus");
	m_categoryValues.push_back(Filter::CAT_BUS);

	m_categoryNames.push_back("Bus");
	m_categoryValues.push_back(Filter::CAT_BUS);

	m_categoryNames.push_back("Clocking");
	m_categoryValues.push_back(Filter::CAT_CLOCK);

	m_categoryNames.push_back("Export");
	m_categoryValues.push_back(Filter::CAT_EXPORT);

	m_categoryNames.push_back("Generation");
	m_categoryValues.push_back(Filter::CAT_GENERATION);

	m_categoryNames.push_back("Math");
	m_categoryValues.push_back(Filter::CAT_MATH);

	m_categoryNames.push_back("Measurement");
	m_categoryValues.push_back(Filter::CAT_MEASUREMENT);

	m_categoryNames.push_back("Memory");
	m_categoryValues.push_back(Filter::CAT_MEMORY);

	m_categoryNames.push_back("Miscellaneous");
	m_categoryValues.push_back(Filter::CAT_MISC);

	m_categoryNames.push_back("Optics");
	m_categoryValues.push_back(Filter::CAT_OPTICAL);

	m_categoryNames.push_back("Power");
	m_categoryValues.push_back(Filter::CAT_POWER);

	m_categoryNames.push_back("RF");
	m_categoryValues.push_back(Filter::CAT_RF);

	m_categoryNames.push_back("Serial");
	m_categoryValues.push_back(Filter::CAT_SERIAL);

	m_categoryNames.push_back("Signal Integrity");
	m_categoryValues.push_back(Filter::CAT_ANALYSIS);
}

CreateFilterBrowser::~CreateFilterBrowser()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool CreateFilterBrowser::DoRender()
{
	auto& refs = m_session.GetReferenceFilters();

	//Filter bars
	ImGui::SetNextItemWidth(8 * ImGui::GetFontSize());
	Combo("Category", m_categoryNames, m_selectedCategoryIndex);
	auto cat = m_categoryValues[m_selectedCategoryIndex];

	ImGui::SetNextItemWidth(8 * ImGui::GetFontSize());
	ImGui::InputText("Search", &m_searchString);

	//Need to check if the mouse is down HERE because we get incorrect values later on in the function!
	//Not yet sure why, but this is at least a usable workaround.
	bool mouseIsDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool isDragging = (ImGui::GetDragDropPayload() != nullptr);

	//Scroll area
	if(ImGui::BeginChild("Scroller", ImVec2(0, 0)))
	{
		auto size = ImGui::GetFontSize() * 5;
		ImVec2 buttonsize(size*2, size);
		auto& style = ImGui::GetStyle();

		//Hackiness based on manual-wrapping example from the demo
		float window_visible_x2 = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		for(auto it : refs)
		{
			//Filter by category
			if( (cat != Filter::CAT_COUNT) && (cat != it.second->GetCategory()) )
				continue;

			//String filtering
			if(m_searchString != "")
			{
				//Case insensitive comparison
				string lowerSearch;
				for(auto c : m_searchString)
					lowerSearch += tolower(c);

				string lowerName;
				for(auto c : it.first)
					lowerName += tolower(c);

				if(lowerName.find(lowerSearch) == string::npos)
					continue;
			}

			//Placeholder for the button
			auto pos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton(it.first.c_str(), buttonsize);

			//Help text
			if(ImGui::IsItemHovered())
				m_parent->AddStatusHelp("mouse_lmb_drag", "Add to filter graph");

			//Tooltip with expanded name in case the full name was trimmed to fit on the button
			//Hide tooltip if we're dragging, to avoid messing with drag-and-drop
			//(see https://github.com/ocornut/imgui/issues/7922)
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) &&
				!mouseIsDown && !isDragging)
			{
				if(ImGui::BeginTooltip())
				{
					ImGui::TextUnformatted(it.first.c_str());
					ImGui::EndTooltip();
				}
			}

			//Figure out the icon to draw
			auto icon = m_parent->GetIconForFilter(it.second);

			//Truncate text to fit in the available space
			string caption = it.first;
			float textmargin = ImGui::GetFontSize();
			float textSpace = buttonsize.x - textmargin*2;
			while(true)
			{
				auto tsize = ImGui::CalcTextSize(caption.c_str());
				if(tsize.x <= textSpace)
					break;

				caption.resize(caption.length() - 1);
			}

			//Make it draggable
			//Do NOT use the autogenerated preview tooltip as this breaks thanks to
			//https://github.com/ocornut/imgui/issues/7922
			//Instead, draw the icon ourselves
			if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip))
			{
				ImGui::SetDragDropPayload("FilterType", it.first.c_str(), it.first.length());
				ImGui::EndDragDropSource();
				//ImGui::TextUnformatted(it.first.c_str());

				DrawIconButton(
					ImGui::GetForegroundDrawList(),
					ImGui::GetMousePos(),
					ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Button]),
					icon,
					caption);
			}

			//Decide whether to wrap after this button
			float last_button_x2 = ImGui::GetItemRectMax().x;
			float next_button_x2 = last_button_x2 + style.ItemSpacing.x + buttonsize.x;
			if(next_button_x2 < window_visible_x2)
				ImGui::SameLine();

			//Draw the button
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImU32 color = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Button]);
			if(ImGui::IsItemHovered())
				color = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_ButtonHovered]);
			DrawIconButton(draw_list, pos, color, icon, caption);
		}

		ImGui::EndChild();
	}

	return true;
}

void CreateFilterBrowser::DrawIconButton(
	ImDrawList* list,
	ImVec2 pos,
	ImU32 color,
	const string& icon,
	const string& caption)
{
	float textmargin = ImGui::GetFontSize();
	auto& style = ImGui::GetStyle();
	auto size = ImGui::GetFontSize() * 5;
	ImVec2 buttonsize(size*2, size);
	ImVec2 iconmargin(ImGui::GetFontSize(), ImGui::GetFontSize());

	//Filling
	list->AddRectFilled(
		pos,
		pos + buttonsize,
		color);

	//Outline
	list->AddRect(
		pos,
		pos + buttonsize,
		ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]));

	//The icon
	if(icon != "")
	{
		//Tweak space so we maintain 2:1 aspect ratio
		auto tl = pos + iconmargin;
		auto br = pos + buttonsize - iconmargin;

		float dx = br.x - tl.x;
		float dy = br.y - tl.y;

		float actualWidth = 2*dy;
		float extraSpace = dx - actualWidth;
		tl.x += extraSpace / 2;
		br.x -= extraSpace / 2;

		list->AddImage(m_parent->GetTexture(icon), tl, br);
	}

	//Draw the text
	list->AddText(
		ImVec2(pos.x + textmargin, pos.y + size - (1.25 * ImGui::GetFontSize()) ),
		ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
		caption.c_str());
}
