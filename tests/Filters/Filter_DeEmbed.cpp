/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
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

TEST_CASE("Filter_DeEmbed")
{
	auto filter = dynamic_cast<DeEmbedFilter*>(Filter::CreateFilter("De-Embed", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_DeEmbed.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create an empty input waveform
	const size_t depth = 1000000;
	UniformAnalogWaveform ua;
	ua.m_timescale = 100000;		//10 Gsps
	ua.m_triggerPhase = 0;

	//Create an empty magnitude and angle trace
	UniformAnalogWaveform umag;
	umag.m_timescale = 1e6;			//1 MHz per point
	umag.m_triggerPhase = 0;
	UniformAnalogWaveform uang;
	uang.m_timescale = 1e6;			//1 MHz per point
	uang.m_triggerPhase = 0;

	//Set up filter configuration
	g_scope->GetChannel(0)->SetData(&ua, 0);
	g_scope->GetChannel(2)->SetData(&umag, 0);
	g_scope->GetChannel(3)->SetData(&uang, 0);
	filter->SetInput("signal", g_scope->GetChannel(0));
	filter->SetInput("mag", g_scope->GetChannel(2));
	filter->SetInput("angle", g_scope->GetChannel(3));

#ifdef __x86_64__
	bool reallyHasAvx2 = g_hasAvx2;
#endif

	const size_t niter = 8;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Create a random input waveform
			FillRandomWaveform(&ua, depth, -1, 1);
			FillRandomWaveform(&umag, depth, -15, 0);
			FillRandomWaveform(&uang, depth, -180, 180);

			//Run the filter once without looking at results, to make sure caches are hot and buffers are allocated etc
			g_gpuFilterEnabled = false;
			{
				QueueLock lock(queue);
				filter->Refresh(cmdbuf, *lock);
			}

			//Baseline on the CPU with no AVX
			#ifdef __x86_64__
				g_hasAvx2 = false;
			#endif
			g_gpuFilterEnabled = false;
			double start = GetTime();
			{
				QueueLock lock(queue);
				filter->Refresh(cmdbuf, *lock);
			}
			double tbase = GetTime() - start;
			LogVerbose("CPU (no AVX)  : %6.2f ms\n", tbase * 1000);

			REQUIRE(dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0)) != nullptr);

			//Copy the result
			AcceleratorBuffer<float> golden;
			golden.CopyFrom(dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples);

			#ifdef __x86_64__
				//Try again with AVX
				if(reallyHasAvx2)
				{
					g_hasAvx2 = true;
					start = GetTime();
					QueueLock lock(queue);
					filter->Refresh(cmdbuf, *lock);
					float dt = GetTime() - start;
					LogVerbose("CPU (AVX2)    : %6.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

					VerifyMatchingResult(
						golden,
						dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples,
						1e-2f
						);
				}
			#endif

			//Run the filter once without looking at results, to make sure caches are hot and buffers are allocated etc
			g_gpuFilterEnabled = true;
			{
				QueueLock lock(queue);
				filter->Refresh(cmdbuf, *lock);
			}

			//Try again on the GPU, this time for score
			start = GetTime();
			{
				QueueLock lock(queue);
				filter->Refresh(cmdbuf, *lock);
			}
			double dt = GetTime() - start;
			LogVerbose("GPU           : %6.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

			VerifyMatchingResult(
				golden,
				dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples,
				1e-2f
				);
		}
	}

	#ifdef __x86_64__
		g_hasAvx2 = reallyHasAvx2;
	#endif

	g_scope->GetChannel(0)->Detach(0);
	g_scope->GetChannel(2)->Detach(0);
	g_scope->GetChannel(3)->Detach(0);

	filter->Release();
}
