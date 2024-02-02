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
	@brief Declaration of Marker
 */
#ifndef Marker_h
#define Marker_h

/**
	@brief A timestamp, measured in seconds + femtoseconds
 */
class TimePoint : public std::pair<time_t, int64_t>
{
public:
	TimePoint(time_t sec, int64_t fs)
	: std::pair<time_t, int64_t>(sec, fs)
	{}

	time_t GetSec() const
	{ return first; }

	int64_t GetFs() const
	{ return second; }

	void SetSec(time_t sec)
	{ first = sec; }

	void SetFs(int64_t fs)
	{ second = fs; }

	std::string PrettyPrint() const;
};

/**
	@brief Data for a marker

	A marker is similar to a cursor, but is persistent and attached to a point in absolute time (a specific location
	within a specific acquisition). Markers, unlike cursors, can be named.
 */
class Marker
{
public:
	Marker(TimePoint t, int64_t o, const std::string& n)
	: m_timestamp(t)
	, m_offset(o)
	, m_name(n)
	{}

	///@brief Timestamp of the parent waveform (UTC)
	TimePoint m_timestamp;

	///@brief Position of the marker within the parent waveform (X axis units)
	int64_t m_offset;

	/**
		@brief Helper to get the absolute timestamp of the marker
	 */
	TimePoint GetMarkerTime()
	{ return TimePoint(m_timestamp.first, m_timestamp.second + m_offset); }

	///@brief Display name of the marker
	std::string m_name;

	///@brief Helper for sorting
	bool operator<(const Marker& rhs) const
	{
		if(m_timestamp < rhs.m_timestamp)
			return true;
		if(m_timestamp > rhs.m_timestamp)
			return false;
		if(m_offset < rhs.m_offset)
			return true;
		return false;
	}
};

#endif
