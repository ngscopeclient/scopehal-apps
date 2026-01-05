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
	@brief Declaration of PowerSupplyState
 */
#ifndef PowerSupplyState_h
#define PowerSupplyState_h

/**
	@brief Current status of a power supply
 */
class PowerSupplyState
{
public:

	PowerSupplyState(size_t n = 0)
	{
		m_masterEnable = false;

		m_channelVoltage = std::make_unique<std::atomic<float>[] >(n);
		m_channelCurrent = std::make_unique<std::atomic<float>[] >(n);
		m_channelConstantCurrent = std::make_unique<std::atomic<bool>[] >(n);
		m_channelFuseTripped = std::make_unique<std::atomic<bool>[] >(n);
		m_channelOn = std::make_unique<std::atomic<bool>[] >(n);

		m_needsUpdate = std::make_unique<std::atomic<bool>[] >(n);

		m_overcurrentShutdownEnabled = std::make_unique<std::atomic<bool>[] >(n);
		m_softStartEnabled = std::make_unique<std::atomic<bool>[] >(n);
		m_committedSetVoltage = std::make_unique<float[]>(n);
		m_setVoltage = std::make_unique<std::string[]>(n);
		m_committedSetCurrent = std::make_unique<float[]>(n);
		m_setCurrent = std::make_unique<std::string[]>(n);
		m_committedSSRamp = std::make_unique<float[]>(n);
		m_setSSRamp = std::make_unique<std::string[]>(n);

		for(size_t i=0; i<n; i++)
		{
			m_channelVoltage[i] = 0;
			m_channelCurrent[i] = 0;
			m_channelConstantCurrent[i] = false;
			m_channelFuseTripped[i] = false;
			m_channelOn[i] = false;

			m_needsUpdate[i] = true;
			m_overcurrentShutdownEnabled[i] = false;
			m_softStartEnabled[i]=false;
			m_committedSetVoltage[i] = FLT_MIN;
			m_committedSetCurrent[i] = FLT_MIN;
			m_committedSSRamp[i] = FLT_MIN;
		}

		m_firstUpdateDone = false;
	}

	std::unique_ptr<std::atomic<float>[]> m_channelVoltage;
	std::unique_ptr<std::atomic<float>[]> m_channelCurrent;
	std::unique_ptr<std::atomic<bool>[]> m_channelConstantCurrent;
	std::unique_ptr<std::atomic<bool>[]> m_channelFuseTripped;
	std::unique_ptr<std::atomic<bool>[]> m_channelOn;

	std::unique_ptr<std::atomic<bool>[]> m_needsUpdate;
	//UI state for dialogs etc
	std::unique_ptr<std::atomic<bool>[]> m_overcurrentShutdownEnabled;
	std::unique_ptr<std::atomic<bool>[]> m_softStartEnabled;

	std::unique_ptr<float[]> m_committedSetVoltage;
	std::unique_ptr<std::string[]> m_setVoltage;
	std::unique_ptr<float[]> m_committedSetCurrent;
	std::unique_ptr<std::string[]> m_setCurrent;
	std::unique_ptr<float[]> m_committedSSRamp;
	std::unique_ptr<std::string[]> m_setSSRamp;


	std::atomic<bool> m_firstUpdateDone;

	std::atomic<bool> m_masterEnable;
};

#endif
