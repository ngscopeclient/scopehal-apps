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
	@brief Unit test for DeEmbed filter
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

TEST_CASE("Filter_DeEmbed")
{
	auto filter = dynamic_cast<DeEmbedFilter*>(Filter::CreateFilter("De-Embed", "#ffffff"));
	REQUIRE(filter != nullptr);
	FilterReferencer ref(filter);

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_DeEmbed.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create an empty input waveform
	const size_t depth = 100000;
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
	g_scope->GetOscilloscopeChannel(0)->SetData(&ua, 0);
	g_scope->GetOscilloscopeChannel(2)->SetData(&umag, 0);
	g_scope->GetOscilloscopeChannel(3)->SetData(&uang, 0);
	filter->SetInput("signal", g_scope->GetOscilloscopeChannel(0));
	filter->SetInput("mag", g_scope->GetOscilloscopeChannel(2));
	filter->SetInput("angle", g_scope->GetOscilloscopeChannel(3));

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
			//Also compute some temporaries we rely on
			filter->Refresh(cmdbuf, queue);

			auto npoints = filter->test_GetNumPoints();
			auto outlen = filter->test_GetOutLen();
			auto nouts = filter->test_GetNouts();

			//Allocate output buffers
			auto& forwardIn = filter->test_GetCachedInputBuffer();
			auto& sines = filter->test_GetResampledSines();
			auto& cosines = filter->test_GetResampledCosines();
			AcceleratorBuffer<float> forwardOut;
			AcceleratorBuffer<float> reverseOut;
			AcceleratorBuffer<float> golden;
			forwardOut.resize(2*nouts);
			reverseOut.resize(npoints);
			golden.resize(outlen);

			//Allocate FFTW plans
			auto forwardPlan = fftwf_plan_dft_r2c_1d(npoints,
				forwardIn.GetCpuPointer(),
				reinterpret_cast<fftwf_complex*>(forwardOut.GetCpuPointer()),
				FFTW_PRESERVE_INPUT | FFTW_ESTIMATE);
			auto reversePlan = fftwf_plan_dft_c2r_1d(npoints,
				reinterpret_cast<fftwf_complex*>(forwardOut.GetCpuPointer()),
				reverseOut.GetCpuPointer(),
				FFTW_PRESERVE_INPUT | FFTW_ESTIMATE);

			//Baseline on the CPU
			//We're only going to check correctness of the inner loop for now, so reuse the calculated S-parameters
			//and padded input buffer
			double start = GetTime();
			forwardIn.PrepareForCpuAccess();
			forwardOut.PrepareForCpuAccess();
			reverseOut.PrepareForCpuAccess();
			golden.PrepareForCpuAccess();
			sines.PrepareForCpuAccess();
			cosines.PrepareForCpuAccess();

			//Do the forward FFT
			fftwf_execute(forwardPlan);

			//Apply the interpolated S-parameters
			for(size_t j=0; j<nouts; j++)
			{
				float sinval = sines[j];
				float cosval = cosines[j];

				//Uncorrected complex value
				float real_orig = forwardOut[j*2 + 0];
				float imag_orig = forwardOut[j*2 + 1];

				//Amplitude correction
				forwardOut[j*2 + 0] = real_orig*cosval - imag_orig*sinval;
				forwardOut[j*2 + 1] = real_orig*sinval + imag_orig*cosval;
			}

			//Calculate the inverse FFT
			fftwf_execute(reversePlan);

			//Copy waveform data after rescaling
			float scale = 1.0f / npoints;
			auto istart = filter->test_GetIstart();
			for(size_t j=0; j<outlen; j++)
				golden[j] = reverseOut[j+istart] * scale;

			golden.MarkModifiedFromCpu();

			double tbase = GetTime() - start;
			LogVerbose("CPU : %6.2f ms\n", tbase * 1000);

			//Run the real filter for score
			start = GetTime();
			filter->Refresh(cmdbuf, queue);
			double dt = GetTime() - start;
			LogVerbose("GPU : %6.2f ms, %.2fx speedup\n", dt * 1000, tbase / dt);

			REQUIRE(dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0)) != nullptr);
			VerifyMatchingResult(golden, dynamic_cast<UniformAnalogWaveform*>(filter->GetData(0))->m_samples, 1e-2f);

			//Clean up
			fftwf_free(forwardPlan);
			fftwf_free(reversePlan);
		}
	}

	g_scope->GetOscilloscopeChannel(0)->Detach(0);
	g_scope->GetOscilloscopeChannel(2)->Detach(0);
	g_scope->GetOscilloscopeChannel(3)->Detach(0);
}
