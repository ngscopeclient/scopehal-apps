/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
	@author Louis A. Goessling
	@brief Tool for measuring de-facto rate at which something is called
 */

#ifndef HzClock_h
#define HzClock_h

/**
	@brief Clock that measures rate at which it is called; windowed average
 */
class HzClock
{
public:
	HzClock(int depth = 32)
		: m_depth(depth)
	{
		Reset();
	}

	void Reset()
	{
		m_lastMs = GetTime();

		for (int i = 0; i < m_depth; i++)
		{
			m_deltas.push_back(0);
		}

		m_runningAverage = 0;
	}

	void Tick()
	{
		uint64_t now = GetTime() * 1000;
		uint64_t delta = now - m_lastMs;

		m_lastMs = now;

		m_runningAverage -= (double)m_deltas.front() / (double)m_depth;
		m_runningAverage += (double)delta / (double)m_depth;

		m_deltas.pop_front();
		m_deltas.push_back(delta);
	}

	double GetAverageMs()
	{
		return m_runningAverage;
	}

	double GetAverageHz()
	{
		return m_runningAverage==0 ? 0 : 1000./m_runningAverage;
	}

	double GetStdDev()
	{
		double dev = 0;

		for (auto i : m_deltas)
		{
			dev += pow(i - m_runningAverage, 2);
		}

		return sqrt(dev / m_depth);
	}

protected:
	int m_depth;
	uint64_t m_lastMs;
	std::deque<uint64_t> m_deltas;
	double m_runningAverage;
};

#endif
