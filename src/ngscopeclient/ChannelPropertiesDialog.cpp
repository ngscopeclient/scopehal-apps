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
	@brief Implementation of ChannelPropertiesDialog
 */

#include "ngscopeclient.h"
#include "ChannelPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ChannelPropertiesDialog::ChannelPropertiesDialog(OscilloscopeChannel* chan, bool graphEditorMode)
	: Dialog(chan->GetHwname(), ImVec2(300, 400))
	, m_channel(chan)
	, m_graphEditorMode(graphEditorMode)
{
	m_committedDisplayName = m_channel->GetDisplayName();
	m_displayName = m_committedDisplayName;

	//Color
	auto color = ColorFromString(m_channel->m_displaycolor);
	m_color[0] = ((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f;
	m_color[1] = ((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f;
	m_color[2] = ((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f;

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
}

/**
	@brief Update input configuration values

	This is typically used with instruments that have a hardware input mux, since the set of available couplings and
	bandwidth limiters etc may change for one input vs another.
 */
void ChannelPropertiesDialog::RefreshInputSettings(Oscilloscope* scope, size_t nchan)
{
	m_couplings.clear();
	m_couplingNames.clear();
	m_bwlValues.clear();
	m_bwlNames.clear();
	m_imuxNames.clear();
	m_modeNames.clear();

	//Attenuation
	m_committedAttenuation = scope->GetChannelAttenuation(nchan);
	m_attenuation = to_string(m_committedAttenuation);

	//Coupling
	m_coupling = 0;
	auto curCoup = m_channel->GetCoupling();
	m_couplings = scope->GetAvailableCouplings(nchan);
	for(size_t i=0; i<m_couplings.size(); i++)
	{
		auto c = m_couplings[i];

		switch(c)
		{
			case OscilloscopeChannel::COUPLE_DC_50:
				m_couplingNames.push_back("DC 50Ω");
				break;

			case OscilloscopeChannel::COUPLE_AC_50:
				m_couplingNames.push_back("AC 50Ω");
				break;

			case OscilloscopeChannel::COUPLE_DC_1M:
				m_couplingNames.push_back("DC 1MΩ");
				break;

			case OscilloscopeChannel::COUPLE_AC_1M:
				m_couplingNames.push_back("AC 1MΩ");
				break;

			case OscilloscopeChannel::COUPLE_GND:
				m_couplingNames.push_back("Ground");
				break;

			default:
				m_couplingNames.push_back("Invalid");
				break;
		}

		if(c == curCoup)
			m_coupling = i;
	}

	//Bandwidth limiters
	m_bwlValues = scope->GetChannelBandwidthLimiters(nchan);
	m_bwl = 0;
	auto curBwl = scope->GetChannelBandwidthLimit(nchan);
	Unit hz(Unit::UNIT_HZ);
	for(size_t i=0; i<m_bwlValues.size(); i++)
	{
		auto b = m_bwlValues[i];
		if(b == 0)
			m_bwlNames.push_back("Full");
		else
			m_bwlNames.push_back(hz.PrettyPrint(b*1e6));

		if(b == curBwl)
			m_bwl = i;
	}

	//Input mux settings
	m_imuxNames = scope->GetInputMuxNames(nchan);
	m_imux = scope->GetInputMuxSetting(nchan);

	//Inversion
	m_inverted = scope->IsInverted(nchan);

	//ADC modes
	m_mode = 0;
	if(scope->IsADCModeConfigurable())
	{
		m_mode = scope->GetADCMode(nchan);
		m_modeNames = scope->GetADCModeNames(nchan);
	}

	//Probe type
	m_probe = scope->GetProbeName(nchan);
	m_canAutoZero = scope->CanAutoZero(nchan);
}

ChannelPropertiesDialog::~ChannelPropertiesDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ChannelPropertiesDialog::DoRender()
{
	//Flags for a header that should be open by default EXCEPT in the graph editor
	ImGuiTreeNodeFlags defaultOpenFlags = m_graphEditorMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen;

	//TODO: handle stream count changing dynamically on some filters

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
				ImGui::InputText("Hardware Channel", &index);
			ImGui::EndDisabled();

			HelpMarker("Physical channel number (starting from 1) on the instrument front panel");
		}

		//TODO: filter info
	}

	//Al channels have display settings
	if(ImGui::CollapsingHeader("Display", defaultOpenFlags))
	{
		auto f = dynamic_cast<Filter*>(m_channel);

		//If it's a filter, using the default name, check for changes made outside of this properties window
		//(e.g. via filter graph editor)
		if(f)
		{
			if(f->IsUsingDefaultName() && (f->GetDisplayName() != m_committedDisplayName))
			{
				m_committedDisplayName = f->GetDisplayName();
				m_displayName = m_committedDisplayName;
			}
		}

		ImGui::SetNextItemWidth(width);
		if(TextInputWithImplicitApply("Nickname", m_displayName, m_committedDisplayName))
		{
			//If it's a filter, we're not using a default name anymore (unless the provided name is blank)
			if(f != nullptr)
			{
				if(m_committedDisplayName == "")
				{
					f->UseDefaultName(true);
					m_committedDisplayName = f->GetDisplayName();
					m_displayName = m_committedDisplayName;
				}
				else
				{
					f->UseDefaultName(false);
					m_channel->SetDisplayName(m_committedDisplayName);
				}
			}

			//No, just set the name
			else
				m_channel->SetDisplayName(m_committedDisplayName);
		}

		if(f)
			HelpMarker("Display name for the filter.\n\nSet blank to use an auto-generated default name.");
		else
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

	//Input settings only make sense if we have an attached scope
	auto nstreams = m_channel->GetStreamCount();
	if(scope)
	{
		if(ImGui::CollapsingHeader("Input", defaultOpenFlags))
		{
			//Type of probe connected
			auto index = m_channel->GetIndex();
			string ptype = m_probe;
			if(ptype == "")
				ptype = "(not detected)";
			ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(width);
				ImGui::InputText("Probe Type", &ptype);
			ImGui::EndDisabled();
			HelpMarker("Type of probe connected to the instrument input");

			//Attenuation
			Unit counts(Unit::UNIT_COUNTS);
			ImGui::SetNextItemWidth(width);
			if(m_probe != "")	//cannot change attenuation on active probes
				ImGui::BeginDisabled();
			if(UnitInputWithImplicitApply("Attenuation", m_attenuation, m_committedAttenuation, counts))
			{
				scope->SetChannelAttenuation(index, m_committedAttenuation);

				//Update offset and range when attenuation is changed
				for(size_t i = 0; i<nstreams; i++)
				{
					auto unit = m_channel->GetYAxisUnits(i);

					m_committedOffset[i] = m_channel->GetOffset(i);
					m_offset[i] = unit.PrettyPrint(m_committedOffset[i]);

					m_committedRange[i] = m_channel->GetVoltageRange(i);
					m_range[i] = unit.PrettyPrint(m_committedRange[i]);
				}
			}
			if(m_probe != "")
				ImGui::EndDisabled();
			HelpMarker("Attenuation setting for the probe (for example, 10 for a 10:1 probe)");

			//Only show coupling box if the instrument has configurable coupling
			if( (m_couplings.size() > 1) && (m_probe == "") )
			{
				ImGui::SetNextItemWidth(width);
				if(Combo("Coupling", m_couplingNames, m_coupling))
					m_channel->SetCoupling(m_couplings[m_coupling]);
				HelpMarker("Coupling configuration for the input");
			}

			//Bandwidth limiters (only show if more than one value available)
			if(m_bwlNames.size() > 1)
			{
				ImGui::SetNextItemWidth(width);
				if(Combo("Bandwidth", m_bwlNames, m_bwl))
					m_channel->SetBandwidthLimit(m_bwlValues[m_bwl]);
				HelpMarker("Hardware bandwidth limiter setting");
			}

			//If there's an input mux, show a combo box for it
			if(scope->HasInputMux(index))
			{
				ImGui::SetNextItemWidth(width);
				if(Combo("Input mux", m_imuxNames, m_imux))
				{
					scope->SetInputMux(index, m_imux);

					//When input mux changes, we need to redo all of the other settings since
					//the set of valid values can change
					RefreshInputSettings(scope, index);
				}

				HelpMarker("Hardware input multiplexer setting");
			}

			//If the scope has configurable ADC modes, show dropdown for that
			if(scope->IsADCModeConfigurable())
			{
				bool nomodes = m_modeNames.size() <= 1;
				if(nomodes)
					ImGui::BeginDisabled();
				ImGui::SetNextItemWidth(width);
				if(Combo("ADC mode", m_modeNames, m_mode))
				{
					scope->SetADCMode(index, m_mode);

					RefreshInputSettings(scope, index);
				}
				if(nomodes)
					ImGui::EndDisabled();

				HelpMarker(
					"Operating mode for the analog-to-digital converter.\n\n"
					"Some instruments allow the ADC to operate in several modes, typically trading bit depth "
					"against sample rate. Available modes may vary depending on the current sample rate and "
					"which channels are in use."
					);
			}

			//If the probe supports inversion, show a checkbox for it
			if(scope->CanInvert(index))
			{
				if(ImGui::Checkbox("Invert", &m_inverted))
					m_channel->Invert(m_inverted);

				HelpMarker(
					"When checked, input value is multiplied by -1.\n\n"
					"For a differential probe, this is equivalent to swapping the positive and negative inputs."
					);
			}

			//If the probe supports auto zeroing, show a button for it
			if(m_canAutoZero)
			{
				if(ImGui::Button("Auto Zero"))
					m_channel->AutoZero();
				HelpMarker(
					"Click to automatically zero offset of active probe.\n\n"
					"Check probe documentation to see whether input signal must be removed before zeroing."
					);
			}
		}
	}

	//Vertical settings are per stream
	for(size_t i = 0; i<nstreams; i++)
	{
		string streamname = "Vertical";
		if(nstreams > 1)
			streamname = m_channel->GetStreamName(i);

		if(ImGui::CollapsingHeader(streamname.c_str()))
		{
			auto unit = m_channel->GetYAxisUnits(i);

			//If no change to offset in dialog, update our input value when we change offset outside the dialog
			auto off = m_channel->GetOffset(i);
			auto soff = unit.PrettyPrint(m_committedOffset[i]);
			if( (m_committedOffset[i] != off) && (soff == m_offset[i]) )
			{
				m_offset[i] = unit.PrettyPrint(off);
				m_committedOffset[i] = off;
			}
			ImGui::SetNextItemWidth(width);
			if(UnitInputWithExplicitApply("Offset", m_offset[i], m_committedOffset[i], unit))
				m_channel->SetOffset(m_committedOffset[i], i);

			//Same for range
			auto range = m_channel->GetVoltageRange(i);
			auto srange = unit.PrettyPrint(m_committedRange[i]);
			if( (m_committedRange[i] != range) && (srange == m_range[i]) )
			{
				m_range[i] = unit.PrettyPrint(range);
				m_committedRange[i] = range;
			}
			ImGui::SetNextItemWidth(width);
			if(UnitInputWithExplicitApply("Range", m_range[i], m_committedRange[i], unit))
				m_channel->SetVoltageRange(m_committedRange[i], i);
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
