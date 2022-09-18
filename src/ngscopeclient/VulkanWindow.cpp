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
	, m_fullscreen(false)
	, m_windowedX(0)
	, m_windowedY(0)
	, m_windowedWidth(0)
	, m_windowedHeight(0)
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
	vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, numImguiDescriptors, poolSizes);
	m_imguiDescriptorPool = make_unique<vk::raii::DescriptorPool>(*g_vkComputeDevice, poolInfo);

	UpdateFramebuffer();

	//Set up command pool
	vk::CommandPoolCreateInfo cmdPoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		g_renderQueueType );
	m_cmdPool = std::make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, cmdPoolInfo);
	vk::CommandBufferAllocateInfo bufinfo(**m_cmdPool, vk::CommandBufferLevel::ePrimary, m_backBuffers.size());

	//Allocate frame state
	vk::SemaphoreCreateInfo sinfo;
	vk::FenceCreateInfo finfo(vk::FenceCreateFlagBits::eSignaled);
	for(size_t i=0; i<m_backBuffers.size(); i++)
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
	info.PhysicalDevice = **g_vkComputePhysicalDevice;
	info.Device = **g_vkComputeDevice;
	info.QueueFamily = g_renderQueueType;
	info.PipelineCache = **g_pipelineCacheMgr->Lookup("ImGui.spv", IMGUI_VERSION_NUM);
	info.DescriptorPool = **m_imguiDescriptorPool;
	info.Subpass = 0;
	info.MinImageCount = IMAGE_COUNT;
	info.ImageCount = m_backBuffers.size();
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.Queue = *queue;
	ImGui_ImplVulkan_Init(&info, **m_renderPass);

	m_plotContext = ImPlot::CreateContext();
}

/**
	@brief Destroys a VulkanWindow
 */
VulkanWindow::~VulkanWindow()
{
	ImPlot::DestroyContext(m_plotContext);
	m_renderPass = nullptr;
	m_swapchain = nullptr;
	m_surface = nullptr;
	glfwDestroyWindow(m_window);
	m_imguiDescriptorPool = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Updates the framebuffer
 */
bool VulkanWindow::UpdateFramebuffer()
{
	LogTrace("Recreating framebuffer due to window resize\n");

	//Wait until any previous rendering has finished
	g_vkComputeDevice->waitIdle();

	//Get current size of the surface
	//If size doesn't match up, early out. We're probably in the middle of a resize.
	//(This will be corrected next frame, so no worries)
	auto caps = g_vkComputePhysicalDevice->getSurfaceCapabilitiesKHR(**m_surface);
	glfwGetFramebufferSize(m_window, &m_width, &m_height);
	if( (caps.maxImageExtent.width < (unsigned int)m_width) ||
		(caps.maxImageExtent.height < (unsigned int)m_height) ||
		(caps.minImageExtent.width > (unsigned int)m_width) ||
		(caps.minImageExtent.height > (unsigned int)m_height) )
	{
		LogTrace("Size mismatch, retry after everything has caught up\n");
		return false;
	}

	float xscale;
	float yscale;
	glfwGetWindowContentScale(m_window, &xscale, &yscale);
	LogTrace("Scale: %.2f, %.2f\n", xscale, yscale);

	const VkFormat requestSurfaceImageFormat[] =
	{
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM
	};
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	auto format = ImGui_ImplVulkanH_SelectSurfaceFormat(
		**g_vkComputePhysicalDevice,
		**m_surface,
		requestSurfaceImageFormat,
		(size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
		requestSurfaceColorSpace);
	vk::Format surfaceFormat = static_cast<vk::Format>(format.format);

	//Save old swapchain
	unique_ptr<vk::raii::SwapchainKHR> oldSwapchain = move(m_swapchain);

	//Makw the swapchain
	vk::SwapchainKHR oldSwapchainIfValid = {};
	if(oldSwapchain != nullptr)
		oldSwapchainIfValid = **oldSwapchain;
	vk::SwapchainCreateInfoKHR chainInfo(
		{},
		**m_surface,
		IMAGE_COUNT,
		surfaceFormat,
		static_cast<vk::ColorSpaceKHR>(format.colorSpace),
		vk::Extent2D(m_width, m_height),
		1,
		vk::ImageUsageFlagBits::eColorAttachment,
		vk::SharingMode::eExclusive,
		{},
		vk::SurfaceTransformFlagBitsKHR::eIdentity,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::PresentModeKHR::eFifo /*vk::PresentModeKHR::eImmediate*/ , //switch to eImmediate for benchmarking FPS
		true,
		oldSwapchainIfValid);
	m_swapchain = make_unique<vk::raii::SwapchainKHR>(*g_vkComputeDevice, chainInfo);
	oldSwapchain = nullptr;

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
	m_backBuffers = m_swapchain->getImages();
	m_backBufferViews.resize(m_backBuffers.size());
	m_framebuffers.resize(m_backBuffers.size());
	for (uint32_t i = 0; i < m_backBuffers.size(); i++)
	{
		vk::ComponentMapping components(
		vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
		vk::ImageSubresourceRange subrange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		vk::ImageViewCreateInfo vinfo(
			{},
			m_backBuffers[i],
			vk::ImageViewType::e2D,
			surfaceFormat,
			components,
			subrange);
		m_backBufferViews[i] = make_unique<vk::raii::ImageView>(*g_vkComputeDevice, vinfo);

		vk::FramebufferCreateInfo fbinfo({}, **m_renderPass, **m_backBufferViews[i], m_width, m_height, 1);
		m_framebuffers[i] = make_unique<vk::raii::Framebuffer>(*g_vkComputeDevice,fbinfo);
	}

	m_resizeEventPending = false;
	return true;
}

void VulkanWindow::Render()
{
	//If we're re-rendering after the window size changed, fix up the framebuffer before we worry about anything else
	if(m_resizeEventPending)
	{
		//If resize fails, wait a frame and try again. Don't redraw onto the incomplete framebuffer.
		if(!UpdateFramebuffer())
			return;
	}

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
		try
		{
			auto result = m_swapchain->acquireNextImage(UINT64_MAX, **m_imageAcquiredSemaphores[m_semaphoreIndex], {});
			m_frameIndex = result.second;
			if(result.first == vk::Result::eSuboptimalKHR)
			{
				LogTrace("eSuboptimalKHR\n");
				m_resizeEventPending = true;
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				Render();
				return;
			}
		}
		catch(const vk::OutOfDateKHRError& err)
		{
			LogTrace("OutOfDateKHR\n");
			m_resizeEventPending = true;
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
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

	//Handle any additional popup windows created by imgui
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	//Present the main window
	if(!main_is_minimized)
	{
		vk::PresentInfoKHR presentInfo(**m_renderCompleteSemaphores[m_semaphoreIndex], **m_swapchain, m_frameIndex);
		m_semaphoreIndex = (m_semaphoreIndex + 1) % m_backBuffers.size();
		try
		{
			if(vk::Result::eSuboptimalKHR == m_renderQueue.presentKHR(presentInfo))
			{
				LogTrace("eSuboptimal at present\n");
				m_resizeEventPending = true;
				return;
			}
		}
		catch(const vk::OutOfDateKHRError& err)
		{
			LogTrace("OutOfDateKHRError at present\n");
			m_resizeEventPending = true;
			return;
		}
	}
}

void VulkanWindow::RenderUI()
{
}

void VulkanWindow::DoRender(vk::raii::CommandBuffer& /*cmdBuf*/)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window management

void VulkanWindow::SetFullscreen(bool fullscreen)
{
	m_fullscreen = fullscreen;

	if(m_fullscreen)
	{
		LogTrace("Entering fullscreen mode\n");

		m_windowedWidth = m_width;
		m_windowedHeight = m_height;
		glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);

		//TODO: figure out which monitor we are currently on and fullscreen to it
		//(may not be the primary)
		glfwSetWindowMonitor(m_window, glfwGetPrimaryMonitor(), 0, 0, 3840, 2160, GLFW_DONT_CARE);
	}

	else
	{
		LogTrace("Leaving fullscreen mode\n");
		glfwSetWindowMonitor(
			m_window,
			nullptr,
			m_windowedX,
			m_windowedY,
			m_windowedWidth,
			m_windowedHeight,
			GLFW_DONT_CARE);
	}
}
