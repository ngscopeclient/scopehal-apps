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
	@brief Declaration of Dialog
 */
#ifndef Dialog_h
#define Dialog_h

#include "imgui_stdlib.h"

/**
	@brief Generic dialog box or other popup window
 */
class Dialog
{
public:
	Dialog(const std::string& title, const std::string& id, ImVec2 defaultSize = ImVec2(300, 100) );
	virtual ~Dialog();

	virtual bool Render();
	void RenderAsChild();
	virtual bool DoRender() =0;

	const std::string& GetID()
	{ return m_id; }

	std::string GetTitleAndID()
	{ return m_title + "###" + m_id; }

	//TODO: this might be better off as a global method?
	static bool Combo(const std::string& label, const std::vector<std::string>& items, int& selection);
	static bool UnitInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		float& committedValue,
		Unit unit);
	static bool UnitInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		double& committedValue,
		Unit unit);
	static bool UnitInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		int64_t& committedValue,
		Unit unit);
	static bool TextInputWithImplicitApply(
		const std::string& label,
		std::string& currentValue,
		std::string& committedValue);

protected:
	bool FloatInputWithApplyButton(const std::string& label, float& currentValue, float& committedValue);
	bool TextInputWithApplyButton(const std::string& label, std::string& currentValue, std::string& committedValue);
	bool IntInputWithImplicitApply(const std::string& label, int& currentValue, int& committedValue);
	bool UnitInputWithExplicitApply(
		const std::string& label,
		std::string& currentValue,
		float& committedValue,
		Unit unit);
public:
	static void Tooltip(const std::string& str, bool allowDisabled = false);
	static void HelpMarker(const std::string& str);
	static void HelpMarker(const std::string& header, const std::vector<std::string>& bullets);

protected:
	void RenderErrorPopup();
	void ShowErrorPopup(const std::string& title, const std::string& msg);

	void Render7SegmentDigit(ImDrawList* drawList, uint8_t digit, ImVec2 size, ImVec2 position, float thikness, ImU32 colorOn, ImU32 colorOff);
	void Render7SegmentValue(const std::string& value, ImVec4 color, float digitHeight);
	void Render7SegmentValue(const std::string& value, ImVec4 color, float digitHeight, bool &clicked, bool &hovered, bool clickable = true);


	bool m_open;
	std::string m_id;
	std::string m_title;
	ImVec2 m_defaultSize;

	std::string m_errorPopupTitle;
	std::string m_errorPopupMessage;
};

#endif
