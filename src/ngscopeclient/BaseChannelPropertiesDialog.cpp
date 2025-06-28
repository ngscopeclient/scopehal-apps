/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of BaseChannelPropertiesDialog
 */

#include "ngscopeclient.h"
#include "BaseChannelPropertiesDialog.h"
#include <imgui_node_editor.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

BaseChannelPropertiesDialog::BaseChannelPropertiesDialog(InstrumentChannel* chan, bool graphEditorMode)
	: EmbeddableDialog(
		chan->GetHwname(),
		string("Channel properties: ") + chan->GetHwname(),
		ImVec2(300, 400),
		graphEditorMode)
	, m_channel(chan)
{
}

BaseChannelPropertiesDialog::~BaseChannelPropertiesDialog()
{
}

bool BaseChannelPropertiesDialog::DoRender()
{
	auto f = dynamic_cast<Filter*>(m_channel);

	//TODO
	auto ochan = dynamic_cast<OscilloscopeChannel*>(m_channel);
	Oscilloscope* scope = nullptr;
	if(ochan)
		scope = ochan->GetScope();

	float width = 10 * ImGui::GetFontSize();

	ImGui::PushID("info");
	if(ImGui::CollapsingHeader("Info"))
	{
		//Scope info
		if(scope)
		{
			auto nickname = scope->m_nickname;
			auto hwname = m_channel->GetHwname();
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

			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(width);
				ImGui::InputText("Hardware Name", &hwname);
			ImGui::EndDisabled();
			HelpMarker("Hardware name for the channel (as used in the instrument API)");
		}

		//Filter info
		if(f)
		{
			string fname = f->GetProtocolDisplayName();

			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(width);
				ImGui::InputText("Filter Type", &fname);
			ImGui::EndDisabled();
			HelpMarker("Type of filter object");
		}
	}
	ImGui::PopID();

	return true;
}
