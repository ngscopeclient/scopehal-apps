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
	@brief Declaration of TutorialWizard
 */
#ifndef TutorialWizard_h
#define TutorialWizard_h

#include "Dialog.h"

class TutorialWizard : public Dialog
{
public:
	TutorialWizard(Session* session, MainWindow* parent);
	virtual ~TutorialWizard();

	virtual bool Render() override;
	virtual bool DoRender() override;

	enum TutorialStep
	{
		TUTORIAL_00_INTRO,
		TUTORIAL_01_ADDINSTRUMENT,
		TUTORIAL_02_CONNECT,
		TUTORIAL_03_ACQUIRE,
		TUTORIAL_04_SCROLLZOOM,

		TUTORIAL_99_FINAL
	};

	TutorialStep GetCurrentStep()
	{ return static_cast<TutorialStep>(m_step); }

	///@brief Move the tutorial to the next step
	void AdvanceToNextStep()
	{
		m_step ++;
		m_continueEnabled = false;
	}

	///@brief Enable the next step but do not advance to it
	void EnableNextStep()
	{ m_continueEnabled = true; }

	void DrawSpeechBubble(
		ImVec2 anchorPos,
		ImGuiDir dirTip,
		std::string str);

protected:

	void MakePathSpeechBubble(
		ImDrawList* list,
		ImGuiDir dirTip,
		ImVec2 anchorPos,
		ImVec2 textsize,
		float tailLength,
		float radius,
		float leftOverhang);

	///@brief Text for each tutorial page
	std::vector<std::string> m_markdownText;

	///@brief Current step of the tutorial
	size_t m_step;

	///@brief True if the "continue" button is active
	bool m_continueEnabled;
};

#endif
