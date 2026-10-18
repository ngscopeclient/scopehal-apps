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
	@brief Unit test for waveform capture and some simple measurements
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "HIL.h"
#include "../../lib/scopeprotocols/FrequencyMeasurement.h"

using namespace std;

class TSTriggerDesyncTest : public HILTest
{
public:
	TSTriggerDesyncTest()
	{}

	virtual ~TSTriggerDesyncTest()
	{}

	virtual void Run()
	{
		auto depth = 1000 * 1000;
		HardwareSetup(depth);

		auto chan = m_scope->GetChannel(0);
		auto favg = dynamic_cast<AverageFilter*>(Filter::CreateFilter("Average", "#ffffff"));
		REQUIRE(favg != nullptr);
		favg->SetInput("in", chan);

		m_nodes.emplace(favg);

		Unit volts(Unit::UNIT_VOLTS);

		//Grab two waveforms to make sure single-shot trigger works properly
		{
			LogNotice("1: Clock, single shot trigger\n");
			LogIndenter li;

			//Enable the output as a clock
			m_siggen->SendCommandQueued("SMA1:MUX CLOCK");
			SignalGeneratorSetupDone();

			//Grab the waveform
			m_scope->StartSingleTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			AcquireWaveformWithTimeout(0.25);

			//Make sure it's plausible
			SanityCheckWaveform(0, depth);
			RunFilterGraph();
			auto vavg = favg->GetScalarValue(0);
			LogNotice("Avg:  %s\n", volts.PrettyPrint(vavg).c_str());

			REQUIRE(vavg > 0.8);
			REQUIRE(vavg < 1.0);
		}

		{
			LogNotice("2: Off, force trigger\n");
			LogIndenter li;

			//Enable the output as a clock
			m_siggen->SendCommandQueued("SMA1:MUX OFF");
			SignalGeneratorSetupDone();

			//Grab the waveform
			m_scope->ForceTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			AcquireWaveformWithTimeout(0.25);

			//Make sure it's plausible
			SanityCheckWaveform(0, depth);
			RunFilterGraph();
			auto vavg = favg->GetScalarValue(0);
			LogNotice("Avg:  %s\n", volts.PrettyPrint(vavg).c_str());

			REQUIRE(vavg > -0.05);
			REQUIRE(vavg < 0.05);
		}

		//Grab five waveforms in normal trigger
		{
			LogNotice("3: Clock, 5 waveforms normal trigger\n");
			LogIndenter li;

			//Enable the output as a clock
			m_siggen->SendCommandQueued("SMA1:MUX CLOCK");
			SignalGeneratorSetupDone();

			//Grab the waveform
			m_scope->Start();
			m_scope->GetTransport()->FlushCommandQueue();
			for(size_t i=0; i</*5*/1; i++)
			{
				LogIndenter li2;

				AcquireWaveformWithTimeout(0.25);

				//Make sure it's plausible
				SanityCheckWaveform(0, depth);
				RunFilterGraph();
				auto vavg = favg->GetScalarValue(0);
				LogNotice("%zu / 5:  avg = %s\n", i+1, volts.PrettyPrint(vavg).c_str());

				REQUIRE(vavg > 0.8);
				REQUIRE(vavg < 1.0);
			}

			m_scope->Stop();
		}

		//We stopped the trigger after capturing a clock waveform.
		//Turn the signal generator off and grab a single-shot trigger.
		//We should see a flatlined input.
		{
			LogNotice("4: Off, force trigger\n");
			LogIndenter li;

			//Enable the output as a clock
			m_siggen->SendCommandQueued("SMA1:MUX OFF");
			SignalGeneratorSetupDone();

			//Grab the waveform
			m_scope->ForceTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			AcquireWaveformWithTimeout(0.25);

			//Make sure it's plausible
			SanityCheckWaveform(0, depth);
			RunFilterGraph();
			auto vavg = favg->GetScalarValue(0);
			LogNotice("Avg:  %s\n", volts.PrettyPrint(vavg).c_str());

			REQUIRE(vavg > -0.05);
			REQUIRE(vavg < 0.05);
		}
	}
};

TEST_CASE("HIL_TSTriggerDesync")
{
	TSTriggerDesyncTest test;
	test.Run();
}
