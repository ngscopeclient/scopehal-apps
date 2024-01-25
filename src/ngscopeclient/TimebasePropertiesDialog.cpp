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
	@brief Implementation of TimebasePropertiesDialog
 */

#include "ngscopeclient.h"
#include "TimebasePropertiesDialog.h"
#include "Session.h"
#include "../scopehal/SCPISpectrometer.h"

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

	Unit hz(Unit::UNIT_HZ);
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

	auto spec = dynamic_cast<SCPISpectrometer*>(scope);
	if(spec)
	{
		Unit fs(Unit::UNIT_FS);
		m_integrationTime = spec->GetIntegrationTime();
		m_integrationText = fs.PrettyPrint(m_integrationTime);
	}
	else
		m_integrationTime = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TimebasePropertiesDialog::TimebasePropertiesDialog(Session* session)
	: Dialog("Timebase", "Timebase", ImVec2(300, 400))
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

	bool needRefresh = false;

	for(auto& p : m_pages)
	{
		auto scope = p->m_scope;
		auto spec = dynamic_cast<SCPISpectrometer*>(p->m_scope);

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
					if(ImGui::Combo("Sampling mode", &p->m_samplingMode, items, 2))
					{
						if(p->m_samplingMode == Oscilloscope::REAL_TIME)
							scope->SetSamplingMode(Oscilloscope::REAL_TIME);
						else
							scope->SetSamplingMode(Oscilloscope::EQUIVALENT_TIME);

						needRefresh = true;
					}
				}

			}

			//Frequency domain configuration
			if(scope->HasFrequencyControls())
			{
				//No sample rate

				//Memory depth (but don't duplicate if we also have time domain controls, like for a SDR/RTSA)
				if(!scope->HasTimebaseControls())
				{
					ImGui::SetNextItemWidth(width);
					if(Combo("Points", p->m_depthNames, p->m_depth))
						scope->SetSampleDepth(p->m_depths[p->m_depth]);
					HelpMarker("Number of points in the sweep");
				}

				//Frequency
				bool changed = false;
				Unit hz(Unit::UNIT_HZ);

				ImGui::SetNextItemWidth(width);
				if(UnitInputWithImplicitApply("Start", p->m_startText, p->m_start, hz))
				{
					double mid = (p->m_start + p->m_end) / 2;
					double span = (p->m_end - p->m_start);
					scope->SetCenterFrequency(0, mid);
					scope->SetSpan(span);
					changed = true;
				}
				HelpMarker("Start of the frequency sweep");

				ImGui::SetNextItemWidth(width);
				if(UnitInputWithImplicitApply("Center", p->m_centerText, p->m_center, hz))
				{
					scope->SetCenterFrequency(0, p->m_center);
					changed = true;
				}
				HelpMarker("Midpoint of the frequency sweep");

				ImGui::SetNextItemWidth(width);
				if(UnitInputWithImplicitApply("Span", p->m_spanText, p->m_span, hz))
				{
					scope->SetSpan(p->m_span);
					changed = true;
				}
				HelpMarker("Width of the frequency sweep");

				ImGui::SetNextItemWidth(width);
				if(UnitInputWithImplicitApply("End", p->m_endText, p->m_end, hz))
				{
					double mid = (p->m_start + p->m_end) / 2;
					double span = (p->m_end - p->m_start);
					scope->SetCenterFrequency(0, mid);
					scope->SetSpan(span);
					changed = true;
				}
				HelpMarker("End of the frequency sweep");

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

			//Spectrometer controls
			if(spec)
			{
				ImGui::SetNextItemWidth(width);

				Unit fs(Unit::UNIT_FS);
				if(UnitInputWithImplicitApply("Integration time", p->m_integrationText, p->m_integrationTime, fs))
					spec->SetIntegrationTime(p->m_integrationTime);
				HelpMarker("Spectrometer integration / exposure time");
			}

			ImGui::PopID();
		}
	}

	if(needRefresh)
		Refresh();

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
