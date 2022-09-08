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
	, m_frameIndex(0)
	, m_width(0)
	, m_height(0)
	, m_imageCount(IMAGE_COUNT)
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
	vk::CommandBufferAllocateInfo bufinfo(**m_cmdPool, vk::CommandBufferLevel::ePrimary, m_imageCount);

	//Allocate frame state
	vk::SemaphoreCreateInfo sinfo;
	vk::FenceCreateInfo finfo(vk::FenceCreateFlagBits::eSignaled);
	for(size_t i=0; i<m_imageCount; i++)
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
	info.ImageCount = m_imageCount;
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.Queue = *queue;
	ImGui_ImplVulkan_Init(&info, **m_renderPass);
}

/**
	@brief Destroys a VulkanWindow
 */
VulkanWindow::~VulkanWindow()
{
	IM_FREE(m_wdata.Frames);
	m_wdata.Frames = nullptr;

	m_renderPass = nullptr;
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
	glfwGetFramebufferSize(m_window, &m_width, &m_height);

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
	auto format = ImGui_ImplVulkanH_SelectSurfaceFormat(
		**g_vkfftPhysicalDevice,
		**m_surface,
		requestSurfaceImageFormat,
		(size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
		requestSurfaceColorSpace);
	vk::Format surfaceFormat = static_cast<vk::Format>(format.format);

	VkSwapchainKHR old_swapchain = m_wdata.Swapchain;
	m_wdata.Swapchain = VK_NULL_HANDLE;

	IM_FREE(m_wdata.Frames);
	m_wdata.Frames = nullptr;
	m_imageCount = 0;

	// Create Swapchain
	{
		VkSwapchainCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = **m_surface;
		info.minImageCount = IMAGE_COUNT;
		info.imageFormat = format.format;
		info.imageColorSpace = format.colorSpace;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
		info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		info.clipped = VK_TRUE;
		info.oldSwapchain = old_swapchain;
		VkSurfaceCapabilitiesKHR cap;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(**g_vkfftPhysicalDevice, **m_surface, &cap);
		if (info.minImageCount < cap.minImageCount)
			info.minImageCount = cap.minImageCount;
		else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
			info.minImageCount = cap.maxImageCount;

		if (cap.currentExtent.width == 0xffffffff)
		{
			info.imageExtent.width = m_width;
			info.imageExtent.height = m_height;
		}
		else
		{
			info.imageExtent.width = m_width = cap.currentExtent.width;
			info.imageExtent.height = m_height = cap.currentExtent.height;
		}
		vkCreateSwapchainKHR(**g_vkComputeDevice, &info, VK_NULL_HANDLE, &m_wdata.Swapchain);
		vkGetSwapchainImagesKHR(**g_vkComputeDevice, m_wdata.Swapchain, &m_imageCount, NULL);
		VkImage backbuffers[16] = {};
		IM_ASSERT(m_imageCount >= IMAGE_COUNT);
		IM_ASSERT(m_imageCount < IM_ARRAYSIZE(backbuffers));
		vkGetSwapchainImagesKHR(**g_vkComputeDevice, m_wdata.Swapchain, &m_imageCount, backbuffers);

		IM_ASSERT(m_wdata.Frames == NULL);
		m_wdata.Frames = (ImGui_ImplVulkanH_Frame*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_Frame) * m_imageCount);
		memset(m_wdata.Frames, 0, sizeof(m_wdata.Frames[0]) * m_imageCount);
		for (uint32_t i = 0; i < m_imageCount; i++)
			m_wdata.Frames[i].Backbuffer = backbuffers[i];
	}
	if (old_swapchain)
		vkDestroySwapchainKHR(**g_vkComputeDevice, old_swapchain, VK_NULL_HANDLE);

	//Make render pass
	vk::AttachmentDescription attachment(
		{},
		surfaceFormat,
		vk::SampleCountFlagBits::e1,
		vk::AttachmentLoadOp::eClear,
		vk::AttachmentStoreOp::eStore,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR);

	vk::AttachmentReference colorAttachment({}, vk::ImageLayout::eColorAttachmentOptimal);
	vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachment);
	vk::SubpassDependency subpassDep(
		VK_SUBPASS_EXTERNAL,
		0,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		{},
		vk::AccessFlagBits::eColorAttachmentWrite,
		{});
	vk::RenderPassCreateInfo passInfo({}, attachment, subpass, subpassDep);
	m_renderPass = make_unique<vk::raii::RenderPass>(*g_vkComputeDevice, passInfo);

	//Make per-frame buffer views and framebuffers
	m_backBufferViews.resize(m_imageCount);
	m_framebuffers.resize(m_imageCount);
	for (uint32_t i = 0; i < m_imageCount; i++)
	{
		vk::ComponentMapping components(
		vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
		vk::ImageSubresourceRange subrange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		vk::ImageViewCreateInfo vinfo(
			{},
			m_wdata.Frames[i].Backbuffer,
			vk::ImageViewType::e2D,
			surfaceFormat,
			components,
			subrange);
		m_backBufferViews[i] = make_unique<vk::raii::ImageView>(*g_vkComputeDevice, vinfo);

		vk::FramebufferCreateInfo fbinfo({}, **m_renderPass, **m_backBufferViews[i], m_width, m_height, 1);
		m_framebuffers[i] = make_unique<vk::raii::Framebuffer>(*g_vkComputeDevice,fbinfo);
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
			&m_frameIndex);
		if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		{
			m_resizeEventPending = true;
			Render();
			return;
		}

		//Make sure the old frame has completed
		g_vkComputeDevice->waitForFences({**m_fences[m_frameIndex]}, VK_TRUE, UINT64_MAX);
		g_vkComputeDevice->resetFences({**m_fences[m_frameIndex]});

		//Start render pass
		auto& cmdBuf = *m_cmdBuffers[m_frameIndex];
		cmdBuf.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		vk::ClearValue clearValue;
		vk::ClearColorValue clearColor;
		clearColor.setFloat32({0.1f, 0.1f, 0.1f, 1.0f});
		clearValue.setColor(clearColor);
		vk::RenderPassBeginInfo passInfo(
			**m_renderPass,
			**m_framebuffers[m_frameIndex],
			vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_width, m_height)),
			clearValue);
		cmdBuf.beginRenderPass(passInfo, vk::SubpassContents::eInline);

		//Draw GUI
		ImGui_ImplVulkan_RenderDrawData(main_draw_data, *cmdBuf);

		//Draw waveform data etc
		DoRender(cmdBuf);

		//Finish up and submit
		cmdBuf.endRenderPass();
		cmdBuf.end();

		vk::PipelineStageFlags flags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::SubmitInfo info(
			**m_imageAcquiredSemaphores[m_semaphoreIndex],
			flags,
			*cmdBuf,
			**m_renderCompleteSemaphores[m_semaphoreIndex]);
		m_renderQueue.submit(info, **m_fences[m_frameIndex]);
	}

	// Update and Render additional Platform Windows
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	// Present Main Platform Window
	if (!main_is_minimized)
	{
		vk::SwapchainKHR tempChain(m_wdata.Swapchain);
		vk::PresentInfoKHR presentInfo(
			**m_renderCompleteSemaphores[m_semaphoreIndex],
			tempChain,
			m_frameIndex);
		auto err = m_renderQueue.presentKHR(presentInfo);
		if (err == vk::Result::eErrorOutOfDateKHR || err == vk::Result::eSuboptimalKHR)
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
