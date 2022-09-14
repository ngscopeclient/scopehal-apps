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
	if(!ImGui::Begin(m_title.c_str(), &m_open, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return false;
	}

	if(!DoRender())
	{
		ImGui::End();
		return false;
	}

	RenderErrorPopup();

	ImGui::End();
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Error messages

/**
	@brief Opens the error popup
 */
void Dialog::ShowErrorPopup(const string& title, const string& msg)
{
	ImGui::OpenPopup(title.c_str());
	m_errorPopupTitle = title;
	m_errorPopupMessage = msg;
}

/**
	@brief Popup message when we fail to connect
 */
void Dialog::RenderErrorPopup()
{
	if(ImGui::BeginPopupModal(m_errorPopupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(m_errorPopupMessage.c_str());
		ImGui::Separator();
		if(ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget helpers for common imgui stuff

/**
	@brief Displays a combo box from a vector<string>
 */
bool Dialog::Combo(const string& label, const vector<string>& items, int& selection)
{
	string preview;
	ImGuiComboFlags flags = 0;

	//Hide arrow button if no items
	if(items.empty())
		flags = ImGuiComboFlags_NoArrowButton;

	//Set preview to currently selected item
	else if( (selection >= 0) && (selection < (int)items.size() ) )
		preview = items[selection];

	bool changed = false;

	//Render the box
	if(ImGui::BeginCombo(label.c_str(), preview.c_str(), flags))
	{
		for(int i=0; i<(int)items.size(); i++)
		{
			bool selected = (i == selection);
			if(ImGui::Selectable(items[i].c_str(), selected))
			{
				changed = true;
				selection = i;
			}
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
}

/**
	@brief Helper based on imgui demo for displaying a help icon and tooltip text
 */
void Dialog::HelpMarker(const string& str)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	Tooltip(str);
}

/**
	@brief Helper based on imgui demo for displaying tooltip text over the previously rendered widget
 */
void Dialog::Tooltip(const string& str)
{
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

/**
	@brief Helper for displaying a floating-point input box with an "apply" button
 */
bool Dialog::FloatInputWithApplyButton(const string& label, float& currentValue, float& committedValue)
{
	ImGui::BeginGroup();

	bool dirty = currentValue != committedValue;
	ImGui::InputFloat(label.c_str(), &currentValue);
	ImGui::SameLine();
	if(!dirty)
		ImGui::BeginDisabled();
	auto applyLabel = string("Apply###Apply") + label;
	bool changed = false;
	if(ImGui::Button(applyLabel.c_str()))
	{
		changed = true;
		committedValue = currentValue;
	}
	if(!dirty)
		ImGui::EndDisabled();

	ImGui::EndGroup();
	return changed;
}

bool Dialog::TextInputWithApplyButton(const string& label, string& currentValue, string& committedValue)
{
	ImGui::BeginGroup();

	bool dirty = currentValue != committedValue;
	ImGui::InputText(label.c_str(), &currentValue);
	ImGui::SameLine();
	if(!dirty)
		ImGui::BeginDisabled();
	auto applyLabel = string("Apply###Apply") + label;
	bool changed = false;
	if(ImGui::Button(applyLabel.c_str()))
	{
		changed = true;
		committedValue = currentValue;
	}
	if(!dirty)
		ImGui::EndDisabled();

	ImGui::EndGroup();
	return changed;
}

bool Dialog::TextInputWithImplicitApply(const string& label, string& currentValue, string& committedValue)
{
	bool dirty = currentValue != committedValue;
	ImGui::InputText(label.c_str(), &currentValue);

	if(!ImGui::IsItemActive() && dirty )
	{
		committedValue = currentValue;
		return true;
	}

	return false;
}

bool Dialog::IntInputWithImplicitApply(const string& label, int& currentValue, int& committedValue)
{
	bool dirty = currentValue != committedValue;
	ImGui::InputInt(label.c_str(), &currentValue);

	if(!ImGui::IsItemActive() && dirty )
	{
		committedValue = currentValue;
		return true;
	}

	return false;
}

/**
	@brief Input box for a floating point value with an associated unit

	@param label			Text label
	@param currentValue		Current text box content
	@param committedValue	Most recently applied value
	@param unit				The unit for the input

	@return	True when focus is lost or user presses enter
 */
bool Dialog::UnitInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		float& committedValue,
		Unit unit)
{
	bool dirty = unit.PrettyPrint(committedValue) != currentValue;

	ImGui::InputText(label.c_str(), &currentValue);

	if(!ImGui::IsItemActive() && dirty )
	{
		committedValue = unit.ParseString(currentValue);
		currentValue = unit.PrettyPrint(committedValue);
		return true;
	}

	return false;
}

/**
	@brief Input box for a floating point value with an associated unit and an "apply" button

	@param label			Text label
	@param currentValue		Current text box content
	@param committedValue	Most recently applied value
	@param unit				The unit for the input

	@return	True the "apply" button is pressed
 */
bool Dialog::UnitInputWithExplicitApply(
		const std::string& label,
		std::string& currentValue,
		float& committedValue,
		Unit unit)
{
	bool dirty = unit.PrettyPrint(committedValue) != currentValue;

	ImGui::BeginGroup();

	ImGui::InputText(label.c_str(), &currentValue);
	ImGui::SameLine();
	if(!dirty)
		ImGui::BeginDisabled();
	auto applyLabel = string("Apply###Apply") + label;
	bool changed = false;
	if(ImGui::Button(applyLabel.c_str()))
	{
		changed = true;
		committedValue = unit.ParseString(currentValue);
		currentValue = unit.PrettyPrint(committedValue);
	}
	if(!dirty)
		ImGui::EndDisabled();

	ImGui::EndGroup();
	return changed;
}
