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
	@brief Unit test for Upsample filter
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

TEST_CASE("Filter_Upsample")
{
	auto filter = dynamic_cast<UpsampleFilter*>(Filter::CreateFilter("Upsample", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_Upsample.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create an empty input waveform
	const size_t depth = 10000000;
	UniformAnalogWaveform ua;

	//Set up filter configuration
	g_scope->GetOscilloscopeChannel(0)->SetData(&ua, 0);
	filter->SetInput("din", g_scope->GetOscilloscopeChannel(0));

	const size_t niter = 5;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Create a random input waveform
			FillRandomWaveform(&ua, depth);

			//Make sure data is in the right spot (don't count this towards execution time)
			ua.PrepareForGpuAccess();
			ua.PrepareForCpuAccess();

			//Run the filter once on CPU and GPU each
			//without looking at results, to make sure caches are hot and buffers are allocated etc
			g_gpuFilterEnabled = false;
			filter->Refresh(cmdbuf, queue);
			g_gpuFilterEnabled = true;
			filter->Refresh(cmdbuf, queue);

			//Baseline on the CPU
			g_gpuFilterEnabled = false;
			double start = GetTime();
			filter->Refresh(cmdbuf, queue);
			double tbase = GetTime() - start;
			LogVerbose("CPU: %.2f ms\n", tbase * 1000);

			//Copy the result
			AcceleratorBuffer<float> golden;
			golden.CopyFrom(dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples);

			//Try again on the GPU
			g_gpuFilterEnabled = true;
			start = GetTime();
			filter->Refresh(cmdbuf, queue);
			double dt = GetTime() - start;
			LogVerbose("GPU: %.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

			VerifyMatchingResult(golden, dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples);
		}
	}

	g_scope->GetOscilloscopeChannel(0)->Detach(0);

	filter->Release();
}
