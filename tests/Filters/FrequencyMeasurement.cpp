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
	@brief Unit test for FrequencyMeasurement filter
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

TEST_CASE("Filter_FrequencyMeasurement")
{
	TestWaveformSource source(g_rng);
	auto filter = dynamic_cast<FrequencyMeasurement*>(Filter::CreateFilter("Frequency", "#ffffff"));
	REQUIRE(filter != nullptr);
	filter->AddRef();

	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("Primitive_FindZeroCrossings.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdBuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	const size_t niter = 25;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Select random frequency, amplitude, and phase
			float gen_freq = uniform_real_distribution<float>(0.5e9, 5e9)(g_rng);
			float gen_period = FS_PER_SECOND / gen_freq;
			float gen_amp = uniform_real_distribution<float>(0.01, 1)(g_rng);

			//Starting phase
			float start_phase = uniform_real_distribution<float>(-M_PI, M_PI)(g_rng);

			//Generate the input signal.
			//50 Gsps, 100K points, no added noise
			auto wfm = new UniformAnalogWaveform;
			source.GenerateNoisySinewave(cmdBuf, queue, wfm, gen_amp, start_phase, gen_period, 20000, 100000, 0);
			g_scope->GetOscilloscopeChannel(0)->SetData(wfm, 0);
			wfm->PrepareForCpuAccess();

			Unit hz(Unit::UNIT_HZ);
			LogVerbose("Frequency: %s\n", hz.PrettyPrint(gen_freq).c_str());
			LogVerbose("Period:    %s\n", Unit(Unit::UNIT_FS).PrettyPrint(gen_period).c_str());
			LogVerbose("Amplitude: %s\n", Unit(Unit::UNIT_VOLTS).PrettyPrint(gen_amp).c_str());

			//Run the filter
			filter->SetInput("din", StreamDescriptor(g_scope->GetOscilloscopeChannel(0), 0));
			filter->Refresh(cmdBuf, queue);

			//Get the output data
			auto data = dynamic_cast<SparseAnalogWaveform*>(filter->GetData(0));
			data->PrepareForCpuAccess();
			REQUIRE(data != nullptr);

			//Counts for each array must be consistent
			REQUIRE(data->size() == data->m_durations.size());
			REQUIRE(data->size() == data->m_offsets.size());
			REQUIRE(data->size() == data->m_samples.size());

			//Process the individual frequency measurements and sanity check them
			//TODO: check timestamps and durations of samples too
			float fmin = FLT_MAX;
			float fmax = 0;
			float avg = 0;
			for(auto f : data->m_samples)
			{
				fmin = min(fmin, (float)f);
				fmax = max(fmax, (float)f);
				avg += f;
			}
			avg /= data->size();
			LogVerbose("Results:\n");
			LogIndenter li2;
			float davg = gen_freq - avg;
			float dmin = gen_freq - fmin;
			float dmax = fmax - gen_freq;
			auto sout = filter->GetScalarValue(1);
			auto dscalar = gen_freq - sout;
			LogVerbose("Scalar: %s (err = %s)\n", hz.PrettyPrint(sout).c_str(), hz.PrettyPrint(dscalar).c_str());
			LogVerbose("Min:    %s (err = %s)\n", hz.PrettyPrint(fmin).c_str(), hz.PrettyPrint(dmin).c_str());
			LogVerbose("Avg:    %s (err = %s)\n", hz.PrettyPrint(avg).c_str(),  hz.PrettyPrint(davg).c_str());
			LogVerbose("Max:    %s (err = %s)\n", hz.PrettyPrint(fmax).c_str(), hz.PrettyPrint(dmax).c_str());

			//Average frequency must be +/- 0.1% (arbitrary threshold for now)
			REQUIRE(fabs(davg) < 0.001 * gen_freq);
			REQUIRE(fabs(dscalar) < 0.001 * gen_freq);

			//Min and max must be +/- 5% (arbitrary threshold for now)
			REQUIRE(fabs(dmin) < 0.05 * gen_freq);
			REQUIRE(fabs(dmax) < 0.05 * gen_freq);

			//TODO: verify stdev?
		}
	}

	filter->Release();
}
