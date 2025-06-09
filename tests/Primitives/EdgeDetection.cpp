/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
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
	@brief Unit test for FindZeroCrossings() and similar
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/LevelCrossingDetector.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Primitives.h"

using namespace std;

TEST_CASE("Primitive_FindZeroCrossings")
{
	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Primitive_FindZeroCrossings.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdBuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	const size_t depth = 50000000;

	//Deterministic PRNG for repeatable testing
	minstd_rand rng;
	rng.seed(0);
	TestWaveformSource source(rng);

	SECTION("UniformAnalogWaveform")
	{
		//Input waveforms
		auto wfm = dynamic_cast<UniformAnalogWaveform*>(
			source.GenerateNoisySinewave(1.0, 0.0, 200000, 20000, depth, 0.1));
		wfm->MarkModifiedFromCpu();

		//Find the reference zero crossings using the base function
		float threshold = 0.05;
		double start = GetTime();
		vector<int64_t> edges;
		Filter::FindZeroCrossings(wfm, threshold, edges);
		double dt = GetTime() - start;
		LogNotice("CPU: %.3f ms, %zu edges, %zu samples\n", dt*1000, edges.size(), depth);

		//Do the GPU version
		//Run twice, second time for score, so we don't count deferred init or allocations in the benchmark
		LevelCrossingDetector ldet;
		auto& gpuedges = ldet.GetResults();
		size_t gpulen = ldet.FindZeroCrossings(wfm, threshold, cmdBuf, queue);
		start = GetTime();
		gpulen = ldet.FindZeroCrossings(wfm, threshold, cmdBuf, queue);
		double gpudt = GetTime() - start;
		LogNotice("GPU: %.3f ms, %zu edges, %.2fx speedup\n", gpudt*1000, gpulen, dt / gpudt);

		//Don't count the final memcpy against run time since we assume the buffer is going to get used on the GPU
		gpuedges.PrepareForCpuAccess();

		//Verify everything matches
		//Allow +/- 1 timebase unit of mismatch for different CPU-GPU rounding behavior in the interpolation
		REQUIRE(gpulen == edges.size());
		for(size_t i=0; i<edges.size(); i++)
		{
			int64_t delta = edges[i] - gpuedges[i];

			if( (delta > 1) || (delta < -1) )
				LogNotice("mismatch at i=%zu\n", i);

			REQUIRE(delta <= 1);
			REQUIRE(delta >= -1);
		}

		//done, clean up
		delete wfm;
	}
}
