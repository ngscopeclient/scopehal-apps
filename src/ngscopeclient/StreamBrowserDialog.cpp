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
	@brief Implementation of StreamBrowserDialog
 */

#include "ngscopeclient.h"
#include "StreamBrowserDialog.h"
#include "MainWindow.h"
#include "PreferenceTypes.h"

using namespace std;

#define ELLIPSIS_CHAR "\xE2\x80\xA6" // "..." character
#define PLUS_CHAR "+"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// StreamBrowserTimebaseInfo

StreamBrowserTimebaseInfo::StreamBrowserTimebaseInfo(shared_ptr<Oscilloscope> scope)
{
	//Interleaving flag
	m_interleaving = scope->IsInterleaving();

	//Sample rate
	Unit srate(Unit::UNIT_SAMPLERATE);
	auto rate = scope->GetSampleRate();
	if(m_interleaving)
		m_rates = scope->GetSampleRatesInterleaved();
	else
		m_rates = scope->GetSampleRatesNonInterleaved();

	m_rate = 0;
	for(size_t i=0; i<m_rates.size(); i++)
	{
		m_rateNames.push_back(srate.PrettyPrint(m_rates[i]));
		if(m_rates[i] == rate)
			m_rate = i;
	}

	//Sample depth
	Unit sdepth(Unit::UNIT_SAMPLEDEPTH);
	auto depth = scope->GetSampleDepth();
	if(m_interleaving)
		m_depths = scope->GetSampleDepthsInterleaved();
	else
		m_depths = scope->GetSampleDepthsNonInterleaved();

	m_depth = 0;
	for(size_t i=0; i<m_depths.size(); i++)
	{
		m_depthNames.push_back(sdepth.PrettyPrint(m_depths[i]));
		if(m_depths[i] == depth)
			m_depth = i;
	}

	Unit hz(Unit::UNIT_HZ);

	m_rbw = scope->GetResolutionBandwidth();
	m_rbwText = hz.PrettyPrintInt64(m_rbw);

	m_span = scope->GetSpan();
	m_spanText = hz.PrettyPrint(m_span);

	//TODO: some instruments have per channel center freq, how to handle this?
	m_center = scope->GetCenterFrequency(0);
	m_centerText = hz.PrettyPrint(m_center);

	m_start = m_center - m_span/2;
	m_startText = hz.PrettyPrint(m_start);
	m_end = m_center + m_span/2;
	m_endText = hz.PrettyPrint(m_end);

	m_samplingMode = scope->GetSamplingMode();

	auto spec = dynamic_pointer_cast<SCPISpectrometer>(scope);
	if(spec)
	{
		Unit fs(Unit::UNIT_FS);
		m_integrationTime = spec->GetIntegrationTime();
		m_integrationText = fs.PrettyPrint(m_integrationTime);
	}
	else
		m_integrationTime = 0;

	m_adcmode = scope->GetADCMode(0);
	m_adcmodeNames = scope->GetADCModeNames(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

StreamBrowserDialog::StreamBrowserDialog(Session& session, MainWindow* parent)
	: Dialog("Stream Browser", "Stream Browser", ImVec2(550, 400))
	, m_session(session)
	, m_parent(parent)
{

}

StreamBrowserDialog::~StreamBrowserDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

//Helper methods for rendering widgets that appear in the StreamBrowserDialog.

/**
	@brief prepare rendering context to display a badge at the end of current line
 */
void StreamBrowserDialog::startBadgeLine()
{
	ImGuiWindow *window = ImGui::GetCurrentWindowRead();
	// roughly, what ImGui::GetCursorPosPrevLineX would be, if it existed; convert from absolute-space to window-space
	m_badgeXMin = (window->DC.CursorPosPrevLine - window->Pos + window->Scroll).x;
	m_badgeXCur = ImGui::GetWindowContentRegionMax().x;
}

/**
	@brief render a badge for an instrument node

	@param inst the instrument to render the badge for
	@param latched true if the redering of this batch should be latched (i.e. only render if previous badge has been here for more than a given time)
	@param badge the badge type
*/
void StreamBrowserDialog::renderInstrumentBadge(std::shared_ptr<Instrument> inst, bool latched, InstrumentBadge badge)
{
	auto& prefs = m_session.GetPreferences();
	double now = GetTime();
	if(latched)
	{
		std::pair<double,InstrumentBadge> old = m_instrumentLastBadge[inst];
		double elapsed = now - old.first;
		if(elapsed < prefs.GetReal("Appearance.Stream Browser.instrument_badge_latch_duration"))
		{	// Keep previous badge
			badge = old.second;
		}
	}
	else
		m_instrumentLastBadge[inst] = std::pair<double,InstrumentBadge>(now,badge);

	switch (badge)
	{
		case StreamBrowserDialog::BADGE_ARMED:
			/* prefer language "ARMED" to "RUN":
				* "RUN" could mean either "waiting
				* for trigger" or "currently
				* capturing samples post-trigger",
				* "ARMED" is unambiguous */
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_armed_badge_color")), "ARMED", "A", NULL);
			break;
		case StreamBrowserDialog::BADGE_STOPPED:
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_stopped_badge_color")), "STOPPED", "STOP", "S", NULL);
			break;
		case StreamBrowserDialog::BADGE_TRIGGERED:
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_triggered_badge_color")), "TRIGGERED", "TRIG'D", "T'D", "T", NULL);
			break;
		case StreamBrowserDialog::BADGE_BUSY:
			/* prefer language "BUSY" to "WAIT":
				* "WAIT" could mean "waiting for
				* trigger", "BUSY" means "I am
				* doing something internally and am
				* not ready for some reason" */
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_busy_badge_color")), "BUSY", "B", NULL);
			break;
		case StreamBrowserDialog::BADGE_AUTO:
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.trigger_auto_badge_color")), "AUTO", "A", NULL);
			break;
		default:
			break;
	}
}

/**
	@brief render a badge at the end of current line with provided color and text

	@param color the color of the badge
	@param ... a null terminated list of labels form the largest to the smallest to use as a badge label according to the available space
*/
void StreamBrowserDialog::renderBadge(ImVec4 color, ... /* labels, ending in NULL */)
{
	va_list ap;
	va_start(ap, color);

	while (const char *label = va_arg(ap, const char *))
	{
		float xsz = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
		if ((m_badgeXCur - xsz) < m_badgeXMin)
			continue;

		// ok, we have enough space -- commit to it!
		m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLine(m_badgeXCur);
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		SmallDisabledButton(label);
		ImGui::PopStyleColor();
		break;
	}
	va_end(ap);
}

/**
	@brief Render a combo box with provided color and values

	@param label	Label for the combo box
	@param alignRight if true
	@param color the color of the combo box
	@param selected the selected value index (in/out)
	@param values the combo box values
	@param useColorForText if true, use the provided color for text (and a darker version of it for background color)
	@param cropTextTo if >0 crop the combo text up to this number of characters to have it fit the available space
	@param hideArrow True to hide the dropdown arrow (defaults to true)
	@param paddingRight the padding to leave at the right of the combo when alighRight is true (defaults to 0)

	@return true if the selected value of the combo has been changed
 */
bool StreamBrowserDialog::renderCombo(
	const char* label,
	bool alignRight,
	ImVec4 color,
	int &selected,
	const vector<string> &values,
	bool useColorForText,
	uint8_t cropTextTo,
	bool hideArrow,
	float paddingRight)
{
	if(selected >= (int)values.size() || selected < 0)
	{
		selected = 0;
	}

	const char* selectedLabel = "";
	if(selected < (int)values.size())
		selectedLabel = values[selected].c_str();

	if(alignRight)
	{
		int padding = ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2 + paddingRight;
		float xsz = ImGui::CalcTextSize(selectedLabel).x + padding;
		string resizedLabel;
		if ((m_badgeXCur - xsz) < m_badgeXMin)
		{
			if(cropTextTo == 0)
				return false; // No room and we don't want to crop text
			resizedLabel = selectedLabel;
			while((m_badgeXCur - xsz) < m_badgeXMin)
			{
				// Try and crop text
				resizedLabel = resizedLabel.substr(0,resizedLabel.size()-1);
				if(resizedLabel.size() < cropTextTo)
					break; // We don't want to make the text that short
				xsz = ImGui::CalcTextSize((resizedLabel + ELLIPSIS_CHAR).c_str()).x + padding;
			}
			if((m_badgeXCur - xsz) < m_badgeXMin)
				return false; // Still no room
			// We found an acceptable size
			resizedLabel = resizedLabel + ELLIPSIS_CHAR;
			selectedLabel = resizedLabel.c_str();
		}
		m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLine(m_badgeXCur);
	}

	if(useColorForText)
	{
		// Use channel color for shape combo, but darken it to make text readable
		float bgmul = 0.4;
		auto bcolor = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x*bgmul, color.y*bgmul, color.z*bgmul, color.w));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bcolor);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, color);
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0));
	int flags = 0;
	if(hideArrow)
		flags |= ImGuiComboFlags_NoArrowButton;
	if(alignRight)
		flags |= ImGuiComboFlags_WidthFitPreview;

	bool changed = false;
	if(ImGui::BeginCombo(label, selectedLabel, flags))
	{
		for(int i = 0 ; i < (int)values.size() ; i++)
		{
			const bool is_selected = (i == selected);
			if(ImGui::Selectable(values[i].c_str(), is_selected))
			{
				selected = i;
				changed = true;
			}
			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	if(useColorForText)
		ImGui::PopStyleColor();
	return changed;
}

/**
   @brief Render a combo box with provded color and values

   @param label	Label for the combo box
   @param alignRight true if the combo should be aligned to the right
   @param color the color of the combo box
   @param selected the selected value index (in/out)
   @param ... the combo box values
   @return true if the selected value of the combo has been changed
 */
bool StreamBrowserDialog::renderCombo(
	const char* label,
	bool alignRight,
	ImVec4 color,
	int *selected, ... /* values, ending in NULL */)
{
	if(!selected)
	{
		LogError("Invalid call to renderCombo() method, 'selected' parameter must not be null.");
		return false;
	}
	va_list ap;
	va_start(ap, selected);
	std::vector<string> values;
	while (const char *item = va_arg(ap, const char *))
		values.push_back(string(item));

	va_end(ap);
	return renderCombo(label, alignRight, color, (*selected), values);
}

/**
   @brief Render a toggle button combo

   @param label	Label for the combo box
   @param alignRight true if the combo should be aligned to the right
   @param color the color of the toggle button
   @param curValue the value of the toggle button
   @param valueOff label for value off (optionnal, defaults to "OFF")
   @param valueOn label for value on (optionnal, defaults to "ON")
   @param cropTextTo if >0 crop the combo text up to this number of characters to have it fit the available space (optionnal, defaults to 0)
   @param paddingRight the padding to leave at the right of the combo when alighRight is true (defaults to 0)
   @return true if selection has changed
 */
bool StreamBrowserDialog::renderToggle(const char* label, bool alignRight, ImVec4 color, bool& curValue, const char* valueOff, const char* valueOn, uint8_t cropTextTo, float paddingRight)
{
	int selection = (int)curValue;
	std::vector<string> values;
	values.push_back(string(valueOff));
	values.push_back(string(valueOn));
	bool ret = renderCombo(label, alignRight, color, selection, values, false, cropTextTo, true, paddingRight);
	curValue = (selection == 1);
	return ret;
}

/**
   @brief Render an on/off toggle button combo

   @param label	Label for the combo box
   @param alignRight true if the combo should be aligned to the right
   @param curValue the value of the toggle button
   @param valueOff label for value off (optionnal, defaults to "OFF")
   @param valueOn label for value on (optionnal, defaults to "ON")
   @param cropTextTo if >0 crop the combo text up to this number of characters to have it fit the available space (optionnal, defaults to 0)
   @param paddingRight the padding to leave at the right of the combo when alighRight is true (defaults to 0)
   @return true if value has changed
 */
bool StreamBrowserDialog::renderOnOffToggle(const char* label, bool alignRight, bool& curValue, const char* valueOff, const char* valueOn, uint8_t cropTextTo, float paddingRight)
{
	auto& prefs = m_session.GetPreferences();
	ImVec4 color = ImGui::ColorConvertU32ToFloat4(
		(curValue ?
			prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color") :
			prefs.GetColor("Appearance.Stream Browser.instrument_off_badge_color")));
	return renderToggle(label, alignRight, color, curValue, valueOff, valueOn, cropTextTo,paddingRight);
}

/**
   @brief Render a numeric value
   @param value the string representation of the value to display (may include the unit)
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param color the color to use (defaults to white)
   @param allow7SegmentDisplay (defaults to false) true if the value can be displayed in 7 segment format
   @param digitHeight the height of a digit (if 0 (defualt), will use ImGui::GetFontSize())
   @param clickable true (default) if the displayed value should be clickable
 */
void StreamBrowserDialog::renderNumericValue(const std::string& value, bool &clicked, bool &hovered, ImVec4 color, bool allow7SegmentDisplay, float digitHeight, bool clickable)
{
	bool use7Segment = false;
	bool changeFont = false;
	auto& prefs = m_session.GetPreferences();
	auto displayType = prefs.GetEnumRaw("Appearance.Stream Browser.numeric_value_display");
	FontWithSize font;
	if(allow7SegmentDisplay)
	{
		use7Segment = displayType == NumericValueDisplay::NUMERIC_DISPLAY_7SEGMENT;
		if(!use7Segment)
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
void StreamBrowserDialog::renderReadOnlyProperty(float width, const string& label, const string& value, const char* tooltip)
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
   @param width the width of the input value (if <=0 will default to 6*ImGui::GetStyle())
   @param label the value label (used as a label for the TextInput)
   @param currentValue the string representation of the current value
   @param comittedValue the last comitted typed (float, double or int64_t) value
   @param unit the Unit of the value
   @param tooltip if not null, will add the provided text as an help marker (defaults to nullptr)
   @param color the color to use
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param allow7SegmentDisplay (defaults to false) true if the value can be displayed in 7 segment format
   @param explicitApply (defaults to false) true if the input value needs to explicitly be applied (by clicking the apply button)
   @return true if the value has changed
 */
bool StreamBrowserDialog::renderEditableProperty(float width, const std::string& label, std::string& currentValue, T& committedValue, Unit unit, const char* tooltip, ImVec4 color, bool allow7SegmentDisplay, bool explicitApply)
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, int64_t>,"renderEditableProperty only supports float or double");
	auto& prefs = m_session.GetPreferences();
	auto displayType = prefs.GetEnumRaw("Appearance.Stream Browser.numeric_value_display");
	bool use7Segment = false;
	bool changeFont = false;
	FontWithSize font;
	if(allow7SegmentDisplay)
	{
		use7Segment = displayType == NumericValueDisplay::NUMERIC_DISPLAY_7SEGMENT;
		if(!use7Segment)
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
	if(width <= 0) width = 6*fontSize;
	ImGui::SetNextItemWidth(width);
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
			ImVec4 buttonColorActive = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.apply_button_color"));
			float bgmul = 0.8f;
			ImVec4 buttonColorHovered = ImVec4(buttonColorActive.x*bgmul, buttonColorActive.y*bgmul, buttonColorActive.z*bgmul, buttonColorActive.w);
			bgmul = 0.7f;
			ImVec4 buttonColor = ImVec4(buttonColorActive.x*bgmul, buttonColorActive.y*bgmul, buttonColorActive.z*bgmul, buttonColorActive.w);
			ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColorHovered);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColorActive);
			ImGui::BeginDisabled(!dirty);
			if(ImGui::Button("\xE2\x8F\x8E")) // Carriage return symbol
			{	// Apply button click
				validateChange = true;
			}
			ImGui::EndDisabled();
			if(dirty && ImGui::IsItemHovered())
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
		if (hovered)
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

template<typename T>
/**
   @brief Render an editable numeric value with explicit apply (if the input value needs to explicitly be applied by clicking the apply button)
   @param width the width of the input value (if <=0 will default to 6*ImGui::GetStyle())
   @param label the value label (used as a label for the TextInput)
   @param currentValue the string representation of the current value
   @param comittedValue the last comitted typed (float, double or int64_t) value
   @param unit the Unit of the value
   @param tooltip if not null, will add the provided text as an help marker (defaults to nullptr)
   @param color the color to use
   @param clicked output value for clicked state
   @param hovered output value for hovered state
   @param allow7SegmentDisplay (defaults to false) true if the value can be displayed in 7 segment format
   @return true if the value has changed
 */
bool StreamBrowserDialog::renderEditablePropertyWithExplicitApply(float width, const std::string& label, std::string& currentValue, T& committedValue, Unit unit, const char* tooltip, ImVec4 color, bool allow7SegmentDisplay)
{
	return renderEditableProperty(width,label,currentValue,committedValue,unit,tooltip,color,allow7SegmentDisplay,true);
}


/**
   @brief Render a download progress bar for a given instrument channel

   @param inst the instrument to render the progress channel for
   @param chan the channel to render the progress for
   @param isLast true if it is the last channel of the instrument
 */
void StreamBrowserDialog::renderDownloadProgress(std::shared_ptr<Instrument> inst, InstrumentChannel *chan, bool isLast)
{
	static const char* const download[] = {"DOWNLOADING", "DOWNLOAD" ,"DL","D", NULL};

	/* prefer language "PENDING" to "WAITING": "PENDING" implies
		* that we are going to do it when we get through a list of
		* other things, "WAITING" could mean that the channel is
		* waiting for something else (trigger?)
		*/
	static const char* const pend[]     = {"PENDING"    , "PEND"     ,"PE","P", NULL};

	/* prefer language "COMPLETE" to "READY": "READY" implies
		* that the channel might be ready to capture or something,
		* but "COMPLETE" at least is not to be confused with that.
		* ("DOWNLOADED" is more specific but is easy to confuse
		* with "DOWNLOADING".  If you can come up with a better
		* mid-length abbreviation for "COMPLETE" than "DL OK" /
		* "OK", give it a go, I guess.)
		*/
	static const char* const ready[]  	= {"COMPLETE"   , "DL OK"    ,"OK","C", NULL};

	/* Let's use active for fast download channels to display when data is available
		*/
	static const char* const active[]   = {"ACTIVE"      , "ACTV"    ,"ACT","A", NULL};

	static const char* const* labels;

	ImVec4 color;
	bool shouldRender = true;
	bool hasProgress = false;
	double elapsed = GetTime() - chan->GetDownloadStartTime();
	auto& prefs = m_session.GetPreferences();


	// determine what label we should apply, and while we are at
	// it, determine if this channel appears to be slow enough
	// to need a progress bar

/// @brief hysteresis threshold for a channel finishing a download faster than this to be declared fast
#define CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS ((double)0.2)

/// @brief hysteresis threshold for a channel finishing a still being in progress for longer than this to be declared slow
#define CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS ((double)0.4)

	switch(chan->GetDownloadState())
	{
		case InstrumentChannel::DownloadState::DOWNLOAD_NONE:
			if (isLast && (elapsed < CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS))
				m_instrumentDownloadIsSlow[inst] = false;
			/* FALLTHRU */
		case InstrumentChannel::DownloadState::DOWNLOAD_UNKNOWN:
			/* There is nothing to say about this --
				* either there is nothing pending at all on
				* the system, or this scope doesn't know
				* how to report it, and in either case, we
				* don't need to render a badge about it.
				*/
			shouldRender = false;
			break;
		case InstrumentChannel::DownloadState::DOWNLOAD_WAITING:
			labels = pend;
			if (elapsed > CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
				m_instrumentDownloadIsSlow[inst] = true;
			hasProgress = m_instrumentDownloadIsSlow[inst];
			color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_wait_badge_color"));
			break;
		case InstrumentChannel::DownloadState::DOWNLOAD_IN_PROGRESS:
			labels = download;
			if (elapsed > CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
				m_instrumentDownloadIsSlow[inst] = true;
			hasProgress = m_instrumentDownloadIsSlow[inst];
			color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_progress_badge_color"));
			break;
		case InstrumentChannel::DownloadState::DOWNLOAD_FINISHED:
			labels = ready;
			if (isLast && (elapsed < CHANNEL_DOWNLOAD_THRESHOLD_FAST_SECONDS))
				m_instrumentDownloadIsSlow[inst] = false;
			color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_finished_badge_color"));
			break;
		default:
			shouldRender = false;
			break;
	}

	// For fast channels, show a constant green badge when a
	// download has started "recently" -- even if we're not
	// downloading at this moment.  This could be slightly
	// misleading (i.e., after a channel goes into STOP mode, we
	// will remain ACTIVE for up to THRESHOLD_SLOW time) but the
	// period of time for which it is misleading is short!
	if(!m_instrumentDownloadIsSlow[inst] && elapsed < CHANNEL_DOWNLOAD_THRESHOLD_SLOW_SECONDS)
	{
		labels = active;
		color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.download_active_badge_color"));
		shouldRender = true;
		hasProgress = false;
	}

	if (!shouldRender)
		return;

/// @brief Width used to display progress bars (e.g. download progress bar)
#define PROGRESS_BAR_WIDTH	80

	// try first adding a bar, and if there isn't enough room
	// for a bar, skip it and try just putting a label
	for (int withoutBar = 0; withoutBar < 2; withoutBar++)
	{
		if (withoutBar)
			hasProgress = false;

		for (int i = 0; labels[i]; i++)
		{
			const char *label = labels[i];

			float xsz = ImGui::CalcTextSize(label).x + (hasProgress ? (ImGui::GetStyle().ItemSpacing.x + PROGRESS_BAR_WIDTH) : 0) + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x;
			if ((m_badgeXCur - xsz) < m_badgeXMin)
				continue;

			// ok, we have enough space -- commit to it!
			m_badgeXCur -= xsz - ImGui::GetStyle().ItemSpacing.x;
			ImGui::SameLine(m_badgeXCur);
			ImGui::PushStyleColor(ImGuiCol_Button, color);
			SmallDisabledButton(label);
			ImGui::PopStyleColor();
			if(hasProgress)
			{
				ImGui::SameLine();
				ImGui::ProgressBar(chan->GetDownloadProgress(), ImVec2(PROGRESS_BAR_WIDTH, ImGui::GetFontSize()));
			}

			return;
		}
	}
	// well, shoot -- I guess there wasn't enough room to do *anything* useful!
}

/**
   @brief Render a PSU properties row

   @param isVoltage true for voltage rows, false for current rows
   @param cc true if the PSU channel is in constant current mode, false for constant voltage mode
   @param chan the PSU channel to render properties for
   @param setValue the set value text
   @param committedValue the last commited value
   @param measuredValue the measured value text
   @param clicked output param for clicked state
   @param hovered output param for hovered state

   @return true if the value has been modified
 */
bool StreamBrowserDialog::renderPsuRows(
	bool isVoltage,
	bool cc,
	PowerSupplyChannel* chan,
	std::string& currentValue, 
	float& committedValue, 
	std::string& measuredValue,
	bool &clicked,
	bool &hovered)
{
	bool changed = false;
	auto& prefs = m_session.GetPreferences();
	// Row 1
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(isVoltage ? "Voltage:" : "Current:");
	ImGui::TableSetColumnIndex(1);
	StreamDescriptor sv(chan, isVoltage ? 1 : 3);
	ImGui::PushID(isVoltage ? "sV" :  "sC");
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_set_label_color")));
	ImGui::Selectable("- Set");
	ImGui::PopStyleColor();
	if(ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("Scalar", &sv, sizeof(sv));
		string dragText = chan->GetDisplayName() + (isVoltage ? " voltage" : " current") + " set value";
		ImGui::TextUnformatted(dragText.c_str());
		ImGui::EndDragDropSource();
	}
	else
		DoItemHelp();
	ImGui::PopID();
	ImGui::TableSetColumnIndex(2);
	ImGui::PushID(isVoltage ? "sV" :  "sC");

	ImVec4 color = ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_7_segment_color"));

	Unit unit(isVoltage ? Unit::UNIT_VOLTS : Unit::UNIT_AMPS);

	if(renderEditablePropertyWithExplicitApply(0,"##psuSetValue",currentValue,committedValue,unit,nullptr,color,true))
	{
		changed = true;
	}

	ImGui::PopID();
	// Row 2
	ImGui::TableNextRow();
	if((isVoltage && !cc) || (!isVoltage && cc))
	{
		ImGui::TableSetColumnIndex(0);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(prefs.GetColor(isVoltage ? "Appearance.Stream Browser.psu_cv_badge_color" : "Appearance.Stream Browser.psu_cc_badge_color")));
		SmallDisabledButton(isVoltage ? "CV" : "CC");
		ImGui::PopStyleColor();
	}
	ImGui::TableSetColumnIndex(1);
	StreamDescriptor mv(chan, isVoltage ? 0 : 2);
	ImGui::PushID(isVoltage ? "mV" :  "mC");
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.psu_meas_label_color")));
	ImGui::Selectable("- Meas.");
	ImGui::PopStyleColor();
	if(ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("Scalar", &mv, sizeof(mv));
		string dragText = chan->GetDisplayName() + (isVoltage ? " voltage" : " current") + " measured value";
		ImGui::TextUnformatted(dragText.c_str());
		ImGui::EndDragDropSource();
	}
	else
		DoItemHelp();
	ImGui::PopID();
	ImGui::TableSetColumnIndex(2);
	ImGui::PushID(isVoltage ? "mV" :  "mC");

	renderNumericValue(measuredValue,clicked,hovered,color,true,0,false);

	ImGui::PopID();
	return changed;
}

/**
   @brief Render DMM channel properties
   @param dmm the DMM to render channel properties for
   @param dmmchan the DMM channel to render properties for
   @param clicked output param for clicked state
   @param hovered output param for hovered state
 */
void StreamBrowserDialog::renderDmmProperties(std::shared_ptr<Multimeter> dmm, MultimeterChannel* dmmchan, bool isMain, bool &clicked, bool &hovered)
{
	size_t streamIndex = isMain ? 0 : 1;
	Unit unit = dmmchan->GetYAxisUnits(streamIndex);
	float mainValue = dmmchan->GetScalarValue(streamIndex);
	string valueText = unit.PrettyPrint(mainValue,dmm->GetMeterDigits());
	ImVec4 color = ImGui::ColorConvertU32ToFloat4(ColorFromString(dmmchan->m_displaycolor));
	string streamName = isMain ? "Main" : "Secondary";

	ImGui::PushID(streamName.c_str());

	// Get available operating and current modes
	auto modemask = isMain ? dmm->GetMeasurementTypes() : dmm->GetSecondaryMeasurementTypes();
	auto curMode = isMain ? dmm->GetMeterMode() : dmm->GetSecondaryMeterMode();

	// Stream name
	bool open = ImGui::TreeNodeEx(streamName.c_str(), (curMode > 0) ? ImGuiTreeNodeFlags_DefaultOpen : 0);

	// Mode combo
	startBadgeLine();
	ImGui::PushID(streamName.c_str());
	vector<std::string> modeNames;
	vector<Multimeter::MeasurementTypes> modes;
	if(!isMain)
	{
		// Add None for secondary measurement to be able to disable it
		modeNames.push_back("None");
		modes.push_back(Multimeter::MeasurementTypes::NONE);
	}
	int modeSelector = 0;
	for(unsigned int i=0; i<32; i++)
	{
		auto mode = static_cast<Multimeter::MeasurementTypes>(1 << i);
		if(modemask & mode)
		{
			modes.push_back(mode);
			modeNames.push_back(dmm->ModeToText(mode));
			if(curMode == mode)
				modeSelector = modes.size() - 1;
		}
	}

	if(renderCombo("##mode", true, color, modeSelector, modeNames,true,3,true))
	{
		curMode = modes[modeSelector];
		if(isMain)
			dmm->SetMeterMode(curMode);
		else
		{
			dmm->SetSecondaryMeterMode(curMode);
			// Open or close tree node according the secondary measure mode
		    ImGuiContext& g = *GImGui;
			ImGui::TreeNodeSetOpen(g.LastItemData.ID,(curMode > 0));
		}
	}
	ImGui::PopID();

	StreamDescriptor s(dmmchan, streamIndex);
	if(ImGui::BeginDragDropSource())
	{
		if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
			ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
		else
			ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

		ImGui::TextUnformatted(s.GetName().c_str());
		ImGui::EndDragDropSource();
	}
	else
		DoItemHelp();

	if(open)
		ImGui::TreePop();

	if(open)
	{
		renderNumericValue(valueText,clicked,hovered,color,true,ImGui::GetFontSize()*2,false);

		if(isMain)
		{
			auto dmmState = m_session.GetDmmState(dmm);
			if(dmmState)
			{
				ImGui::PushID("autorange");
				// For main, also show the autorange combo
				startBadgeLine();
				bool autorange = dmmState->m_autoRange.load();
				if(renderOnOffToggle("##autorange",true,autorange,"Manual Range","Autorange",3))
				{
					dmm->SetMeterAutoRange(autorange);
					dmmState->m_needsUpdate = true;
				}
				ImGui::PopID();
			}
		}
	}

	ImGui::PopID();
}

/**
   @brief Render AWG channel properties

   @param awg the AWG to render channel properties for
   @param awgchan the AWG channel to render properties for
 */
void StreamBrowserDialog::renderAwgProperties(std::shared_ptr<FunctionGenerator> awg, FunctionGeneratorChannel* awgchan)
{
	Unit volts(Unit::UNIT_VOLTS);
	Unit hz(Unit::UNIT_HZ);
	Unit percent(Unit::UNIT_PERCENT);
	Unit fs(Unit::UNIT_FS);

	size_t channelIndex = awgchan->GetIndex();
	auto awgState = m_session.GetFunctionGeneratorState(awg);
	if(!awgState)
		return;

	auto dwidth = ImGui::GetFontSize() * 6;

	//Check if anything changed hardware side
	float amp = awgState->m_channelAmplitude[channelIndex];
	if(amp != awgState->m_committedAmplitude[channelIndex])
	{
		awgState->m_committedAmplitude[channelIndex] = amp;
		awgState->m_strAmplitude[channelIndex] = volts.PrettyPrint(amp);
	}
	float off = awgState->m_channelOffset[channelIndex];
	if(off != awgState->m_committedOffset[channelIndex])
	{
		awgState->m_committedOffset[channelIndex] = off;
		awgState->m_strOffset[channelIndex] = volts.PrettyPrint(off);
	}
	float freq = awgState->m_channelFrequency[channelIndex];
	if(freq != awgState->m_committedFrequency[channelIndex])
	{
		awgState->m_committedFrequency[channelIndex] = freq;
		awgState->m_strFrequency[channelIndex] = hz.PrettyPrint(freq);
	}
	float dutyCycle = awgState->m_channelDutyCycle[channelIndex];
	if(dutyCycle != awgState->m_committedDutyCycle[channelIndex])
	{
		awgState->m_committedDutyCycle[channelIndex] = dutyCycle;
		awgState->m_strDutyCycle[channelIndex] = percent.PrettyPrint(dutyCycle);
	}
	float riseTime = awgState->m_channelRiseTime[channelIndex];
	if(riseTime != awgState->m_committedRiseTime[channelIndex])
	{
		awgState->m_committedRiseTime[channelIndex] = riseTime;
		awgState->m_strRiseTime[channelIndex] = fs.PrettyPrint(riseTime);
	}
	float fallTime = awgState->m_channelFallTime[channelIndex];
	if(fallTime != awgState->m_committedFallTime[channelIndex])
	{
		awgState->m_committedFallTime[channelIndex] = fallTime;
		awgState->m_strFallTime[channelIndex] = fs.PrettyPrint(fallTime);
	}

	auto& prefs = m_session.GetPreferences();

	// Row 1
	ImGui::Text("Waveform:");
	startBadgeLine(); // Needed for shape combo
	// Shape combo
	// Get current shape and  shape index
	FunctionGenerator::WaveShape shape = awgState->m_channelShape[channelIndex];
	int shapeIndex = awgState->m_channelShapeIndexes[channelIndex][shape];
	if(renderCombo(
		"##waveform",
		true,
		ImGui::ColorConvertU32ToFloat4(ColorFromString(awgchan->m_displaycolor)),
		shapeIndex, awgState->m_channelShapeNames[channelIndex],
		true,
		3,true))
	{
		shape = awgState->m_channelShapes[channelIndex][shapeIndex];
		awg->SetFunctionChannelShape(channelIndex, shape);
		// Update state right now to cover from slow intruments
		awgState->m_channelShape[channelIndex]=shape;
		// Tell intrument thread that the FunctionGenerator state has to be updated
		awgState->m_needsUpdate[channelIndex] = true;
	}

	// Store current Y position for shape preview
	float shapePreviewY = ImGui::GetCursorPosY();

	// Row 2
	// Frequency label
	if(renderEditableProperty(dwidth,
		"Frequency",
		awgState->m_strFrequency[channelIndex],
		awgState->m_committedFrequency[channelIndex],
		hz/*,"Frequency of the generated waveform"*/))
	{
		awg->SetFunctionChannelFrequency(channelIndex, awgState->m_committedFrequency[channelIndex]);
		awgState->m_needsUpdate[channelIndex] = true;
	}
	/*StreamDescriptor sfreq(awgchan, 0);
	if(ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("Scalar", &sfreq, sizeof(sfreq));
		string dragText = awgchan->GetDisplayName() + " frequency";
		ImGui::TextUnformatted(dragText.c_str());
		ImGui::EndDragDropSource();
	}
	else
	DoItemHelp();
	*/

	//Row 2
	//Duty cycle
	if(renderEditableProperty(dwidth,
		"Duty cycle",
		awgState->m_strDutyCycle[channelIndex],
		awgState->m_committedDutyCycle[channelIndex],
		percent/*,"Duty cycle of the waveform, in percent. Not applicable to all waveform types."*/))
	{
		awg->SetFunctionChannelDutyCycle(channelIndex, awgState->m_committedDutyCycle[channelIndex]);
		awgState->m_needsUpdate[channelIndex] = true;
	}

	// Shape preview
	startBadgeLine();
	auto height = ImGui::GetFontSize() * 2;
	auto width =  height * 2;
	if ((m_badgeXCur - width) >= m_badgeXMin)
	{
		// ok, we have enough space draw preview
		m_badgeXCur -= width;
		// save current y position to restore it after drawing the preview
		float currentY = ImGui::GetCursorPosY();
		// Continue layout on current line (row 3)
		ImGui::SameLine(m_badgeXCur);
		// But use y position of row 2
		ImGui::SetCursorPosY(shapePreviewY);
		ImGui::Image(
			m_parent->GetTextureManager()->GetTexture(m_parent->GetIconForWaveformShape(shape)),
			ImVec2(width,height));
		// Now that we're done with shape preview, restore y position of row 3
		ImGui::SetCursorPosY(currentY);
	}

	if(awg->HasFunctionRiseFallTimeControls(channelIndex))
	{	//Row 3
		//Fall Time
		if(renderEditableProperty(dwidth,
			"Rise Time",
			awgState->m_strRiseTime[channelIndex],
			awgState->m_committedRiseTime[channelIndex],
			fs))
		{
			awg->SetFunctionChannelRiseTime(channelIndex, awgState->m_committedRiseTime[channelIndex]);
			awgState->m_needsUpdate[channelIndex] = true;
		}
		//Row 4
		//Fall Time
		if(renderEditableProperty(dwidth,
			"Fall Time",
			awgState->m_strFallTime[channelIndex],
			awgState->m_committedFallTime[channelIndex],
			fs))
		{
			awg->SetFunctionChannelFallTime(channelIndex, awgState->m_committedFallTime[channelIndex]);
			awgState->m_needsUpdate[channelIndex] = true;
		}
	}

	// Row 5
	if(renderEditablePropertyWithExplicitApply(dwidth,
		"Amplitude",
		awgState->m_strAmplitude[channelIndex],
		awgState->m_committedAmplitude[channelIndex],
		volts,"Peak-to-peak amplitude of the generated waveform"))
	{
		awg->SetFunctionChannelAmplitude(channelIndex, awgState->m_committedAmplitude[channelIndex]);
		awgState->m_needsUpdate[channelIndex] = true;
	}

	//Row 6
	//Offset
	if(renderEditablePropertyWithExplicitApply(dwidth,
		"Offset",
		awgState->m_strOffset[channelIndex],
		awgState->m_committedOffset[channelIndex],
		volts,"DC offset for the waveform above (positive) or below (negative) ground"))
	{
		awg->SetFunctionChannelOffset(channelIndex, awgState->m_committedOffset[channelIndex]);
		awgState->m_needsUpdate[channelIndex] = true;
	}

	//Row 7
	//Impedance
	ImGui::SetNextItemWidth(dwidth);
	FunctionGenerator::OutputImpedance impedance = awgState->m_channelOutputImpedance[channelIndex];
	bool isHiZ = (impedance == FunctionGenerator::OutputImpedance::IMPEDANCE_HIGH_Z);
	int comboValue = isHiZ ? 0 : 1;
	bool changed = renderCombo(
		"Impedance",
		false,
		ImGui::ColorConvertU32ToFloat4(prefs.GetColor(
			isHiZ ? "Appearance.Stream Browser.awg_hiz_badge_color" :
			"Appearance.Stream Browser.awg_50ohms_badge_color")),
		&comboValue,
		"Hi-Z",
		"50 Ω",
		nullptr);
	HelpMarker(
		"Select the expected load impedance.\n\n"
		"If set incorrectly, amplitude and offset will be inaccurate due to reflections.");


	if(changed)
	{
		impedance = ((comboValue == 0) ? FunctionGenerator::OutputImpedance::IMPEDANCE_HIGH_Z : FunctionGenerator::OutputImpedance::IMPEDANCE_50_OHM);

		awg->SetFunctionChannelOutputImpedance(channelIndex,impedance);

		// Update state right now to cover from slow intruments
		awgState->m_channelOutputImpedance[channelIndex]=impedance;

		// Tell intrument thread that the FunctionGenerator state has to be updated
		awgState->m_needsUpdate[channelIndex] = true;
	}
}

/**
   @brief Rendering of an instrument node

   @param instrument the instrument to render
 */
void StreamBrowserDialog::renderInstrumentNode(shared_ptr<Instrument> instrument)
{
	// Get preferences for colors
	auto& prefs = m_session.GetPreferences();

	ImGui::PushID(instrument.get());
	bool instIsOpen = ImGui::TreeNodeEx(instrument->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
	startBadgeLine();

	auto state = m_session.GetInstrumentConnectionState(instrument);

	size_t channelCount = instrument->GetChannelCount();

	// Render ornaments for this scope: offline, trigger status, ...
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);
	if (scope)
	{
		if (scope->IsOffline())
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_offline_badge_color")), "OFFLINE", "OFFL", NULL);
		else
		{
			Oscilloscope::TriggerMode mode = state ? state->m_lastTriggerState : Oscilloscope::TRIGGER_MODE_STOP;
			if(!scope->IsTriggerArmed())
				mode = Oscilloscope::TRIGGER_MODE_STOP;
			switch (mode)
			{
			case Oscilloscope::TRIGGER_MODE_RUN:
				renderInstrumentBadge(instrument,true,BADGE_ARMED);
				break;
			case Oscilloscope::TRIGGER_MODE_STOP:
				renderInstrumentBadge(instrument,true,BADGE_STOPPED);
				break;
			case Oscilloscope::TRIGGER_MODE_TRIGGERED:
				renderInstrumentBadge(instrument,false,BADGE_TRIGGERED);
				break;
			case Oscilloscope::TRIGGER_MODE_WAIT:
				renderInstrumentBadge(instrument,true,BADGE_BUSY);
				break;
			case Oscilloscope::TRIGGER_MODE_AUTO:
				renderInstrumentBadge(instrument,false,BADGE_AUTO);
				break;
			default:
				break;
			}
		}
	}

	// Render ornaments for this PSU: on/off status, ...
	auto psu = dynamic_pointer_cast<SCPIPowerSupply>(instrument);
	if (psu)
	{
		//Get the state
		auto psustate = m_session.GetPSUState(psu);

		bool allOn = false;
		bool someOn = false;
		if(psu->SupportsMasterOutputSwitching())
			allOn = psustate->m_masterEnable;
		else
		{
			allOn = true;
			for(size_t i = 0 ; i < channelCount ; i++)
			{
				if(psustate->m_channelOn[i])
					someOn = true;
				else
					allOn = false;
			}
		}
		bool result;
		if(allOn || someOn)
		{
			result = true;
			renderToggle(
				"###psuon",
				true,
				allOn ?
				ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_on_badge_color")) :
				ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_partial_badge_color")), result);
		}
		else
		{
			result = false;
			renderOnOffToggle("###psuon", true, result);
		}
		if(result != allOn)
		{
			if(psu->SupportsMasterOutputSwitching())
				psu->SetMasterPowerEnable(result);
			else
			{
				for(size_t i = 0 ; i < channelCount ; i++)
				{
					psu->SetPowerChannelActive(i,result);
				}
			}
		}
	}

	if(instIsOpen)
	{
		vector<Oscilloscope::DigitalBank> digitalBanks;
		vector<size_t> analogChannels;
		vector<size_t> otherChannels;
		size_t lastEnabledChannelIndex = 0;
		if (scope)
		{
			if(ImGui::TreeNodeEx("Timebase", ImGuiTreeNodeFlags_DefaultOpen))
			{
				BeginBlock("timebase");
				if(scope->HasTimebaseControls())
					DoTimebaseSettings(scope);
				if(scope->HasFrequencyControls())
					DoFrequencySettings(scope);
				auto spec = dynamic_pointer_cast<SCPISpectrometer>(scope);
				if(spec)
					DoSpectrometerSettings(spec);
				EndBlock();
				ImGui::TreePop();
			}

			digitalBanks = scope->GetDigitalBanks();

			for(size_t i = 0; i<channelCount; i++)
			{
				if(scope->IsChannelEnabled(i))
					lastEnabledChannelIndex = i;
				auto scopechan = scope->GetChannel(i);
				auto streamType = scopechan->GetType(0);
				if(streamType != Stream::STREAM_TYPE_DIGITAL)
				{
					if(streamType ==  Stream::STREAM_TYPE_ANALOG)
						analogChannels.push_back(i);
					else
						otherChannels.push_back(i);
				}
			}
		}

		if(digitalBanks.size() > 0)
		{	// If digital banks are avaialble, gather digital channels in banks
			for(size_t i : analogChannels)
			{	// Iterate on analog channel first
				renderChannelNode(instrument,i,(i == lastEnabledChannelIndex));
			}
			int bankNumber = 1;
			for(auto bank : digitalBanks)
			{	// Iterate on digital banks
				string nodeName = "Digital Bank " + to_string(bankNumber);
				if(ImGui::TreeNodeEx(nodeName.c_str()))
				{
					ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
					for(auto channel : bank)
					{	// Iterate on bank's channel
						size_t i = channel->GetIndex();
						renderChannelNode(instrument,i,(i == lastEnabledChannelIndex));
					}
					ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
					ImGui::TreePop();
				}
				bankNumber++;
			}
			for(size_t i : otherChannels)
			{	// Finally iterate on other channels
				renderChannelNode(instrument,i,(i == lastEnabledChannelIndex));
			}
		}
		else
		{	// Display all channels if no digital bank is available
			for(size_t i=0; i<channelCount; i++)
			{	// Iterate on each channel
				renderChannelNode(instrument,i,(i == lastEnabledChannelIndex));
			}
		}

		ImGui::TreePop();
	}
	ImGui::PopID();

}

shared_ptr<StreamBrowserTimebaseInfo> StreamBrowserDialog::GetTimebaseInfoFor(shared_ptr<Oscilloscope>& scope)
{
	//If no timebase info, create it
	if(m_timebaseConfig.find(scope) == m_timebaseConfig.end())
	{
		LogTrace("Creating initial timebase info\n");
		m_timebaseConfig[scope] = make_shared<StreamBrowserTimebaseInfo>(scope);
	}

	//If we had info, but it's clearly out of date, recreate it
	else if(m_timebaseConfig[scope]->GetRate() != scope->GetSampleRate())
	{
		LogTrace("Recreating timebase info (out of date sample rate)\n");
		m_timebaseConfig[scope] = make_shared<StreamBrowserTimebaseInfo>(scope);
	}

	//Use whatever is left
	return m_timebaseConfig[scope];
}

void StreamBrowserDialog::DoFrequencySettings(shared_ptr<Oscilloscope> scope)
{
	auto p = GetTimebaseInfoFor(scope);

	//Memory depth (but don't duplicate if we also have time domain controls, like for a SDR/RTSA)
	auto width = ImGui::GetFontSize() * 6;
	if(!scope->HasTimebaseControls())
	{
		ImGui::SetNextItemWidth(width);
		if(Combo("Points", p->m_depthNames, p->m_depth))
			scope->SetSampleDepth(p->m_depths[p->m_depth]);
		HelpMarker("Number of points in the sweep");
	}

	Unit hz(Unit::UNIT_HZ);

	// Resolution Bandwidh
	if(renderEditableProperty(width,"Rbw", p->m_rbwText, p->m_rbw, hz, "Resolution Bandwidth"))
	{
		scope->SetResolutionBandwidth(p->m_rbw);
		// Update with values from the device
		p->m_rbw = scope->GetResolutionBandwidth();
		p->m_rbwText = hz.PrettyPrintInt64(p->m_rbw);
	}

	//Frequency
	bool changed = false;

	if(renderEditableProperty(width,"Start", p->m_startText, p->m_start, hz, "Start of the frequency sweep"))
	{
		double mid = (p->m_start + p->m_end) / 2;
		double span = (p->m_end - p->m_start);
		scope->SetCenterFrequency(0, mid);
		scope->SetSpan(span);
		changed = true;
	}

	if(renderEditableProperty(width,"Center", p->m_centerText, p->m_center, hz, "Midpoint of the frequency sweep"))
	{
		scope->SetCenterFrequency(0, p->m_center);
		changed = true;
	}

	if(renderEditableProperty(width,"Span", p->m_spanText, p->m_span, hz, "Width of the frequency sweep"))
	{
		scope->SetSpan(p->m_span);
		changed = true;
	}

	if(renderEditableProperty(width,"End", p->m_endText, p->m_end, hz, "End of the frequency sweep"))
	{
		double mid = (p->m_start + p->m_end) / 2;
		double span = (p->m_end - p->m_start);
		scope->SetCenterFrequency(0, mid);
		scope->SetSpan(span);
		changed = true;
	}

	//Update everything if one setting is changed
	if(changed)
	{
		p->m_span = scope->GetSpan();
		p->m_center = scope->GetCenterFrequency(0);
		p->m_start = p->m_center - p->m_span/2;
		p->m_end = p->m_center + p->m_span/2;

		p->m_spanText = hz.PrettyPrint(p->m_span);
		p->m_centerText = hz.PrettyPrint(p->m_center);
		p->m_startText = hz.PrettyPrint(p->m_start);
		p->m_endText = hz.PrettyPrint(p->m_end);
	}
}
void StreamBrowserDialog::DoSpectrometerSettings(shared_ptr<SCPISpectrometer> spec)
{
	auto scope = dynamic_pointer_cast<SCPIOscilloscope>(spec);
	if(m_timebaseConfig.find(scope) == m_timebaseConfig.end())
		m_timebaseConfig[scope] = make_shared<StreamBrowserTimebaseInfo>(scope);
	auto config = m_timebaseConfig[scope];

	auto width = ImGui::GetFontSize() * 5;

	Unit fs(Unit::UNIT_FS);
	if(renderEditableProperty(width, "Integration time", config->m_integrationText, config->m_integrationTime, fs, "Spectrometer integration / exposure time"))
		spec->SetIntegrationTime(config->m_integrationTime);
}

/**
	@brief Add nodes for timebase controls under an instrument
 */
void StreamBrowserDialog::DoTimebaseSettings(shared_ptr<Oscilloscope> scope)
{
	auto width = ImGui::GetFontSize() * 7;

	//If we don't have timebase settings for the scope, create them
	auto config = GetTimebaseInfoFor(scope);

	//Interleaving
	bool refresh = false;
	if(scope->HasInterleavingControls())
	{
		ImGui::SetNextItemWidth(width);
		bool disabled = !scope->CanInterleave();
		ImGui::BeginDisabled(disabled);
		if(renderOnOffToggle("Interleaving", false, config->m_interleaving))
		{
			scope->SetInterleaving(config->m_interleaving);
			refresh = true;
		}
		ImGui::EndDisabled();
		HelpMarker(
			"Combine ADCs from multiple channels to get higher sampling rate on a subset of channels.\n"
			"\n"
			"Some instruments do not have an explicit interleaving switch, but available sample rates "
			"may vary depending on which channels are active."
			);
	}

	//Show sampling mode iff both are available
	if(
		scope->IsSamplingModeAvailable(Oscilloscope::REAL_TIME) &&
		scope->IsSamplingModeAvailable(Oscilloscope::EQUIVALENT_TIME) )
	{
		static const char* items[]=
		{
			"Real time",
			"Equivalent time"
		};
		ImGui::SetNextItemWidth(width);
		if(ImGui::Combo("Sampling mode", &config->m_samplingMode, items, 2))
		{
			if(config->m_samplingMode == Oscilloscope::REAL_TIME)
				scope->SetSamplingMode(Oscilloscope::REAL_TIME);
			else
				scope->SetSamplingMode(Oscilloscope::EQUIVALENT_TIME);

			refresh = true;
		}

		HelpMarker(
			"Switch the acquisition system between real time (continuous capture of consecutive samples\n"
			"and equivalent time (undersampling with a narrow sample-and-hold to build up a high resolution\n"
			"view of a repetitive signal over many acquisitions)"
			);
	}

	//Sample rate
	ImGui::SetNextItemWidth(width);
	if(renderCombo(
		"Sample Rate",
		false,
		ImGui::GetStyleColorVec4(ImGuiCol_FrameBg),
		config->m_rate,
		config->m_rateNames,
		false,
		0,
		false))
	{
		scope->SetSampleRate(config->m_rates[config->m_rate]);
		refresh = true;
	}
	HelpMarker(
		"Adjust the ADC sampling rate.\n\n"
		"Note that with some instruments, the set of available sampling rates varies depending on which channels are active."
		);

	//Memory depth
	ImGui::SetNextItemWidth(width);
	if(renderCombo(
		"Memory Depth",
		false,
		ImGui::GetStyleColorVec4(ImGuiCol_FrameBg),
		config->m_depth,
		config->m_depthNames,
		false,
		0,
		false))
	{
		scope->SetSampleDepth(config->m_depths[config->m_depth]);
		refresh = true;
	}
	HelpMarker(
		"Adjust the number of samples captured each trigger event.\n\n"
		"Note that with some instruments, the maximum memory depth varies depending on which channels are active."
		);

	//Global ADC mode switch
	if(scope->IsADCModeConfigurable() && !scope->IsADCModePerChannel())
	{
		bool nomodes = config->m_adcmodeNames.size() <= 1;
		if(nomodes)
			ImGui::BeginDisabled();
		ImGui::SetNextItemWidth(width);
		if(renderCombo(
			"ADC mode",
			false,
			ImGui::GetStyleColorVec4(ImGuiCol_FrameBg),
			config->m_adcmode,
			config->m_adcmodeNames,
			false,
			0,
			false))
		{
			scope->SetADCMode(0, config->m_adcmode);
			refresh = true;
		}
		if(nomodes)
			ImGui::EndDisabled();

		HelpMarker(
			"Operating mode for the analog-to-digital converter.\n\n"
			"Some instruments allow the ADC to operate in several modes, typically trading bit depth "
			"against sample rate. Available modes may vary depending on the current sample rate and "
			"which channels are in use."
			);
	}

	if(refresh)
	{
		m_timebaseConfig[scope] = make_shared<StreamBrowserTimebaseInfo>(scope);
		LogTrace("Refreshing timebase config for %s\n", scope->m_nickname.c_str());
	}
}

/**
   @brief Rendering of a channel node

   @param instrument the instrument containing the instrument to render
   @param channelIndex the index of the channel to render
   @param isLast true if this is the last channel of the instrument
 */
void StreamBrowserDialog::renderChannelNode(shared_ptr<Instrument> instrument, size_t channelIndex, bool isLast)
{
	// Get preferences for colors
	auto& prefs = m_session.GetPreferences();

	InstrumentChannel* channel = instrument->GetChannel(channelIndex);

	ImGui::PushID(channelIndex);

	auto psu = std::dynamic_pointer_cast<SCPIPowerSupply>(instrument);
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);
	auto awg = std::dynamic_pointer_cast<FunctionGenerator>(instrument);
	auto dmm = std::dynamic_pointer_cast<Multimeter>(instrument);

	bool singleStream = channel->GetStreamCount() == 1;
	auto scopechan = dynamic_cast<OscilloscopeChannel *>(channel);
	auto psuchan = dynamic_cast<PowerSupplyChannel *>(channel);
	auto awgchan = dynamic_cast<FunctionGeneratorChannel *>(channel);
	auto dmmchan = dynamic_cast<MultimeterChannel *>(channel);
	bool renderProps = false;
	if (scopechan)
	{
		renderProps = scopechan->IsEnabled();
	}
	else if(awg && awgchan)
	{
		renderProps = m_session.GetFunctionGeneratorState(awg)->m_channelActive[channelIndex];
	}
	else if(dmm && dmmchan)
	{
		renderProps = m_session.GetDmmState(dmm)->m_started;
	}

	bool hasChildren = !singleStream || renderProps;

	if (channel->m_displaycolor != "")
		ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(channel->m_displaycolor));

	int flags = 0;
	if(!hasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf;

	flags |= ImGuiTreeNodeFlags_OpenOnArrow;

	bool open = ImGui::TreeNodeEx(
		channel->GetDisplayName().c_str(),
		flags);
	if (channel->m_displaycolor != "")
		ImGui::PopStyleColor();

	//Open properties dialog on double click
	if(ImGui::IsItemHovered())
	{
		if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if(scopechan)
				m_parent->ShowChannelProperties(scopechan);
			else if(psuchan)
				m_parent->ShowInstrumentProperties(psu);
			else
				LogWarning("Don't know how to open channel properties yet\n");
		}

		m_parent->AddStatusHelp("mouse_lmb_double", "Open properties");
	}

	//Single stream: drag the stream not the channel
	if(singleStream)
	{
		StreamDescriptor s(channel, 0);
		if(ImGui::BeginDragDropSource())
		{
			if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
				ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
			else
				ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

			ImGui::TextUnformatted(s.GetName().c_str());
			ImGui::EndDragDropSource();
		}
		else
			DoItemHelp();
	}

	//Drag source for the channel itself (if we have zero or >1 streams)
	else if(ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("Channel", &channel, sizeof(channel));

		ImGui::TextUnformatted(channel->GetDisplayName().c_str());
		ImGui::EndDragDropSource();
	}

	// Channel decoration
	startBadgeLine();
	if (scopechan)
	{
		//"trigger" badge on trigger inputs to show they're not displayable channels
		if(scopechan->GetType(0) == Stream::STREAM_TYPE_TRIGGER)
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_disabled_badge_color")), "TRIG ONLY", "TRIG","--", nullptr);

		// Scope channel
		else if (!scopechan->IsEnabled())
			renderBadge(ImGui::ColorConvertU32ToFloat4(prefs.GetColor("Appearance.Stream Browser.instrument_disabled_badge_color")), "DISABLED", "DISA","--", nullptr);

		//Download in progress
		else
			renderDownloadProgress(instrument, channel, isLast);
	}
	else if(psu)
	{
		// PSU Channel
		//Get the state
		auto psustate = m_session.GetPSUState(psu);

		bool active = psustate->m_channelOn[channelIndex];
		bool result = active;
		renderOnOffToggle("###active", true, result);
		if(result != active)
			psu->SetPowerChannelActive(channelIndex,result);
	}
	else if(awg && awgchan)
	{
		// AWG Channel : get the state
		auto awgstate = m_session.GetFunctionGeneratorState(awg);
		if(renderOnOffToggle("###active", true,awgstate->m_channelActive[channelIndex]))
		{
			awg->SetFunctionChannelActive(channelIndex,awgstate->m_channelActive[channelIndex]);
			// Tell intrument thread that the FunctionGenerator state has to be updated
			awgstate->m_needsUpdate[channelIndex] = true;
		}
	}
	else if(dmm && dmmchan)
	{
		// DMM Channel : get the state
		auto dmmstate = m_session.GetDmmState(dmm);

		if(renderOnOffToggle("###active", true, dmmstate->m_started))
		{
			if(dmmstate->m_started)
				dmm->StartMeter();
			else
				dmm->StopMeter();
			// Tell intrument thread that the Dmm state has to be updated
			dmmstate->m_needsUpdate = true;
		}
	}

	if(open)
	{
		ImGui::PushID(instrument.get());
		if(psu)
		{
			// For PSU we will have a special handling for the 4 streams associated to a PSU channel
			if(BeginBlock("psu_params",true,"Open PSU channel properties"))
			{
				m_parent->ShowInstrumentProperties(psu);
			}

			auto svoltage_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(psuchan->GetVoltageSetPoint ());
			auto mvoltage_txt = Unit(Unit::UNIT_VOLTS).PrettyPrint(psuchan->GetVoltageMeasured());
			auto scurrent_txt = Unit(Unit::UNIT_AMPS).PrettyPrint(psuchan->GetCurrentSetPoint ());
			auto mcurrent_txt = Unit(Unit::UNIT_AMPS).PrettyPrint(psuchan->GetCurrentMeasured ());

			bool cc = false;
			auto psuState = m_session.GetPSUState(psu);
			if(psuState)
			{
				cc = psuState->m_channelConstantCurrent[channelIndex].load();

				bool clicked = false;
				bool hovered = false;

				if (ImGui::BeginTable("table1", 3))
				{
					// Voltage
					if(renderPsuRows(true,cc,psuchan,psuState->m_setVoltage[channelIndex],psuState->m_committedSetVoltage[channelIndex],mvoltage_txt,clicked,hovered))
					{	// Update set voltage
						psu->SetPowerVoltage(channelIndex, psuState->m_committedSetVoltage[channelIndex]);
						psuState->m_needsUpdate[channelIndex] = true;
					}
					// Current
					if(renderPsuRows(false,cc,psuchan,psuState->m_setCurrent[channelIndex],psuState->m_committedSetCurrent[channelIndex],mcurrent_txt,clicked,hovered))
					{	// Update set current 
						psu->SetPowerCurrent(channelIndex, psuState->m_committedSetCurrent[channelIndex]);
						psuState->m_needsUpdate[channelIndex] = true;
					}
					// End table
					ImGui::EndTable();
				}
			}
			EndBlock();
		}
		else if(awg && awgchan)
		{
			BeginBlock("awgparams");
			renderAwgProperties(awg, awgchan);
			EndBlock();
		}
		else if(dmm && dmmchan)
		{
			BeginBlock("dmm_params");
			// Always 2 streams for dmm channel => render properties on channel node
			bool clicked = false;
			bool hovered = false;
			// Main measurement
			renderDmmProperties(dmm,dmmchan,true,clicked,hovered);
			// Secondary measurement
			renderDmmProperties(dmm,dmmchan,false,clicked,hovered);

			EndBlock();
		}
		else
		{
			if(!singleStream)
			{
				auto scopeState = m_session.GetOscilloscopeState(scope);
				if(scopeState)
				{
					if(BeginBlock("stream_params",true,"Open channel properties"))
					{
						m_parent->ShowChannelProperties(scopechan);
					}
					renderChannelProperties(scope,scopechan,channelIndex,scopeState);
				EndBlock();
				}
			}
			size_t streamCount = channel->GetStreamCount();
			for(size_t j=0; j<streamCount; j++)
			{
				// Iterate on each stream
				renderStreamNode(instrument,channel,j,!singleStream,renderProps);
			}
		}
		ImGui::PopID();
	}

	if(open)
		ImGui::TreePop();

	ImGui::PopID();
}

/**
   @brief Rendering of channel properties

   @param scope the scope
   @param scopechan the scope channel
   @param channelIndex the index of the channel
   @param scopeState the OscilloscopeState
 */
void StreamBrowserDialog::renderChannelProperties(std::shared_ptr<Oscilloscope> scope, OscilloscopeChannel* scopechan, size_t channelIndex, shared_ptr<OscilloscopeState> scopeState)
{
	float fontSize = ImGui::GetFontSize();
	float width = 6*fontSize;

	Unit counts(Unit::UNIT_COUNTS);
	if(renderEditableProperty(width,"Attenuation",scopeState->m_strAttenuation[channelIndex],scopeState->m_committedAttenuation[channelIndex],counts,
	"Attenuation setting for the probe (for example, 10 for a 10:1 probe)"))
	{	// Update offset
		scopechan->SetAttenuation(scopeState->m_committedAttenuation[channelIndex]);
		scopeState->m_needsUpdate[channelIndex] = true;
	}
	//Only show coupling box if the instrument has configurable coupling
	if( (scopeState->m_couplings[channelIndex].size() > 1) && (scopeState->m_probeName[channelIndex] == "") )
	{
		ImGui::SetNextItemWidth(width);
		if(renderCombo(
			"Coupling",
			false,
			ImGui::GetStyleColorVec4(ImGuiCol_FrameBg),
			scopeState->m_channelCoupling[channelIndex],
			scopeState->m_couplingNames[channelIndex],
			false,
			0,
			false))
		{
			scope->SetChannelCoupling(channelIndex,scopeState->m_couplings[channelIndex][scopeState->m_channelCoupling[channelIndex]]);
			scopeState->m_needsUpdate[channelIndex] = true;
		}
		HelpMarker("Coupling configuration for the input");
	}
	//Bandwidth limiters (only show if more than one value available)
	if(scopeState->m_bandwidthLimitNames[channelIndex].size() > 1)
	{
		ImGui::SetNextItemWidth(width);
		if(renderCombo(
			"Bandwidth",
			false,
			ImGui::GetStyleColorVec4(ImGuiCol_FrameBg),
			scopeState->m_channelBandwidthLimit[channelIndex],
			scopeState->m_bandwidthLimitNames[channelIndex],
			false,
			0,
			false))
		{
			scopechan->SetBandwidthLimit(scopeState->m_bandwidthLimits[channelIndex][scopeState->m_channelBandwidthLimit[channelIndex]]);
			scopeState->m_needsUpdate[channelIndex] = true;
		}
		HelpMarker("Hardware bandwidth limiter setting");
	}
	//If the probe supports inversion, show a checkbox for it
	if(scope->CanInvert(channelIndex))
	{
		ImGui::SetNextItemWidth(width);
		if(renderOnOffToggle("Invert",false,scopeState->m_channelInverted[channelIndex]))
		{
			scope->Invert(channelIndex,scopeState->m_channelInverted[channelIndex]);
			scopeState->m_needsUpdate[channelIndex] = true;
		}
		HelpMarker(
			"When ON, input value is multiplied by -1.\n"
			"For a differential probe, this is equivalent to swapping the positive and negative inputs."
			);
	}

}

/**
   @brief Rendering of a stream node

   @param instrument the instrument containing the stream to render (may be null if the stream is in a Filter)
   @param channel the channel or the Filter containing the stream to render
   @param streamIndex the index of the stream to render
   @param renderName true if the name of the stream should be rendred as a selectable item
   @param renderProps true if a properties block should be rendered for this stream
 */
void StreamBrowserDialog::renderStreamNode(shared_ptr<Instrument> instrument, InstrumentChannel* channel, size_t streamIndex, bool renderName, bool renderProps)
{
	auto scope = std::dynamic_pointer_cast<Oscilloscope>(instrument);
	auto scopechan = dynamic_cast<OscilloscopeChannel *>(channel);
	Stream::StreamType type = scopechan ? scopechan->GetType(streamIndex) : Stream::StreamType::STREAM_TYPE_ANALOG;

	ImGui::PushID(streamIndex);

	// Stream name
	if (renderName)
	{
		ImGui::Selectable(channel->GetStreamName(streamIndex).c_str());
		StreamDescriptor s(channel, streamIndex);
		if(ImGui::BeginDragDropSource())
		{
			if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
				ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
			else
				ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

			ImGui::TextUnformatted(s.GetName().c_str());
			ImGui::EndDragDropSource();
		}
		else
		{
			if(channel->GetType(0) == Stream::STREAM_TYPE_TRIGGER)
			{}
			else
				DoItemHelp();
		}
	}
	// Channel/stream properties block
	if(renderProps && scopechan)
	{
		// If no properties are available for this stream, only show a "Properties" link if it is the last stream of the channel/filter
		bool hasProps = false;
		switch (type)
		{
			case Stream::STREAM_TYPE_ANALOG:
				hasProps = true;
				break;
			case Stream::STREAM_TYPE_DIGITAL:
				if(scope)
				{
					hasProps = true;
				}
				break;
			default:
				break;
		}
		if(hasProps)
		{
			auto scopeState = m_session.GetOscilloscopeState(scope);
			if(scopeState)
			{ 	// For now, only show properties for scope channel / streams
				if(BeginBlock("stream_params",true,"Open channel properties"))
				{
					m_parent->ShowChannelProperties(scopechan);
				}

				Unit unit = channel->GetYAxisUnits(streamIndex);
				size_t channelIndex = scopechan->GetIndex();

				switch (type)
				{
					case Stream::STREAM_TYPE_ANALOG:
						{
							if(!renderName)
							{	// No streams => display channel properties here
								renderChannelProperties(scope,scopechan,channelIndex,scopeState);
							}
							if(renderEditablePropertyWithExplicitApply(0,"Offset",scopeState->m_strOffset[channelIndex][streamIndex],scopeState->m_committedOffset[channelIndex][streamIndex],unit))
							{	// Update offset
								scopechan->SetOffset(scopeState->m_committedOffset[channelIndex][streamIndex],streamIndex);
								scopeState->m_needsUpdate[channelIndex] = true;
							}
							if(renderEditablePropertyWithExplicitApply(0,"Vertical range",scopeState->m_strRange[channelIndex][streamIndex],scopeState->m_committedRange[channelIndex][streamIndex],unit))
							{	// Update offset
								scopechan->SetVoltageRange(scopeState->m_committedRange[channelIndex][streamIndex],streamIndex);
								scopeState->m_needsUpdate[channelIndex] = true;
							}
						}
						break;
					case Stream::STREAM_TYPE_DIGITAL:
						if(scope)
						{
							if(scope->IsDigitalThresholdConfigurable())
							{
								if(renderEditablePropertyWithExplicitApply(0,"Threshold",scopeState->m_strDigitalThreshold[channelIndex],scopeState->m_committedDigitalThreshold[channelIndex],unit))
								{	// Update offset
									scopechan->SetDigitalThreshold(scopeState->m_committedDigitalThreshold[channelIndex]);
									scopeState->m_needsUpdate[channelIndex] = true;
								}
							}
							else
							{
								auto threshold_txt = unit.PrettyPrint(scope->GetDigitalThreshold(scopechan->GetIndex()));
								renderReadOnlyProperty(0,"Threshold", threshold_txt);
							}
							break;
						}
						//fall through
					default:
						break;
				}
				EndBlock();
			}
		}
	}
	ImGui::PopID();
}

/**
   @brief Rendering of a Filter node

   @param filter the filter to render
 */
void StreamBrowserDialog::renderFilterNode(Filter* filter)
{
	ImGui::PushID(filter);

	bool singleStream = filter->GetStreamCount() == 1;

	if (filter->m_displaycolor != "")
		ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(filter->m_displaycolor));

	//Don't expand filters with a single stream by default
	int flags = ImGuiTreeNodeFlags_OpenOnArrow;
	if(!singleStream)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	bool open = ImGui::TreeNodeEx(filter->GetDisplayName().c_str(), flags);
	if (filter->m_displaycolor != "")
		ImGui::PopStyleColor();

	//Open properties dialog on double click
	if(ImGui::IsItemHovered())
	{
		if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			m_parent->ShowChannelProperties(filter);
		m_parent->AddStatusHelp("mouse_lmb_double", "Open properties");
	}

	//Single stream: drag the stream not the filter
	if(singleStream)
	{
		StreamDescriptor s(filter, 0);
		if(ImGui::BeginDragDropSource())
		{
			if(s.GetType() == Stream::STREAM_TYPE_ANALOG_SCALAR)
				ImGui::SetDragDropPayload("Scalar", &s, sizeof(s));
			else
				ImGui::SetDragDropPayload("Stream", &s, sizeof(s));

			ImGui::TextUnformatted(s.GetName().c_str());
			ImGui::EndDragDropSource();
		}
		else
			DoItemHelp();
	}

	//Drag source for the channel itself (if we have zero or >1 streams)
	else if(ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("Channel", &filter, sizeof(filter));

		ImGui::TextUnformatted(filter->GetDisplayName().c_str());
		ImGui::EndDragDropSource();
	}

	// Channel decoration
	if(open)
	{
		ImGui::PushID(filter);

		size_t streamCount = filter->GetStreamCount();
		for(size_t j=0; j<streamCount; j++)
			renderStreamNode(nullptr,filter,j,!singleStream,true);

		ImGui::PopID();
	}

	if(open)
		ImGui::TreePop();

	ImGui::PopID();
}

void StreamBrowserDialog::FlushConfigCache()
{
	m_timebaseConfig.clear();
}

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool StreamBrowserDialog::DoRender()
{
	//Add all instruments
	auto insts = m_session.GetInstruments();
	for(auto inst : insts)
	{
		renderInstrumentNode(inst);
	}

	//Add all filters
	if(ImGui::TreeNodeEx("Filters", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto filters = Filter::GetAllInstances();
		for(auto f : filters)
		{
			renderFilterNode(f);
		}
		ImGui::TreePop();
	}

	return true;
}

void StreamBrowserDialog::DoItemHelp()
{
	if(ImGui::IsItemHovered())
		m_parent->AddStatusHelp("mouse_lmb_drag", "Add to filter graph or plot");
}

bool StreamBrowserDialog::BeginBlock(const char* label, bool withButton, const char* tooltip)
{
	bool clicked = false;
	auto& prefs = m_session.GetPreferences();
	ImGuiWindowFlags flags = ImGuiChildFlags_AutoResizeY;
	bool withBorders = false;
	if(prefs.GetBool("Appearance.Stream Browser.show_block_border"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
		flags |= ImGuiChildFlags_Borders;
		withBorders = true;
	}
	ImGui::BeginChild(label, ImVec2(0, 0), flags);
	if(withButton)
	{	// Create a "+" button on the top right corner of the box
		ImVec2 oldPos = ImGui::GetCursorPos();
		float padding = ImGui::GetStyle().FramePadding.x;
		float shift = withBorders ? padding*1.5 : 0;
		float xsz = ImGui::GetFontSize();
		ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - xsz + shift);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY()-shift);
		// Use the same color as border for the button
		ImVec4 border = ImGui::GetStyle().Colors[ImGuiCol_Border];
		ImVec4 hover = ImVec4(border.x * 1.2f, border.y * 1.2f, border.z * 1.2f, border.w);
		ImVec4 active = ImVec4(border.x * 0.9f, border.y * 0.9f, border.z * 0.9f, border.w);
		ImGui::PushStyleColor(ImGuiCol_Button, border);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
		clicked = ImGui::Button(PLUS_CHAR,ImVec2(xsz, xsz));
		if(ImGui::IsItemHovered())
		{	// Hand cursor
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			if(tooltip)
			{
				m_parent->AddStatusHelp("mouse_lmb", tooltip);
			}
		}
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(3);
		ImGui::SetCursorPos(oldPos);
	}
	return clicked;
}

void StreamBrowserDialog::EndBlock()
{
	ImGui::EndChild();
	auto& prefs = m_session.GetPreferences();
	if(prefs.GetBool("Appearance.Stream Browser.show_block_border"))
	{
		ImGui::PopStyleVar();
	}
}