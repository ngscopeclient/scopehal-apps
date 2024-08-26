/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
{

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

	//TODO: filtering by type
	//TODO: string filtering

	ImVec2 iconmargin(ImGui::GetFontSize(), ImGui::GetFontSize());

	auto size = ImGui::GetFontSize() * 6;
	ImVec2 buttonsize(size*2, size);
	auto& style = ImGui::GetStyle();

	float window_visible_x2 = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	for(auto it : refs)
	{
		//Hackiness based on manual-wrapping example from the demo

		//Placeholder for the button
		auto pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(it.first.c_str(), buttonsize);

		//Decide whether to wrap the button
		float last_button_x2 = ImGui::GetItemRectMax().x;
		float next_button_x2 = last_button_x2 + style.ItemSpacing.x + buttonsize.x;
		if(next_button_x2 < window_visible_x2)
			ImGui::SameLine();

		//Figure out the icon to draw
		auto icon = m_parent->GetIconForFilter(it.second);

		//Draw the button
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		if(ImGui::IsItemHovered())
		{
			draw_list->AddRectFilled(
				pos,
				pos + buttonsize,
				ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_ButtonHovered]));
		}
		else
		{
			draw_list->AddRectFilled(
				pos,
				pos + buttonsize,
				ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Button]));
		}
		draw_list->AddRect(
			pos,
			pos + buttonsize,
			ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]));

		//Draw the icon
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

			draw_list->AddImage(m_parent->GetTexture(icon), tl, br);
		}

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

		//Draw the text
		draw_list->AddText(
			ImVec2(pos.x + textmargin, pos.y + size - (1.25 * ImGui::GetFontSize()) ),
			ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]),
			caption.c_str());
	}

	return true;
}
