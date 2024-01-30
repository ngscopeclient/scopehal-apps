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
	@brief Declaration of PacketManager
 */
#ifndef PacketManager_h
#define PacketManager_h

#include "../../lib/scopehal/PacketDecoder.h"
#include "Marker.h"

/**
	@brief Context data for a single row (used for culling)
 */
class RowData
{
public:
	RowData()
	: m_height(0)
	, m_totalHeight(0)
	, m_stamp(0, 0)
	, m_packet(nullptr)
	{}

	RowData(TimePoint t, Packet* p)
	: m_height(0)
	, m_totalHeight(0)
	, m_stamp(t)
	, m_packet(p)
	{}

	///@brief Height of this row
	double m_height;

	///@brief Total height of the entire list up to this point
	double m_totalHeight;

	///@brief Timestamp of the waveform this packet came from
	TimePoint m_stamp;

	///@brief The packet in this row
	Packet* m_packet;
};

class ProtocolDisplayFilter;

class ProtocolDisplayFilterClause
{
public:
	ProtocolDisplayFilterClause(std::string str, size_t& i);
	ProtocolDisplayFilterClause(const ProtocolDisplayFilterClause&) =delete;
	ProtocolDisplayFilterClause& operator=(const ProtocolDisplayFilterClause&) =delete;

	virtual ~ProtocolDisplayFilterClause();

	bool Validate(std::vector<std::string> headers);

	std::string Evaluate(const Packet* pack);

	static std::string EatSpaces(std::string str);

	enum
	{
		TYPE_DATA,
		TYPE_IDENTIFIER,
		TYPE_STRING,
		TYPE_REAL,
		TYPE_INT,
		TYPE_EXPRESSION,
		TYPE_ERROR
	} m_type;

	std::string m_identifier;
	std::string m_string;
	float m_real;
	long m_long;
	ProtocolDisplayFilter* m_expression;
	bool m_invert;
};

class ProtocolDisplayFilter
{
public:
	ProtocolDisplayFilter(std::string str, size_t& i);
	ProtocolDisplayFilter(const ProtocolDisplayFilterClause&) =delete;
	ProtocolDisplayFilter& operator=(const ProtocolDisplayFilter&) =delete;
	virtual ~ProtocolDisplayFilter();

	static void EatSpaces(std::string str, size_t& i);

	bool Validate(std::vector<std::string> headers, bool nakedLiteralOK = false);

	bool Match(const Packet* pack);
	std::string Evaluate(const Packet* pack);

protected:
	std::vector<ProtocolDisplayFilterClause*> m_clauses;
	std::vector<std::string> m_operators;
};

/**
	@brief Keeps track of packetized data history from a single protocol analyzer filter
 */
class PacketManager
{
public:
	PacketManager(PacketDecoder* pd);
	virtual ~PacketManager();

	void Update();
	void RemoveHistoryFrom(TimePoint timestamp);

	std::recursive_mutex& GetMutex()
	{ return m_mutex; }

	const std::map<TimePoint, std::vector<Packet*> >& GetPackets()
	{ return m_packets; }

	const std::vector<Packet*>& GetChildPackets(Packet* pack)
	{ return m_childPackets[pack]; }

	const std::map<TimePoint, std::vector<Packet*> >& GetFilteredPackets()
	{ return m_filteredPackets; }

	const std::vector<Packet*>& GetFilteredChildPackets(Packet* pack)
	{ return m_filteredChildPackets[pack]; }

	/**
		@brief Sets the current filter expression
	 */
	void SetDisplayFilter(std::shared_ptr<ProtocolDisplayFilter> filter)
	{
		m_filterExpression = filter;
		FilterPackets();
	}

	void FilterPackets();

	bool IsChildOpen(Packet* pack)
	{ return m_lastChildOpen[pack]; }

	void SetChildOpen(Packet* pack, bool open)
	{ m_lastChildOpen[pack] = open; }

	std::vector<RowData>& GetRows()
	{ return m_rows; }

protected:
	void RemoveChildHistoryFrom(Packet* pack);

	///@brief Mutex controlling access to m_packets
	std::recursive_mutex m_mutex;

	///@brief The filter we're managing
	PacketDecoder* m_filter;

	///@brief Our saved packet data
	std::map<TimePoint, std::vector<Packet*> > m_packets;

	///@brief Merged child packets
	std::map<Packet*, std::vector<Packet*> > m_childPackets;

	///@brief Subset of m_packets that passed the current filter expression
	std::map<TimePoint, std::vector<Packet*> > m_filteredPackets;

	///@brief Subset of m_filteredChildPackets that passed the current filter expression
	std::map<Packet*, std::vector<Packet*> > m_filteredChildPackets;

	///@brief Cache key for the current waveform
	WaveformCacheKey m_cachekey;

	///@brief Current filter expression
	std::shared_ptr<ProtocolDisplayFilter> m_filterExpression;

	///@brief Update the list of rows being displayed
	void RefreshRows();

	///@brief The set of rows that are to be displayed, based on current tree expansion and filter state
	std::vector<RowData> m_rows;

	///@brief Map of packets to child-open flags from last frame
	std::map<Packet*, bool> m_lastChildOpen;
};

#endif
