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
#endif
#include "Filters.h"

using namespace std;

minstd_rand g_rng;
MockOscilloscope* g_scope;

int main(int argc, char* argv[])
{
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(Severity::VERBOSE));

	//Global scopehal initialization
	VulkanInit();
	TransportStaticInit();
	DriverStaticInit();
	InitializePlugins();
	ScopeProtocolStaticInit();

	//Add search path
	g_searchPaths.push_back(GetDirOfCurrentExecutable() + "/../../src/ngscopeclient/");

	//Initialize the RNG
	g_rng.seed(0);

	int ret;
	{
		//Create some fake scope channels
		MockOscilloscope scope("Test Scope", "Antikernel Labs", "12345", "null", "mock", "");
		scope.AddChannel(new OscilloscopeChannel(
			&scope, "CH1", "#ffffffff", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS)));
		scope.AddChannel(new OscilloscopeChannel(
			&scope, "CH2", "#ffffffff", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS)));

		scope.AddChannel(new OscilloscopeChannel(
			&scope, "Mag", "#ffffffff", Unit(Unit::UNIT_HZ), Unit(Unit::UNIT_DB)));
		scope.AddChannel(new OscilloscopeChannel(
			&scope, "Angle", "#ffffffff", Unit(Unit::UNIT_HZ), Unit(Unit::UNIT_DEGREES)));
		g_scope = &scope;

		//Run the actual test
		ret = Catch::Session().run(argc, argv);
	}

	//Clean up and return after the scope goes out of scope (pun not intended)
	ScopehalStaticCleanup();
	return ret;
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
