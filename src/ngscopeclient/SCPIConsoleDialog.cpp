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
	@brief Implementation of SCPIConsoleDialog
 */

#include "ngscopeclient.h"
#include "MainWindow.h"
#include "SCPIConsoleDialog.h"

using namespace std;
using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SCPIConsoleDialog::SCPIConsoleDialog(MainWindow* parent, SCPIInstrument* inst)
	: Dialog(("SCPI Console: ") + inst->m_nickname, ImVec2(500, 300))
	, m_parent(parent)
	, m_inst(inst)
	, m_commandPending(false)
{
}

SCPIConsoleDialog::~SCPIConsoleDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool SCPIConsoleDialog::DoRender()
{
	auto csize = ImGui::GetContentRegionAvail();

	//Check for results of pending commands
	if(m_commandPending)
	{
		if(m_commandReturnValue.wait_for(0s) == future_status::ready)
		{
			auto str = m_commandReturnValue.get();
			if(Trim(str).empty())
				m_output.push_back("Request timed out.");
			else
				m_output.push_back(str);
			m_commandPending = false;
		}
	}

	//Scroll area for console output is full window minus command box
	ImVec2 scrollarea(csize.x, csize.y - 1.5*ImGui::GetTextLineHeightWithSpacing());
	ImGui::BeginChild("scrollview", scrollarea, false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::PushFont(m_parent->GetMonospaceFont());
		for(auto& line : m_output)
			ImGui::TextUnformatted(line.c_str());
		ImGui::PopFont();

		if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();

	//Command input box
	ImGui::SetNextItemWidth(csize.x);
	bool pending = m_commandPending;
	if(pending)
		ImGui::BeginDisabled();
	if(ImGui::InputText("Command", &m_command, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		//Show command immediately
		m_output.push_back(string("> ") + m_command);

		//Push command to the instrument immediately
		auto trans = m_inst->GetTransport();
		if(m_command.find('?') == string::npos)
			trans->SendCommandQueued(m_command);

		//Commands are sent immediately, but reply is deferred to avoid blocking the UI
		else
		{
			m_commandPending = true;
			string cmd = m_command;
			m_commandReturnValue = async(launch::async, [cmd, trans]{ return trans->SendCommandQueuedWithReply(cmd); });
		}

		m_command = "";

		//Re-set focus back into the box
		//because imgui defaults to unfocusing once it's closed
		ImGui::SetKeyboardFocusHere(-1);
	}
	if(pending)
		ImGui::EndDisabled();

	return true;
}
