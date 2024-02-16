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
	@brief Implementation of TriggerPropertiesDialog
 */

#include "ngscopeclient.h"
#include "ChannelPropertiesDialog.h"
#include "FilterPropertiesDialog.h"
#include "TriggerPropertiesDialog.h"
#include "Session.h"
#include "../scopehal/CDRTrigger.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TriggerPropertiesPage

TriggerPropertiesPage::TriggerPropertiesPage(Oscilloscope* scope)
	: m_scope(scope)
	, m_committedLevel(0)
	, m_cdrLockState(false)
	, m_tLastCdrPoll(0)
{
	auto trig = m_scope->GetTrigger();
	if(!trig)
		return;

	Unit volts(Unit::UNIT_VOLTS);

	m_committedLevel = trig->GetLevel();
	m_triggerLevel = volts.PrettyPrint(m_committedLevel);

	Unit fs(Unit::UNIT_FS);
	m_committedTriggerOffset = scope->GetTriggerOffset();
	m_triggerOffset = fs.PrettyPrint(m_committedTriggerOffset);
}

/**
	@brief Starts a hierarchical block of widgets

	May be either a collapsing header or tree node depending on if we're embedded in the filter graph editor or not
 */
bool TriggerPropertiesPage::StartSection(const string& name, bool graphEditorMode)
{
	if(graphEditorMode)
		return ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
	else
		return ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
}

/**
	@brief Ends a section started with StartSection()
 */
void TriggerPropertiesPage::EndSection(bool graphEditorMode)
{
	if(!graphEditorMode)
		return ImGui::TreePop();
}

/**
	@brief Run the properties for this page
 */
void TriggerPropertiesPage::Render(bool graphEditorMode)
{
	auto trig = m_scope->GetTrigger();
	if(!trig)
		return;

	//Show inputs (if we have any)
	bool updated = false;
	if(trig->GetInputCount() != 0)
	{
		if(StartSection("Position", graphEditorMode))
		{
			//Check if trigger offset changed outside the dialog
			Unit fs(Unit::UNIT_FS);
			float off = m_scope->GetTriggerOffset();
			if(m_committedTriggerOffset != off)
			{
				m_committedTriggerOffset = off;
				m_triggerOffset = fs.PrettyPrint(off);
			}

			//Actual trigger position
			if(Dialog::UnitInputWithImplicitApply(
				"Delay",
				m_triggerOffset,
				m_committedTriggerOffset,
				fs))
			{
				m_scope->SetTriggerOffset(off);
			}

			Dialog::HelpMarker(
				"Time offset from the nominal zero-time point of the waveform to the trigger point.\n\n"
				""
				);

			EndSection(graphEditorMode);
		}

		if(StartSection("Inputs", graphEditorMode))
		{
			//TODO: cache some of this?
			vector<StreamDescriptor> streams;
			FindAllStreams(streams);

			for(size_t i=0; i<trig->GetInputCount(); i++)
			{
				//Find the set of legal streams for this input
				vector<StreamDescriptor> matchingInputs;
				vector<string> names;
				int sel = -1;
				for(auto stream : streams)
				{
					if(!trig->ValidateChannel(i, stream))
						continue;

					if(trig->GetInput(i) == stream)
						sel = matchingInputs.size();

					matchingInputs.push_back(stream);
					names.push_back(stream.GetName());
				}

				//The actual combo box
				if(Dialog::Combo(trig->GetInputName(i).c_str(), names, sel))
				{
					trig->SetInput(i, matchingInputs[sel]);
					updated = true;
				}
				Dialog::HelpMarker(
					"Select the channel to use as input to the trigger circuit.\n\n"
					"Some instruments have restrictions on which channels can be used for some trigger types\n"
					"(for example, dedicated routing to a CDR board)\n");
			}

			EndSection(graphEditorMode);
		}

		if(StartSection("Thresholds", graphEditorMode))
		{
			//Primary level
			Unit volts(Unit::UNIT_VOLTS);
			if(Dialog::UnitInputWithImplicitApply(
				"Level",
				m_triggerLevel,
				m_committedLevel,
				volts))
			{
				trig->SetLevel(m_committedLevel);
				updated = true;
			}

			//Check for changes made elsewhere in the GUI (dragging arrow etc)
			if(trig->GetLevel() != m_committedLevel)
			{
				m_committedLevel = trig->GetLevel();
				m_triggerLevel = volts.PrettyPrint(m_committedLevel);
			}

			//TODO: if we have a secondary level, do that

			EndSection(graphEditorMode);
		}

		auto cdrtrig = dynamic_cast<CDRTrigger*>(trig);
		if(cdrtrig)
		{
			if(StartSection("CDR", graphEditorMode))
			{
				//Show bit rate
				if(FilterPropertiesDialog::DoParameter(trig->GetParameter("Bit Rate"), "Bit Rate", m_paramTempValues))
					updated = true;

				//Show autobaud button
				if(cdrtrig->IsAutomaticBitRateCalculationAvailable())
				{
					ImGui::SameLine();
					if(ImGui::Button("Auto"))
					{
						cdrtrig->CalculateBitRate();

						//pull updated bit rate etc from hardware
						m_paramTempValues.clear();
					}
				}

				//Show lock status, but limit polling rate to 1 Hz
				double now = GetTime();
				bool locked = m_cdrLockState;
				if( (now - m_tLastCdrPoll) > 1)
				{
					locked = cdrtrig->IsCDRLocked();
					m_tLastCdrPoll = now;
				}

				ImGui::BeginDisabled();
				ImGui::Checkbox("PLL Lock", &locked);
				ImGui::EndDisabled();

				if(locked != m_cdrLockState)
				{
					//pull updated bit rate etc from hardware if we just re-locked
					if(locked)
						m_paramTempValues.clear();

					m_cdrLockState = locked;
				}

				EndSection(graphEditorMode);
			}
		}

		if(StartSection("Parameters", graphEditorMode))
		{
			for(auto it = trig->GetParamBegin(); it != trig->GetParamEnd(); it++)
			{
				//Skip trigger level as that's redundant
				if(it->first == "Level")
					continue;

				if(FilterPropertiesDialog::DoParameter(it->second, it->first, m_paramTempValues))
					updated = true;
			}

			EndSection(graphEditorMode);
		}
	}

	if(updated)
		m_scope->PushTrigger();
}

/**
	@brief Get every stream that might be usable as an input to this trigger
 */
void TriggerPropertiesPage::FindAllStreams(vector<StreamDescriptor>& streams)
{
	for(size_t i=0; i<m_scope->GetChannelCount(); i++)
	{
		auto chan = m_scope->GetOscilloscopeChannel(i);
		if(!chan)
			continue;
		if(m_scope->CanEnableChannel(i) || (chan == m_scope->GetExternalTrigger()) )
		{
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				streams.push_back(StreamDescriptor(chan, j));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TriggerPropertiesDialog::TriggerPropertiesDialog(Session* session)
	: Dialog("Trigger", "Trigger", ImVec2(300, 400))
	, m_session(session)
{
	Refresh();
}

TriggerPropertiesDialog::~TriggerPropertiesDialog()
{
}

void TriggerPropertiesDialog::Refresh()
{
	m_pages.clear();
	m_triggerTypeIndexes.clear();

	auto scopes = m_session->GetScopes();
	for(auto s : scopes)
	{
		m_pages.push_back(make_unique<TriggerPropertiesPage>(s));

		//Figure out combo index for active trigger
		int index = -1;
		vector<string> types = s->GetTriggerTypes();
		auto trig = s->GetTrigger();
		string ttype;
		if(trig)
			ttype = trig->GetTriggerDisplayName();
		for(size_t i=0; i<types.size(); i++)
		{
			if(types[i] == ttype)
			{
				index = i;
				break;
			}
		}
		m_triggerTypeIndexes.push_back(index);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool TriggerPropertiesDialog::DoRender()
{
	//float width = 10 * ImGui::GetFontSize();

	for(size_t i=0; i<m_pages.size(); i++)
	{
		auto& p = m_pages[i];

		auto scope = p->m_scope;

		if(ImGui::CollapsingHeader(scope->m_nickname.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID(scope->m_nickname.c_str());

			//Dropdown with list of trigger types is outside the main trigger page
			//TODO: cache some of this?
			vector<string> types = scope->GetTriggerTypes();
			if(Combo("Type", types, m_triggerTypeIndexes[i]))
			{
				//Save the level and inputs of the old trigger so we can reuse it
				auto oldTrig = scope->GetTrigger();
				float level = 0;
				vector<StreamDescriptor> inputs;
				if(oldTrig)
				{
					level = oldTrig->GetLevel();
					for(size_t j=0; j<oldTrig->GetInputCount(); j++)
						inputs.push_back(oldTrig->GetInput(j));
				}

				//Create the new trigger
				auto newTrig = Trigger::CreateTrigger(types[m_triggerTypeIndexes[i]], scope);
				if(newTrig)
				{
					//Copy settings over from old trigger to new
					//TODO: copy both levels if both are two level triggers
					newTrig->SetLevel(level);
					for(size_t j=0; (j<newTrig->GetInputCount()) && (j < inputs.size()); j++)
						newTrig->SetInput(j, inputs[j]);

					//Push changes to the scope all at once after the new trigger is set up
					scope->SetTrigger(newTrig);
					scope->PushTrigger();

					//Replace the properties page with whatever the new trigger eeds
					m_pages[i] = make_unique<TriggerPropertiesPage>(scope);
				}
			}
			HelpMarker("Select the type of trigger for this instrument\n");

			m_pages[i]->Render(false);

			ImGui::PopID();
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers
