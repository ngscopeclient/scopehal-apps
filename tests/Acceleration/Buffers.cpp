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
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Acceleration.h"

using namespace std;

void FillAndVerifyBuffer(AcceleratorBuffer<int32_t>& buf, size_t len);
void FillBuffer(AcceleratorBuffer<int32_t>& buf, size_t len);
void VerifyBuffer(AcceleratorBuffer<int32_t>& buf, size_t len);

TEST_CASE("Buffers_CpuOnly")
{
	AcceleratorBuffer<int32_t> buf;

	//CPU-only buffer
	SECTION("FrequentCPU")
	{
		LogVerbose("AcceleratorBuffer: CPU HINT_LIKELY, GPU HINT_NEVER (host memory)\n");
		LogIndenter li;

		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER);

		FillAndVerifyBuffer(buf, 5);

		REQUIRE(buf.HasCpuBuffer());
		REQUIRE(!buf.HasGpuBuffer());
	}

	//CPU-only buffer in file backed memory
	SECTION("InfrequentCPU")
	{
		LogVerbose("AcceleratorBuffer: CPU HINT_UNLIKELY, GPU HINT_NEVER (file backed)\n");
		LogIndenter li;

		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER);
		buf.PrepareForCpuAccess();

		FillAndVerifyBuffer(buf, 5);

		REQUIRE(buf.HasCpuBuffer());
		REQUIRE(!buf.HasGpuBuffer());
	}

	//Pinned memory which can be shared with GPU (but only ever actually used from CPU for this test)
	SECTION("PinnedCPU")
	{
		LogVerbose("AcceleratorBuffer: CPU HINT_LIKELY, GPU HINT_UNLIKELY (pinned memory)\n");
		LogIndenter li;

		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
		buf.PrepareForCpuAccess();

		//Run the test
		FillAndVerifyBuffer(buf, 5);

		REQUIRE(buf.HasCpuBuffer());
		REQUIRE(!buf.HasGpuBuffer());
	}

	//Allocate a single buffer, then move it around between different types of memory
	SECTION("MovingCPUBuffer")
	{
		LogVerbose("AcceleratorBuffer: moving around\n");
		LogIndenter li;

		{
			LogVerbose("CPU HINT_LIKELY, GPU HINT_NEVER (host memory)\n");
			LogIndenter li2;

			buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);
			buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER);
			FillAndVerifyBuffer(buf, 5);

			REQUIRE(buf.HasCpuBuffer());
			REQUIRE(!buf.HasGpuBuffer());
		}

		{
			LogVerbose("CPU HINT_UNLIKELY, GPU HINT_NEVER (file backed)\n");
			LogIndenter li2;

			buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
			buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER);
			VerifyBuffer(buf, 5);

			REQUIRE(buf.HasCpuBuffer());
			REQUIRE(!buf.HasGpuBuffer());
		}

		{
			LogVerbose("CPU HINT_UNLIKELY, GPU HINT_UNLIKELY (pinned memory)\n");
			LogIndenter li2;

			buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
			buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_UNLIKELY);
			VerifyBuffer(buf, 5);

			REQUIRE(buf.HasCpuBuffer());
			REQUIRE(!buf.HasGpuBuffer());
		}

		{
			LogVerbose("Making a copy of the buffer\n");

			//Make a copy of it
			AcceleratorBuffer<int32_t> buf2;
			buf2.CopyFrom(buf);

			REQUIRE(buf2.HasCpuBuffer());
			REQUIRE(!buf2.HasGpuBuffer());
			VerifyBuffer(buf2, 5);
		}
	}
}

TEST_CASE("Buffers_CpuGpu")
{
	AcceleratorBuffer<int32_t> buf;

	//CPU-side pinned memory plus GPU-side dedicated buffer,
	//but only ever used from the CPU
	SECTION("MirrorCopy")
	{
		LogVerbose("AcceleratorBuffer: CPU HINT_LIKELY, GPU HINT_LIKELY (host memory with GPU mirror), but only using from CPU\n");
		LogIndenter li;

		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);
		buf.SetGpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY);

		FillAndVerifyBuffer(buf, 5);


		if(g_vulkanDeviceHasUnifiedMemory)
		{
			//On a system with unified memory:
			//At this point, the buffer is on the CPU, and there is no GPU buffer
			REQUIRE(!buf.IsCpuBufferStale());
			REQUIRE(buf.IsSingleSharedBuffer());
			REQUIRE(buf.HasCpuBuffer());
			REQUIRE(!buf.HasGpuBuffer());
		}
		else
		{
			//On a system without unified memory:
			//At this point, the buffer is on the CPU, and there is a stale buffer on the GPU with no content
			//We modified by calling push_back so the GPU buffer should already be marked stale.
			REQUIRE(!buf.IsCpuBufferStale());
			REQUIRE(buf.IsGpuBufferStale());
			REQUIRE(!buf.IsSingleSharedBuffer());
			REQUIRE(buf.HasCpuBuffer());
			REQUIRE(buf.HasGpuBuffer());
		}

		//Prepare for GPU-side access by copying the buffer to the GPU.
		//At this point we should be fully up to date
		//On a system with unified memory, this should be a no-op:
		buf.PrepareForGpuAccess();

		REQUIRE(buf.HasCpuBuffer());
		REQUIRE(!buf.IsCpuBufferStale());
		if(!g_vulkanDeviceHasUnifiedMemory)
		{
			REQUIRE(buf.HasGpuBuffer());
			REQUIRE(!buf.IsGpuBufferStale());
		}
		else
		{
			REQUIRE(!buf.HasGpuBuffer());
			REQUIRE(buf.IsSingleSharedBuffer());
		}

		//Now, mark the CPU side buffer as never being used so we can free it and have a GPU-only buffer
		//(unless the system has unified memory, in which case this should be a no-op)
		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_NEVER, true);

		if(!g_vulkanDeviceHasUnifiedMemory)
		{
			REQUIRE(!buf.HasCpuBuffer());
			REQUIRE(buf.HasGpuBuffer());
			REQUIRE(!buf.IsGpuBufferStale());
		}
		else
		{
			REQUIRE(buf.HasCpuBuffer());
			REQUIRE(!buf.HasGpuBuffer());
			REQUIRE(buf.IsSingleSharedBuffer());
			REQUIRE(!buf.IsCpuBufferStale());
		}

		//Make a copy of the GPU-only buffer
		AcceleratorBuffer<int32_t> buf2;
		buf2.CopyFrom(buf);

		if(!g_vulkanDeviceHasUnifiedMemory)
		{
			REQUIRE(!buf2.HasCpuBuffer());
			REQUIRE(buf2.HasGpuBuffer());
			REQUIRE(!buf2.IsGpuBufferStale());
		}
		else
		{
			//If the system has unified memory, this should still be a CPU buffer
			REQUIRE(buf2.HasCpuBuffer());
			REQUIRE(!buf2.HasGpuBuffer());
			REQUIRE(buf2.IsSingleSharedBuffer());
			REQUIRE(!buf2.IsCpuBufferStale());
		}
		//Give it a CPU-capable hint so we can see it, then verify
		buf2.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY, true);
		VerifyBuffer(buf2, 5);

		//Mark the CPU-side buffer as being frequently used again, but don't copy data over to it.
		//If the platform does not have unified memory, we should now have a CPU-side buffer,
		//but it should be stale (while the GPU-side buffer is current)
		//If we have unified memory, then the buffer will have not changed and will be current
		//as there is only ever a "CPU-side" buffer
		buf.SetCpuAccessHint(AcceleratorBuffer<int32_t>::HINT_LIKELY, true);

		if(!g_vulkanDeviceHasUnifiedMemory)
		{
			REQUIRE(buf.HasGpuBuffer());
			REQUIRE(buf.IsCpuBufferStale());
			REQUIRE(!buf.IsGpuBufferStale());
		}
		else
		{
			REQUIRE(!buf.HasGpuBuffer());
			REQUIRE(!buf.IsCpuBufferStale());
			REQUIRE(buf.IsSingleSharedBuffer());
		}

		//Verify the CPU-side buffer
		VerifyBuffer(buf, 5);

		//Remove one item from it
		buf.pop_back();
		VerifyBuffer(buf, 4);

		//Remove the first item
		buf.pop_front();

		//Verify the pop_front worked
		REQUIRE(buf.size() == 3);
		for(int32_t i=0; i<3; i++)
			REQUIRE(buf[i] == i+1);

		//Empty the buffer
		buf.clear();
		REQUIRE(buf.size() == 0);
		REQUIRE(buf.empty());
	}
}

void FillBuffer(AcceleratorBuffer<int32_t>& buf, size_t len)
{
	buf.PrepareForCpuAccess();

	//Add some elements to it
	for(size_t i=0; i<len; i++)
		buf.push_back(i);
}

void VerifyBuffer(AcceleratorBuffer<int32_t>& buf, size_t len)
{
	//We need a buffer to read!
	REQUIRE(buf.HasCpuBuffer());

	buf.PrepareForCpuAccess();

	//Should never be stale by this point
	REQUIRE(!buf.IsCpuBufferStale());

	//Verify they're there
	REQUIRE(buf.size() == len);
	REQUIRE(buf.capacity() >= len);
	for(size_t i=0; i<len; i++)
		REQUIRE(buf[i] == (int32_t)i);

	//Verify again, but looping using iterators rather than array indexing
	int32_t iexpected = 0;
	for(auto n : buf)
	{
		REQUIRE(n == iexpected);
		iexpected ++;
	}
}

void FillAndVerifyBuffer(AcceleratorBuffer<int32_t>& buf, size_t len)
{
	FillBuffer(buf, len);
	VerifyBuffer(buf, len);
}
