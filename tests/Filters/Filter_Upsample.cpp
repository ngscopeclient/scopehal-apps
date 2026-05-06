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

using namespace std;

float sinc(float x, float width);
float blackman(float x, float width);

float sinc(float x, float width)
{
	float xi = x - width/2;

	if(fabs(xi) < 1e-7)
		return 1.0f;
	else
	{
		float px = M_PI*xi;
		return sin(px) / px;
	}
}

float blackman(float x, float width)
{
	if(x > width)
		return 0;
	return 0.42 - 0.5*cos(2*M_PI * x / width) + 0.08 * cos(4*M_PI*x/width);
}

TEST_CASE("Filter_Upsample")
{
	auto filter = dynamic_cast<UpsampleFilter*>(Filter::CreateFilter("Upsample", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Filter_Upsample.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->GetQueue()->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create an empty input waveform
	const size_t depth = 100000;
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

			double start = GetTime();

			//TODO: poke the filter to make sure this is right
			const int upsample_factor = 10;

			//Create the interpolation filter
			vector<float> coeffs;
			size_t window = 5;
			size_t kernel = window*upsample_factor;
			float frac_kernel = kernel * 1.0f / upsample_factor;
			coeffs.resize(kernel);
			for(size_t j=0; j<kernel; j++)
			{
				float frac = j*1.0f / upsample_factor;
				coeffs[j] = sinc(frac, frac_kernel) * blackman(frac, frac_kernel);
			}

			//Generate the golden output
			AcceleratorBuffer<float> golden;
			size_t len = ua.size();
			size_t imax = len - window;
			size_t outlen = imax*upsample_factor;
			golden.resize(outlen);
			golden.PrepareForCpuAccess();
			for(size_t m=0; m < imax; m++)
			{
				size_t offset = m * upsample_factor;
				for(size_t j=0; j<upsample_factor; j++)
				{
					size_t nstart = 0;
					size_t sstart = 0;
					if(j > 0)
					{
						sstart = 1;
						nstart = upsample_factor - j;
					}

					float f = 0;
					for(size_t k = nstart; k<kernel; k += upsample_factor, sstart ++)
						f += coeffs[k] * ua.m_samples[m + sstart];

					golden[offset + j] = f;
				}
			}
			golden.MarkModifiedFromCpu();
			double tbase = GetTime() - start;
			LogVerbose("CPU: %.2f ms\n", tbase * 1000);

			//Run the filter once to get shaders loaded etc
			filter->Refresh(cmdbuf, queue);

			//Run the real filter for score
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
