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
	@brief Implementation of Dialog
 */
#include "ngscopeclient.h"
#include "Dialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Dialog::Dialog(const string& title, ImVec2 defaultSize)
	: m_open(true)
	, m_title(title)
	, m_defaultSize(defaultSize)
{
}

Dialog::~Dialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool Dialog::Render()
{
	if(!m_open)
		return false;

	ImGui::SetNextWindowSize(m_defaultSize, ImGuiCond_Appearing);
	if(!ImGui::Begin(m_title.c_str(), &m_open))
	{
		ImGui::End();
		return false;
	}

	if(!DoRender())
	{
		ImGui::End();
		return false;
	}

	ImGui::End();
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget helpers for STL-ifying imgui objects

/**
	@brief Displays a combo box from a vector<string>
 */
void Dialog::Combo(const string& label, const vector<string>& items, int& selection)
{
	string preview;
	ImGuiComboFlags flags = 0;

	//Hide arrow button if no items
	if(items.empty())
		flags = ImGuiComboFlags_NoArrowButton;

	//Set preview to currently selected item
	else
		preview = items[selection];

	//Render the box
	if(ImGui::BeginCombo(label.c_str(), preview.c_str(), flags))
	{
		for(int i=0; i<(int)items.size(); i++)
		{
			bool selected = (i == selection);
			if(ImGui::Selectable(items[i].c_str(), selected))
				selection = i;
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

/**
	@brief Helper based on imgui demo for displaying a help icon and tooltip text
 */
void Dialog::HelpMarker(const string& str)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(str.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

/**
	@brief Helper based on imgui demo for displaying a help icon and tooltip text consisting of a header and bulleted text
 */
void Dialog::HelpMarker(const string& header, const vector<string>& bullets)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(header.c_str());
		for(auto s : bullets)
			ImGui::BulletText(s.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}
