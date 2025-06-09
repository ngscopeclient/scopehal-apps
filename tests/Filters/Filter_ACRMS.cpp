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
	@brief Unit test for AC RMS filter
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Filters.h"

using namespace std;

/*
void VerifyAdditionResult(UniformAnalogWaveform* pa, UniformAnalogWaveform* pb, UniformAnalogWaveform* padd);
void AddCpu(UniformAnalogWaveform* pout, UniformAnalogWaveform* pa, UniformAnalogWaveform* pb);
*/

TEST_CASE("Filter_ACRMS")
{
	auto filter = dynamic_cast<ACRMSMeasurement*>(Filter::CreateFilter("AC RMS", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_ACRMS.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	TestWaveformSource source(g_rng);

	SECTION("UniformAnalogWaveform")
	{
		//const size_t depth = 50000000;
		const size_t depth = 5000000;

		//Input waveforms
		auto wfm = dynamic_cast<UniformAnalogWaveform*>(
			source.GenerateNoisySinewave(1.0, 0.0, 200000, 20000, depth, 0.01));

		//Add a small DC offset to make sure we null it out right
		float offset = 0.314159;
		for(size_t i=0; i<depth; i++)
			wfm->m_samples[i] += offset;
		wfm->MarkModifiedFromCpu();

		//Calculate the RMS average
		g_scope->GetOscilloscopeChannel(0)->SetData(wfm, 0);
		filter->SetInput("din", g_scope->GetOscilloscopeChannel(0));

		//Set up the filter (don't count this towards execution time)
		wfm->PrepareForGpuAccess();

		//Run the filter once without looking at results, to make sure caches are hot and buffers are allocated etc
		filter->Refresh(cmdbuf, queue);

		//TODO: CPU side reference implementation for speed comparison

		float tbase = 1.0;

		//Run on the GPU for score
		double start = GetTime();
		filter->Refresh(cmdbuf, queue);
		float gpurms = filter->GetScalarValue(1);
		double dt = GetTime() - start;
		//LogVerbose("GPU: %.2f ms, RMS = %f, %.2fx speedup\n", dt * 1000, gpurms, tbase / dt);
		LogVerbose("CPU: %.2f ms, RMS = %f\n", dt * 1000, gpurms);

		const float epsilon = 0.001;
		REQUIRE(fabs(gpurms - 0.353553) < epsilon);

		g_scope->GetOscilloscopeChannel(0)->SetData(nullptr, 0);
	}

	filter->Release();
}

/*
void AddCpu(UniformAnalogWaveform* pout, UniformAnalogWaveform* pa, UniformAnalogWaveform* pb)
{
	REQUIRE(pout != nullptr);
	REQUIRE(pa != nullptr);
	REQUIRE(pb != nullptr);
	REQUIRE(pout->size() == pa->size());
	REQUIRE(pa->size() == pb->size());

	pout->PrepareForCpuAccess();
	pa->PrepareForCpuAccess();
	pb->PrepareForCpuAccess();

	float* out = pout->m_samples.GetCpuPointer();

	size_t len = pa->size();

	for(size_t i=0; i<len; i++)
		out[i] = pa->m_samples[i] + pb->m_samples[i];

	pout->MarkModifiedFromCpu();
}

void VerifyAdditionResult(UniformAnalogWaveform* pa, UniformAnalogWaveform* pb, UniformAnalogWaveform* padd)
{
	REQUIRE(padd != nullptr);
	REQUIRE(padd->size() == min(pa->size(), pb->size()) );

	pa->PrepareForCpuAccess();
	pb->PrepareForCpuAccess();
	padd->PrepareForCpuAccess();

	size_t len = padd->size();

	for(size_t i=0; i<len; i++)
	{
		float expected = pa->m_samples[i] + pb->m_samples[i];
		REQUIRE(fabs(padd->m_samples[i] - expected) < 1e-6);
	}
}
*/
