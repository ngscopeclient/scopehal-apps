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
	@brief Implementation of HistoryManager
 */
#include "ngscopeclient.h"
#include "HistoryManager.h"
#include "Session.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HistoryPoint

HistoryPoint::HistoryPoint()
	: m_time(0, 0)
	, m_pinned(false)
	, m_nickname("")
{
}

HistoryPoint::~HistoryPoint()
{
	for(auto it : m_history)
	{
		auto scope = it.first;
		auto hist = it.second;
		for(auto jt : hist)
		{
			auto wfm = jt.second;

			//Add known waveform types to pool for reuse
			//Delete anything else
			//TODO: this assumes the waveforms are currently configured for GPU-local or mirrored memory.
			//This will have to change when we start paging old waveforms out to disk.
			if(dynamic_cast<UniformAnalogWaveform*>(wfm) != nullptr)
				scope->AddWaveformToAnalogPool(wfm);
			else if(dynamic_cast<SparseDigitalWaveform*>(wfm) != nullptr)
				scope->AddWaveformToDigitalPool(wfm);
			else
				delete wfm;
		}
	}
}

/**
	@brief Update all instruments in the specified session with our saved historical data
 */
void HistoryPoint::LoadHistoryToSession(Session& session)
{
	//We don't want to keep capturing if we're trying to look at a historical waveform. That would be a bit silly.
	session.StopTrigger();

	//Go over each scope in the session and load the relevant history
	//We do this rather than just looping over the scopes in the history so that we can handle missing data.
	auto scopes = session.GetScopes();
	for(auto scope : scopes)
	{
		//Scope is not in history! Must have been added recently
		//Set all channels' data to null
		if(m_history.find(scope) == m_history.end() )
		{
			for(size_t i=0; i<scope->GetChannelCount(); i++)
			{
				auto chan = scope->GetChannel(i);
				for(size_t j=0; j<chan->GetStreamCount(); j++)
				{
					chan->Detach(j);
					chan->SetData(nullptr, j);
				}
			}
		}

		//Scope is in history. Load our saved waveform data
		else
		{
			LogTrace("Loading saved history\n");
			auto hist = m_history[scope];
			for(auto it : hist)
			{
				auto stream = it.first;
				stream.m_channel->Detach(stream.m_stream);
				stream.m_channel->SetData(it.second, stream.m_stream);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HistoryManager::HistoryManager(Session& session)
	: m_maxDepth(10)
	, m_session(session)
{
}

HistoryManager::~HistoryManager()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// History processing

void HistoryManager::AddHistory(const vector<Oscilloscope*>& scopes)
{
	bool foundTimestamp = false;
	TimePoint tp(0,0);

	//First pass: find first waveform with a timestamp
	for(auto scope : scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
			{
				auto wfm = chan->GetData(j);
				if(wfm)
				{
					tp.SetSec(wfm->m_startTimestamp);
					tp.SetFs(wfm->m_startFemtoseconds);
					foundTimestamp = true;
					break;
				}
			}
		}
	}

	//If we get here, there were no waveforms anywhere!
	//Nothing for us to do
	if(!foundTimestamp)
		return;

	//All good. Generate a new history point and add it
	auto pt = make_shared<HistoryPoint>();
	m_history.push_back(pt);
	pt->m_time = tp;
	pt->m_pinned = false;

	//Add waveforms
	for(auto scope : scopes)
	{
		WaveformHistory hist;

		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				hist[StreamDescriptor(chan, j)] = chan->GetData(j);
		}

		pt->m_history[scope] = hist;
	}

	//TODO: check history size in MB/GB etc
	//TODO: convert older stuff to disk, free GPU memory, etc?
	while(m_history.size() > (size_t) m_maxDepth)
	{
		bool deletedSomething = false;

		//Delete first un-pinned entry
		for(auto it = m_history.begin(); it != m_history.end(); it++)
		{
			auto& point = (*it);
			if(point->m_pinned)
				continue;
			if(!m_session.GetMarkers(point->m_time).empty())
				continue;

			m_session.RemoveMarkers(point->m_time);
			m_history.erase(it);
			deletedSomething = true;
			break;
		}

		//If nothing deleted, all remaining items are pinned. Stop.
		if(!deletedSomething)
			break;
	}
}

/**
	@brief Gets the timestamp of the most recent waveform
 */
TimePoint HistoryManager::GetMostRecentPoint()
{
	if(m_history.empty())
		return TimePoint(0, 0);
	else
		return (*m_history.rbegin())->m_time;
}

/**
	@brief Gets the history point for a specific timestamp
 */
shared_ptr<HistoryPoint> HistoryManager::GetHistory(TimePoint t)
{
	for(auto it : m_history)
	{
		if(it->m_time == t)
			return it;
	}

	return nullptr;
}
