/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
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

#include <fftw3.h>

using namespace std;

void NormalizeOutputLog(AcceleratorBuffer<float>& outbuf, AcceleratorBuffer<float>& data, size_t nouts, float scale);
void NormalizeOutputLinear(AcceleratorBuffer<float>& outbuf, AcceleratorBuffer<float>& data, size_t nouts, float scale);

//Window function helpers
void ApplyWindow(const float* data, size_t len, float* out, FFTFilter::WindowFunction func);
void HannWindow(const float* data, size_t len, float* out);
void HammingWindow(const float* data, size_t len, float* out);
void CosineSumWindow(const float* data, size_t len, float* out, float alpha0);
void BlackmanHarrisWindow(const float* data, size_t len, float* out);

TEST_CASE("Filter_FFT")
{
	auto filter = dynamic_cast<FFTFilter*>(Filter::CreateFilter("FFT", "#ffffff"));
	REQUIRE(filter != nullptr);

	FilterReferencer ref(filter);

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_FFT.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create an empty input waveform
	const size_t depth = 131072;
	UniformAnalogWaveform ua;
	ua.m_timescale = 10000;		//100 Gsps
	ua.m_triggerPhase = 0;

	//Set up filter configuration
	g_scope->GetOscilloscopeChannel(0)->SetData(&ua, 0);
	filter->SetInput("din", g_scope->GetOscilloscopeChannel(0));

	const size_t niter = 8;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Create a random input waveform
			FillRandomWaveform(&ua, depth);

			//Set window function for the filter
			//(there are 4 supported window functions so this will test all of them)
			auto window = static_cast<FFTFilter::WindowFunction>(i % 4);
			filter->SetWindowFunction(window);

			//Make sure data is in the right spot (don't count this towards execution time)
			ua.PrepareForGpuAccess();
			ua.PrepareForCpuAccess();

			//Run the filter once without looking at results, to make sure caches are hot and buffers are allocated etc
			//Also precompute some sizes we need for the test code
			filter->Refresh(cmdbuf, queue);

			//Output buffer
			auto npoints = filter->test_GetNumPoints();
			auto nouts = filter->test_GetNumOuts();
			UniformAnalogWaveform golden;
			golden.Resize(nouts);
			AcceleratorBuffer<float> inbuf;
			inbuf.resize(npoints);
			AcceleratorBuffer<float> outbuf;
			outbuf.resize(2*nouts);
			//fftwf_complex can be safely treated as a float[2] array, according to https://www.fftw.org/doc/Complex-numbers.html
			auto plan = fftwf_plan_dft_r2c_1d(npoints,
				inbuf.GetCpuPointer(),
				reinterpret_cast<fftwf_complex*>(outbuf.GetCpuPointer()),
				FFTW_PRESERVE_INPUT);

			//Calculate output scale
			float scale = sqrt(2) / npoints;
			switch(window)
			{
				case FFTFilter::WINDOW_HAMMING:
					scale *= 1.862;
					break;

				case FFTFilter::WINDOW_HANN:
					scale *= 2.013;
					break;

				case FFTFilter::WINDOW_BLACKMAN_HARRIS:
					scale *= 2.805;
					break;

				//unit
				case FFTFilter::WINDOW_RECTANGULAR:
				default:
					break;
			}

			//Baseline on the CPU
			double start = GetTime();
			ua.PrepareForCpuAccess();
			inbuf.PrepareForCpuAccess();
			golden.PrepareForCpuAccess();
			outbuf.PrepareForCpuAccess();

			//TODO: test log_output=false case
			bool log_output = true;

			//Copy the input with windowing
			ApplyWindow(
				ua.m_samples.GetCpuPointer(),
				npoints,
				inbuf.GetCpuPointer(),
				window);

			//Calculate the FFT
			fftwf_execute(plan);
			//Normalize magnitudes
			if(log_output)
				NormalizeOutputLog(outbuf, golden.m_samples, nouts, scale);
			else
				NormalizeOutputLinear(outbuf, golden.m_samples, nouts, scale);

			golden.m_samples.MarkModifiedFromCpu();

			double tfft = GetTime() - start;
			LogVerbose("CPU :         %5.2f ms\n", tfft * 1000);

			//Peak search to keep time fair vs GPU filter
			start = GetTime();
			PeakDetector det;
			det.FindPeaks(&golden, 10, 500000, true, cmdbuf, queue);
			double tpeak = GetTime() - start;
			LogVerbose("Peak search : %5.2f ms\n", tpeak * 1000);

			double tbase = tpeak + tfft;

			//Clean up
			fftwf_free(plan);

			//Try again on the GPU, this time for score
			start = GetTime();
			filter->Refresh(cmdbuf, queue);
			double dt = GetTime() - start;
			LogVerbose("GPU         : %5.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

			VerifyMatchingResult(
				golden.m_samples,
				dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples,
				4e-3f
				);
		}
	}

	g_scope->GetOscilloscopeChannel(0)->Detach(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Normalization

/**
	@brief Normalize FFT output and convert to dBm (unoptimized C++ implementation)
 */
void NormalizeOutputLog(AcceleratorBuffer<float>& outbuf, AcceleratorBuffer<float>& data, size_t nouts, float scale)
{
	//assume constant 50 ohms for now
	const float impedance = 50;
	const float sscale = scale*scale / impedance;
	for(size_t i=0; i<nouts; i++)
	{
		float real = outbuf[i*2];
		float imag = outbuf[i*2 + 1];

		float vsq = real*real + imag*imag;

		//Convert to dBm
		data[i] = (10 * log10(vsq * sscale) + 30);
	}
}

/**
	@brief Normalize FFT output and output in native Y-axis units (unoptimized C++ implementation)
 */
void NormalizeOutputLinear(AcceleratorBuffer<float>& outbuf, AcceleratorBuffer<float>& data, size_t nouts, float scale)
{
	for(size_t i=0; i<nouts; i++)
	{
		float real = outbuf[i*2];
		float imag = outbuf[i*2 + 1];

		data[i] = sqrtf(real*real + imag*imag) * scale;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window functions

void ApplyWindow(const float* data, size_t len, float* out, FFTFilter::WindowFunction func)
{
	switch(func)
	{
		case FFTFilter::WINDOW_BLACKMAN_HARRIS:
			return BlackmanHarrisWindow(data, len, out);

		case FFTFilter::WINDOW_HANN:
			return HannWindow(data, len, out);

		case FFTFilter::WINDOW_HAMMING:
			return HammingWindow(data, len, out);

		case FFTFilter::WINDOW_RECTANGULAR:
		default:
			memcpy(out, data, len * sizeof(float));
	}
}

void CosineSumWindow(const float* data, size_t len, float* out, float alpha0)
{
	float alpha1 = 1 - alpha0;
	float scale = 2.0f * (float)M_PI / len;

	float* aligned_data = (float*)__builtin_assume_aligned(data, 32);
	float* aligned_out = (float*)__builtin_assume_aligned(out, 32);
	for(size_t i=0; i<len; i++)
	{
		float w = alpha0 - alpha1*cosf(i*scale);
		aligned_out[i] = w * aligned_data[i];
	}
}

void BlackmanHarrisWindow(const float* data, size_t len, float* out)
{
	float alpha0 = 0.35875;
	float alpha1 = 0.48829;
	float alpha2 = 0.14128;
	float alpha3 = 0.01168;
	float scale = 2 *(float)M_PI / len;

	for(size_t i=0; i<len; i++)
	{
		float num = i * scale;
		float w =
			alpha0 -
			alpha1 * cosf(num) +
			alpha2 * cosf(2*num) -
			alpha3 * cosf(6*num);
		out[i] = w * data[i];
	}
}

void HannWindow(const float* data, size_t len, float* out)
{
	CosineSumWindow(data, len, out, 0.5);
}

void HammingWindow(const float* data, size_t len, float* out)
{
	CosineSumWindow(data, len, out, 25.0f / 46);
}
