/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
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
	@brief Unit test for SampleOn* primitives
 */
#include <catch2/catch.hpp>

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Acceleration.h"

using namespace std;

void FillAndVerifyBuffer(AcceleratorBuffer<int32_t>& buf, size_t len);

TEST_CASE("Buffers_CpuAccess")
{
	//CPU-only buffer
	SECTION("FrequentCPU")
	{
		//Set up the buffer and prepare our expected access patterns
		LogVerbose("Creating a buffer with CPU HINT_LIKELY, GPU HINT_NEVER\n");
		AcceleratorBuffer<int32_t> buf;
		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER);

		//Run the test
		FillAndVerifyBuffer(buf, 5);
	}

	//CPU-only buffer in file backed memory
	SECTION("InfrequentCPU")
	{
		//Set up the buffer and prepare our expected access patterns
		LogVerbose("Creating a buffer with CPU HINT_UNLIKELY, GPU HINT_NEVER\n");
		AcceleratorBuffer<int32_t> buf;
		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER);
		buf.PrepareForCpuAccess();

		//Run the test
		FillAndVerifyBuffer(buf, 5);
	}

	//Pinned memory which can be shared with GPU (but only ever actually used from CPU for this test)
	SECTION("PinnedCPU")
	{
		//Set up the buffer and prepare our expected access patterns
		LogVerbose("Creating a buffer with CPU HINT_LIKELY, GPU HINT_UNLIKELY\n");
		AcceleratorBuffer<int32_t> buf;
		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
		buf.PrepareForCpuAccess();

		//Run the test
		FillAndVerifyBuffer(buf, 5);
	}
}

void FillAndVerifyBuffer(AcceleratorBuffer<int32_t>& buf, size_t len)
{
	buf.PrepareForCpuAccess();

	//Add some elements to it
	LogVerbose("Filling it\n");
	for(size_t i=0; i<len; i++)
		buf.push_back(i);

	//Verify they're there
	LogVerbose("Verifying initial contents (capacity is now %zu)\n", buf.capacity());
	REQUIRE(buf.size() == len);
	REQUIRE(buf.capacity() >= len);
	for(size_t i=0; i<len; i++)
		REQUIRE(buf[i] == i);
}
