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
	@brief Implementation of TimebasePropertiesDialog
 */

#include "ngscopeclient.h"
#include "TimebasePropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TimebasePropertiesDialog::TimebasePropertiesDialog(Session* session)
	: Dialog("Timebase properties", ImVec2(300, 400))
	, m_session(session)
{
	/*
	m_committedDisplayName = m_channel->GetDisplayName();
	m_displayName = m_committedDisplayName;

	//Vertical settings are per stream
	size_t nstreams = m_channel->GetStreamCount();
	m_committedOffset.resize(nstreams);
	m_offset.resize(nstreams);
	m_committedRange.resize(nstreams);
	m_range.resize(nstreams);
	for(size_t i = 0; i<nstreams; i++)
	{
		auto unit = m_channel->GetYAxisUnits(i);

		m_committedOffset[i] = chan->GetOffset(i);
		m_offset[i] = unit.PrettyPrint(m_committedOffset[i]);

		m_committedRange[i] = chan->GetVoltageRange(i);
		m_range[i] = unit.PrettyPrint(m_committedRange[i]);
	}

	//Hardware acquisition settings if this is a scope channel
	auto scope = m_channel->GetScope();
	if(scope)
	{
		auto nchan = m_channel->GetIndex();

		RefreshInputSettings(scope, nchan);
	}
	else
	{
		m_committedAttenuation = 1;
		m_attenuation = "";
	}
	*/
}

TimebasePropertiesDialog::~TimebasePropertiesDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool TimebasePropertiesDialog::DoRender()
{
	//TODO: handle stream count changing dynamically on some filters

	/*
	float width = 10 * ImGui::GetFontSize();

	auto scope = m_channel->GetScope();
	if(ImGui::CollapsingHeader("Info"))
	{
		//Scope info
		if(scope)
		{
			auto nickname = scope->m_nickname;
			auto index = to_string(m_channel->GetIndex() + 1);	//use one based index for display

			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(width);
				ImGui::InputText("Instrument", &nickname);
			ImGui::EndDisabled();
			HelpMarker("The instrument this channel was measured by");

			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(width);
				ImGui::InputText("Hardware Timebase", &index);
			ImGui::EndDisabled();

			HelpMarker("Physical channel number (starting from 1) on the instrument front panel");
		}

		//TODO: filter info
	}
	*/

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
