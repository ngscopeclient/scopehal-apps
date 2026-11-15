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
	@brief Unit test for kernel fusion
 */
#ifdef _CATCH2_V3
#include <catch2/catch_all.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "../../lib/scopehal/scopehal.h"
#include "../../lib/scopehal/ShaderBaker.h"
#include "../../lib/scopehal/TestWaveformSource.h"
#include "../../lib/scopeprotocols/scopeprotocols.h"
#include "Primitives.h"
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>

using namespace std;

TEST_CASE("Primitive_KernelFusion")
{
	//Create a queue and command buffer
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetQueueFromPool(
		QueueManager::QUEUE_POOL_FILTER,
		"Primitive_KernelFusion.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->GetQueue()->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdBuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	//Create an empty input waveforms
	const size_t depth = 10000000;
	UniformAnalogWaveform ua;
	ua.Resize(depth);
	g_scope->GetOscilloscopeChannel(0)->SetData(&ua, 0);

	//Set up filter configuration
	auto emphasis = dynamic_cast<EmphasisFilter*>(Filter::CreateFilter("Emphasis", "#ffffff"));
	auto upsample = dynamic_cast<UpsampleFilter*>(Filter::CreateFilter("Upsample", "#ffffff"));
	REQUIRE(emphasis != nullptr);
	REQUIRE(upsample != nullptr);
	emphasis->AddRef();
	upsample->AddRef();
	emphasis->SetInput("in",  g_scope->GetOscilloscopeChannel(0));
	upsample->SetInput("din", StreamDescriptor(emphasis, 0));

	//Set up filter graph
	FilterGraphExecutor executor;
	set<FlowGraphNode*> nodes;
	nodes.emplace(emphasis);
	nodes.emplace(upsample);

	//First stage shader
	auto es = make_shared<FusibleShader>("EmphasisFilter.glsl", "EmphasisFilter");
	auto stage1 = make_shared<BakedShaderStage>(es);

	//Second stage shader
	auto us = make_shared<FusibleShader>("UpsampleFilter.glsl", "UpsampleFilter");
	auto stage2 = make_shared<BakedShaderStage>(us);

	//Set up the shader fusion
	ShaderBaker baker;
	baker.AddStage(stage1);
	baker.AddStage(stage2);
	baker.SetOutputStage(stage2);
	auto fused = baker.Bake();

	/*
	//Make the fused shader by hand to start
	auto ttop = ReadDataFile("shaderfusion/Template_TopLevel.glsl");
	auto tbuf = ReadDataFile("shaderfusion/Template_SSBO.glsl");
	auto shader1 = ReadDataFile("shaderfusion/EmphasisFilter.glsl");
	auto shader2 = ReadDataFile("shaderfusion/UpsampleFilter.glsl");

	//Configure the input SSBO
	auto sdin = str_replace("[[BINDING_NUM]]", "0", tbuf);
	sdin = str_replace("[[ACCESS_TYPE]]", "readonly", sdin);
	sdin = str_replace("[[TYPE]]", "float", sdin);
	sdin = str_replace("[[NAME]]", "din", sdin);

	//Configure the kernel SSBO
	auto skernel = str_replace("[[BINDING_NUM]]", "1", tbuf);
	skernel = str_replace("[[ACCESS_TYPE]]", "readonly", skernel);
	skernel = str_replace("[[TYPE]]", "float", skernel);
	skernel = str_replace("[[NAME]]", "fkernel", skernel);

	//Configure the output SSBO
	auto sout = str_replace("[[BINDING_NUM]]", "2", tbuf);
	sout = str_replace("[[ACCESS_TYPE]]", "writeonly", sout);
	sout = str_replace("[[TYPE]]", "float", sout);
	sout = str_replace("[[NAME]]", "dout", sout);

	//Final buffer list
	auto sbuffers = sdin + "\n" + skernel + "\n" + sout;

	//Final  template replacement
	ttop = str_replace("[[BUFFERS]]", sbuffers, ttop);
	*/
	//Save the output
	FILE* fp = fopen("/tmp/foo.glsl", "w");
	fwrite(fused.c_str(), 1, fused.size(), fp);
	fclose(fp);

	/*

	//Try compiling a shader
	//TODO use current vulkan version for jit
	auto shaderSource = ReadDataFile("shaderfusion/EmphasisFilter.glsl");
	{
		//source buffers must remain valid for the entire lifetime of the TShader object
		glslang::TShader shader(EShLangCompute);
		shader.addSourceText(shaderSource.c_str(), shaderSource.length());
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

		double start = GetTime();
		shader.parse(GetResources(), 460, false, EShMsgDefault);
		double dt = GetTime() - start;
		LogNotice("shader parse took %.3f ms\n", dt * 1000);
	}*/

	const size_t niter = 5;
	for(size_t i=0; i<niter; i++)
	{
		SECTION(string("Iteration ") + to_string(i))
		{
			LogVerbose("Iteration %zu\n", i);
			LogIndenter li;

			//Create two random input waveforms
			FillRandomWaveform(&ua, g_rng, depth);

			//Get it on the GPU (don't count this towards execution time)
			ua.PrepareForGpuAccess();

			double start = GetTime();
			executor.RunBlocking(nodes);
			double dt = GetTime() - start;
			LogNotice("Unfused: %.3f ms\n", dt * 1000);
		}
	}

	/*
	SECTION("UniformAnalogWaveform")
	{
		//Input waveforms
		UniformAnalogWaveform wfm;
		source.GenerateNoisySinewave(cmdBuf, queue, &wfm, 1.0, 0.0, 200000, 20000, depth, 0.1);
		wfm.PrepareForCpuAccess();

		//Find the reference zero crossings using the base function
		float threshold = 0.05;
		double start = GetTime();
		vector<int64_t> edges;
		Filter::FindZeroCrossings(&wfm, threshold, edges);
		double dt = GetTime() - start;
		LogNotice("CPU: %.3f ms, %zu edges, %zu samples\n", dt*1000, edges.size(), depth);

		//Do the GPU version
		//Run twice, second time for score, so we don't count deferred init or allocations in the benchmark
		LevelCrossingDetector ldet;
		auto& gpuedges = ldet.GetResults();
		ldet.FindZeroCrossings(&wfm, threshold, cmdBuf, queue);
		start = GetTime();
		size_t gpulen = ldet.FindZeroCrossings(&wfm, threshold, cmdBuf, queue);
		double gpudt = GetTime() - start;
		LogNotice("GPU: %.3f ms, %zu edges, %.2fx speedup\n", gpudt*1000, gpulen, dt / gpudt);

		//Don't count the final memcpy against run time since we assume the buffer is going to get used on the GPU
		gpuedges.PrepareForCpuAccess();

		//Verify everything matches
		//Allow +/- 1 timebase unit of mismatch for different CPU-GPU rounding behavior in the interpolation
		REQUIRE(gpulen == edges.size());
		for(size_t i=0; i<edges.size(); i++)
		{
			int64_t delta = edges[i] - gpuedges[i];

			if( (delta > 1) || (delta < -1) )
				LogNotice("mismatch at i=%zu (delta=%" PRIi64 ")\n", i, delta);

			REQUIRE(delta <= 1);
			REQUIRE(delta >= -1);
		}
	}
	*/

	g_scope->GetOscilloscopeChannel(0)->Detach(0);

	for(auto n : nodes)
		n->DetachInputs();
	for(auto n : nodes)
		delete n;
}
