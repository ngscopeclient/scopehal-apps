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

TEST_CASE("HIL_UARTDecode")
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
		LogNotice("Acquisition config for UART\n");
		LogIndenter li;

		scope.SetSampleDepth(1000 * 1000 * 10);
		scope.SetTriggerOffset(1e12);
		scope.PushTrigger();
		scope.GetTransport()->FlushCommandQueue();
	}

	{
		LogNotice("Acquire waveform\n");
		LogIndenter li;

		//Enable the output as UART
		LogNotice("Enable UART output\n");
		siggen.SendCommandQueued("SMA1:MUX UART");
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
		REQUIRE(dwfm->size() == (1000 * 1000 * 10));
	}

	//Set up filter graph
	auto fthresh = dynamic_cast<ThresholdFilter*>(Filter::CreateFilter("Threshold", "#ffffff"));
	REQUIRE(fthresh != nullptr);
	fthresh->GetThreshold().SetFloatVal(0.9);

	auto fuart = dynamic_cast<UARTDecoder*>(Filter::CreateFilter("UART", "#ffffff"));
	REQUIRE(fuart != nullptr);
	fuart->GetBaud().SetIntVal(115200);

	fthresh->SetInput("din", chan);
	fuart->SetInput("din", fthresh);

	//Set up executor
	FilterGraphExecutor exec;
	set<FlowGraphNode*> nodes;
	nodes.emplace(fthresh);
	nodes.emplace(fuart);

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

	//Delete filters
	for(auto n : nodes)
		n->DetachInputs();
	for(auto n : nodes)
		delete n;
}
