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
	@brief Main code for Filters test case
 */

#define CATCH_CONFIG_RUNNER
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#define EventListenerBase TestEventListenerBase
#endif
#include "Filters.h"

using namespace std;

minstd_rand g_rng;
MockOscilloscope* g_scope;

// Global initialization
class testRunListener : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override
    {
		g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(Severity::VERBOSE));

		if(!VulkanInit(true))
			exit(1);
		TransportStaticInit();
		DriverStaticInit();
		InitializePlugins();
		ScopeProtocolStaticInit();

		//Add search path
		g_searchPaths.push_back(GetDirOfCurrentExecutable() + "/../../src/ngscopeclient/");

		//Initialize the RNG
		g_rng.seed(0);

		//Create some fake scope channels
		g_scope = new MockOscilloscope("Test Scope", "Antikernel Labs", "12345", "null", "mock", "");
		g_scope->AddChannel(new OscilloscopeChannel(
			g_scope, "CH1", "#ffffffff", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS)));
		g_scope->AddChannel(new OscilloscopeChannel(
			g_scope, "CH2", "#ffffffff", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS)));

		g_scope->AddChannel(new OscilloscopeChannel(
			g_scope, "Mag", "#ffffffff", Unit(Unit::UNIT_HZ), Unit(Unit::UNIT_DB)));
		g_scope->AddChannel(new OscilloscopeChannel(
			g_scope, "Angle", "#ffffffff", Unit(Unit::UNIT_HZ), Unit(Unit::UNIT_DEGREES)));

	}

	//Clean up after the scope goes out of scope (pun not intended)
	void testRunEnded([[maybe_unused]] Catch::TestRunStats const& testRunStats) override
	{
		delete g_scope;
		ScopehalStaticCleanup();
	}
};
CATCH_REGISTER_LISTENER(testRunListener)

int main(int argc, char* argv[])
{
	return Catch::Session().run(argc, argv);
}

/**
	@brief Fills a waveform with random content, uniformly distributed from fmin to fmax
 */
void FillRandomWaveform(UniformAnalogWaveform* wfm, size_t size, float fmin, float fmax)
{
	auto rdist = uniform_real_distribution<float>(fmin, fmax);

	wfm->PrepareForCpuAccess();
	wfm->Resize(size);

	for(size_t i=0; i<size; i++)
		wfm->m_samples[i] = rdist(g_rng);

	wfm->MarkModifiedFromCpu();

	wfm->m_revision ++;
	if(wfm->m_timescale == 0)
		wfm->m_timescale = 1000;
}

void VerifyMatchingResult(AcceleratorBuffer<float>& golden, AcceleratorBuffer<float>& observed, float tolerance)
{
	REQUIRE(golden.size() == observed.size());

	golden.PrepareForCpuAccess();
	observed.PrepareForCpuAccess();
	size_t len = golden.size();

	bool firstFail = true;
	for(size_t i=0; i<len; i++)
	{
		float delta = fabs(golden[i] - observed[i]);

		if( (delta >= tolerance) && firstFail)
		{
			LogError("first fail at i=%zu (delta=%f, tolerance=%f)\n", i, delta, tolerance);
			firstFail = false;
		}

		REQUIRE(delta < tolerance);
	}
}
