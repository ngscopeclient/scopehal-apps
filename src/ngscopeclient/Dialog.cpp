/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
#include "MainWindow.h"
#include "PreferenceTypes.h"

using namespace std;

#define CARRIAGE_RETURN_CHAR "⏎"
#define DEFAULT_APPLY_BUTTON_COLOR "#4CCC4C"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Dialog::Dialog(const string& title, const string& id, ImVec2 defaultSize, Session* session, MainWindow* parent)
	: m_open(true)
	, m_id(id)
	, m_title(title)
	, m_defaultSize(defaultSize)
	, m_session(session)
	, m_parent(parent)
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

	string name = m_title + "###" + m_id;
	ImGui::SetNextWindowSize(m_defaultSize, ImGuiCond_Appearing);
	if(!ImGui::Begin(name.c_str(), &m_open, ImGuiWindowFlags_NoCollapse))
	{
		//If we get here, the window is tabbed out or the content area is otherwise not visible.
		//Save time by not drawing anything, but don't close the window!
		ImGui::End();
		return true;
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

/**
	@brief Runs the dialog's contents directly into a parent window
 */
void Dialog::RenderAsChild()
{
	DoRender();
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
		ImGui::TextUnformatted(m_errorPopupMessage.c_str());
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
void Dialog::Tooltip(const string& str, bool allowDisabled)
{
	int extraFlags = 0;
	if(allowDisabled)
		extraFlags |= ImGuiHoveredFlags_AllowWhenDisabled;

	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | extraFlags))
	{
		MainWindow::SetTooltipPosition();

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
		MainWindow::SetTooltipPosition();

		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(header.c_str());
		for(auto s : bullets)
			ImGui::BulletText("%s", s.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
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
	@brief Input box for a double precision floating point value with an associated unit

	@param label			Text label
	@param currentValue		Current text box content
	@param committedValue	Most recently applied value
	@param unit				The unit for the input

	@return	True when focus is lost or user presses enter
 */
bool Dialog::UnitInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		double& committedValue,
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
	@brief Input box for an integer value with an associated unit

	@param label			Text label
	@param currentValue		Current text box content
	@param committedValue	Most recently applied value
	@param unit				The unit for the input

	@return	True when focus is lost or user presses enter
 */
bool Dialog::UnitInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		int64_t& committedValue,
		Unit unit)
{
	//	return renderEditableProperty(-1,label,currentValue,committedValue,unit,);
	bool dirty = unit.PrettyPrintInt64(committedValue) != currentValue;

	ImGui::InputText(label.c_str(), &currentValue);

	if(!ImGui::IsItemActive() && dirty )
	{
		//Float path if the user input a decimal value like "3.5G"
		if(currentValue.find(".") != string::npos)
			committedValue = unit.ParseString(currentValue);

		//Integer path otherwise for full precision
		else
			committedValue = unit.ParseStringInt64(currentValue);

		currentValue = unit.PrettyPrintInt64(committedValue);
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
	return renderEditablePropertyWithExplicitApply(-1,label,currentValue,committedValue,unit);
}

/**
   @brief Render a numeric value
   @param value the string representation of the value to display (may include the unit)
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param optcolor the optional color to use (defaults to text color)
   @param allow7SegmentDisplay (defaults to false) true if the value can be displayed in 7 segment format
   @param digitHeight the height of a digit (if 0 (defualt), will use ImGui::GetFontSize())
   @param clickable true (default) if the displayed value should be clickable
 */
void Dialog::renderNumericValue(const std::string& value, bool &clicked, bool &hovered, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay, float digitHeight, bool clickable)
{
	bool use7Segment = false;
	bool changeFont = false;
	int64_t displayType = NumericValueDisplay::NUMERIC_DISPLAY_DEFAULT_FONT;
	ImVec4 color = optcolor ? optcolor.value() : ImGui::GetStyleColorVec4(ImGuiCol_Text);
	if(m_session)
	{
		auto& prefs = m_session->GetPreferences();
		displayType = prefs.GetEnumRaw("Appearance.Stream Browser.numeric_value_display");
	}
	FontWithSize font;
	if(allow7SegmentDisplay)
	{
		use7Segment = (displayType == NumericValueDisplay::NUMERIC_DISPLAY_7SEGMENT);
		if(!use7Segment && m_parent)
		{
			font = m_parent->GetFontPref(displayType == NumericValueDisplay::NUMERIC_DISPLAY_DEFAULT_FONT ? "Appearance.General.default_font" : "Appearance.General.console_font");
			changeFont = true;
		}
	}
	if(use7Segment)
	{
		if(digitHeight <= 0) digitHeight = ImGui::GetFontSize();
		
	    Render7SegmentValue(value,color,digitHeight,clicked,hovered,clickable);
	}
	else
	{
		if(clickable)
		{
			ImVec2 pos = ImGui::GetCursorPos();
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			if(changeFont) ImGui::PushFont(font.first, font.second);
			ImGui::TextUnformatted(value.c_str());
			if(changeFont) ImGui::PopFont();
			ImGui::PopStyleColor();

			clicked |= ImGui::IsItemClicked();
			if(ImGui::IsItemHovered())
			{	// Hand cursor
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);			
				// Lighter if hovered
				color.x = color.x * 1.2f;
				color.y = color.y * 1.2f;
				color.z = color.z * 1.2f;
				ImGui::SetCursorPos(pos);
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(value.c_str());
				ImGui::PopStyleColor();
				hovered = true;
			}
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			if(changeFont) ImGui::PushFont(font.first, font.second);
			ImGui::TextUnformatted(value.c_str());
			if(changeFont) ImGui::PopFont();
			ImGui::PopStyleColor();
		}
	}
}

/**
   @brief Render a read-only instrument property value
   @param label the value label (used as a label for the property)
   @param currentValue the string representation of the current value
   @param tooltip if not null, will add the provided text as an help marker (defaults to nullptr)
*/
void Dialog::renderReadOnlyProperty(float width, const string& label, const string& value, const char* tooltip)
{
	ImGui::PushID(label.c_str());	// Prevent collision if several sibling links have the same linktext
	float fontSize = ImGui::GetFontSize();
	if(width <= 0) width = 6*fontSize;
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4 bg = style.Colors[ImGuiCol_FrameBg];
	ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
	ImGui::BeginChild("##readOnlyValue", ImVec2(width, ImGui::GetFontSize()),false,ImGuiWindowFlags_None);
	ImGui::TextUnformatted(value.c_str());
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextUnformatted(label.c_str());
	ImGui::PopID();
	if(tooltip)
	{
		HelpMarker(tooltip);
	}
}


template<typename T>
/**
   @brief Render an editable numeric value
   @param width the width of the input value (if <0 will be ignored, if =0 will default to 6*ImGui::GetStyle())
   @param label the value label (used as a label for the TextInput)
   @param currentValue the string representation of the current value
   @param comittedValue the last comitted typed (float, double or int64_t) value
   @param unit the Unit of the value
   @param tooltip if not null, will add the provided text as an help marker (defaults to nullptr)
   @param optcolor the optional color to use (defaults to text color)
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param allow7SegmentDisplay (defaults to false) true if the value can be displayed in 7 segment format
   @param explicitApply (defaults to false) true if the input value needs to explicitly be applied (by clicking the apply button)
   @return true if the value has changed
 */
bool Dialog::renderEditableProperty(float width, const std::string& label, std::string& currentValue, T& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay, bool explicitApply)
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, int64_t>,"renderEditableProperty only supports int64_t, float or double");
	bool use7Segment = false;
	bool changeFont = false;
	int64_t displayType = NumericValueDisplay::NUMERIC_DISPLAY_DEFAULT_FONT;
	ImVec4 buttonColor;
	ImVec4 color = optcolor ? optcolor.value() : ImGui::GetStyleColorVec4(ImGuiCol_Text);
	if(m_session)
	{
		auto& prefs = m_session->GetPreferences();
		displayType = prefs.GetEnumRaw("Appearance.Stream Browser.numeric_value_display");
		buttonColor = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.General.apply_button_color"));
	}
	else
	{
		buttonColor = ImGui::ColorConvertU32ToFloat4(ColorFromString(DEFAULT_APPLY_BUTTON_COLOR));
	}
	FontWithSize font;
	if(allow7SegmentDisplay)
	{
		use7Segment = (displayType == NumericValueDisplay::NUMERIC_DISPLAY_7SEGMENT);
		if(!use7Segment && m_parent)
		{
			font = m_parent->GetFontPref(displayType == NumericValueDisplay::NUMERIC_DISPLAY_DEFAULT_FONT ? "Appearance.General.default_font" : "Appearance.General.console_font");
			changeFont = true;
		}
	}

	bool changed = false;
	bool validateChange = false;
	bool cancelEdit = false;
	bool keepEditing = false;
	bool dirty;
	float fontSize = ImGui::GetFontSize();
	if(width >= 0)
	{
		if(width == 0) width = 6*fontSize;
		ImGui::SetNextItemWidth(width);
	}
	if constexpr (std::is_same_v<T, int64_t>)
		dirty = unit.PrettyPrintInt64(committedValue) != currentValue;
	else
		dirty = unit.PrettyPrint(committedValue) != currentValue;
	string editLabel = label+"##Edit";
	ImGuiID editId = ImGui::GetID(editLabel.c_str());
	ImGuiID labelId = ImGui::GetID(label.c_str());
	if(m_editedItemId == editId)
	{	// Item currently beeing edited
		ImGui::BeginGroup();
		float inputXPos = ImGui::GetCursorPosX();
	    ImGuiContext& g = *GImGui;
		float inputWidth = g.NextItemData.Width;
		// Allow overlap for apply button
		ImGui::PushItemFlag(ImGuiItemFlags_AllowOverlap, true);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		if(changeFont) ImGui::PushFont(font.first, font.second);
		if(ImGui::InputText(editLabel.c_str(), &currentValue, ImGuiInputTextFlags_EnterReturnsTrue))
		{	// Input validated (but no apply button)
			if(!explicitApply)
			{	// Implcit apply => validate change
				validateChange = true;
			}
			else
			{	// Explicit apply needed => keep editing
				keepEditing = true;
			}
		}
		if(changeFont) ImGui::PopFont();
		ImGui::PopStyleColor();
		ImGui::PopItemFlag();
		if(explicitApply)
		{	// Add Apply button
			float buttonWidth = ImGui::GetFontSize() * 2;
			// Position the button just before the right side of the text input
			ImGui::SameLine(inputXPos+inputWidth-ImGui::GetCursorPosX()-buttonWidth+2*ImGui::GetStyle().ItemInnerSpacing.x);
			ImVec4 buttonColorHovered = buttonColor;
			float bgmul = 0.8f;
			ImVec4 buttonColorDefault = ImVec4(buttonColor.x*bgmul, buttonColor.y*bgmul, buttonColor.z*bgmul, buttonColor.w);
			bgmul = 0.7f;
			ImVec4 buttonColorActive = ImVec4(buttonColor.x*bgmul, buttonColor.y*bgmul, buttonColor.z*bgmul, buttonColor.w);
			ImGui::PushStyleColor(ImGuiCol_Button, buttonColorDefault);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColorHovered);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColorActive);
			ImGui::BeginDisabled(!dirty);
			if(ImGui::Button(CARRIAGE_RETURN_CHAR)) // Carriage return symbol
			{	// Apply button click
				validateChange = true;
			}
			ImGui::EndDisabled();
			if(dirty && ImGui::IsItemHovered() && m_parent)
			{	// Help to explain apply button
				m_parent->AddStatusHelp("mouse_lmb", "Apply value changes and send them to the instrument");
			}
			ImGui::PopStyleColor(3);
		}
		if(!validateChange)
		{
			if(keepEditing)
			{	// Give back focus to test input
				ImGui::ActivateItemByID(editId);
			}
			else if(ImGui::IsKeyPressed(ImGuiKey_Escape))
			{	// Detect escape => stop editing
				cancelEdit = true;
				//Prevent focus from going to parent node
				ImGui::ActivateItemByID(0);
			}
			else if((ImGui::GetActiveID() != editId) && (!explicitApply || !ImGui::IsItemActive() /* This is here to prevent detecting focus lost when apply button is clicked */))  
			{	// Detect focus lost => stop editing too
				if(explicitApply)
				{	// Cancel on focus lost
					cancelEdit = true;
				}
				else
				{	// Validate on focus list
					validateChange = true;
				}
			}
		}
		ImGui::EndGroup();
	}
	else
	{
		if(m_lastEditedItemId == editId)
		{	// Focus lost
			if(explicitApply)
			{	// Cancel edit
				cancelEdit = true;
			}
			else
			{	// Validate change
				validateChange = true;
			}
			m_lastEditedItemId = 0;
		}
		bool clicked = false;
		bool hovered = false;
		if(use7Segment)
		{
			ImGui::PushID(labelId);
			Render7SegmentValue(currentValue,color,ImGui::GetFontSize(),clicked,hovered);
			ImGui::PopID();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			if(changeFont) ImGui::PushFont(font.first, font.second);
			ImGui::InputText(label.c_str(),&currentValue,ImGuiInputTextFlags_ReadOnly);
			if(changeFont) ImGui::PopFont();
			ImGui::PopStyleColor();
			clicked |= ImGui::IsItemClicked();
			if(ImGui::IsItemHovered())
			{	// Keep hand cursor while read-only
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				hovered = true;
			}
		}

		if (clicked)
		{
			m_lastEditedItemId = m_editedItemId;
			m_editedItemId = editId;
			ImGui::ActivateItemByID(editId);
		}
		if (hovered && m_parent)
			m_parent->AddStatusHelp("mouse_lmb", "Edit value");
	}
	if(validateChange)
	{
		if(m_editedItemId == editId)
		{
			m_lastEditedItemId = 0;
			m_editedItemId = 0;
		}
		if(dirty)
		{	// Content actually changed
			if constexpr (std::is_same_v<T, int64_t>)
			{
				//Float path if the user input a decimal value like "3.5G"
				if(currentValue.find(".") != string::npos)
					committedValue = unit.ParseString(currentValue);
				//Integer path otherwise for full precision
				else
					committedValue = unit.ParseStringInt64(currentValue);

				currentValue = unit.PrettyPrintInt64(committedValue);
			}
			else
			{
				committedValue = static_cast<T>(unit.ParseString(currentValue));
				if constexpr (std::is_same_v<T, int64_t>)
					currentValue = unit.PrettyPrintInt64(committedValue);
				else
					currentValue = unit.PrettyPrint(committedValue);
			}
			changed = true;
		}
	}
	else if(cancelEdit)
	{	// Restore value
		if constexpr (std::is_same_v<T, int64_t>)
			currentValue = unit.PrettyPrintInt64(committedValue);
		else
			currentValue = unit.PrettyPrint(committedValue);
		if(m_editedItemId == editId)
		{
			m_lastEditedItemId = 0;
			m_editedItemId = 0;
		}
	}
	if(tooltip)
	{
		HelpMarker(tooltip);
	}
	return changed;
}

template bool Dialog::renderEditableProperty<float>(float width, const std::string& label, std::string& currentValue, float& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay, bool explicitApply);
template bool Dialog::renderEditableProperty<double>(float width, const std::string& label, std::string& currentValue, double& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay, bool explicitApply);
template bool Dialog::renderEditableProperty<int64_t>(float width, const std::string& label, std::string& currentValue, int64_t& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay, bool explicitApply);

template<typename T>
/**
   @brief Render an editable numeric value with explicit apply (if the input value needs to explicitly be applied by clicking the apply button)
   @param width the width of the input value (if <0 will be ignored, if =0 will default to 6*ImGui::GetStyle())
   @param label the value label (used as a label for the TextInput)
   @param currentValue the string representation of the current value
   @param comittedValue the last comitted typed (float, double or int64_t) value
   @param unit the Unit of the value
   @param tooltip if not null, will add the provided text as an help marker (defaults to nullptr)
   @param optcolor the optional color to use (defaults to text color)
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param allow7SegmentDisplay (defaults to false) true if the value can be displayed in 7 segment format
   @return true if the value has changed
 */
bool Dialog::renderEditablePropertyWithExplicitApply(float width, const std::string& label, std::string& currentValue, T& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay)
{
	return renderEditableProperty(width,label,currentValue,committedValue,unit,tooltip,optcolor,allow7SegmentDisplay,true);
}

template bool Dialog::renderEditablePropertyWithExplicitApply<float>(float width, const std::string& label, std::string& currentValue, float& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay);
template bool Dialog::renderEditablePropertyWithExplicitApply<double>(float width, const std::string& label, std::string& currentValue, double& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay);
template bool Dialog::renderEditablePropertyWithExplicitApply<int64_t>(float width, const std::string& label, std::string& currentValue, int64_t& committedValue, Unit unit, const char* tooltip, std::optional<ImVec4> optcolor, bool allow7SegmentDisplay);


/**
  @brief Segment on/off state for each of the 10 digits + "L" (needed for OL / Overload)
	0b01000000 : Top h segment
	0b00100000 : Top right v seglent
	0b00010000 : Bottom right v segment
	0b00001000 : Bottom h segment
	0b00000100 : Bottom left v segment
	0b00000010 : Top left v segment
	0b00000001 : Center h segment
 */
static char SEGMENTS[] = 
{
	0x7E, // 0
	0x30, // 1
	0x6D, // 2
	0x79, // 3
	0x33, // 4
	0x5B, // 5
	0x5F, // 6
	0x70, // 7
	0x7F, // 8
	0x7B, // 9
	0x0E, // L
	0x01, // -
};

/**
   @brief Render a single digit in 7 segment display style
   @param drawList the drawList used for rendering
   @param digit the digit to render
   @param size the size of the digit
   @param position the position of the digit
   @param thickness the thickness of a segment
   @param colorOn the color for an "on" segment
   @param colorOff the color for an "off" segment
 */
void Dialog::Render7SegmentDigit(ImDrawList* drawList, uint8_t digit, ImVec2 size, ImVec2 position, float thickness, ImU32 colorOn, ImU32 colorOff)
{
	// Inspired by https://github.com/ocornut/imgui/issues/3606#issuecomment-736855952
	if(digit == '-')
		digit = 11;	// Minus sign
	else if(digit > 10)
		digit = 10; // 10 is for L of OL (Overload)
	size.y += thickness;
	ImVec2 halfSize(size.x/2,size.y/2);
	ImVec2 centerPosition(position.x+halfSize.x,position.y+halfSize.y);
	float w = thickness;
	float h = thickness/2;
	float segmentSpec[7][4] = 
	{
		{-1, -1,  h,  h},		// Top h segment
		{ 1, -1, -h,  h},		// Top right v seglent
		{ 1,  0, -h, -h},		// Bottom right v segment
		{-1,  1,  h, -w * 1.5f},// Bottom h segment
		{-1,  0,  h, -h}, 		// Bottom left v segment
		{-1, -1,  h,  h},		// Top left v segment
		{-1,  0,  h, -h}, 		// Center h segment
	};
	for(int i = 0; i < 7; i++)
	{
		ImVec2 topLeft, bottomRight;
		if(i % 3 == 0)
		{	
			// Horizontal segment
			topLeft = ImVec2(centerPosition.x + segmentSpec[i][0] * halfSize.x + segmentSpec[i][2], centerPosition.y + segmentSpec[i][1] * halfSize.y + segmentSpec[i][3] - h);
			bottomRight = ImVec2(topLeft.x + size.x - w, topLeft.y + w);
		}
		else
		{
			// Vertical segment
			topLeft = ImVec2(centerPosition.x + segmentSpec[i][0] * halfSize.x + segmentSpec[i][2] - h, centerPosition.y + segmentSpec[i][1] * halfSize.y + segmentSpec[i][3]);
			bottomRight = ImVec2(topLeft.x + w, topLeft.y + halfSize.y - w);
		}
		ImVec2 segmentSize = bottomRight - topLeft;
		float space = w * 0.6;
		float u = space - h;
		if(segmentSize.x > segmentSize.y)
		{
			// Horizontal segment
			ImVec2 points[] = 
			{
				{topLeft.x + u, topLeft.y + segmentSize.y * .5f},
				{topLeft.x + space, topLeft.y},
				{bottomRight.x - space, topLeft.y},
				{bottomRight.x - u, topLeft.y + segmentSize.y * .5f},
				{bottomRight.x - space, bottomRight.y},
				{topLeft.x + space, bottomRight.y}
			};
			drawList->AddConvexPolyFilled(points, 6, (SEGMENTS[digit] >> (6 - i)) & 1 ? colorOn : colorOff);
		}
		else
		{
			// Vertical segment
			ImVec2 points[] = {
				{topLeft.x + segmentSize.x * .5f, topLeft.y + u},
				{bottomRight.x, topLeft.y + space},
				{bottomRight.x, bottomRight.y - space},
				{bottomRight.x - segmentSize.x * .5f, bottomRight.y - u},
				{topLeft.x, bottomRight.y - space},
				{topLeft.x, topLeft.y + space}};
			drawList->AddConvexPolyFilled(points, 6, (SEGMENTS[digit] >> (6 - i)) & 1 ? colorOn : colorOff);
		}
	}
}

// @brief ratio between unit font size and digit size
#define UNIT_SCALE 0.80f

// @brief ratio between digit width and height
#define DIGIT_WIDTH_RATIO 0.50f

/**
   @brief Render a numeric value with a 7 segment display style
   @param value the string representation of the value to display (may include the unit)
   @param color the color to use
   @param digitHeight the height of a digit
 */
void Dialog::Render7SegmentValue(const std::string& value, ImVec4 color, float digitHeight)
{
	bool ignoredClicked, ignoredHovered;
	Render7SegmentValue(value,color,digitHeight,ignoredClicked,ignoredHovered,false);
}

/**
   @brief Render a numeric value with a 7 segment display style
   @param value the string representation of the value to display (may include the unit)
   @param color the color to use
   @param digitHeight the height of a digit
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param clickable true (default) if the displayed value should be clickable
 */
void Dialog::Render7SegmentValue(const std::string& value, ImVec4 color, float digitHeight, bool &clicked, bool &hovered, bool clickable)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// Compute digit width according th height
	float digitWidth = digitHeight*DIGIT_WIDTH_RATIO;

	// Compute front and back color
	float bgmul = 0.15;
	auto bcolor = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x*bgmul, color.y*bgmul, color.z*bgmul, color.w));
	auto fcolor = ImGui::ColorConvertFloat4ToU32(color);


	// Parse value string to get integer and fractional part + unit
	bool inIntPart = true;
	bool inFractPart = false;
	vector<uint8_t> intPart;
	vector<uint8_t> fractPart;
	string unit;

	// TODO : move this to Unit.h
	#define UNIT_OVERLOAD_LABEL "Overload"

	if(value == UNIT_OVERLOAD_LABEL)
	{
		// Overload
		intPart.push_back(0);
		intPart.push_back(10); // 10 is for L
		unit = "Inf.";
	}
	else
	{
		// Iterate on each char of the value string
		for(const char c : value) 
		{
			if(c >= '0' && c <='9')
			{
				// This is a numeric digit
				if(inIntPart)
					intPart.push_back((uint8_t)(c-'0'));
				else if(inFractPart)
					fractPart.push_back((uint8_t)(c-'0'));
				else
					unit += c;
			}
			else if(c == '-')
			{
				// This is the decimal separator
				if(inIntPart)
				{
					intPart.push_back(c);
				}
				else
					LogWarning("Unexpected sign '%c' in value '%s'.\n",c,value.c_str());
			}
			else if(c == '.' || c == std::use_facet<std::numpunct<char> >(std::locale()).decimal_point() || c == ',')
			{
				// This is the decimal separator
				if(inIntPart)
				{
					inFractPart = true;
					inIntPart = false;
				}
				else
					LogWarning("Unexpected decimal separator '%c' in value '%s'.\n",c,value.c_str());
			}
			else if(isspace(c) || c == std::use_facet< std::numpunct<char> >(std::locale()).thousands_sep())
			{
				// We ingore spaces (except in unit part)
				if(inIntPart || inFractPart) {} // Ignore
				else
					unit += c;
			}
			else // Anything else
			{
				// This is the unit
				inFractPart = false;
				inIntPart = false;
				unit += c;
			}
		}
		// Trim the unit string
		unit = Trim(unit);

		// Fill fractional part with 2 zeros if it's empty
		if(fractPart.empty())
		{
			fractPart.push_back(0);
			fractPart.push_back(0);
		}
	}

	// Segment thickness
	float thickness = digitHeight/10;

	// Space between digits
	float spacing = 0.08 * digitWidth;

	// Size of decimal separator
	float dotSize = 2*thickness;

	// Size of unit font and unit text
	float unitSize = digitHeight*UNIT_SCALE;
	float unitTextWidth = ImGui::GetFont()->CalcTextSizeA(unitSize,FLT_MAX, 0.0f,unit.c_str()).x;

    ImVec2 size(digitWidth*(intPart.size()+fractPart.size())+dotSize+2*spacing+unitTextWidth+thickness, digitHeight);

	if(clickable)
	{
		bgmul = 0.0f;
		ImVec4 buttonColor = ImVec4(color.x*bgmul, color.y*bgmul, color.z*bgmul, 0);
		bgmul = 0.2f;
		ImVec4 buttonColorHovered = ImVec4(color.x*bgmul, color.y*bgmul, color.z*bgmul, color.w);
		bgmul = 0.3f;
		ImVec4 buttonColorActive = ImVec4(color.x*bgmul, color.y*bgmul, color.z*bgmul, color.w);
		ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColorHovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColorActive);
		clicked |= ImGui::Button(" ",size);
		hovered |= ImGui::IsItemHovered();
		ImGui::PopStyleColor(3);
		if(hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	else
    	ImGui::InvisibleButton("seven", size, ImGuiButtonFlags_EnableNav);

    ImVec2 position = ImGui::GetItemRectMin();

 	// Actual digit width (without space)
	float digitActualWidth = digitWidth - spacing;
	// Current x position
	float x = 0;

	// Integer part
	for(size_t i = 0; i < intPart.size(); i++)
	{
		Render7SegmentDigit(draw_list, intPart[i], ImVec2(digitActualWidth, digitHeight), ImVec2(position.x + x, position.y),thickness,fcolor,bcolor);
		x += digitWidth;
	}
	// Decimal separator
	x+= spacing;
	draw_list->AddCircleFilled(ImVec2(position.x+x+dotSize/2-spacing/2,position.y+digitHeight-dotSize/2),dotSize/2,fcolor);
	x+= dotSize;
	x+= spacing;
	// Factional part
	for(size_t i = 0; i < fractPart.size(); i++)
	{
		Render7SegmentDigit(draw_list, fractPart[i], ImVec2(digitActualWidth, digitHeight), ImVec2(position.x + x, position.y),thickness,fcolor,bcolor);
		x += digitWidth;
	}
	// Unit
	draw_list->AddText(NULL,unitSize,
		ImVec2(position.x + x + thickness, position.y),
		fcolor,
		unit.c_str());
}