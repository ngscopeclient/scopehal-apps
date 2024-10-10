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
	@author Andrew D. Zonenberg
	@brief Unit test for EyePattern filter
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

void DumpEye(EyeWaveform* wfm, const char* path, size_t width, size_t height);
void DumpEyeMask(vector<uint8_t>& pixels, const char* path, size_t width, size_t height);

TEST_CASE("Filter_EyePattern")
{
	auto filter = dynamic_cast<EyePattern*>(Filter::CreateFilter("Eye pattern", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_EyePattern.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create input waveform for clock: square wave
	const size_t depth = 3200000;
	const int64_t timescale = 100000;
	const int64_t clockToggleInterval = 32;
	const int64_t center = clockToggleInterval / 2;
	const size_t nclks = (depth / clockToggleInterval);
	SparseDigitalWaveform clk;
	clk.Resize(nclks);
	clk.PrepareForCpuAccess();
	clk.m_timescale = timescale;
	clk.m_triggerPhase = timescale / 2;	//center edge in the sample clock window
	bool b = false;
	for(size_t i=0; i<nclks; i++)
	{
		clk.m_samples[i] = b;
		clk.m_durations[i] = clockToggleInterval;
		clk.m_offsets[i] = (clockToggleInterval * i) - center;

		b = !b;
	}
	clk.MarkModifiedFromCpu();

	//Create input waveform for data: slightly noisy stretched sinusoid
	UniformAnalogWaveform data;
	auto rdist = uniform_real_distribution<float>(-0.01, 0.01);
	data.Resize(depth);
	data.PrepareForCpuAccess();
	data.m_timescale = timescale;
	data.m_triggerPhase = 0;
	size_t period = 2*clockToggleInterval;
	for(size_t i=0; i<depth; i++)
	{
		float phase = 2*M_PI * (i % period) / period;
		data.m_samples[i] = rdist(g_rng) + sin(rdist(g_rng) + phase) * 0.3;
	}
	data.MarkModifiedFromCpu();

	//Set up channels
	g_scope->GetOscilloscopeChannel(4)->SetData(&clk, 0);
	g_scope->GetOscilloscopeChannel(0)->SetData(&data, 0);
	g_scope->GetOscilloscopeChannel(0)->SetVoltageRange(0.7, 0);
	filter->SetInput("din", g_scope->GetOscilloscopeChannel(0));
	filter->SetInput("clk", g_scope->GetOscilloscopeChannel(4));

	//Configure the mask and dimensions of the eye
	size_t width = 64;
	size_t height = 64;
	auto maskpath = FindDataFile("masks/pcie-gen2-5gbps-rx.yml");
	filter->GetParameter("Mask").SetStringVal(maskpath);
	filter->SetWidth(width);
	filter->SetHeight(height);

	SECTION("Baseline")
	{
		LogVerbose("Baseline (expecting no mask hits)\n");
		LogIndenter li;
		REQUIRE(maskpath != "");

		//Run the filter
		filter->Refresh(cmdbuf, queue);

		//Expect that we've integrated the right number of UIs (OK to lose one at each end to edge effects)
		auto eyewfm = dynamic_cast<EyeWaveform*>(filter->GetData(0));
		REQUIRE(eyewfm != nullptr);
		size_t nuis = eyewfm->GetTotalUIs();
		LogVerbose("Total UIs: %zu\n", nuis);
		//DumpEye(eyewfm, "/tmp/eye.csv", width, height);
		REQUIRE(nuis >= (nclks - 2));
		REQUIRE(nuis <= nclks);

		//Expect there to be no hits on the mask starting out
		auto hitrate = filter->GetScalarValue(1);
		LogVerbose("Mask hit rate: %e\n", hitrate);

		REQUIRE(hitrate == 0);
	}

	SECTION("ShouldFail1")
	{
		//We have one SAMPLE hitting the mask
		//Per SFF-8431 appendix D.2.1, hit rate is (samples touching mask) / (total samples integrated)
		float expectedHitRate = 1.0 / depth;

		LogVerbose("Add one sample at center of eye (expecting a single mask hit, %e)\n", expectedHitRate);
		LogIndenter li;
		REQUIRE(maskpath != "");

		//center of UI, at 0V - exact middle of eye opening
		size_t nsample = 2 * clockToggleInterval + center;
		LogVerbose("Old value at %zu was %f\n", nsample, data.m_samples[nsample]);
		data.m_samples[nsample] = 0;
		data.m_revision ++;
		data.MarkModifiedFromCpu();

		//Run the filter again
		filter->Refresh(cmdbuf, queue);

		//Expect that we've integrated the right number of UIs (OK to lose one at each end to edge effects)
		auto eyewfm = dynamic_cast<EyeWaveform*>(filter->GetData(0));
		REQUIRE(eyewfm != nullptr);
		size_t nuis = eyewfm->GetTotalUIs();
		LogVerbose("Total UIs: %zu\n", nuis);
		//DumpEye(eyewfm, "/tmp/eye.csv", width, height);
		REQUIRE(nuis >= (nclks - 2));
		REQUIRE(nuis <= nclks);

		//Dump the eye mask
		vector<uint8_t> pixels;
		filter->GetMask().GetPixels(pixels);
		//DumpEyeMask(pixels, "/tmp/mask.csv", width, height);

		//We now expect a single hit out of nsamples samples (minus rounding artifacts for fractional UIs)
		auto hitrate = filter->GetScalarValue(1);
		float deltaHitRate = (hitrate - expectedHitRate) / expectedHitRate;
		REQUIRE(deltaHitRate < 0.001);
		LogVerbose("Mask hit rate: %e (error = %.2f %%)\n", hitrate, deltaHitRate * 100);
	}

	g_scope->GetOscilloscopeChannel(0)->Detach(0);
	g_scope->GetOscilloscopeChannel(4)->Detach(0);

	filter->Release();
}

///@brief Helper function not called if the test passes, but may be useful for troubleshooting if it fails
void DumpEye(EyeWaveform* wfm, const char* path, size_t width, size_t height)
{
	FILE* fp = fopen(path, "w");

	for(size_t y = 0; y < height; y++)
	{
		auto prow = wfm->GetData() + (y * width);
		for(size_t x=0; x < width; x++)
			fprintf(fp, "%.6f, ", prow[x]);
		fprintf(fp, "\n");
	}

	fclose(fp);
}

///@brief Helper function not called if the test passes, but may be useful for troubleshooting if it fails
void DumpEyeMask(vector<uint8_t>& pixels, const char* path, size_t width, size_t height)
{
	FILE* fp = fopen(path, "w");

	for(size_t y = 0; y < height; y++)
	{
		auto prow = &pixels[y * width * 4];
		for(size_t x=0; x < width; x++)
			fprintf(fp, "%u, ", (uint8_t)prow[x * 4]);
		fprintf(fp, "\n");
	}

	fclose(fp);
}
