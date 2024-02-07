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

#include "ngscopeclient.h"
#include "TriggerGroup.h"
#include "Session.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TriggerGroup::TriggerGroup(Oscilloscope* primary, Session* session)
	: m_primary(primary)
	, m_default(true)
	, m_session(session)
	, m_multiScopeFreeRun(false)
{
}

TriggerGroup::~TriggerGroup()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument management

/**
	@brief Make a scope (which must currently be a secondary) the primary
 */
void TriggerGroup::MakePrimary(Oscilloscope* scope)
{
	m_secondaries.push_back(m_primary);
	m_primary = scope;

	//Remove the scope from the secondary
	for(size_t i=0; i<m_secondaries.size(); i++)
	{
		if(m_secondaries[i] == scope)
		{
			m_secondaries.erase(m_secondaries.begin() + i);
			return;
		}
	}

	//Turn on the trig-out port for the new primary
	m_primary->EnableTriggerOutput();
}

/**
	@brief Adds a secondary scope to this group
 */
void TriggerGroup::AddSecondary(Oscilloscope* scope)
{
	//If we do not have a primary, we're probably a filter-only group
	//Make the new scope the primary instead
	if(!m_primary)
	{
		m_primary = scope;
		return;
	}

	//Turn on the trig-out port for the primary if we didn't have any secondaries before
	if(m_secondaries.empty())
		m_primary->EnableTriggerOutput();

	m_secondaries.push_back(scope);
}

void TriggerGroup::RemoveScope(Oscilloscope* scope)
{
	if(m_primary == scope)
	{
		//If we have any secondaries, promote the first secondary to primary
		if(!m_secondaries.empty())
		{
			m_primary = m_secondaries[0];
			m_secondaries.erase(m_secondaries.begin());
		}
		else
			m_primary = nullptr;
	}

	//Remove from the secondary list
	for(size_t i=0; i<m_secondaries.size(); i++)
	{
		if(m_secondaries[i] == scope)
		{
			m_secondaries.erase(m_secondaries.begin() + i);
			return;
		}
	}
}

void TriggerGroup::RemoveFilter(PausableFilter* f)
{
	for(size_t i=0; i<m_filters.size(); i++)
	{
		if(m_filters[i] == f)
		{
			m_filters.erase(m_filters.begin() + i);
			return;
		}
	}
}

void TriggerGroup::AddFilter(PausableFilter* f)
{
	m_filters.push_back(f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triggering

/**
	@brief Arm the trigger for the group
 */
void TriggerGroup::Arm(TriggerType type)
{
	if(m_primary)
		LogTrace("Arming trigger for group %s\n", m_primary->m_nickname.c_str());
	else
		LogTrace("Arming trigger for filter group\n");
	LogIndenter li;

	bool oneshot = (type == TriggerGroup::TRIGGER_TYPE_FORCED) || (type == TriggerGroup::TRIGGER_TYPE_SINGLE);

	//In multi-scope mode, make sure all scopes are stopped with no pending waveforms
	if(!m_secondaries.empty())
	{
		lock_guard<shared_mutex> lock(m_session->GetWaveformDataMutex());

		for(auto scope : m_secondaries)
		{
			scope->Stop();

			if(scope->HasPendingWaveforms())
			{
				LogWarning("Scope %s had pending waveforms before arming\n", scope->m_nickname.c_str());
				scope->ClearPendingWaveforms();
			}
		}

		m_primary->Stop();

		if(m_primary->HasPendingWaveforms())
		{
			LogWarning("Scope %s had pending waveforms before arming\n", m_primary->m_nickname.c_str());
			m_primary->ClearPendingWaveforms();
		}
	}

	//We're in multiscope normal mode if we're doing a non-oneshot trigger and have secondaries
	m_multiScopeFreeRun = !oneshot && !m_secondaries.empty();

	//Start secondaries (always in single shot mode)
	for(auto scope : m_secondaries)
	{
		LogTrace("Starting trigger for secondary scope %s\n", scope->m_nickname.c_str());
		scope->StartSingleTrigger();
	}

	//Verify all secondaries are armed
	for(auto scope : m_secondaries)
	{
		double start = GetTime();

		while(!scope->PeekTriggerArmed())
		{
			//After 3 sec of no activity, time out
			//(must be longer than the default 2 sec socket timeout)
			double now = GetTime();
			if( (now - start) > 3)
			{
				LogWarning("Timeout waiting for scope %s to arm\n",  scope->m_nickname.c_str());
				scope->Stop();
				scope->StartSingleTrigger();
				start = now;
			}
		}
		LogTrace("Secondary is armed\n");

		//Scope is armed. Clear any garbage in the pending queue
		//TODO: this should now be redundant, but verify?
		scope->ClearPendingWaveforms();
	}

	//Start the primary normally
	//But if we have secondaries, do a single trigger so it doesn't re-arm before we've set up the secondaries
	if(m_primary)
	{
		switch(type)
		{
			case TriggerGroup::TRIGGER_TYPE_NORMAL:
				if(!m_secondaries.empty())
				{
					LogTrace("Starting trigger for primary\n");
					m_primary->StartSingleTrigger();
				}
				else
					m_primary->Start();
				break;

			case TriggerGroup::TRIGGER_TYPE_AUTO:
				LogError("ArmTrigger(TRIGGER_TYPE_AUTO) not implemented\n");
				break;

			case TriggerGroup::TRIGGER_TYPE_SINGLE:
				m_primary->StartSingleTrigger();
				break;

			case TriggerGroup::TRIGGER_TYPE_FORCED:
				m_primary->ForceTrigger();
				break;


			default:
				break;
		}
	}

	//Start our filters
	for(auto f : m_filters)
	{
		if(type == TriggerGroup::TRIGGER_TYPE_SINGLE)
			f->Single();
		else
			f->Run();
	}
}

string TriggerGroup::GetDescription()
{
	if(m_primary)
		return m_primary->m_nickname;
	else if(!m_filters.empty())
		return m_filters[0]->GetDisplayName();
	else
		return "(empty)";
}

/**
	@brief Stop the trigger for the group

	Clear out any pending data (the user doesn't want it, and we don't want stale stuff hanging around)
 */
void TriggerGroup::Stop()
{
	m_multiScopeFreeRun = false;

	if(m_primary)
	{
		m_primary->Stop();
		m_primary->ClearPendingWaveforms();
	}

	for(auto scope : m_secondaries)
	{
		scope->Stop();
		scope->ClearPendingWaveforms();
	}

	for(auto f : m_filters)
		f->Stop();
}

/**
	@brief Return true if all of the scopes in the group have triggered
 */
bool TriggerGroup::CheckForPendingWaveforms()
{
	if(!m_primary)
		return false;

	//Make sure we have pending waveforms on everything
	if(!m_primary->HasPendingWaveforms())
		return false;
	for(auto scope : m_secondaries)
	{
		if(!scope->HasPendingWaveforms())
			return false;
	}

	/*
	//Keep track of when the primary instrument triggers.
	if(m_multiScopeFreeRun)
	{
		//See when the primary triggered
		if( (m_tPrimaryTrigger < 0) && m_oscilloscopes[0]->HasPendingWaveforms() )
			m_tPrimaryTrigger = GetTime();

		//All instruments should trigger within 1 sec (arbitrary threshold) of the primary.
		//If it's been longer than that, something went wrong. Discard all pending data and re-arm the trigger.
		double twait = GetTime() - m_tPrimaryTrigger;
		if( (m_tPrimaryTrigger > 0) && ( twait > 1 ) )
		{
			LogWarning("Timed out waiting for one or more secondary instruments to trigger (%.2f ms). Resetting...\n",
				twait*1000);

			//Cancel any pending triggers
			StopTrigger();

			//Discard all pending waveform data
			for(auto scope : m_oscilloscopes)
			{
				//Don't touch anything offline
				if(scope->IsOffline())
					continue;

				scope->IDPing();
				scope->ClearPendingWaveforms();
			}

			//Re-arm the trigger and get back to polling
			ArmTrigger(TriggerGroup::TRIGGER_TYPE_NORMAL);
			return false;
		}
	}
	*/

	//If we get here, we had waveforms on all instruments
	return true;
}

/**
	@brief Grab waveforms from the group
 */
void TriggerGroup::DownloadWaveforms()
{
	//Grab the data from the primary
	if(!m_primary->IsAppendingToWaveform())
		DetachAllWaveforms(m_primary);
	m_primary->PopPendingWaveform();

	//All good if we're a single-scope trigger group.
	//If not, we have more work to do
	if(m_secondaries.empty())
		return;

	LogTrace("Multi scope: patching timestamps\n");

	//Get the timestamp of the primary scope's first waveform
	bool hit = false;
	time_t timeSec = 0;
	int64_t timeFs  = 0;
	for(size_t i=0; i<m_primary->GetChannelCount(); i++)
	{
		auto chan = m_primary->GetOscilloscopeChannel(i);
		if(!chan)
			continue;
		for(size_t j=0; j<chan->GetStreamCount(); j++)
		{
			auto data = chan->GetData(j);
			if(data != nullptr)
			{
				timeSec = data->m_startTimestamp;
				timeFs = data->m_startFemtoseconds;
				hit = true;
				break;
			}
		}
		if(hit)
			break;
	}

	//Grab the data from secondaries and retcon the timestamps so they match the primary's trigger
	for(auto scope : m_secondaries)
	{
		if(!scope->IsAppendingToWaveform())
			DetachAllWaveforms(scope);
		scope->PopPendingWaveform();

		for(size_t j=0; j<scope->GetChannelCount(); j++)
		{
			auto chan = scope->GetOscilloscopeChannel(j);
			if(!chan)
				continue;
			for(size_t k=0; k<chan->GetStreamCount(); k++)
			{
				auto data = chan->GetData(k);
				if(data == nullptr)
					continue;

				data->m_startTimestamp = timeSec;
				data->m_startFemtoseconds = timeFs;
				data->m_triggerPhase -= m_session->GetDeskew(scope);
			}
		}
	}
}

void TriggerGroup::DetachAllWaveforms(Oscilloscope* scope)
{
	//Detach old waveforms since they're now owned by history manager
	for(size_t i=0; i<scope->GetChannelCount(); i++)
	{
		auto chan = scope->GetOscilloscopeChannel(i);
		if(!chan)
			continue;

		for(size_t j=0; j<chan->GetStreamCount(); j++)
			chan->Detach(j);
	}
}

void TriggerGroup::RearmIfMultiScope()
{
	if(m_multiScopeFreeRun)
		Arm(TRIGGER_TYPE_NORMAL);
}
