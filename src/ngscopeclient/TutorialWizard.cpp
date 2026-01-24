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
	@brief Implementation of TutorialWizard
 */

#include "ngscopeclient.h"
#include "TutorialWizard.h"
#include "Session.h"
#include <imgui_markdown.h>
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TutorialWizard::TutorialWizard(Session* session, MainWindow* parent)
	: Dialog("Tutorial", "Tutorial", ImVec2(650, 350), session, parent)
	, m_step(TUTORIAL_00_INTRO)
	, m_continueEnabled(true)
{
	m_markdownText.push_back(ReadDataFile("md/tutorial_00_intro.md"));
	m_markdownText.push_back(ReadDataFile("md/tutorial_01_addinstrument.md"));
	m_markdownText.push_back(ReadDataFile("md/tutorial_02_connect.md"));
	m_markdownText.push_back(ReadDataFile("md/tutorial_03_acquire.md"));
	m_markdownText.push_back(ReadDataFile("md/tutorial_04_scrollzoom.md"));

	m_markdownText.push_back(ReadDataFile("md/tutorial_99_final.md"));

	//DEBUG: autiomatically jump ahead a bit
	m_step = TUTORIAL_03_ACQUIRE;
}

TutorialWizard::~TutorialWizard()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Draw the dialog, making sure it spawns in a sane place
 */
bool TutorialWizard::Render()
{
	auto wpos = ImGui::GetWindowPos();
	auto wsize = ImGui::GetWindowSize();

	ImVec2 center(wpos.x + wsize.x / 2, wpos.y + wsize.y/2);
	ImVec2 pos(center.x + 50, center.y - 50);

	ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing);

	return Dialog::Render();
}

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool TutorialWizard::DoRender()
{
	auto mdConfig = m_parent->GetMarkdownConfig();

	auto& mdText = m_markdownText[m_step];
	ImGui::Markdown(mdText.c_str(), mdText.length(), mdConfig );

	ImGui::Separator();

	//move near the right edge
	ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x * 0.8);

	//Show back button unless at first step
	//TODO: is this necessary? will it break things? For now turn it off
	/*
	ImGui::BeginDisabled(m_step == TUTORIAL_00_INTRO);
		if(ImGui::Button("<< Back"))
			m_step --;
	ImGui::EndDisabled();
	ImGui::SameLine();
	*/

	//Show forward button
	//If last step, close dialog when pressed
	auto buttonStartPos = ImGui::GetCursorScreenPos();
	if(m_step == (m_markdownText.size()-1) )
	{
		if(ImGui::Button("Finish"))
			return false;
	}
	else
	{
		ImGui::BeginDisabled(!m_continueEnabled);
			if(ImGui::Button("Continue >>"))
				AdvanceToNextStep();
		ImGui::EndDisabled();
	}

	//Show hints on continue button for some steps
	if(m_continueEnabled)
	{
		ImVec2 anchorPos(
			buttonStartPos.x + 2*ImGui::GetFontSize(),
			buttonStartPos.y + 2*ImGui::GetFontSize());
		switch(m_step)
		{
			case TUTORIAL_00_INTRO:
				DrawSpeechBubble(anchorPos, ImGuiDir_Up, "Begin the tutorial");
				break;

			case TUTORIAL_04_SCROLLZOOM:
				DrawSpeechBubble(anchorPos, ImGuiDir_Up, "Continue when you are comfortable zooming the plot");
				break;

			//show nothing otherwise
			default:
				break;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers for rendering speech bubbles

void TutorialWizard::DrawSpeechBubble(
		ImVec2 anchorPos,
		[[maybe_unused]] ImGuiDir dirTip,
		string str)
{
	auto& prefs = m_session->GetPreferences();
	auto outlineColor = prefs.GetColor("Appearance.Help.bubble_outline_color");
	auto fillColor = prefs.GetColor("Appearance.Help.bubble_fill_color");
	auto list = ImGui::GetForegroundDrawList();

	auto textsize = ImGui::CalcTextSize(str.c_str(), nullptr);
	auto size = ImGui::GetFontSize();

	//Default is for the arrow to be 1/4 of the way across
	auto leftOverhang = textsize.x / 4;
	float radius = 0.5 * size;

	//Update overhang if the button would go off the start of the window
	//For now, we only care about the left side
	auto viewport = ImGui::GetWindowViewport();
	auto wpos = viewport->Pos;
	float farleft = anchorPos.x - (leftOverhang + radius);
	float offLeftEdge = farleft - wpos.x;
	if(offLeftEdge < 0)
	{
		//for now just clamp the anchor
		//but this breaks things we can't make it too small
		//anchorPos.x += fabs(offLeftEdge) + size;
		leftOverhang = 2*size;

		//Make the text a bit wider and recalculate the size
		//Quick hack, but it keeps it visible!
		str = "   " + str;
		textsize = ImGui::CalcTextSize(str.c_str(), nullptr);
	}

	//Anchor position is the tip of the speech bubble
	float tailLength = size;
	ImVec2 textPos(
		anchorPos.x - leftOverhang,
		anchorPos.y + tailLength + radius);

	//Fill
	MakePathSpeechBubble(list, dirTip, anchorPos, textsize, tailLength, radius, leftOverhang);
	list->PathFillConcave(fillColor);

	//Outline
	MakePathSpeechBubble(list, dirTip, anchorPos, textsize, tailLength, radius, leftOverhang);
	list->PathStroke(outlineColor, 0, 0.25 * size);

	//Text
	auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
	list->AddText(textPos, textColor, str.c_str());
}

void TutorialWizard::MakePathSpeechBubble(
	ImDrawList* list,
	ImGuiDir dirTip,
	ImVec2 anchorPos,
	ImVec2 textsize,
	float tailLength,
	float radius,
	float leftOverhang)
{
	auto size = ImGui::GetFontSize();
	auto tailWidth = size;
	auto rightOverhang = textsize.x - leftOverhang;

	//ImGui wants clockwise winding. Starting from the tip of the speech bubble go down, then across
	//Angles are measured clockwise from the 3 o'clock position
	ImVec2 tailCorner(anchorPos.x, anchorPos.y + tailLength);
	ImVec2 topRight(tailCorner.x + rightOverhang, tailCorner.y);
	ImVec2 topRightRadiusCenter(topRight.x, topRight.y + radius);
	ImVec2 rightTop(topRightRadiusCenter.x + radius, topRightRadiusCenter.y);
	ImVec2 rightBottom(rightTop.x, rightTop.y + textsize.y);
	ImVec2 bottomRightRadiusCenter(topRightRadiusCenter.x, rightBottom.y);
	ImVec2 bottomRight(bottomRightRadiusCenter.x, bottomRightRadiusCenter.y + radius);
	ImVec2 bottomLeft(anchorPos.x - leftOverhang, bottomRight.y);
	ImVec2 bottomLeftRadiusCenter(bottomLeft.x, bottomRightRadiusCenter.y);
	ImVec2 leftBottom(bottomLeftRadiusCenter.x - radius, rightBottom.y);
	ImVec2 leftTop(leftBottom.x, rightTop.y);
	ImVec2 topLeftRadiusCenter(bottomLeftRadiusCenter.x, topRightRadiusCenter.y);
	ImVec2 topLeft(topLeftRadiusCenter.x, topRight.y);
	ImVec2 leftTailCorner(anchorPos.x - tailWidth, topRight.y);

	list->PathLineTo(anchorPos);
	list->PathLineTo(tailCorner);
	list->PathLineTo(topRight);
	//list->PathArcTo(topRightRadiusCenter, radius, -M_PI_2, 0);
	list->PathLineTo(rightTop);
	list->PathLineTo(rightBottom);
	//list->PathArcTo(bottomRightRadiusCenter, radius, 0, M_PI_2);
	list->PathLineTo(bottomRight);
	list->PathLineTo(bottomLeft);
	//list->PathArcTo(bottomLeftRadiusCenter, radius, M_PI_2, M_PI);
	list->PathLineTo(leftBottom);
	list->PathLineTo(leftTop);
	//list->PathArcTo(topLeftRadiusCenter, radius, M_PI, 1.5*M_PI);
	list->PathLineTo(topLeft);
	list->PathLineTo(leftTailCorner);
	list->PathLineTo(anchorPos);
}
