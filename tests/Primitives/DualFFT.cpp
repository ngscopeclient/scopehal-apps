/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Unit test for two FFTs in quick succession
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Primitives.h"

using namespace std;

size_t FindPeak(AcceleratorBuffer<float>& buf, size_t start, size_t end);
void VerifyPeak(
	AcceleratorBuffer<float>& buf,
	float bin_uhz,
	size_t len,
	int64_t expectedPeakUhz,
	int64_t toleranceUhz);

TEST_CASE("Primitive_DualFFT")
{
	const size_t wavelen = 1000000;
	const int64_t fs_per_sample = 1e6;
	const int64_t hz_per_mhz = 1e6;
	const int64_t uhz_per_hz = 1e6;

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Primitive_DualFFT.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->GetQueue()->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdBuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Deterministic PRNG for repeatable testing
	minstd_rand rng1;
	rng1.seed(0);
	TestWaveformSource source1(rng1);

	minstd_rand rng2;
	rng2.seed(0);
	TestWaveformSource source2(rng2);

	//Generate a 100 MHz sinewave sampled at 1 Gsps with no AWGN
	LogNotice("Generating 100 MHz sinewave\n");
	float sine1_period = 1e7;
	UniformAnalogWaveform sine_100;
	source1.GenerateNoisySinewave(
		cmdBuf,
		queue,
		&sine_100,
		1,
		0,
		sine1_period,
		fs_per_sample,
		wavelen,
		0);

	//Repeat for 105 MHz
	LogNotice("Generating 105 MHz sinewave\n");
	float sine2_period = 9.524e6;
	UniformAnalogWaveform sine_105;
	source2.GenerateNoisySinewave(
		cmdBuf,
		queue,
		&sine_105,
		1,
		0,
		sine2_period,
		fs_per_sample,
		wavelen,
		0);

	//Calculate number of complex output points and FFT bin sizes
	size_t nouts = (wavelen/2) + 1;
	double sample_ghz = 1e6 / fs_per_sample;
	double bin_uhz_raw = 0.5f * sample_ghz * 1e15f / nouts;
	int64_t bin_uhz = round(bin_uhz_raw);

	//Make output buffers
	AcceleratorBuffer<float> freq1;
	AcceleratorBuffer<float> freq2;
	freq1.resize(2*nouts, true);
	freq2.resize(2*nouts, true);

	//Set up the FFT plans
	VulkanFFTPlan plan1(wavelen, nouts, VulkanFFTPlan::DIRECTION_FORWARD);
	VulkanFFTPlan plan2(wavelen, nouts, VulkanFFTPlan::DIRECTION_FORWARD);

	//FFT the input data
	cmdBuf.begin({});

	LogNotice("FFT 100 MHz\n");
	plan1.AppendForward(sine_100.m_samples, freq1, cmdBuf);

	LogNotice("FFT 105 MHz\n");
	plan2.AppendForward(sine_105.m_samples, freq2, cmdBuf);

	cmdBuf.end();
	queue->SubmitAndBlock(cmdBuf);

	//Copy the data off the GPU
	freq1.PrepareForCpuAccess();

	//Find the peak locations and make sure nothing is funky
	LogNotice("Verify 100 MHz\n");
	VerifyPeak(freq1, bin_uhz, nouts, (100 * hz_per_mhz) * uhz_per_hz, 5000 * uhz_per_hz);

	LogNotice("Verify 105 MHz\n");
	VerifyPeak(freq2, bin_uhz, nouts, (105 * hz_per_mhz) * uhz_per_hz, 5000 * uhz_per_hz);
}

void VerifyPeak(
	AcceleratorBuffer<float>& buf,
	float bin_uhz,
	size_t len,
	int64_t expectedPeakUhz,
	int64_t toleranceUhz)
{
	auto uhz = Unit(Unit::UNIT_MICROHZ);
	auto peakPos = FindPeak(buf, 0, len - 1);
	auto peak_uhz = bin_uhz * peakPos;
	auto error_uhz = expectedPeakUhz - peak_uhz;
	LogNotice("Peak is at sample %zu (%s), expected %s, error = %s\n",
		peakPos,
		uhz.PrettyPrint(peak_uhz, 6).c_str(),
		uhz.PrettyPrint(expectedPeakUhz, 6).c_str(),
		uhz.PrettyPrint(error_uhz, 6).c_str());
	REQUIRE(labs(error_uhz) < toleranceUhz);
}

size_t FindPeak(AcceleratorBuffer<float>& buf, size_t start, size_t end)
{
	size_t imax = 0;
	float fmax = -FLT_MAX;
	for(size_t n=start; n<end; n++)
	{
		float i = buf[n*2];
		float q = buf[n*2 + 1];

		float f = sqrt(i*i + q*q);
		if(f > fmax)
		{
			fmax = f;
			imax = n;
		}
	}

	return imax;
}
