/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Unit test for Subtract filter
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

void VerifySubtractionResult(UniformAnalogWaveform* pa, UniformAnalogWaveform* pb, UniformAnalogWaveform* psub);
void SubtractCpu(UniformAnalogWaveform* pout, UniformAnalogWaveform* pa, UniformAnalogWaveform* pb);

TEST_CASE("Filter_Subtract")
{
	auto filter = dynamic_cast<SubtractFilter*>(Filter::CreateFilter("Subtract", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_Subtract.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create two empty input waveforms
	const size_t depth = 10000000;
	UniformAnalogWaveform ua;
	UniformAnalogWaveform ub;
	UniformAnalogWaveform ubase;
	ubase.Resize(depth);

	//Set up filter configuration
	g_scope->GetOscilloscopeChannel(0)->SetData(&ua, 0);
	g_scope->GetOscilloscopeChannel(1)->SetData(&ub, 0);
	filter->SetInput("IN+", g_scope->GetOscilloscopeChannel(0));
	filter->SetInput("IN-", g_scope->GetOscilloscopeChannel(1));

	const size_t niter = 5;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Create two random input waveforms
			FillRandomWaveform(&ua, depth);
			FillRandomWaveform(&ub, depth);

			//Set up the filter (don't count this towards execution time)
			ua.PrepareForGpuAccess();
			ub.PrepareForGpuAccess();

			//Run the filter once without looking at results, to make sure caches are hot and buffers are allocated etc
			filter->Refresh(cmdbuf, queue);

			//Baseline on the CPU
			double start = GetTime();
			SubtractCpu(&ubase, &ua, &ub);
			double tbase = GetTime() - start;
			LogVerbose("CPU: %.2f ms\n", tbase * 1000);

			VerifySubtractionResult(&ua, &ub, &ubase);

			start = GetTime();
			filter->Refresh(cmdbuf, queue);
			double dt = GetTime() - start;
			LogVerbose("GPU: %.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

			VerifySubtractionResult(&ua, &ub, dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0)));
		}
	}

	g_scope->GetOscilloscopeChannel(0)->Detach(0);
	g_scope->GetOscilloscopeChannel(1)->Detach(0);

	filter->Release();
}

void SubtractCpu(UniformAnalogWaveform* pout, UniformAnalogWaveform* pa, UniformAnalogWaveform* pb)
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
		out[i] = pa->m_samples[i] - pb->m_samples[i];

	pout->MarkModifiedFromCpu();
}

void VerifySubtractionResult(UniformAnalogWaveform* pa, UniformAnalogWaveform* pb, UniformAnalogWaveform* psub)
{
	REQUIRE(psub != nullptr);
	REQUIRE(psub->size() == min(pa->size(), pb->size()) );

	pa->PrepareForCpuAccess();
	pb->PrepareForCpuAccess();
	psub->PrepareForCpuAccess();

	size_t len = psub->size();

	for(size_t i=0; i<len; i++)
	{
		float expected = pa->m_samples[i] - pb->m_samples[i];
		REQUIRE(fabs(psub->m_samples[i] - expected) < 1e-6);
	}
}
