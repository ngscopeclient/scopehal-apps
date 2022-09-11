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
	@brief Declaration of PowerSupplyDialog
 */
#ifndef PowerSupplyDialog_h
#define PowerSupplyDialog_h

#include "Dialog.h"
#include "Session.h"

/**
	@brief Realtime plot helper (based on implot_demo)
 */
struct RollingBuffer
{
	float Span;
	ImVector<ImVec2> Data;
	RollingBuffer()
	{
		Span = 10.0f;
		Data.reserve(2000);
	}
	void AddPoint(float x, float y)
	{
		Data.push_back(ImVec2(x, y));

		while(!Data.empty())
		{
			float tfirst = Data.begin()->x;
			if(tfirst < (x - Span))
				Data.erase(Data.begin());
			else
				break;
		}
	}
};

/**
	@brief UI state for a single power supply channel

	Stores uncommitted values we haven't pushed to hardware, trends of previous values, etc
 */
class PowerSupplyChannelUIState
{
public:

	bool m_outputEnabled;
	bool m_overcurrentShutdownEnabled;
	bool m_softStartEnabled;

	float m_setVoltage;
	float m_setCurrent;

	PowerSupplyChannelUIState(SCPIPowerSupply* psu, int chan)
		: m_outputEnabled(psu->GetPowerChannelActive(chan))
		, m_overcurrentShutdownEnabled(psu->GetPowerOvercurrentShutdownEnabled(chan))
		, m_softStartEnabled(psu->IsSoftStartEnabled(chan))
		, m_setVoltage(psu->GetPowerVoltageNominal(chan))
		, m_setCurrent(psu->GetPowerCurrentNominal(chan))
	{}

	RollingBuffer m_voltageHistory;
	RollingBuffer m_currentHistory;
};

class PowerSupplyDialog : public Dialog
{
public:
	PowerSupplyDialog(SCPIPowerSupply* psu, std::shared_ptr<PowerSupplyState> state);
	virtual ~PowerSupplyDialog();

	virtual bool DoRender();

protected:
	void CombinedTrendPlot(float etime);
	void ChannelSettings(int i, float v, float a, float etime);

	//@brief Global power enable (if we have one)
	bool m_masterEnable;

	///@brief Timestamp of when we opened the dialog
	float m_tstart;

	///@brief Depth for historical sample data
	float m_historyDepth;

	///@brief The PSU we're controlling
	SCPIPowerSupply* m_psu;

	///@brief Current channel stats, live updated
	std::shared_ptr<PowerSupplyState> m_state;

	///@brief Channel state for the UI
	std::vector<PowerSupplyChannelUIState> m_channelUIState;
};

#endif
