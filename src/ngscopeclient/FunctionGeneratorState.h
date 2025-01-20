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
	@brief Declaration of FunctionGeneratorState
 */
#ifndef FunctionGeneratorState_h
#define FunctionGeneratorState_h

/**
	@brief Current status of a Function Generator
 */
class FunctionGeneratorState
{
public:

	FunctionGeneratorState(std::shared_ptr<FunctionGenerator> generator)
	{
		size_t n = generator->GetChannelCount();
		m_channelActive = std::make_unique<std::atomic<bool>[] >(n);
		m_channelAmplitude = std::make_unique<std::atomic<float>[] >(n);
		m_channelOffset= std::make_unique<std::atomic<float>[] >(n);
		m_channelFrequency = std::make_unique<std::atomic<float>[] >(n);
		m_channelShape = std::make_unique<std::atomic<FunctionGenerator::WaveShape>[] >(n);
		m_channelOutputImpedance = std::make_unique<std::atomic<FunctionGenerator::OutputImpedance>[] >(n);
		m_channelShapes = std::make_unique<std::vector<FunctionGenerator::WaveShape>[] >(n);
		m_channelShapeIndexes = std::make_unique<std::map<FunctionGenerator::WaveShape,int>[] >(n);
		m_channelShapeNames = std::make_unique<std::vector<std::string>[] >(n);

		m_needsUpdate = std::make_unique<std::atomic<bool>[] >(n);

		m_strOffset = std::make_unique<std::string[]>(n);
		m_committedOffset = std::make_unique<float[]>(n);
		m_strAmplitude = std::make_unique<std::string[]>(n);
		m_committedAmplitude = std::make_unique<float[]>(n);
		m_strFrequency = std::make_unique<std::string[]>(n);
		m_committedFrequency = std::make_unique<float[]>(n);

		Unit volts(Unit::UNIT_VOLTS);

		for(size_t i=0; i<n; i++)
		{
			m_channelActive[i] = false;
			m_channelAmplitude[i] = 0;
			m_channelOffset[i] = 0;
			m_channelFrequency[i] = 0;
			m_channelShape[i] = FunctionGenerator::WaveShape::SHAPE_SINE;
			m_channelOutputImpedance[i] = FunctionGenerator::OutputImpedance::IMPEDANCE_HIGH_Z;
			// Init shape list and names
			m_channelShapes[i] = generator->GetAvailableWaveformShapes(i);
			for(size_t j=0; j<m_channelShapes[i].size(); j++)
			{
				m_channelShapeNames[i].push_back(generator->GetNameOfShape(m_channelShapes[i][j]));
				m_channelShapeIndexes[i][m_channelShapes[i][j]] = j;
			}
			m_needsUpdate[i] = true;

			m_committedAmplitude[i] = FLT_MIN;
			m_committedOffset[i] = FLT_MIN;
			m_committedFrequency[i] = FLT_MIN;
		}
	}

	std::unique_ptr<std::atomic<bool>[]> m_channelActive;
	std::unique_ptr<std::atomic<float>[]> m_channelAmplitude;
	std::unique_ptr<std::atomic<float>[]> m_channelOffset;
	std::unique_ptr<std::atomic<float>[]> m_channelFrequency;
	std::unique_ptr<std::atomic<FunctionGenerator::WaveShape>[]> m_channelShape;
	std::unique_ptr<std::atomic<FunctionGenerator::OutputImpedance>[]> m_channelOutputImpedance;
	std::unique_ptr<std::vector<FunctionGenerator::WaveShape>[]> m_channelShapes;
	std::unique_ptr<std::map<FunctionGenerator::WaveShape,int>[]> m_channelShapeIndexes;
	std::unique_ptr<std::vector<std::string>[]> m_channelShapeNames;

	std::unique_ptr<std::atomic<bool>[]> m_needsUpdate;

	//UI state for dialogs etc
	std::unique_ptr<float[]> m_committedOffset;
	std::unique_ptr<std::string[]> m_strOffset;

	std::unique_ptr<float[]> m_committedAmplitude;
	std::unique_ptr<std::string[]> m_strAmplitude;

	std::unique_ptr<float[]> m_committedFrequency;
	std::unique_ptr<std::string[]> m_strFrequency;
};

#endif
