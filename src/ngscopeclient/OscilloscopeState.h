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
	@author Frederic BORRY
	@brief Declaration of OscilloscopeState
 */
#ifndef OscilloscopeState_h
#define OscilloscopeState_h

/**
	@brief Current status of an Oscilloscope
 */
class OscilloscopeState
{
public:

	OscilloscopeState(std::shared_ptr<Oscilloscope> scope)
	{
		size_t n = scope->GetChannelCount();
		m_channelNumner = n;
		m_channelInverted = std::make_unique<bool[] >(n);
		m_channelOffset = std::make_unique<std::vector<float>[] >(n);
		m_channelRange = std::make_unique<std::vector<float>[] >(n);
		
		m_channelDigitalTrehshold = std::make_unique<std::atomic<float>[] >(n);
		m_channelAttenuation = std::make_unique<std::atomic<float>[] >(n);

		m_needsUpdate = std::make_unique<std::atomic<bool>[] >(n);

		m_probeName = std::make_unique<std::string[]>(n);

		m_channelCoupling = std::make_unique<int[] >(n);
		m_couplings = std::make_unique<std::vector<OscilloscopeChannel::CouplingType>[]>(n);
		m_couplingNames = std::make_unique<std::vector<std::string>[]>(n);

		m_channelBandwidthLimit = std::make_unique<int[] >(n);
		m_bandwidthLimits = std::make_unique<std::vector<uint32_t>[]>(n);
		m_bandwidthLimitNames = std::make_unique<std::vector<std::string>[]>(n);

		m_committedOffset = std::make_unique<std::vector<float>[]>(n);
		m_strOffset = std::make_unique<std::vector<std::string>[]>(n);

		m_committedRange = std::make_unique<std::vector<float>[]>(n);
		m_strRange = std::make_unique<std::vector<std::string>[]>(n);

		m_committedDigitalThreshold = std::make_unique<float[]>(n);
		m_strDigitalThreshold = std::make_unique<std::string[]>(n);

		m_committedAttenuation = std::make_unique<float[]>(n);
		m_strAttenuation = std::make_unique<std::string[]>(n);

		Unit volts(Unit::UNIT_VOLTS);

		for(size_t i=0; i<n; i++)
		{
			m_channelInverted[i] = false;

			m_channelDigitalTrehshold[i] = 0;
			m_channelAttenuation[i] = 0;
			m_channelBandwidthLimit[i] = 0;
			m_channelCoupling[i] = 0;
			m_channelBandwidthLimit[i] = 0;

			// Offset and range ar per stream
			OscilloscopeChannel* chan = dynamic_cast<OscilloscopeChannel*>(scope->GetChannel(i));
			if(chan)
			{
				size_t nstreams = chan->GetStreamCount();
				for(size_t j=0; j<nstreams; j++)
				{
					m_channelOffset[i].push_back(0);
					m_channelRange[i].push_back(0);
					m_committedOffset[i].push_back(FLT_MIN);
					m_committedRange[i].push_back(FLT_MIN);
					m_strOffset[i].push_back("");
					m_strRange[i].push_back("");
				}
			}

			m_needsUpdate[i] = true;

			m_committedDigitalThreshold[i] = FLT_MIN;
			m_committedAttenuation[i] = INT_MIN;
		}
	}

	void FlushConfigCache()
	{
		for(size_t i = 0 ; i < m_channelNumner.load() ; i++)
			m_needsUpdate[i] = true;
	}

	std::unique_ptr<bool[]> m_channelInverted;
	std::unique_ptr<std::vector<float>[]> m_channelOffset;
	std::unique_ptr<std::vector<float>[]> m_channelRange;
	std::unique_ptr<std::atomic<float>[]> m_channelDigitalTrehshold;
	std::unique_ptr<std::atomic<float>[]> m_channelAttenuation;

	std::unique_ptr<std::atomic<bool>[]> m_needsUpdate;

	std::atomic<size_t> m_channelNumner;

	//UI state for dialogs etc
	std::unique_ptr<std::string[]> m_probeName;

	std::unique_ptr<int[]> m_channelBandwidthLimit;
	std::unique_ptr<std::vector<uint32_t>[]> m_bandwidthLimits;
	std::unique_ptr<std::vector<std::string>[]> m_bandwidthLimitNames;

	std::unique_ptr<int[]> m_channelCoupling;
	std::unique_ptr<std::vector<OscilloscopeChannel::CouplingType>[]> m_couplings;
	std::unique_ptr<std::vector<std::string>[]> m_couplingNames;

	std::unique_ptr<std::vector<float>[]> m_committedOffset;
	std::unique_ptr<std::vector<std::string>[]> m_strOffset;

	std::unique_ptr<std::vector<float>[]> m_committedRange;
	std::unique_ptr<std::vector<std::string>[]> m_strRange;

	std::unique_ptr<float[]> m_committedDigitalThreshold;
	std::unique_ptr<std::string[]> m_strDigitalThreshold;

	std::unique_ptr<float[]> m_committedAttenuation;
	std::unique_ptr<std::string[]> m_strAttenuation;
};

#endif
