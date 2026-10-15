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
	@brief Unit test for basic waveform acquisition
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "HIL.h"
#include "../../lib/scopeprotocols/BaseMeasurement.h"
#include "../../lib/scopeprotocols/FrequencyMeasurement.h"
#include "../../lib/scopeprotocols/TopMeasurement.h"
#include "../../lib/scopeprotocols/MinimumFilter.h"
#include "../../lib/scopeprotocols/MaximumFilter.h"

using namespace std;

TEST_CASE("HIL_BasicMetrics")
{
	//TODO: actually have scopehal driver here instead of doing it raw
	LogNotice("Connecting to signal generator board\n");
	SCPISocketTransport siggen(g_siggenPath);
	auto siginfo = siggen.SendCommandQueuedWithReply("*IDN?");
	LogNotice("siginfo = %s\n", siginfo.c_str());

	//TODO: Configure mux board we dont yet have

	//Try to connect to the scope
	ThunderScopeOscilloscope scope(new SCPITwinLanTransport(g_scopePath));
	LogNotice("Connected to scope make=%s model=%s serial=%s\n",
		scope.GetVendor().c_str(), scope.GetName().c_str(), scope.GetSerial().c_str());

	{
		LogNotice("Hardware setup\n");
		LogIndenter li;

		//Default setup for one digital input
		SetupScopeForSingleChannelDigitalInput(scope);
	}

	{
		LogNotice("Acquire waveform\n");
		LogIndenter li;

		//Enable the output as a clock
		LogNotice("Enable clock output\n");
		siggen.SendCommandQueued("SMA1:MUX CLOCK");
		siggen.FlushCommandQueue();
		this_thread::sleep_for(chrono::milliseconds(250));

		//Grab the waveform
		LogNotice("Arm trigger\n");
		scope.StartSingleTrigger();
		scope.GetTransport()->FlushCommandQueue();
		AcquireWaveformWithTimeout(scope, 0.5);
	}

	auto chan = scope.GetChannel(0);
	auto data = chan->GetData(0);
	{
		LogNotice("Sanity check waveform\n");
		LogIndenter li;

		//Verify it's plausible (non-null, right number of samples)
		REQUIRE(data != nullptr);
		auto dwfm = dynamic_cast<UniformAnalogWaveform*>(data);
		REQUIRE(dwfm != nullptr);
		LogNotice("Data is a non-null uniform analog waveform, %zu samples\n", dwfm->size());
		REQUIRE(dwfm->size() == (1000 * 1000));
	}

	//Set up filter graph
	auto fbase = dynamic_cast<BaseMeasurement*>(Filter::CreateFilter("Base", "#ffffff"));
	REQUIRE(fbase != nullptr);
	auto ftop = dynamic_cast<TopMeasurement*>(Filter::CreateFilter("Top", "#ffffff"));
	REQUIRE(ftop != nullptr);
	auto ffreq = dynamic_cast<FrequencyMeasurement*>(Filter::CreateFilter("Frequency", "#ffffff"));
	REQUIRE(ffreq != nullptr);
	auto ffmax = dynamic_cast<MaximumFilter*>(Filter::CreateFilter("Maximum", "#ffffff"));
	REQUIRE(ffmax != nullptr);
	auto ffmin = dynamic_cast<MinimumFilter*>(Filter::CreateFilter("Minimum", "#ffffff"));
	REQUIRE(ffmin != nullptr);
	auto favg = dynamic_cast<AverageFilter*>(Filter::CreateFilter("Average", "#ffffff"));
	REQUIRE(favg != nullptr);
	fbase->SetInput("din", chan);
	ftop->SetInput("din", chan);
	ffreq->SetInput("din", chan);
	ffmax->SetInput("in", chan);
	ffmin->SetInput("in", chan);
	favg->SetInput("in", chan);

	//Set up executor
	FilterGraphExecutor exec;
	set<FlowGraphNode*> nodes;
	nodes.emplace(fbase);
	nodes.emplace(ftop);
	nodes.emplace(ffreq);
	nodes.emplace(ffmax);
	nodes.emplace(ffmin);
	nodes.emplace(favg);

	{
		LogNotice("Filter graph\n");
		LogIndenter li;

		//Run it
		double start = GetTime();
		exec.RunBlocking(nodes);
		double dt = GetTime() - start;
		LogNotice("Filter graph took %.3f ms\n", dt * 1000);
	}

	{
		LogNotice("Validate results. Expecting 125 MHz LVCMOS18 squarewave\n");
		LogIndenter li;

		Unit volts(Unit::UNIT_VOLTS);

		//This is a LVCMOS18 signal into a hi-Z input.
		//We should have base roughly around zero and top roughly around 1.8V
		auto vbase = fbase->GetScalarValue(1);
		auto vtop = ftop->GetScalarValue(1);
		auto vmin = ffmin->GetScalarValue(1);
		auto vmax = ffmax->GetScalarValue(1);
		auto vavg = favg->GetScalarValue(0);
		LogNotice("Min:  %s\n", volts.PrettyPrint(vmin).c_str());
		LogNotice("Base: %s\n", volts.PrettyPrint(vbase).c_str());
		LogNotice("Avg:  %s\n", volts.PrettyPrint(vavg).c_str());
		LogNotice("Top:  %s\n", volts.PrettyPrint(vtop).c_str());
		LogNotice("Max:  %s\n", volts.PrettyPrint(vmax).c_str());

		REQUIRE(vmin > -0.05);
		REQUIRE(vmin < 0.15);

		REQUIRE(vbase > -0.05);
		REQUIRE(vbase < 0.15);

		REQUIRE(vavg > 0.8);
		REQUIRE(vavg < 1.0);

		REQUIRE(vtop > 1.5);
		REQUIRE(vtop < 1.95);

		REQUIRE(vmax > 1.5);
		REQUIRE(vmax < 1.95);

		//Should be 125 MHz
		Unit hz(Unit::UNIT_HZ);
		auto nfreq = ffreq->GetScalarValue(1);
		LogNotice("Frequency: %s\n", hz.PrettyPrint(nfreq).c_str());
		REQUIRE(nfreq > 124999000);
		REQUIRE(nfreq < 125001000);
	}

	//Delete filters
	for(auto n : nodes)
		delete n;
}
