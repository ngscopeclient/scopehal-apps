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
#include "../../lib/scopeprotocols/UARTDecoder.h"

using namespace std;

class UARTDecodeTest : public HILTest
{
public:
	UARTDecodeTest()
	{}

	virtual ~UARTDecodeTest()
	{}

	virtual void Run()
	{
		auto depth = 1000 * 1000 * 10;
		HardwareSetup(depth);

		{
			LogNotice("Acquire waveform\n");
			LogIndenter li;

			//Enable the output
			LogNotice("Enable UART output\n");
			m_siggen->SendCommandQueued("SMA1:MUX UART");
			SignalGeneratorSetupDone();

			//Grab the waveform
			LogNotice("Arm trigger\n");
			m_scope->SetTriggerOffset(1e12);
			m_scope->PushTrigger();
			m_scope->StartSingleTrigger();
			m_scope->GetTransport()->FlushCommandQueue();
			AcquireWaveformWithTimeout(0.25);
		}

		//Make sure it's plausible
		auto chan = m_scope->GetChannel(0);
		SanityCheckWaveform(0, depth);

		//Set up filter graph
		auto fthresh = dynamic_cast<ThresholdFilter*>(Filter::CreateFilter("Threshold", "#ffffff"));
		REQUIRE(fthresh != nullptr);
		fthresh->GetThreshold().SetFloatVal(0.9);

		auto fuart = dynamic_cast<UARTDecoder*>(Filter::CreateFilter("UART", "#ffffff"));
		REQUIRE(fuart != nullptr);
		fuart->GetBaud().SetIntVal(115200);

		fthresh->SetInput("din", chan);
		fuart->SetInput("din", fthresh);

		m_nodes.emplace(fuart);
		m_nodes.emplace(fthresh);

		RunFilterGraph();

		{
			LogNotice("Validate results. Expecting hello world + index\n");
			LogIndenter li;

			LogNotice("UART has %zu error messages\n", fuart->GetMessages().size());
			REQUIRE(!fuart->HasMessages());

			//Check the decode
			auto udata = dynamic_cast<ByteWaveform*>(fuart->GetData(0));
			REQUIRE(udata != nullptr);
			udata->PrepareForCpuAccess();
			LogNotice("Got %zu bytes of data\n", udata->size());

			//Get the data
			string str;
			for(size_t i=0; i<udata->size(); i++)
				str += udata->m_samples[i];
			LogNotice("Data: %s\n", str.c_str());

			char num[128];
			REQUIRE(1 == sscanf(str.c_str(), "hello world %127[0-9]", num));
		}
	}
};

TEST_CASE("HIL_UARTDecode")
{
	UARTDecodeTest test;
	test.Run();
}
