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
	@brief Implementation of PacketManager
 */
#include "ngscopeclient.h"
#include "PacketManager.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PacketManager::PacketManager(PacketDecoder* pd)
	: m_filter(pd)
{

}

PacketManager::~PacketManager()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Waveform data processing

/**
	@brief Handle newly arrived waveform data (may be a change to parameters or a freshly arrived waveform)
 */
void PacketManager::Update()
{
	//Do nothing if there's no waveform to get a timestamp from
	auto data = m_filter->GetData(0);
	if(!data)
		return;
	TimePoint time(data->m_startTimestamp, data->m_startFemtoseconds);

	//If waveform is unchanged, no action needed
	WaveformCacheKey key(data);
	if(key == m_cachekey)
		return;

	//If we get here, waveform changed. Update cache key
	m_cachekey = key;

	//Remove any old history we might have had from this timestamp
	RemoveHistoryFrom(time);

	//Copy the new packets and detach them so the filter doesn't delete them
	{
		lock_guard<mutex> lock(m_mutex);
		m_packets[time] = m_filter->GetPackets();
	}
	m_filter->DetachPackets();
}

/**
	@brief Removes all history from the specified timestamp
 */
void PacketManager::RemoveHistoryFrom(TimePoint timestamp)
{
	lock_guard<mutex> lock(m_mutex);

	auto& packets = m_packets[timestamp];
	for(auto p : packets)
		delete p;
	m_packets.erase(timestamp);
}
