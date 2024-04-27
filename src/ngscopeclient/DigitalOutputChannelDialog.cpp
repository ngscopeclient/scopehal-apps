/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of DigitalOutputChannelDialog
 */

#include "ngscopeclient.h"
#include "MainWindow.h"
#include "DigitalOutputChannelDialog.h"
#include <imgui_node_editor.h>
#include "../scopehal/BufferedSwitchMatrixOutputChannel.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

DigitalOutputChannelDialog::DigitalOutputChannelDialog(DigitalOutputChannel* chan, MainWindow* parent, bool graphEditorMode)
	: EmbeddableDialog(chan->GetHwname(), string("Channel properties: ") + chan->GetHwname(), ImVec2(300, 400), graphEditorMode)
	, m_channel(chan)
	, m_parent(parent)
	, m_drive("")
	, m_committedDrive(0)
{
	m_committedDisplayName = m_channel->GetDisplayName();
	m_displayName = m_committedDisplayName;

	//Color
	auto color = ColorFromString(m_channel->m_displaycolor);
	m_color[0] = ((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f;
	m_color[1] = ((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f;
	m_color[2] = ((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f;

	auto bso = dynamic_cast<BufferedSwitchMatrixOutputChannel*>(m_channel);
	if(bso && bso->MuxHasConfigurableDrive())
	{
		m_committedDrive = bso->GetMuxOutputDrive();
		m_drive = Unit(Unit::UNIT_VOLTS).PrettyPrint(m_committedDrive);
	}
}

DigitalOutputChannelDialog::~DigitalOutputChannelDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool DigitalOutputChannelDialog::DoRender()
{
	//Flags for a header that should be open by default EXCEPT in the graph editor
	ImGuiTreeNodeFlags defaultOpenFlags = m_graphEditorMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen;

	float width = 10 * ImGui::GetFontSize();

	auto bso = dynamic_cast<BufferedSwitchMatrixOutputChannel*>(m_channel);
	auto inst = m_channel->GetParent();
	if(!inst)
		return true;

	if(ImGui::CollapsingHeader("Info"))
	{
		auto nickname = inst->m_nickname;
		auto index = to_string(m_channel->GetIndex() + 1);	//use one based index for display

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Instrument", &nickname);
		ImGui::EndDisabled();
		HelpMarker("The instrument this channel was measured by");

		ImGui::BeginDisabled();
			ImGui::SetNextItemWidth(width);
			ImGui::InputText("Hardware Channel", &index);
		ImGui::EndDisabled();
		HelpMarker("Physical channel number (starting from 1) on the instrument front panel");
	}

	//All channels have display settings
	if(ImGui::CollapsingHeader("Display", defaultOpenFlags))
	{
		ImGui::SetNextItemWidth(width);
		if(TextInputWithImplicitApply("Nickname", m_displayName, m_committedDisplayName))
			m_channel->SetDisplayName(m_committedDisplayName);

		HelpMarker("Display name for the channel");

		if(ImGui::ColorEdit3(
			"Color",
			m_color,
			ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8))
		{
			char tmp[32];
			snprintf(tmp, sizeof(tmp), "#%02x%02x%02x",
				static_cast<int>(round(m_color[0] * 255)),
				static_cast<int>(round(m_color[1] * 255)),
				static_cast<int>(round(m_color[2] * 255)));
			m_channel->m_displaycolor = tmp;
		}
	}

	//Buffered channels have output voltage selector, if available
	if(bso && bso->MuxHasConfigurableDrive())
	{
		if(ImGui::CollapsingHeader("Output buffer", defaultOpenFlags))
		{
			ImGui::SetNextItemWidth(width);
			if(UnitInputWithExplicitApply("Level", m_drive, m_committedDrive, Unit(Unit::UNIT_VOLTS)))
				bso->SetMuxOutputDrive(m_committedDrive);

			HelpMarker("Nominal VCC level of the output driver\n");
		}
	}

	return true;
}
