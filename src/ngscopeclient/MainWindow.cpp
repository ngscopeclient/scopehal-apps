/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
	@brief Implementation of MainWindow
 */
#include "ngscopeclient.h"
#include "MainWindow.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

MainWindow::MainWindow(vk::raii::Queue& queue)
	: VulkanWindow("ngscopeclient", queue)
{
}

MainWindow::~MainWindow()
{
}

void MainWindow::DoRender(vk::raii::CommandBuffer& cmdbuf)
{
	ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

	//Do the actual render
	ImGui::Render();
	ImDrawData* main_draw_data = ImGui::GetDrawData();
	const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
	m_wdata.ClearValue.color.float32[0] = clear_color.x * clear_color.w;
	m_wdata.ClearValue.color.float32[1] = clear_color.y * clear_color.w;
	m_wdata.ClearValue.color.float32[2] = clear_color.z * clear_color.w;
	m_wdata.ClearValue.color.float32[3] = clear_color.w;
	if (!main_is_minimized)
	{
		VkResult err;
		err = vkAcquireNextImageKHR(
			**g_vkComputeDevice,
			m_wdata.Swapchain,
			UINT64_MAX,
			**m_imageAcquiredSemaphores[m_semaphoreIndex],
			VK_NULL_HANDLE,
			&m_wdata.FrameIndex);
		if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		{
			m_resizeEventPending = true;
			return;
		}

		g_vkComputeDevice->waitForFences({**m_fences[m_wdata.FrameIndex]}, VK_TRUE, UINT64_MAX);
		g_vkComputeDevice->resetFences({**m_fences[m_wdata.FrameIndex]});

		ImGui_ImplVulkanH_Frame* fd = &m_wdata.Frames[m_wdata.FrameIndex];
		{
			vkResetCommandPool(**g_vkComputeDevice, fd->CommandPool, 0);
			VkCommandBufferBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(fd->CommandBuffer, &info);
		}
		{
			VkRenderPassBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = m_wdata.RenderPass;
			info.framebuffer = fd->Framebuffer;
			info.renderArea.extent.width = m_wdata.Width;
			info.renderArea.extent.height = m_wdata.Height;
			info.clearValueCount = 1;
			info.pClearValues = &m_wdata.ClearValue;
			vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		}

		// Record dear imgui primitives into command buffer
		ImGui_ImplVulkan_RenderDrawData(main_draw_data, fd->CommandBuffer);

		// Submit command buffer
		vkCmdEndRenderPass(fd->CommandBuffer);
		{
			VkSemaphore sem = **m_renderCompleteSemaphores[m_semaphoreIndex];
			VkSemaphore asem = **m_imageAcquiredSemaphores[m_semaphoreIndex];

			VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.waitSemaphoreCount = 1;
			info.pWaitSemaphores = &asem;
			info.pWaitDstStageMask = &wait_stage;
			info.commandBufferCount = 1;
			info.pCommandBuffers = &fd->CommandBuffer;
			info.signalSemaphoreCount = 1;
			info.pSignalSemaphores = &sem;

			vkEndCommandBuffer(fd->CommandBuffer);
			vkQueueSubmit(*m_renderQueue, 1, &info, **m_fences[m_wdata.FrameIndex]);
		}
	}
}
