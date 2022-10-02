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

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HistoryPoint

HistoryPoint::HistoryPoint()
	: m_timestamp(0)
	, m_fs(0)
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HistoryManager::HistoryManager()
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
	time_t stamp = 0;
	int64_t fs = 0;

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
					stamp = wfm->m_startTimestamp;
					fs = wfm->m_startFemtoseconds;
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
	pt->m_timestamp = stamp;
	pt->m_fs = fs;
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
	while(m_history.size() > 10)
		m_history.pop_front();
}
