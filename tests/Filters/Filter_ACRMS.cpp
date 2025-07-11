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

float ReferenceImplementation(UniformAnalogWaveform* wfm, SparseAnalogWaveform& cycles);

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
		/*
			Xeon 6144 + RTX 2080 Ti, 50M points
				Baseline naive summation	456.16 ms
				Kahan						560.00 ms
				GPU average					511.00 ms
				GPU edge detect				401.00 ms
				GPU initial RMS				174.79 ms
				Preallocate output			172.80 ms
				GPU cycle-by-cycle output	 10.84 ms
		*/

		//50M is a good benchmark, but drop down to 10M because CI uses llvmpipe which has maxStorageBufferRange
		//of 134217728 (128 MB = 32M float32s)
		const size_t depth = 10000000;

		//Input waveforms
		UniformAnalogWaveform* wfm = new UniformAnalogWaveform;
		source.GenerateNoisySinewave(cmdbuf, queue, wfm, 1.0, 0.5, 200000, 20000, depth, 0.0);

		//Add a small DC offset to make sure we null it out right
		wfm->PrepareForCpuAccess();
		float offset = 0.314159;
		for(size_t i=0; i<depth; i++)
			wfm->m_samples[i] += offset;
		wfm->MarkModifiedFromCpu();

		//Calculate the RMS average
		g_scope->GetOscilloscopeChannel(0)->SetData(wfm, 0);
		filter->SetInput("din", g_scope->GetOscilloscopeChannel(0));

		//Set up the filter (don't count this towards execution time)
		wfm->PrepareForGpuAccess();
		wfm->PrepareForCpuAccess();

		//Run the filter once without looking at results, to make sure caches are hot and buffers are allocated etc
		filter->Refresh(cmdbuf, queue);

		//Clear zero crossing cache
		Filter::ClearAnalysisCache();

		//CPU side reference implementation for speed comparison
		SparseAnalogWaveform cycles;
		double start = GetTime();
		float cpurms = ReferenceImplementation(wfm, cycles);
		double tbase = GetTime() - start;
		LogVerbose("CPU: %.2f ms, RMS = %f, %zu samples\n", tbase * 1000, cpurms, cycles.size());

		//Clear zero crossing cache
		Filter::ClearAnalysisCache();

		//Run on the GPU for score
		start = GetTime();
		filter->Refresh(cmdbuf, queue);
		float gpurms = filter->GetScalarValue(1);
		double dt = GetTime() - start;
		LogVerbose("GPU: %.2f ms, RMS = %f, %.2fx speedup\n", dt * 1000, gpurms, tbase / dt);

		//Verify the overall results are roughly in the right ballpark (randomness means it won't be perfect)
		const float epsilon1 = 0.04;
		REQUIRE(fabs(cpurms - 0.353553) < epsilon1);
		REQUIRE(fabs(gpurms - 0.353553) < epsilon1);

		//and that CPU and GPU match
		const float epsilon3 = 0.0001;
		REQUIRE(fabs(cpurms - gpurms) < epsilon3);

		//Verify the cycle-by-cycle results
		const float epsilon2 = 0.03;
		SparseAnalogWaveform& gpucycles = *dynamic_cast<SparseAnalogWaveform*>(filter->GetData(0));
		gpucycles.PrepareForCpuAccess();
		if(cycles.size() != gpucycles.size())
		{
			LogWarning("size mismatch, CPU found %zu edges, GPU found %zu\n", cycles.size(), gpucycles.size());
			size_t nlast = cycles.size() - 1;
			LogNotice("last CPU times: %" PRIi64 ", %" PRIi64 "\n", cycles.m_offsets[nlast], cycles.m_offsets[nlast-1]);

			nlast = gpucycles.size() - 1;
			LogNotice("last GPU times: %" PRIi64 ", %" PRIi64 "\n", gpucycles.m_offsets[nlast], gpucycles.m_offsets[nlast-1]);
		}
		REQUIRE(cycles.size() == gpucycles.size());
		for(size_t i=0; i<gpucycles.size(); i++)
		{
			int64_t doff = cycles.m_offsets[i] == gpucycles.m_offsets[i];
			int64_t ddur = cycles.m_durations[i] == gpucycles.m_durations[i];
			REQUIRE(llabs(doff) <= 1);
			REQUIRE(llabs(ddur) <= 1);

			float delta = cycles.m_samples[i] - gpucycles.m_samples[i];
			if(fabs(delta) >= epsilon2)
			{
				LogNotice(
					"delta = %f, i = %zu, cpu = %f, gpu = %f\n",
					delta,
					i,
					cycles.m_samples[i],
					gpucycles.m_samples[i]);

				LogNotice("cputime = %" PRIi64 ", gputime = %" PRIi64 "\n",
					cycles.m_offsets[i],
					gpucycles.m_offsets[i]);
			}
			REQUIRE(fabs(delta) < epsilon2);
		}

		g_scope->GetOscilloscopeChannel(0)->SetData(nullptr, 0);
	}

	filter->Release();
}

float ReferenceImplementation(UniformAnalogWaveform* wfm, SparseAnalogWaveform& cycles)
{
	float average = Filter::GetAvgVoltage(wfm);
	auto length = wfm->size();

	//Calculate the global RMS value
	//Sum the squares of all values after subtracting the DC value
	//Kahan summation for improved accuracy
	float temp = 0;
	float c = 0;
	for (size_t i = 0; i < length; i++)
	{
		float delta = wfm->m_samples[i] - average;
		float deltaSquared = delta * delta;
		float y = deltaSquared - c;
		float t = temp + y;
		c = (t - temp) - y;
		temp = t;
	}
	float rms = sqrt(temp / length);

	//Auto-threshold analog signals at average of the full scale range
	vector<int64_t> edges;
	Filter::FindZeroCrossings(wfm, average, edges);
	cycles.clear();
	cycles.PrepareForCpuAccess();

	size_t elen = edges.size();
	for(size_t i = 0; i < (elen - 2); i += 2)
	{
		//Measure from edge to 2 edges later, since we find all zero crossings regardless of polarity
		int64_t start = edges[i] / wfm->m_timescale;
		int64_t end = edges[i + 2] / wfm->m_timescale;
		int64_t j = 0;

		//Simply sum the squares of all values in a cycle after subtracting the DC value
		temp = 0;
		for(j = start; (j <= end) && (j < (int64_t)length); j++)
			temp += ((wfm->m_samples[j] - average) * (wfm->m_samples[j] - average));

		//Get the difference between the end and start of cycle. This would be the number of samples
		//on which AC RMS calculation was performed
		int64_t delta = j - start - 1;

		if (delta == 0)
			temp = 0;
		else
		{
			//Divide by total number of samples for one cycle
			temp /= delta;

			//Take square root to get the final AC RMS Value of one cycle
			temp = sqrt(temp);
		}

		//Push values to the waveform
		cycles.m_offsets.push_back(start);
		cycles.m_durations.push_back(delta);
		cycles.m_samples.push_back(temp);
	}

	return rms;
}
