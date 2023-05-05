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
	@brief Declaration of HistoryManager
 */
#ifndef HistoryManager_h
#define HistoryManager_h

#include "Marker.h"

//Waveform history for a single instrument
typedef std::map<StreamDescriptor, WaveformBase*> WaveformHistory;

/**
	@brief A single point of waveform history
 */
class HistoryPoint
{
public:
	HistoryPoint();
	~HistoryPoint();

	///@brief Timestamp of the point
	TimePoint m_time;

	///@brief Set true to "pin" this waveform so it won't be purged from history regardless of age
	bool m_pinned;

	///@brief Free-form text nickname for this acquisition (may be blank)
	std::string m_nickname;

	///@brief Waveform data
	std::map<Oscilloscope*, WaveformHistory> m_history;

	void LoadHistoryToSession(Session& session);
};

/**
	@brief Keeps track of recently acquired waveforms
 */
class HistoryManager
{
public:
	HistoryManager(Session& session);
	~HistoryManager();

	void AddHistory(
		const std::vector<Oscilloscope*>& scopes,
		bool deleteOld = true,
		bool pin = false,
		std::string nick = "");

	void SetMaxToCurrentDepth()
	{ m_maxDepth = m_history.size(); }

	std::shared_ptr<HistoryPoint> GetHistory(TimePoint t);

	TimePoint GetMostRecentPoint();

	void clear()
	{ m_history.clear(); }

	std::list<std::shared_ptr<HistoryPoint>> m_history;

	///@brief has to be an int for imgui compatibility
	int m_maxDepth;

protected:
	Session& m_session;
};

#endif
