/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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

Dialog::Dialog(const string& title, const string& id, ImVec2 defaultSize)
	: m_open(true)
	, m_id(id)
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
			ImGui::BulletText("%s", s.c_str());
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
	if(digit > 10)
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