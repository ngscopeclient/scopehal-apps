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
	@brief Unit test for UART
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "HIL.h"
#include "../../lib/scopeprotocols/ThresholdFilter.h"
#include "../../lib/scopeprotocols/PRBSCheckerFilter.h"
#include "../../lib/scopeprotocols/ClockRecoveryFilter.h"
#include "../../lib/scopeprotocols/FrequencyMeasurement.h"

using namespace std;

class PRBS7DecodeTest : public HILTest
{
public:
	PRBS7DecodeTest()
	{}

	virtual ~PRBS7DecodeTest()
	{}

	virtual void Run()
	{
		auto depth = 1000 * 1000 * 10;
		HardwareSetup(depth);

		{
			LogNotice("Acquire waveform\n");
			LogIndenter li;

			//Enable the output
			LogNotice("Enable PRBS7 output\n");
			m_siggen->SendCommandQueued("SMA1:MUX PRBS7");
			SignalGeneratorSetupDone();

			//Grab the waveform
			LogNotice("Arm trigger\n");
			m_scope->StartSingleTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			AcquireWaveformWithTimeout(0.25);
		}

		//Make sure it's plausible
		auto chan = m_scope->GetChannel(0);
		SanityCheckWaveform(0, depth);

		//Set up filter graph
		auto fcdr = dynamic_cast<ClockRecoveryFilter*>(Filter::CreateFilter("Clock Recovery (PLL)", "#ffffff"));
		fcdr->GetBaudRate().SetIntVal(250 * 1000 * 1000);
		fcdr->GetThreshold().SetFloatVal(0.9);

		auto ffreq = dynamic_cast<FrequencyMeasurement*>(Filter::CreateFilter("Frequency", "#ffffff"));
		REQUIRE(ffreq != nullptr);

		auto fthresh = dynamic_cast<ThresholdFilter*>(Filter::CreateFilter("Threshold", "#ffffff"));
		REQUIRE(fthresh != nullptr);
		fthresh->GetThreshold().SetFloatVal(0.9);

		auto fprbs7 = dynamic_cast<PRBSCheckerFilter*>(Filter::CreateFilter("PRBS Checker", "#ffffff"));
		REQUIRE(fprbs7 != nullptr);
		fprbs7->GetPolynomial().SetIntVal(PRBSGeneratorFilter::POLY_PRBS7);

		fcdr->SetInput("IN", chan);
		ffreq->SetInput("din", StreamDescriptor(fcdr, 0));
		fthresh->SetInput("din", StreamDescriptor(fcdr, 1));
		fprbs7->SetInput("sampledData", StreamDescriptor(fthresh, 0), true);	//must force
																				//because we don't know threshold type
		m_nodes.emplace(fcdr);
		m_nodes.emplace(ffreq);
		m_nodes.emplace(fthresh);
		m_nodes.emplace(fprbs7);

		RunFilterGraph();

		{
			LogNotice("Validate results. Expecting valid PRBS7 and 125 MHz recovered clock\n");
			LogIndenter li;

			//Should be 125 MHz
			auto clkdata = dynamic_cast<SparseDigitalWaveform*>(fcdr->GetData(0));
			auto nsamples = clkdata->size();
			Unit hz(Unit::UNIT_HZ);
			auto nfreq = ffreq->GetScalarValue(1);
			LogNotice("Recovered clock frequency: %s (%zu edges)\n",
				hz.PrettyPrint(nfreq).c_str(),
				nsamples);
			REQUIRE(nfreq > 124999000);
			REQUIRE(nfreq < 125001000);

			//Expect one sample per UI of the input
			auto prbs7data = dynamic_cast<SparseDigitalWaveform*>(fprbs7->GetData(0));
			REQUIRE(prbs7data != nullptr);
			REQUIRE(prbs7data->size() == nsamples);
			prbs7data->PrepareForCpuAccess();

			//Make sure they're all 0 (no errors)
			for(size_t i=0; i<nsamples; i++)
				REQUIRE(prbs7data->m_samples[i] == 0);
		}

		//Grab another waveform, but this time a PRBS31
		{
			LogNotice("Acquire invalid waveform\n");
			LogIndenter li;

			//Enable the output
			LogNotice("Enable PRBS31 output\n");
			m_siggen->SendCommandQueued("SMA1:MUX PRBS31");
			SignalGeneratorSetupDone();

			//Grab the waveform
			LogNotice("Arm trigger\n");
			m_scope->StartSingleTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			AcquireWaveformWithTimeout(0.25);
		}

		RunFilterGraph();

		{
			LogNotice("Validate results. Expecting 125 MHz recovered clock and a lot of PRBS errors\n");
			LogIndenter li;

			//Should be 125 MHz
			auto clkdata = dynamic_cast<SparseDigitalWaveform*>(fcdr->GetData(0));
			auto nsamples = clkdata->size();
			Unit hz(Unit::UNIT_HZ);
			auto nfreq = ffreq->GetScalarValue(1);
			LogNotice("Recovered clock frequency: %s (%zu edges)\n",
				hz.PrettyPrint(nfreq).c_str(),
				nsamples);
			REQUIRE(nfreq > 124999000);
			REQUIRE(nfreq < 125001000);

			//Expect one sample per UI of the input
			auto prbs7data = dynamic_cast<SparseDigitalWaveform*>(fprbs7->GetData(0));
			REQUIRE(prbs7data != nullptr);
			REQUIRE(prbs7data->size() == nsamples);
			prbs7data->PrepareForCpuAccess();

			//We're checking against the wrong polynomial. Count errors, there should be a lot
			size_t errors = 0;
			for(size_t i=0; i<nsamples; i++)
				errors += (prbs7data->m_samples[i] != 0);
			LogNotice("Found %zu bit errors when checking PRBS31 data stream against PRBS7 polynomial\n", errors);

			//We expect roughly half the bits to be in error, but it'll vary a bit.
			//We're testing with 2.5M UIs so we should have roughly 1.25M bit errors
			size_t expectedAverageErrors = nsamples / 2;
			size_t minAllowedErrors = expectedAverageErrors - 10000;
			size_t maxAllowedErrors = expectedAverageErrors + 10000;
			REQUIRE(expectedAverageErrors > minAllowedErrors);
			REQUIRE(expectedAverageErrors < maxAllowedErrors);
		}
	}
};

TEST_CASE("HIL_PRBS7Decode")
{
	PRBS7DecodeTest test;
	test.Run();
}
