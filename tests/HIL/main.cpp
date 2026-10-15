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
	@brief Main code for HIL test cases
 */

#define CATCH_CONFIG_RUNNER
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#define EventListenerBase TestEventListenerBase
#endif
#include "HIL.h"

std::string g_siggenPath;
std::string g_scopePath;

using namespace std;

class testRunListener : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

	// Global initialization
    void testRunStarting(Catch::TestRunInfo const&) override
    {
		auto sink = new ColoredSTDLogSink(Severity::VERBOSE);
		//auto sink = new ColoredSTDLogSink(Severity::TRACE);
		//g_trace_filters.emplace("SCPISocketTransport");

		g_log_sinks.emplace(g_log_sinks.begin(), sink);

		if(!VulkanInit(true))
			exit(1);
		TransportStaticInit();
		DriverStaticInit();
		ScopeProtocolStaticInit();
		InitializePlugins();

		//This is a bit hacky but should work at least on Linux
		auto searchPath = GetDirOfCurrentExecutable() + "/../../src/ngscopeclient";
		//LogNotice("Adding %s to search path\n", searchPath.c_str());
		g_searchPaths.push_back(searchPath);
	}

	//Clean up after the scope goes out of scope (pun not intended)
    void testRunEnded([[maybe_unused]] Catch::TestRunStats const& testRunStats) override
    {
		ScopehalStaticCleanup();
	}
};
CATCH_REGISTER_LISTENER(testRunListener)

int main(int argc, char* argv[])
{
	Catch::Session session;

	auto cli =
		session.cli() |
		Catch::Clara::Opt(g_siggenPath, "signal generator")["--siggen"]("host:port of signal generator") |
		Catch::Clara::Opt(g_scopePath, "scope")["--scope"]("host:port of oscilloscope");
    session.cli(cli);

    auto result = session.applyCommandLine(argc, argv);
    if(0 != result)
		return result;

	//Run the actual test
	return session.run();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
	@brief Helper for initializing a scope with a single digital input channel
 */
void SetupScopeForSingleChannelDigitalInput(SCPIOscilloscope& scope)
{
	//Enable channel 1 and only channel 1
	LogNotice("Channel setup\n");
	scope.EnableChannel(0);
	scope.DisableChannel(1);
	scope.DisableChannel(2);
	scope.DisableChannel(3);

	//Set up timebase: 1M samples @ 1 Gsps
	LogNotice("Timebase setup\n");
	scope.SetSampleDepth(1000 * 1000);
	scope.SetSampleRate(1000 * 1000 * 1000);

	//Set up trigger: rising edge, 900 mV
	LogNotice("Trigger setup\n");
	auto trig = new EdgeTrigger(&scope);
	trig->SetType(EdgeTrigger::EDGE_RISING);
	trig->SetInput(0, scope.GetChannel(0));
	trig->SetLevel(0.9);
	scope.SetTrigger(trig);
	scope.PushTrigger();

	//Stop the trigger if it had been previously armed
	scope.Stop();

	//Configure the channel
	LogNotice("Frontend setup\n");
	scope.SetChannelCoupling(0, OscilloscopeChannel::COUPLE_DC_1M);
	scope.SetChannelAttenuation(0, 1.0);
	scope.SetChannelBandwidthLimit(0, 0);
	scope.SetChannelVoltageRange(0, 0, 2.25);
	scope.SetChannelOffset(0, 0, -0.9);

	scope.GetTransport()->FlushCommandQueue();

	//Sanity check only CH1 is enabled
	REQUIRE(scope.GetEnabledChannelCount() == 1);
}

/**
	@brief Helper to make sure the scope does not trigger
 */
void VerifyNoTrigger(Oscilloscope& scope)
{
	LogNotice("Wait, verify no trigger\n");
	this_thread::sleep_for(chrono::milliseconds(250));
	auto stat = scope.PollTrigger();

	if(stat != Oscilloscope::TRIGGER_MODE_RUN)
	{
		LogError("Scope should not have triggered, but did anyway\n");
	}

	REQUIRE(stat == Oscilloscope::TRIGGER_MODE_RUN);
}

/**
	@brief Acquire a waveform and complain if it doesn't trigger within the timeout period
 */
bool AcquireWaveformWithTimeout(Oscilloscope& scope, float timeoutSec)
{
	double now = GetTime();
	double deadline = now + timeoutSec;

	//Make sure we got a waveform
	LogNotice("Wait for waveform\n");
	while(true)
	{
		auto stat = scope.PollTrigger();
		if(stat == Oscilloscope::TRIGGER_MODE_TRIGGERED)
			break;

		now = GetTime();
		if(now >= deadline)
		{
			REQUIRE(now < deadline);
			return false;
		}
	}

	//Grab it
	LogNotice("Download waveform\n");
	scope.AcquireData();
	while(true)
	{
		scope.BackgroundProcessing();
		if(scope.GetPendingWaveformCount() >= 1)
			break;

		now = GetTime();
		if(now >= deadline)
		{
			REQUIRE(now < deadline);
			return false;
		}
	}

	scope.PopPendingWaveform();
	return true;
}
