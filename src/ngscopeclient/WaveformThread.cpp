/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of WaveformThread
 */
#include "ngscopeclient.h"
#include "pthread_compat.h"
#include "Session.h"
#include "WaveformArea.h"

using namespace std;

Event g_rerenderRequestedEvent;
Event g_rerenderDoneEvent;

Event g_refilterRequestedEvent;
Event g_partialRefilterRequestedEvent;
Event g_refilterDoneEvent;

Event g_waveformReadyEvent;
Event g_waveformProcessedEvent;

///@brief Time spent on the last cycle of waveform rendering shaders
atomic<int64_t> g_lastWaveformRenderTime;

void RenderAllWaveforms(vk::raii::CommandBuffer& cmdbuf, Session* session, shared_ptr<QueueHandle> queue);

/**
	@brief Mutex for controlling access to background Vulkan activity

	Arbitrarily many threads can own this mutex at once, but recreating the swapchain conflicts with any and all uses
 */
std::shared_mutex g_vulkanActivityMutex;

void WaveformThread(Session* session, atomic<bool>* shuttingDown)
{
	pthread_setname_np_compat("WaveformThread");

	LogTrace("Starting\n");

	//Create a queue and command buffer for this thread's accelerated processing
	shared_ptr<QueueHandle> queue(g_vkQueueManager->GetComputeQueue("WaveformThread.queue"));
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(*pool, vk::CommandBufferLevel::ePrimary, 1);
	vk::raii::CommandBuffer cmdbuf(std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

	if(g_hasDebugUtils)
	{
		string prefix = "WaveformThread";
		string poolname = prefix + ".pool";
		string bufname = prefix + ".cmdbuf";

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eCommandPool,
				reinterpret_cast<uint64_t>(static_cast<VkCommandPool>(*pool)),
				poolname.c_str()));

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eCommandBuffer,
				reinterpret_cast<int64_t>(static_cast<VkCommandBuffer>(*cmdbuf)),
				bufname.c_str()));
	}

	while(!*shuttingDown)
	{
		//If re-running the filter graph was requested, do that (and re-render)
		if(g_refilterRequestedEvent.Peek())
		{
			//Clear any partial filter refresh event, if one was present (it's now redundant)
			g_partialRefilterRequestedEvent.Peek();

			LogTrace("WaveformThread: re-running filter graph and re-rendering\n");
			session->RefreshAllFilters();
			RenderAllWaveforms(cmdbuf, session, queue);
			g_refilterDoneEvent.Signal();
			continue;
		}

		if(g_partialRefilterRequestedEvent.Peek())
		{
			LogTrace("WaveformThread: re-running partial filter graph and re-rendering\n");
			if(session->RefreshDirtyFilters())
				RenderAllWaveforms(cmdbuf, session, queue);
			g_refilterDoneEvent.Signal();
			continue;
		}

		//If re-rendering was requested due to a window resize etc, do that.
		if(g_rerenderRequestedEvent.Peek())
		{
			LogTrace("WaveformThread: re-rendering\n");
			RenderAllWaveforms(cmdbuf, session, queue);
			g_rerenderDoneEvent.Signal();
			continue;
		}

		//Wait for data to be available from all scopes
		if(!session->CheckForPendingWaveforms())
		{
			this_thread::sleep_for(chrono::milliseconds(1));
			continue;
		}

		//We've got data. Download it, then run the filter graph
		session->DownloadWaveforms();
		session->RefreshAllFilters();

		//Rerun the heavyweight rendering shaders
		RenderAllWaveforms(cmdbuf, session, queue);

		//Unblock the UI threads, then wait for acknowledgement that it's processed
		g_waveformReadyEvent.Signal();
		g_waveformProcessedEvent.Block();
	}

	LogTrace("Shutting down\n");
}

void RenderAllWaveforms(vk::raii::CommandBuffer& cmdbuf, Session* session, shared_ptr<QueueHandle> queue)
{
	double tstart = GetTime();

	//Must lock mutexes in this order to avoid deadlock
	shared_lock<shared_mutex> lock1(session->GetWaveformDataMutex());
	shared_lock<shared_mutex> lock2(g_vulkanActivityMutex);
	lock_guard<mutex> lock3(session->GetRasterizedWaveformMutex());

	//Keep references to all displayed channels open until the rendering finishes
	//This prevents problems if we close a WaveformArea or remove a channel from it before the shader completes
	vector< shared_ptr<DisplayedChannel> > channels;
	cmdbuf.begin({});
	session->RenderWaveformTextures(cmdbuf, channels);
	cmdbuf.end();
	queue->SubmitAndBlock(cmdbuf);

	g_lastWaveformRenderTime = (GetTime() - tstart) * FS_PER_SECOND;
}
