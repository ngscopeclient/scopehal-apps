/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
#include "Session.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PacketManager::PacketManager(PacketDecoder* pd, Session& session)
	: m_session(session)
	, m_filter(pd)
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

void PacketManager::RefreshRows()
{
	LogTrace("Refreshing rows\n");

	lock_guard<recursive_mutex> lock(m_mutex);

	//Clear all existing row state
	m_rows.clear();

	//Make a list of waveform timestamps and make sure we display them in order
	vector<TimePoint> times;
	for(auto& it : m_filteredPackets)
		times.push_back(it.first);
	std::sort(times.begin(), times.end());

	double totalHeight = 0;
	double lineheight = ImGui::CalcTextSize("dummy text").y;
	double padding = ImGui::GetStyle().CellPadding.y;

	//Process packets from each waveform
	for(auto wavetime : times)
	{
		auto& wpackets = m_filteredPackets[wavetime];

		//Get markers for this waveform, if any
		auto& markers = m_session.GetMarkers(wavetime);
		size_t imarker = 0;
		int64_t lastoff = 0;

		for(auto pack : wpackets)
		{
			//Add marker before this packet if needed
			//(loop because we might have two or more markers between packets)
			while( (imarker < markers.size()) &&
				(markers[imarker].m_offset >= lastoff) &&
				(markers[imarker].m_offset < pack->m_offset) )
			{
				RowData row(wavetime, markers[imarker]);
				row.m_height = padding*2 + lineheight;
				totalHeight += row.m_height;
				row.m_totalHeight = totalHeight;
				m_rows.push_back(row);

				imarker ++;
			}

			//See if we have child packets
			auto children = m_filteredChildPackets[pack];

			//Add an entry for the top level
			RowData dat(wavetime, pack);
			lastoff = pack->m_offset;

			//Calculate row height
			double height = padding*2 + lineheight;

			//Integrate heights
			dat.m_height = height;
			totalHeight += height;
			dat.m_totalHeight = totalHeight;

			//Save this row
			m_rows.push_back(dat);

			if(IsChildOpen(pack))
			{
				for(auto child : children)
				{
					//Add an entry for the top level
					RowData cdat(wavetime, child);

					//Calculate row height
					height = padding*2 + lineheight;

					//Integrate heights
					cdat.m_height = height;
					totalHeight += height;
					cdat.m_totalHeight = totalHeight;

					//Save this row
					m_rows.push_back(cdat);
				}
			}
		}
	}
}

void PacketManager::OnMarkerChanged()
{
	lock_guard<recursive_mutex> lock(m_mutex);
	RefreshRows();
}

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

	LogTrace("Updating\n");

	//If we get here, waveform changed. Update cache key
	m_cachekey = key;

	//Remove any old history we might have had from this timestamp
	RemoveHistoryFrom(time);

	//Copy the new packets and detach them so the filter doesn't delete them.
	//Do the merging now
	{
		lock_guard<recursive_mutex> lock(m_mutex);

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
	}
	m_filter->DetachPackets();

	//Run filters
	FilterPackets();
}

/**
	@brief Run the filter expression against the packets
 */
void PacketManager::FilterPackets()
{
	//If we do NOT have a filter, early out: just copy stuff
	if(m_filterExpression == nullptr)
	{
		m_filteredPackets = m_packets;
		m_filteredChildPackets = m_childPackets;

		//but still refresh the set of rows being displayed
		RefreshRows();

		return;
	}

	//We have a filter! Start out by clearing output, then we can re-add the ones that match
	m_filteredPackets.clear();
	m_filteredChildPackets.clear();

	//Check all top level packets against the filter
	for(auto it : m_packets)
	{
		auto timestamp = it.first;
		auto& packets = it.second;
		for(auto p : packets)
		{
			//If no children, just check the top level packet for a match
			if(m_childPackets[p].empty())
			{
				if(m_filterExpression->Match(p))
					m_filteredPackets[timestamp].push_back(p);
			}

			//We have children.
			//Check them for matches, and add the parent if any child matches
			else
			{
				bool anyChildMatched = false;
				for(auto c : m_childPackets[p])
				{
					if(m_filterExpression->Match(c))
					{
						m_filteredChildPackets[p].push_back(c);
						anyChildMatched = true;
					}
				}
				if(anyChildMatched)
					m_filteredPackets[timestamp].push_back(p);
			}
		}
	}

	//Refresh the set of rows being displayed
	RefreshRows();
}

/**
	@brief Removes all history from the specified timestamp
 */
void PacketManager::RemoveHistoryFrom(TimePoint timestamp)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	auto& packets = m_packets[timestamp];
	for(auto p : packets)
	{
		RemoveChildHistoryFrom(p);
		delete p;
	}
	m_packets.erase(timestamp);

	m_filteredPackets.erase(timestamp);

	//update the list of displayed rows so we don't have anything left pointing to stale packets
	RefreshRows();
}

void PacketManager::RemoveChildHistoryFrom(Packet* pack)
{
	//For now, we can only have one level of hierarchy
	//so no need to check for children of children
	auto& children = m_childPackets[pack];
	for(auto p : children)
		delete p;
	m_childPackets.erase(pack);
	m_filteredChildPackets.erase(pack);
	m_lastChildOpen.erase(pack);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolDisplayFilter

ProtocolDisplayFilter::ProtocolDisplayFilter(string str, size_t& i)
{
	//One or more clauses separated by operators
	while(i < str.length())
	{
		//Read the clause
		m_clauses.push_back(new ProtocolDisplayFilterClause(str, i));

		//Remove spaces before the operator
		EatSpaces(str, i);
		if( (i >= str.length()) || (str[i] == ')') || (str[i] == ']') )
			break;

		//Read the operator, if any
		string tmp;
		while(i < str.length())
		{
			if(isspace(str[i]) || (str[i] == '\"') || (str[i] == '(') || (str[i] == ')') )
				break;

			//An alphanumeric character after an operator other than text terminates it
			if( (tmp != "") && !isalnum(tmp[0]) && isalnum(str[i]) )
				break;

			tmp += str[i];
			i++;
		}
		m_operators.push_back(tmp);
	}
}

ProtocolDisplayFilter::~ProtocolDisplayFilter()
{
	for(auto c : m_clauses)
		delete c;
}

bool ProtocolDisplayFilter::Validate(vector<string> headers, bool nakedLiteralOK)
{
	//No clauses? valid all-pass filter
	if(m_clauses.empty())
		return true;

	//We should always have one more clause than operator
	if( (m_operators.size() + 1) != m_clauses.size())
		return false;

	//Operators must make sense. For now only equal/unequal and boolean and/or allowed
	for(auto op : m_operators)
	{
		if( (op != "==") &&
			(op != "!=") &&
			(op != "||") &&
			(op != "&&") &&
			(op != "startswith") &&
			(op != "contains")
		)
		{
			return false;
		}
	}

	//If any clause is invalid, we're invalid
	for(auto c : m_clauses)
	{
		if(!c->Validate(headers))
			return false;
	}

	//A single literal is not a legal filter, it has to be compared to something
	//(But for sub-expressions used as indexes etc, it's OK)
	if(!nakedLiteralOK)
	{
		if(m_clauses.size() == 1)
		{
			if(m_clauses[0]->m_type != ProtocolDisplayFilterClause::TYPE_EXPRESSION)
				return false;
		}
	}

	return true;
}

void ProtocolDisplayFilter::EatSpaces(string str, size_t& i)
{
	while( (i < str.length()) && isspace(str[i]) )
		i++;
}

bool ProtocolDisplayFilter::Match(const Packet* pack)
{
	if(m_clauses.empty())
		return true;
	else
		return Evaluate(pack) != "0";
}

string ProtocolDisplayFilter::Evaluate(const Packet* pack)
{
	//Calling code checks for validity so no need to verify here

	//For now, all operators have equal precedence and are evaluated left to right.
	string current = m_clauses[0]->Evaluate(pack);
	for(size_t i=1; i<m_clauses.size(); i++)
	{
		string rhs = m_clauses[i]->Evaluate(pack);
		string op = m_operators[i-1];

		bool a = (current != "0");
		bool b = (rhs != "0");

		//== and != do exact string equality checks
		bool temp = false;
		if(op == "==")
			temp = (current == rhs);
		else if(op == "!=")
			temp = (current != rhs);

		//&& and || do boolean operations
		else if(op == "&&")
			temp = (a && b);
		else if(op == "||")
			temp = (a || b);

		//String prefix
		else if(op == "startswith")
			temp = (current.find(rhs) == 0);
		else if(op == "contains")
			temp = (current.find(rhs) != string::npos);

		//done, convert back to string
		current = temp ? "1" : "0";
	}
	return current;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProtocolDisplayFilterClause

ProtocolDisplayFilterClause::ProtocolDisplayFilterClause(string str, size_t& i)
{
	ProtocolDisplayFilter::EatSpaces(str, i);

	m_real = 0;
	m_long = 0;
	m_expression = 0;
	m_invert = false;

	//Parenthetical expression
	if( (str[i] == '(') || (str[i] == '!') )
	{
		//Inversion
		if(str[i] == '!')
		{
			m_invert = true;
			i++;

			if(str[i] != '(')
			{
				m_type = TYPE_ERROR;
				i++;
				return;
			}
		}

		i++;
		m_type = TYPE_EXPRESSION;
		m_expression = new ProtocolDisplayFilter(str, i);

		//eat trailing spaces
		ProtocolDisplayFilter::EatSpaces(str, i);

		//expect closing parentheses
		if(str[i] != ')')
			m_type = TYPE_ERROR;
		i++;
	}

	//Quoted string
	else if(str[i] == '\"')
	{
		m_type = TYPE_STRING;
		i++;

		while( (i < str.length()) && (str[i] != '\"') )
		{
			m_string += str[i];
			i++;
		}

		if(str[i] != '\"')
			m_type = TYPE_ERROR;

		i++;
	}

	//Number
	else if(isdigit(str[i]) || (str[i] == '-') || (str[i] == '.') )
	{
		string tmp;
		while( (i < str.length()) && (isdigit(str[i]) || (str[i] == '-')  || (str[i] == '.') || (str[i] == 'x')) )
		{
			tmp += str[i];
			i++;
		}

		//Hex string
		if(tmp.find("0x") == 0)
		{
			sscanf(tmp.c_str(), "%lx", (unsigned long*)&m_long);
			m_type = TYPE_INT;
		}

		//Number with decimal point
		else if(tmp.find('.') != string::npos)
		{
			m_real = atof(tmp.c_str());
			m_type = TYPE_REAL;
		}

		//Number without decimal point
		else
		{
			m_real = atol(tmp.c_str());
			m_type = TYPE_INT;
		}
	}

	//Identifier (or data)
	else
	{
		m_type = TYPE_IDENTIFIER;

		while( (i < str.length()) && isalnum(str[i]) )
		{
			m_identifier += str[i];
			i++;
		}

		//Opening square bracket
		if(str[i] == '[')
		{
			if(m_identifier == "data")
			{
				m_type = TYPE_DATA;
				i++;

				//Read the index expression
				m_expression = new ProtocolDisplayFilter(str, i);

				//eat trailing spaces
				ProtocolDisplayFilter::EatSpaces(str, i);

				//expect closing square bracket
				if(str[i] != ']')
					m_type = TYPE_ERROR;
				i++;
			}

			else
			{
				m_type = TYPE_ERROR;
				i++;
			}
		}

		if(m_identifier == "")
		{
			i++;
			m_type = TYPE_ERROR;
		}
	}
}

/**
	@brief Returns a copy of the input string with spaces removed
 */
string ProtocolDisplayFilterClause::EatSpaces(string str)
{
	string ret;
	for(auto c : str)
	{
		if(!isspace(c))
			ret += c;
	}
	return ret;
}

string ProtocolDisplayFilterClause::Evaluate(const Packet* pack)
{
	char tmp[32];

	switch(m_type)
	{
		case TYPE_DATA:
			{
				string sindex = m_expression->Evaluate(pack);
				int index = atoi(sindex.c_str());

				//Bounds check
				if(pack->m_data.size() <= (size_t)index)
					return "NaN";

				return to_string(pack->m_data[index]);
			}
			break;

		case TYPE_IDENTIFIER:
			{
				auto it = pack->m_headers.find(m_identifier);
				if(it != pack->m_headers.end())
					return it->second;
				else
					return "NaN";
			}

		case TYPE_STRING:
			return m_string;

		case TYPE_REAL:
			snprintf(tmp, sizeof(tmp), "%f", m_real);
			return tmp;

		case TYPE_INT:
			snprintf(tmp, sizeof(tmp), "%ld", m_long);
			return tmp;

		case TYPE_EXPRESSION:
			if(m_invert)
			{
				if(m_expression->Evaluate(pack) == "1")
					return "0";
				else
					return "1";
			}
			else
				return m_expression->Evaluate(pack);

		case TYPE_ERROR:
		default:
			return "NaN";
	}

	//never happens because of the 'default" clause, but prevents -Wreturn-type warning with some gcc versions
	return "NaN";
}

ProtocolDisplayFilterClause::~ProtocolDisplayFilterClause()
{
	if(m_expression)
		delete m_expression;
}

bool ProtocolDisplayFilterClause::Validate(vector<string> headers)
{
	switch(m_type)
	{
		case TYPE_ERROR:
			return false;

		case TYPE_DATA:
			return m_expression->Validate(headers, true);

		//If we're an identifier, we must be a valid header field
		//TODO: support comparisons on data
		case TYPE_IDENTIFIER:
			for(auto h : headers)
			{
				//Match, removing spaces from header names if needed
				//Note that m_identifier is now the real, un-spaced version of the identifier name
				//so we can look it up in the packet
				if(EatSpaces(h) == m_identifier)
				{
					m_identifier = h;
					return true;
				}
			}

			return false;

		//If we're an expression, it must be valid
		case TYPE_EXPRESSION:
			return m_expression->Validate(headers);

		default:
			return true;
	}
}
