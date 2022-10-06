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
#include "Session.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TimebasePropertiesPage

TimebasePropertiesPage::TimebasePropertiesPage(Oscilloscope* scope)
	: m_scope(scope)
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
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TimebasePropertiesDialog::TimebasePropertiesDialog(Session* session)
	: Dialog("Timebase", ImVec2(300, 400))
	, m_session(session)
{
	Refresh();
}

TimebasePropertiesDialog::~TimebasePropertiesDialog()
{
}

/**
	@brief Refreshes the dialog whenever the set of valid configurations changes

	Examples of events that will need to trigger a refresh:
	* Enabling or disabling a channel
	* Changing ADC mode
 */
void TimebasePropertiesDialog::Refresh()
{
	m_pages.clear();

	auto scopes = m_session->GetScopes();
	for(auto s : scopes)
		m_pages.push_back(make_unique<TimebasePropertiesPage>(s));
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
	float width = 10 * ImGui::GetFontSize();

	for(auto& p : m_pages)
	{
		auto scope = p->m_scope;

		if(ImGui::CollapsingHeader(scope->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(scope->m_nickname.c_str());

			//Time domain configuration
			if(scope->HasTimebaseControls())
			{

				//Sample rate
				ImGui::SetNextItemWidth(width);
				if(Combo("Sample rate", p->m_rateNames, p->m_rate))
					scope->SetSampleRate(p->m_rates[p->m_rate]);
				HelpMarker(
					"Time domain sample rate.\n\n"
					"For some instruments, available sample rates may vary depending on which channels are active."
					);

				//Memory depth
				ImGui::SetNextItemWidth(width);
				if(Combo("Sample depth", p->m_depthNames, p->m_depth))
					scope->SetSampleDepth(p->m_depths[p->m_depth]);
				HelpMarker(
					"Acquisition record length, in samples.\n\n"
					"For some instruments, available memory depths may vary depending on which channels are active."
					);

				//Interleaving
				if(scope->CanInterleave())
				{
					if(ImGui::Checkbox("Interleaving", &p->m_interleaving))
					{
						scope->SetInterleaving(p->m_interleaving);
						Refresh();
					}

					HelpMarker(
						"Combine ADCs from multiple channels to get higher sampling rate on a subset of channels.\n"
						"\n"
						"Some instruments do not have an explicit interleaving switch, but available sample rates "
						"may vary depending on which channels are active."
						);
				}

			}

			//Frequency domain configuration
			if(scope->HasFrequencyControls())
			{
				//TODO
			}

			ImGui::PopID();
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
