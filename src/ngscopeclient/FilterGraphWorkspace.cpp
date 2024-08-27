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
	@brief Implementation of FilterGraphWorkspace
 */
#include "ngscopeclient.h"
#include "FilterGraphWorkspace.h"
#include "FilterGraphEditor.h"
#include "CreateFilterBrowser.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FilterGraphWorkspace::FilterGraphWorkspace(
	Session& session,
	shared_ptr<FilterGraphEditor> graphEditor,
	shared_ptr<CreateFilterBrowser> palette)
	: Workspace(session)
	, m_firstRun(true)
	, m_graphEditor(graphEditor)
	, m_palette(palette)
{
	m_title = "Filter Graph";
}

bool FilterGraphWorkspace::Render()
{
	//Closed, nothing to do
	if(!m_open)
		return false;

	auto dockspace_id = ImGui::GetID(m_id.c_str());

	string name = m_title + "###" + m_id;
	ImGui::SetNextWindowSize(m_defaultSize, ImGuiCond_Appearing);
	if(!ImGui::Begin(name.c_str(), &m_open, ImGuiWindowFlags_NoCollapse))
	{
		//If we get here, the window is tabbed out or the content area is otherwise not visible.
		//Need to keep the dockspace node alive still, though!
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly, nullptr);

		ImGui::End();
		return true;
	}

	if(ImGui::BeginPopupContextItem())
	{
		ImGui::InputText("Name", &m_title);
		ImGui::EndPopup();
	}

	//First run? special case
	if(m_firstRun)
	{
		auto topNode = ImGui::DockBuilderGetNode(dockspace_id);
		if(topNode)
		{
			//Split the top into two sub nodes
			ImGuiID leftPanelID;
			ImGuiID rightPanelID;
			ImGui::DockBuilderSplitNode(topNode->ID, ImGuiDir_Right, 0.2, &rightPanelID, &leftPanelID);

			ImGui::DockBuilderDockWindow(m_graphEditor->GetTitleAndID().c_str(), leftPanelID);
			ImGui::DockBuilderDockWindow(m_palette->GetTitleAndID().c_str(), rightPanelID);
			ImGui::DockBuilderFinish(dockspace_id);

			//Remove references in case user wants to close the dialogs later
			m_graphEditor = nullptr;
			m_palette = nullptr;
			m_firstRun = false;
		}
	}

	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), 0, nullptr);

	ImGui::End();
	return true;
}
