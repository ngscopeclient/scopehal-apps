/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Declaration of ScopeDeskewWizard
 */
#ifndef ScopeDeskewWizard_h
#define ScopeDeskewWizard_h

#include "Dialog.h"
#include "Session.h"

class UniformCrossCorrelateArgs
{
public:
	UniformCrossCorrelateArgs(UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec, int64_t delta);

	int64_t priTimescale;
	int64_t secTimescale;

	int64_t trigPhaseDelta;

	int32_t startingDelta;
	int32_t numDeltas;

	int32_t priLen;
	int32_t secLen;
};

class ScopeDeskewWizard : public Dialog
{
public:
	ScopeDeskewWizard(
		std::shared_ptr<TriggerGroup> group,
		std::shared_ptr<Oscilloscope> secondary,
		MainWindow* parent,
		Session& session);
	virtual ~ScopeDeskewWizard();

	virtual bool DoRender();

protected:
	void DoMainProcessingFlow();
	void StartCorrelation();
	void DoProcessWaveformUniformUnequalRate(UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec);
	void DoProcessWaveformUniform4xRateVulkan(UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec);
	void DoProcessWaveformUniformUnequalRateVulkan(UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec);
	void DoProcessWaveformUniformEqualRateVulkan(UniformAnalogWaveform* ppri, UniformAnalogWaveform* psec);
	void PostprocessVulkanCorrelation();
	void DoProcessWaveformSparse(SparseAnalogWaveform* ppri, SparseAnalogWaveform* psec);
	void ChannelSelector(const char* name, std::shared_ptr<Oscilloscope> scope, StreamDescriptor& stream);

	enum state_t
	{
		STATE_WELCOME_1,
		STATE_WELCOME_2,
		STATE_WELCOME_3,
		STATE_WELCOME_4,
		STATE_WELCOME_5,
		STATE_ACQUIRE,
		STATE_CORRELATE,
		STATE_DONE,
		STATE_CLOSE
	} m_state;

	std::shared_ptr<TriggerGroup> m_group;
	std::shared_ptr<Oscilloscope> m_secondary;

	MainWindow* m_parent;
	Session& m_session;

	bool m_useExtRefPrimary;
	bool m_useExtRefSecondary;

	int m_measureCycle;

	time_t m_lastTriggerTimestamp;
	int64_t m_lastTriggerFs;

	StreamDescriptor m_primaryStream;
	StreamDescriptor m_secondaryStream;

	//Combined measurements from all waveforms to date
	std::vector<float> m_correlations;
	std::vector<int64_t> m_skews;

	//Best results found from the current waveform
	float m_bestCorrelation;
	int64_t m_bestCorrelationOffset;

	bool m_gpuCorrelationAvailable;

	//Maximum number of samples offset to consider
	int64_t m_maxSkewSamples;

	///@brief Calculated total skew
	int64_t m_medianSkew;

	//Vulkan processing queues etc
	std::shared_ptr<QueueHandle> m_queue;
	vk::raii::CommandPool m_pool;
	vk::raii::CommandBuffer m_cmdBuf;

	//Vulkan compute pipelines
	std::shared_ptr<ComputePipeline> m_uniform4xRatePipeline;
	std::shared_ptr<ComputePipeline> m_uniformUnequalRatePipeline;
	std::shared_ptr<ComputePipeline> m_uniformEqualRatePipeline;

	//Output correlation data
	AcceleratorBuffer<float> m_corrOut;
};

#endif
