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
	for(auto& it : m_packets)
	{
		for(auto p : it.second)
		{
			RemoveChildHistoryFrom(p);
			delete p;
		}
	}
	m_packets.clear();
	m_childPackets.clear();
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

	//Copy the new packets and detach them so the filter doesn't delete them.
	//Do the merging now
	{
		lock_guard<mutex> lock(m_mutex);

		auto& outpackets = m_packets[time];
		outpackets.clear();

		auto& packets = m_filter->GetPackets();
		auto npackets = packets.size();
		Packet* parentOfGroup = nullptr;
		Packet* firstChildPacketOfGroup = nullptr;
		Packet* lastPacket = nullptr;
		for(size_t i=0; i<npackets; i++)
		{
			auto p = packets[i];

			//See if we should start a new merge group
			bool starting_new_group;
			if(i+1 >= npackets)									//No next packet to merge with
				starting_new_group = false;
			else if(!m_filter->CanMerge(p, p, packets[i+1]))	//This packet isn't compatible with the next
				starting_new_group = false;
			else if(firstChildPacketOfGroup == nullptr)			//If we get here, we're merging. But are we already?
				starting_new_group = true;
			else												//Already in a group, but it's not the same as the new one
				starting_new_group = !m_filter->CanMerge(firstChildPacketOfGroup, lastPacket, p);

			if(starting_new_group)
			{
				//Create the summary packet
				firstChildPacketOfGroup = p;
				parentOfGroup = m_filter->CreateMergedHeader(p, i);
				outpackets.push_back(parentOfGroup);
			}

			//End a merge group
			else if( (firstChildPacketOfGroup != nullptr) && !m_filter->CanMerge(firstChildPacketOfGroup, lastPacket, p) )
			{
				firstChildPacketOfGroup = nullptr;
				parentOfGroup = nullptr;
			}

			//If we're a child of an group, add under the parent node
			if(parentOfGroup)
				m_childPackets[parentOfGroup].push_back(p);

			//Otherwise add at the top level
			else
				outpackets.push_back(p);

			lastPacket = p;
		}

		//TODO: apply filtering rules somewhere
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
	{
		RemoveChildHistoryFrom(p);
		delete p;
	}
	m_packets.erase(timestamp);
}

void PacketManager::RemoveChildHistoryFrom(Packet* pack)
{
	//For now, we can only have one level of hierarchy
	//so no need to check for children of children
	auto& children = m_childPackets[pack];
	for(auto p : children)
		delete p;
	m_childPackets.erase(pack);
}
