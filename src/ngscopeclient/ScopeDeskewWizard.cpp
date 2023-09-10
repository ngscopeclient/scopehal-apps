/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of ScopeDeskewWizard
 */

#include "ngscopeclient.h"
#include "ScopeDeskewWizard.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ScopeDeskewWizard::ScopeDeskewWizard(
	shared_ptr<TriggerGroup> group,
	Oscilloscope* secondary,
	MainWindow* parent,
	Session& session)
	: Dialog(
		string("Deskew Oscilloscope: ") + secondary->m_nickname,
		"Deskew" + secondary->m_nickname,
		ImVec2(700, 400))
	, m_state(STATE_WELCOME_1)
	, m_group(group)
	, m_secondary(secondary)
	, m_parent(parent)
	, m_session(session)
	, m_useExtRefPrimary(true)
	, m_useExtRefSecondary(true)
	, m_measureCycle(0)
	, m_lastTriggerTimestamp(0)
	, m_lastTriggerFs(0)
{
}

ScopeDeskewWizard::~ScopeDeskewWizard()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ScopeDeskewWizard::DoRender()
{
	switch(m_state)
	{
		case STATE_WELCOME_1:
			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Welcome");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::TextWrapped(
				"This wizard measures the trigger-path propagation delay between the primary instrument (%s) "
				"and the secondary instrument (%s), and calibrates out the delay so waveforms from both "
				"instruments appear correctly aligned in the ngscopeclient timeline.",
				m_group->m_primary->m_nickname.c_str(),
				m_secondary->m_nickname.c_str() );

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_2;

			break;

		case STATE_WELCOME_2:
			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Cross-Trigger Cabling");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Connect the trigger output of %s to any channel of %s which may be used as a trigger.",
				m_group->m_primary->m_nickname.c_str(),
				m_secondary->m_nickname.c_str());

			ImGui::Bullet();
			ImGui::TextWrapped(
				"It is suggested to use the external trigger input if one is available, in order "
				"to leave signal inputs free.");

			ImGui::Bullet();
			ImGui::TextWrapped(
				"If %s does not have a trigger output, it cannot be used as the primary of the trigger group.",
				m_group->m_primary->m_nickname.c_str());

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_3;
			break;

		case STATE_WELCOME_3:

			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Cross-Trigger Setup");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Configure %s to trigger on the channel connected to the cross-trigger signal and adjust "
				"the trigger level appropriately.",
				m_secondary->m_nickname.c_str());

			ImGui::Bullet();
			ImGui::TextWrapped("To test if the cabling and trigger level are correct, "
				"press the \"trigger arm\" button on the toolbar and verify both instruments trigger.");

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_4;
			break;

		case STATE_WELCOME_4:

			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Calibration Signal Setup");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Connect a signal with minimal autocorrelation to one channel of %s and one channel of %s.",
				m_group->m_primary->m_nickname.c_str(),
				m_secondary->m_nickname.c_str() );

			ImGui::Bullet();
			ImGui::TextWrapped(
				"You may use an RF splitter and coaxial cabling, or simply touch a probe from each instrument to a "
				"common point. Note that the delays of this cabling or probes will be included in the calibration."
				);

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Scrambled serial data signals and long-period PRBS patterns are good choices for the calibration signal.");

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Avoid clocks, 8B/10B coded serial data signals, and short PRBS patterns (PRBS7, PRBS9) as these contain "
				"repeating patterns which can lead to false alignments.");

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Configure both channels with appropriate coupling, gain, offset, etc. for the calibration signal.");

			ChannelSelector("Primary", m_group->m_primary, m_primaryStream);
			ChannelSelector("Secondary", m_secondary, m_secondaryStream);

			if(ImGui::Button("Continue"))
				m_state = STATE_WELCOME_5;
			break;

		case STATE_WELCOME_5:

			ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
			ImGui::TextUnformatted("Reference Clock Setup");
			ImGui::PopFont();
			ImGui::Separator();

			ImGui::Bullet();
			ImGui::TextWrapped(
				"Connecting a common reference clock to both instruments is strongly recommended.\n"
				"It is possible to operate multi-instrument setups without a shared reference clock,\n"
				"however timebase drift will result in increasingly worse alignment between the waveforms\n"
				"at samples further away from the trigger point.");

			ImGui::Checkbox("Use external reference on primary", &m_useExtRefPrimary);
			ImGui::Checkbox("Use external reference on secondary", &m_useExtRefSecondary);

			if(ImGui::Button("Start"))
			{
				m_state = STATE_ACQUIRE;

				//Enable external ref on each if requested
				m_group->m_primary->SetUseExternalRefclk(m_useExtRefPrimary);
				m_secondary->SetUseExternalRefclk(m_useExtRefSecondary);

				//Record the current waveform timestamp on each channel (if any)
				//so we can check if new data has shown up
				{
					shared_lock<shared_mutex> lock(m_session.GetWaveformDataMutex());
					auto data = m_primaryStream.GetData();
					if(data)
					{
						m_lastTriggerTimestamp = data->m_startTimestamp;
						m_lastTriggerFs = data->m_startFemtoseconds;
					}
				}

				//Acquire the first test waveform
				m_group->Arm(TriggerGroup::TRIGGER_TYPE_SINGLE);
			}

			break;

		default:
			DoMainProcessingFlow();
			break;
	}

	return true;
}

void ScopeDeskewWizard::ChannelSelector(const char* name, Oscilloscope* scope, StreamDescriptor& stream)
{
	vector<StreamDescriptor> streams;
	vector<string> names;
	int sel = 0;

	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		auto chan = scope->GetChannel(i);

		//Skip it if not enabled (we need to be able to grab data off it)
		if(!scope->CanEnableChannel(i))
			continue;
		if(!scope->IsChannelEnabled(i))
			continue;

		for(size_t j=0; j<chan->GetStreamCount(); j++)
		{
			//only allow compatible channels that make sense to use as trigger sources
			auto stype = chan->GetType(j);
			switch(stype)
			{
				case Stream::STREAM_TYPE_ANALOG:
				case Stream::STREAM_TYPE_DIGITAL:
				case Stream::STREAM_TYPE_TRIGGER:
					break;

				//not usable as a trigger
				default:
					continue;
			}

			StreamDescriptor s(chan, j);

			if(stream == s)
				sel = streams.size();

			streams.push_back(s);
			names.push_back(s.GetName());
		}
	}

	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
	if(Combo(name, names, sel))
		stream = streams[sel];

	//If our stream is null, select the first input
	if(!stream)
		stream = streams[0];
}

void ScopeDeskewWizard::DoMainProcessingFlow()
{
	const int nWaveforms = 10;

	ImGui::PushFont(m_parent->GetFontPref("Appearance.General.title_font"));
	ImGui::TextUnformatted("Calibration Measurements");
	ImGui::PopFont();
	ImGui::Separator();

	//Draw progress table
	ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit |
		ImGuiTableFlags_NoKeepColumnsVisible;

	if(ImGui::BeginTable("groups", 3, flags))
	{
		float width = ImGui::GetFontSize();
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Acquire", ImGuiTableColumnFlags_WidthFixed, 10*width);
		ImGui::TableSetupColumn("Correlate", ImGuiTableColumnFlags_WidthFixed, 10*width);
		ImGui::TableSetupColumn("Skew", ImGuiTableColumnFlags_WidthFixed, 10*width);
		ImGui::TableHeadersRow();

		//Past measurements
		for(int i=0; i<m_measureCycle; i++)
		{
			ImGui::PushID(i);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Done");

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("Done");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("FIXME");

			ImGui::PopID();
		}

		//Current measurement
		if(m_state != STATE_DONE)
		{
			ImGui::PushID(m_measureCycle);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			ImGui::TableSetColumnIndex(0);
			if(m_state == STATE_ACQUIRE)
				ImGui::TextUnformatted("Acquiring");
			else
				ImGui::TextUnformatted("Done");

			ImGui::TableSetColumnIndex(1);
			if(m_state == STATE_CORRELATE)
				ImGui::TextUnformatted("Calculating");
			else
				ImGui::TextUnformatted("Pending");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("--");

			ImGui::PopID();
		}

		//Future measurements
		for(int i=m_measureCycle+1; i<nWaveforms; i++)
		{
			ImGui::PushID(i);
			ImGui::TableNextRow(ImGuiTableRowFlags_None);

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Pending");

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("Pending");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("--");

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	switch(m_state)
	{
		case STATE_ACQUIRE:
			{
				shared_lock<shared_mutex> lock(m_session.GetWaveformDataMutex());

				//Make sure we have a waveform
				auto data = m_primaryStream.GetData();
				if(!data)
					return;

				//If it's the same timestamp we're looking at stale data, nothing to do
				if( (m_lastTriggerTimestamp == data->m_startTimestamp) &&
					(m_lastTriggerFs == data->m_startFemtoseconds) )
				{
					return;
				}

				//New measurement! Record the timestamp
				m_lastTriggerTimestamp = data->m_startTimestamp;
				m_lastTriggerFs = data->m_startFemtoseconds;

				//We're now ready to do the correlation
				//TODO: actually do it
				m_state = STATE_CORRELATE;
			}
			break;

		case STATE_CORRELATE:
			{
				//TODO: check if the correlation is done

				m_measureCycle ++;

				//Done with acquisition?
				if(m_measureCycle >= nWaveforms)
				{
					m_state = STATE_DONE;
					return;
				}

				//Ready to grab next waveform
				m_group->Arm(TriggerGroup::TRIGGER_TYPE_SINGLE);
				m_state = STATE_ACQUIRE;
			}

		default:
			break;
	}
}
