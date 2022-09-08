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
	@brief Implementation of VulkanWindow
 */
#include "ngscopeclient.h"
#include "VulkanWindow.h"
#include "VulkanFFTPlan.h"

using namespace std;

#define IMAGE_COUNT 2

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Creates a new top level window with the specified title
 */
VulkanWindow::VulkanWindow(const string& title, vk::raii::Queue& queue)
	: m_renderQueue(queue)
	, m_resizeEventPending(false)
	, m_semaphoreIndex(0)
{
	//Don't configure Vulkan or center the mouse
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_FALSE);

	//Create the window
	m_window = glfwCreateWindow(1280, 720, title.c_str(), nullptr, nullptr);
	if(!m_window)
	{
		LogError("Window creation failed\n");
		abort();
	}

	//Create a Vulkan surface for drawing onto
	VkSurfaceKHR surface;
	if(VK_SUCCESS != glfwCreateWindowSurface(**g_vkInstance, m_window, nullptr, &surface))
	{
		LogError("Vulkan surface creation failed\n");
		abort();
	}

	//Encapsulate the generated surface in a C++ object for easier access
	m_surface = make_shared<vk::raii::SurfaceKHR>(*g_vkInstance, surface);
	m_wdata.Surface = **m_surface;

	//Make a descriptor pool for ImGui
	//TODO: tune sizes?
	const int numImguiDescriptors = 1000;
	vector<vk::DescriptorPoolSize> poolSizes;
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eSampler, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, numImguiDescriptors));
	poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, numImguiDescriptors));
	vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSizes);
	m_imguiDescriptorPool = make_unique<vk::raii::DescriptorPool>(*g_vkComputeDevice, poolInfo);

	UpdateFramebuffer();

	//Set up command pool
	vk::CommandPoolCreateInfo cmdPoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		g_renderQueueType );
	m_cmdPool = std::make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, cmdPoolInfo);
	auto count = m_wdata.ImageCount;
	vk::CommandBufferAllocateInfo bufinfo(**m_cmdPool, vk::CommandBufferLevel::ePrimary, count);

	//Allocate frame state
	vk::SemaphoreCreateInfo sinfo;
	vk::FenceCreateInfo finfo(vk::FenceCreateFlagBits::eSignaled);
	for(size_t i=0; i<m_wdata.ImageCount; i++)
	{
		m_imageAcquiredSemaphores.push_back(make_unique<vk::raii::Semaphore>(*g_vkComputeDevice, sinfo));
		m_renderCompleteSemaphores.push_back(make_unique<vk::raii::Semaphore>(*g_vkComputeDevice, sinfo));
		m_fences.push_back(make_unique<vk::raii::Fence>(*g_vkComputeDevice, finfo));
		m_cmdBuffers.push_back(make_unique<vk::raii::CommandBuffer>(
			move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front())));
	}

	//Initialize ImGui
	ImGui_ImplGlfw_InitForVulkan(m_window, true);
	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance = **g_vkInstance;
	info.PhysicalDevice = **g_vkfftPhysicalDevice;
	info.Device = **g_vkComputeDevice;
	info.QueueFamily = g_renderQueueType;
	info.PipelineCache = **g_pipelineCacheMgr->Lookup("ImGui.spv", IMGUI_VERSION_NUM);
	info.DescriptorPool = **m_imguiDescriptorPool;
	info.Subpass = 0;
	info.MinImageCount = IMAGE_COUNT;
	info.ImageCount = m_wdata.ImageCount;
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.Queue = *queue;
	ImGui_ImplVulkan_Init(&info, m_wdata.RenderPass);
}

/**
	@brief Destroys a VulkanWindow
 */
VulkanWindow::~VulkanWindow()
{
	for (uint32_t i = 0; i < m_wdata.ImageCount; i++)
	{
		auto fd = &m_wdata.Frames[i];
		vkDestroyCommandPool(**g_vkComputeDevice, fd->CommandPool, VK_NULL_HANDLE);
		vkDestroyFramebuffer(**g_vkComputeDevice, fd->Framebuffer, VK_NULL_HANDLE);
	}
	IM_FREE(m_wdata.Frames);
	m_wdata.Frames = nullptr;

	vkDestroyPipeline(**g_vkComputeDevice, m_wdata.Pipeline, VK_NULL_HANDLE);
	vkDestroyRenderPass(**g_vkComputeDevice, m_wdata.RenderPass, VK_NULL_HANDLE);
	vkDestroySwapchainKHR(**g_vkComputeDevice, m_wdata.Swapchain, VK_NULL_HANDLE);

	m_surface = nullptr;
	glfwDestroyWindow(m_window);
	m_imguiDescriptorPool = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

void VulkanWindow::UpdateFramebuffer()
{
	//Figure out how big our framebuffer is
	int width;
	int height;
	glfwGetFramebufferSize(m_window, &width, &height);

	//Wait until any previous rendering has finished
	g_vkComputeDevice->waitIdle();

	const VkFormat requestSurfaceImageFormat[] =
	{
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM
	};
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	m_wdata.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
		**g_vkfftPhysicalDevice,
		m_wdata.Surface,
		requestSurfaceImageFormat,
		(size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
		requestSurfaceColorSpace);

	VkSwapchainKHR old_swapchain = m_wdata.Swapchain;
	m_wdata.Swapchain = VK_NULL_HANDLE;

	// We don't use ImGui_ImplVulkanH_DestroyWindow() because we want to preserve the old swapchain to create the new one.
	// Destroy old Framebuffer
	for (uint32_t i = 0; i < m_wdata.ImageCount; i++)
	{
		auto fd = &m_wdata.Frames[i];
		vkDestroyFramebuffer(**g_vkComputeDevice, fd->Framebuffer, VK_NULL_HANDLE);
	}
	IM_FREE(m_wdata.Frames);
	m_wdata.Frames = nullptr;
	m_wdata.ImageCount = 0;
	if (m_wdata.RenderPass)
		vkDestroyRenderPass(**g_vkComputeDevice, m_wdata.RenderPass, VK_NULL_HANDLE);
	if (m_wdata.Pipeline)
		vkDestroyPipeline(**g_vkComputeDevice, m_wdata.Pipeline, VK_NULL_HANDLE);

	// Create Swapchain
	{
		VkSwapchainCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = m_wdata.Surface;
		info.minImageCount = IMAGE_COUNT;
		info.imageFormat = m_wdata.SurfaceFormat.format;
		info.imageColorSpace = m_wdata.SurfaceFormat.colorSpace;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
		info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		info.clipped = VK_TRUE;
		info.oldSwapchain = old_swapchain;
		VkSurfaceCapabilitiesKHR cap;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(**g_vkfftPhysicalDevice, m_wdata.Surface, &cap);
		if (info.minImageCount < cap.minImageCount)
			info.minImageCount = cap.minImageCount;
		else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
			info.minImageCount = cap.maxImageCount;

		if (cap.currentExtent.width == 0xffffffff)
		{
			info.imageExtent.width = m_wdata.Width = width;
			info.imageExtent.height = m_wdata.Height = height;
		}
		else
		{
			info.imageExtent.width = m_wdata.Width = cap.currentExtent.width;
			info.imageExtent.height = m_wdata.Height = cap.currentExtent.height;
		}
		vkCreateSwapchainKHR(**g_vkComputeDevice, &info, VK_NULL_HANDLE, &m_wdata.Swapchain);
		vkGetSwapchainImagesKHR(**g_vkComputeDevice, m_wdata.Swapchain, &m_wdata.ImageCount, NULL);
		VkImage backbuffers[16] = {};
		IM_ASSERT(m_wdata.ImageCount >= IMAGE_COUNT);
		IM_ASSERT(m_wdata.ImageCount < IM_ARRAYSIZE(backbuffers));
		vkGetSwapchainImagesKHR(**g_vkComputeDevice, m_wdata.Swapchain, &m_wdata.ImageCount, backbuffers);

		IM_ASSERT(m_wdata.Frames == NULL);
		m_wdata.Frames = (ImGui_ImplVulkanH_Frame*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_Frame) * m_wdata.ImageCount);
		memset(m_wdata.Frames, 0, sizeof(m_wdata.Frames[0]) * m_wdata.ImageCount);
		for (uint32_t i = 0; i < m_wdata.ImageCount; i++)
			m_wdata.Frames[i].Backbuffer = backbuffers[i];
	}
	if (old_swapchain)
		vkDestroySwapchainKHR(**g_vkComputeDevice, old_swapchain, VK_NULL_HANDLE);

	// Create the Render Pass
	{
		VkAttachmentDescription attachment = {};
		attachment.format = m_wdata.SurfaceFormat.format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = m_wdata.ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference color_attachment = {};
		color_attachment.attachment = 0;
		color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment;
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		VkRenderPassCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &dependency;
		vkCreateRenderPass(**g_vkComputeDevice, &info, VK_NULL_HANDLE, &m_wdata.RenderPass);
	}

	//Make back buffer views
	vk::ComponentMapping components(
		vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
	vk::ImageSubresourceRange subrange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	vk::Format surfaceFormat = static_cast<vk::Format>(m_wdata.SurfaceFormat.format);
	vk::ImageViewCreateInfo vinfo(
		{},
		{},
		vk::ImageViewType::e2D,
		surfaceFormat,
		components,
		subrange
	);
	m_backBufferViews.resize(m_wdata.ImageCount);
	for (uint32_t i = 0; i < m_wdata.ImageCount; i++)
	{
		vinfo.image = m_wdata.Frames[i].Backbuffer;
		m_backBufferViews[i] = std::make_unique<vk::raii::ImageView>(*g_vkComputeDevice, vinfo);
	}

	// Create Framebuffer
	{
		VkImageView attachment[1];
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = m_wdata.RenderPass;
		info.attachmentCount = 1;
		info.pAttachments = attachment;
		info.width = m_wdata.Width;
		info.height = m_wdata.Height;
		info.layers = 1;
		for (uint32_t i = 0; i < m_wdata.ImageCount; i++)
		{
			ImGui_ImplVulkanH_Frame* fd = &m_wdata.Frames[i];
			attachment[0] = **m_backBufferViews[i];
			vkCreateFramebuffer(**g_vkComputeDevice, &info, VK_NULL_HANDLE, &fd->Framebuffer);
		}
	}

	m_resizeEventPending = false;
}

void VulkanWindow::Render()
{
	if(m_resizeEventPending)
		UpdateFramebuffer();

	//Start frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	//Draw all of our application UI objects
	RenderUI();

	//Internal GUI rendering
	ImGui::Render();

	//Render the main window
	ImDrawData* main_draw_data = ImGui::GetDrawData();
	const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
	if(!main_is_minimized)
	{
		//Get the next frame to draw onto
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
			Render();
			return;
		}

		//Make sure the old frame has completed
		g_vkComputeDevice->waitForFences({**m_fences[m_wdata.FrameIndex]}, VK_TRUE, UINT64_MAX);
		g_vkComputeDevice->resetFences({**m_fences[m_wdata.FrameIndex]});

		auto& cmdBuf = *m_cmdBuffers[m_wdata.FrameIndex];

		ImGui_ImplVulkanH_Frame* fd = &m_wdata.Frames[m_wdata.FrameIndex];

		cmdBuf.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

		{
			VkRenderPassBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = m_wdata.RenderPass;
			info.framebuffer = fd->Framebuffer;
			info.renderArea.extent.width = m_wdata.Width;
			info.renderArea.extent.height = m_wdata.Height;
			info.clearValueCount = 1;
			info.pClearValues = &m_wdata.ClearValue;
			vkCmdBeginRenderPass(*cmdBuf, &info, VK_SUBPASS_CONTENTS_INLINE);
		}

		// Record dear imgui primitives into command buffer
		ImGui_ImplVulkan_RenderDrawData(main_draw_data, *cmdBuf);

		//Draw anything else we might want to draw
		DoRender(cmdBuf);

		// Submit command buffer
		vkCmdEndRenderPass(*cmdBuf);
		cmdBuf.end();

		vk::PipelineStageFlags flags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::SubmitInfo info(
			**m_imageAcquiredSemaphores[m_semaphoreIndex],
			flags,
			*cmdBuf,
			**m_renderCompleteSemaphores[m_semaphoreIndex]);
		m_renderQueue.submit(info, **m_fences[m_wdata.FrameIndex]);
	}

	// Update and Render additional Platform Windows
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	// Present Main Platform Window
	if (!main_is_minimized)
	{
		VkSemaphore render_complete_semaphore = **m_renderCompleteSemaphores[m_semaphoreIndex];
		VkPresentInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &render_complete_semaphore;
		info.swapchainCount = 1;
		info.pSwapchains = &m_wdata.Swapchain;
		info.pImageIndices = &m_wdata.FrameIndex;
		VkResult err = vkQueuePresentKHR(*m_renderQueue, &info);
		if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		{
			m_resizeEventPending = true;
			Render();
			return;
		}
		m_semaphoreIndex = (m_semaphoreIndex+ 1) % IMAGE_COUNT;
	}

	//Handle resize events
	if(m_resizeEventPending)
		Render();
}

void VulkanWindow::RenderUI()
{
}

void VulkanWindow::DoRender(vk::raii::CommandBuffer& /*cmdBuf*/)
{
}
