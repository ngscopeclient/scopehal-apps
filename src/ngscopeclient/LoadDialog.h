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
	@brief Declaration of LoadDialog
 */
#ifndef LoadDialog_h
#define LoadDialog_h

#include "Dialog.h"
#include "Session.h"

#include <future>

/**
	@brief UI state for a single load channel

	Stores uncommitted values we haven't pushed to hardware, etc
 */
class LoadChannelUIState
{
public:
	bool m_loadEnabled;

	int m_voltageRangeIndex;
	std::vector<std::string> m_voltageRangeNames;

	int m_currentRangeIndex;
	std::vector<std::string> m_currentRangeNames;

	Load::LoadMode m_mode;

	float m_committedSetPoint;
	std::string m_setPoint;

	LoadChannelUIState()
		: m_loadEnabled(false)
		, m_voltageRangeIndex(0)
		, m_currentRangeIndex(0)
		, m_mode(Load::MODE_CONSTANT_CURRENT)
		, m_committedSetPoint(0)
		, m_chan(0)
		, m_load(nullptr)
	{}

	LoadChannelUIState(std::shared_ptr<SCPILoad> load, size_t chan)
		: m_loadEnabled(load->GetLoadActive(chan))
		, m_mode(load->GetLoadMode(chan))
		, m_chan(chan)
		, m_load(load)
	{
		//Voltage ranges
		Unit volts(Unit::UNIT_VOLTS);
		auto vranges = load->GetLoadVoltageRanges(chan);
		for(auto v : vranges)
			m_voltageRangeNames.push_back(volts.PrettyPrint(v));
		m_voltageRangeIndex = load->GetLoadVoltageRange(chan);

		//Current ranges
		Unit amps(Unit::UNIT_AMPS);
		auto iranges = load->GetLoadCurrentRanges(chan);
		for(auto i : iranges)
			m_currentRangeNames.push_back(amps.PrettyPrint(i));
		m_currentRangeIndex = load->GetLoadCurrentRange(chan);

		RefreshSetPoint();
	}

	/**
		@brief Pulls the set point from hardware
	 */
	void RefreshSetPoint()
	{
		//can happen if we're a placeholder prior to completion of async init
		if(m_load == nullptr)
			return;

		m_committedSetPoint = m_load->GetLoadSetPoint(m_chan);

		//Mode
		Unit volts(Unit::UNIT_VOLTS);
		Unit amps(Unit::UNIT_AMPS);
		Unit watts(Unit::UNIT_WATTS);
		Unit ohms(Unit::UNIT_OHMS);
		switch(m_mode)
		{
			case Load::MODE_CONSTANT_CURRENT:
				m_setPoint = amps.PrettyPrint(m_committedSetPoint);
				break;

			case Load::MODE_CONSTANT_VOLTAGE:
				m_setPoint = volts.PrettyPrint(m_committedSetPoint);
				break;

			case Load::MODE_CONSTANT_POWER:
				m_setPoint = watts.PrettyPrint(m_committedSetPoint);
				break;

			case Load::MODE_CONSTANT_RESISTANCE:
				m_setPoint = ohms.PrettyPrint(m_committedSetPoint);
				break;

			default:
				break;
		}
	}

protected:
	size_t m_chan;
	std::shared_ptr<Load> m_load;
};


class LoadDialog : public Dialog
{
public:
	LoadDialog(std::shared_ptr<SCPILoad> load, std::shared_ptr<LoadState> state, Session* session);
	virtual ~LoadDialog();

	virtual bool DoRender();

	std::shared_ptr<SCPILoad> GetLoad()
	{ return m_load; }

	void RefreshFromHardware();

protected:
	void ChannelSettings(size_t channel);

	///@brief Session handle so we can remove the load when closed
	Session* m_session;

	///@brief Timestamp of when we opened the dialog
	double m_tstart;

	///@brief The load we're controlling
	std::shared_ptr<SCPILoad> m_load;

	///@brief Current channel stats, live updated
	std::shared_ptr<LoadState> m_state;

	///@brief Set of channel names
	std::vector<std::string> m_channelNames;

	//Future channel state during loading
	std::vector<std::future<LoadChannelUIState> > m_futureUIState;

	///@brief Channel state for the UI
	std::vector<LoadChannelUIState> m_channelUIState;
};



#endif
