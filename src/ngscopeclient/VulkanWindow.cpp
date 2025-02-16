/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg                                                                          *
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
#include "TextureManager.h"
#include "VulkanWindow.h"
#include "VulkanFFTPlan.h"

using namespace std;

#define PREFERRED_IMAGE_COUNT 2

static void Mutexed_ImGui_ImplVulkan_CreateWindow(ImGuiViewport* viewport);
static void Mutexed_ImGui_ImplVulkan_DestroyWindow(ImGuiViewport* viewport);
static void Mutexed_ImGui_ImplVulkan_SetWindowSize(ImGuiViewport* viewport, ImVec2 size);

//original function pointers
void (*ImGui_ImplVulkan_CreateWindow)(ImGuiViewport* viewport);
void (*ImGui_ImplVulkan_DestroyWindow)(ImGuiViewport* viewport);
void (*ImGui_ImplVulkan_SetWindowSize)(ImGuiViewport* viewport, ImVec2 size);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Creates a new top level window with the specified title
 */
VulkanWindow::VulkanWindow(const string& title, shared_ptr<QueueHandle> queue)
	: m_renderQueue(queue)
	, m_resizeEventPending(false)
	, m_softwareResizeRequested(false)
	, m_pendingWidth(0)
	, m_pendingHeight(0)
	, m_semaphoreIndex(0)
	, m_frameIndex(0)
	, m_lastFrameIndex(0)
	, m_width(0)
	, m_height(0)
	, m_fullscreen(false)
	, m_windowedX(0)
	, m_windowedY(0)
	, m_windowedWidth(0)
	, m_windowedHeight(0)
{
	//Initialize ImGui
	IMGUI_CHECKVERSION();
	LogDebug("Using ImGui version %s\n", IMGUI_VERSION);
	m_context = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	//Don't serialize UI config for now
	//TODO: serialize to scopesession or something? https://github.com/ocornut/imgui/issues/4294
	io.IniFilename = nullptr;

	//Set up appearance settings
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.Colors[ImGuiCol_WindowBg].w = 1.0f;

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
	vk::DescriptorPoolCreateInfo poolInfo(
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		numImguiDescriptors,
		poolSizes);
	m_imguiDescriptorPool = make_shared<vk::raii::DescriptorPool>(*g_vkComputeDevice, poolInfo);

	UpdateFramebuffer();

	//Set up command pool
	vk::CommandPoolCreateInfo cmdPoolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
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
			std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front())));
	}

	//Initialize ImGui
	ImGui_ImplGlfw_InitForVulkan(m_window, true);
	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance = **g_vkInstance;
	info.PhysicalDevice = **g_vkComputePhysicalDevice;
	info.Device = **g_vkComputeDevice;
	info.QueueFamily = queue->m_family;
	info.PipelineCache = **g_pipelineCacheMgr->Lookup("ImGui.spv", IMGUI_VERSION_NUM);
	info.DescriptorPool = **m_imguiDescriptorPool;
	info.Subpass = 0;
	info.MinImageCount = m_minImageCount;
	info.ImageCount = m_backBuffers.size();
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.RenderPass = **m_renderPass;

	//HERE BE DRAGONS:
	// We're handing imgui a VkQueue here without holding the lock.
	// This is only safe as long as we hold the QueueLock during any imgui rendering!!
	{
		QueueLock lock(m_renderQueue);
		info.Queue = **lock;
		ImGui_ImplVulkan_Init(&info);
	}

	float ui_scale = GetUIScale(), font_scale = GetFontScale();
	LogTrace("UI scale: %.2f\n", ui_scale);
	LogTrace("Text density: %.2f dpi = 96 dpi × %.2f (UI scale) × %.2f (font scale)\n", 96.0 * ui_scale * font_scale, ui_scale, font_scale);

	ImGui::GetStyle().ScaleAllSizes(ui_scale);

	//Hook a couple of backend functions with mutexing
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	ImGui_ImplVulkan_CreateWindow = platform_io.Renderer_CreateWindow;
	ImGui_ImplVulkan_DestroyWindow = platform_io.Renderer_DestroyWindow;
	ImGui_ImplVulkan_SetWindowSize = platform_io.Renderer_SetWindowSize;
	platform_io.Renderer_CreateWindow = Mutexed_ImGui_ImplVulkan_CreateWindow;
	platform_io.Renderer_DestroyWindow = Mutexed_ImGui_ImplVulkan_DestroyWindow;
	platform_io.Renderer_SetWindowSize = Mutexed_ImGui_ImplVulkan_SetWindowSize;

	//Name a bunch of objects
	if(g_hasDebugUtils)
	{
		string prefix = "VulkanWindow.";
		string poolName = prefix + "imguiDescriptorPool";
		string surfName = prefix + "renderSurface";
		string rpName = prefix + "renderCommandPool";

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eDescriptorPool,
				reinterpret_cast<uint64_t>(static_cast<VkDescriptorPool>(**m_imguiDescriptorPool)),
				poolName.c_str()));

		//Workaround for Mesa bug, see https://gitlab.freedesktop.org/mesa/mesa/-/issues/8596
		if(g_vulkanDeviceIsAnyMesa)
		{
			LogDebug("Vulkan driver is Mesa.\n");
			LogDebug("Disabling vkSetDebugUtilsObjectNameEXT on VkSurfaceKHR objects to work around driver bug.\n");
		}
		else
		{
			g_vkComputeDevice->setDebugUtilsObjectNameEXT(
				vk::DebugUtilsObjectNameInfoEXT(
					vk::ObjectType::eSurfaceKHR,
					reinterpret_cast<uint64_t>(static_cast<VkSurfaceKHR>(**m_surface)),
					surfName.c_str()));
		}

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eCommandPool,
				reinterpret_cast<uint64_t>(static_cast<VkCommandPool>(**m_cmdPool)),
				rpName.c_str()));

		for(size_t i=0; i<m_backBuffers.size(); i++)
		{
			string iaName = prefix + "imageAcquired[" + to_string(i) + "]";
			string rcName = prefix + "renderComplete[" + to_string(i) + "]";
			string fName = prefix + "fence[" + to_string(i) + "]";
			string cbName = prefix + "cmdBuf[" + to_string(i) + "]";

			g_vkComputeDevice->setDebugUtilsObjectNameEXT(
				vk::DebugUtilsObjectNameInfoEXT(
					vk::ObjectType::eSemaphore,
					reinterpret_cast<uint64_t>(static_cast<VkSemaphore>(**m_imageAcquiredSemaphores[i])),
					iaName.c_str()));

			g_vkComputeDevice->setDebugUtilsObjectNameEXT(
				vk::DebugUtilsObjectNameInfoEXT(
					vk::ObjectType::eSemaphore,
					reinterpret_cast<uint64_t>(static_cast<VkSemaphore>(**m_renderCompleteSemaphores[i])),
					rcName.c_str()));

			g_vkComputeDevice->setDebugUtilsObjectNameEXT(
				vk::DebugUtilsObjectNameInfoEXT(
					vk::ObjectType::eFence,
					reinterpret_cast<uint64_t>(static_cast<VkFence>(**m_fences[i])),
					fName.c_str()));

			g_vkComputeDevice->setDebugUtilsObjectNameEXT(
				vk::DebugUtilsObjectNameInfoEXT(
					vk::ObjectType::eCommandBuffer,
					reinterpret_cast<uint64_t>(static_cast<VkCommandBuffer>(**m_cmdBuffers[i])),
					cbName.c_str()));
		}
	}
}

/**
	@brief Destroys a VulkanWindow
 */
VulkanWindow::~VulkanWindow()
{
	LogTrace("Shutting down Vulkan\n");

	g_vkComputeDevice->waitIdle();

	m_texturesUsedThisFrame.clear();

	m_renderPass = nullptr;
	m_swapchain = nullptr;
	m_surface = nullptr;
	glfwDestroyWindow(m_window);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext(m_context);

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
	lock_guard<shared_mutex> lock(g_vulkanActivityMutex);

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

	m_minImageCount = caps.minImageCount;
	int imageCount;
	if(caps.minImageCount > PREFERRED_IMAGE_COUNT)
	{
		imageCount = caps.minImageCount;
	}
	else if((caps.maxImageCount != 0) && (caps.maxImageCount  < PREFERRED_IMAGE_COUNT))
	{
		imageCount = caps.maxImageCount;
	}
	else
	{
		imageCount = PREFERRED_IMAGE_COUNT;
	}

	const VkFormat requestSurfaceImageFormat[] =
	{
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM
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
	unique_ptr<vk::raii::SwapchainKHR> oldSwapchain = std::move(m_swapchain);

	//Make the swapchain
	vk::SwapchainKHR oldSwapchainIfValid = {};
	if(oldSwapchain != nullptr)
		oldSwapchainIfValid = **oldSwapchain;
	vk::SwapchainCreateInfoKHR chainInfo(
		{},
		**m_surface,
		imageCount,
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
	auto nbuffers = m_backBuffers.size();
	m_backBufferViews.resize(nbuffers);
	m_framebuffers.resize(nbuffers);
	m_texturesUsedThisFrame.resize(nbuffers);
	for (uint32_t i = 0; i < nbuffers; i++)
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

float VulkanWindow::GetUIScale()
{
	if (const char *scale_override = getenv("NGSCOPECLIENT_UI_SCALE"))
		return atof(scale_override);
#if defined(WIN32)
	// FIXME
#elif defined(__APPLE__)
	// FIXME
#else
	// KDE also sets these variables.
	if (const char *gdk_scale = getenv("GDK_SCALE"))
		return atoi(gdk_scale);
#endif

	return 1.0;
}

float VulkanWindow::GetFontScale()
{
	if (const char *scale_override = getenv("NGSCOPECLIENT_FONT_SCALE"))
		return atof(scale_override);
#if defined(WIN32)
	// FIXME
#elif defined(__APPLE__)
	// FIXME
#else
	// KDE also sets these variables.
	if (const char *gdk_dpi_scale = getenv("GDK_DPI_SCALE"))
		return atof(gdk_dpi_scale);
#endif
	return 1.0;
}

void VulkanWindow::Render()
{
	if(m_softwareResizeRequested)
	{
		m_softwareResizeRequested = false;
		LogTrace("Software window resize to (%d, %d)\n", m_pendingWidth, m_pendingHeight);

		//can't resize the window during any other vulkan activity
		lock_guard<shared_mutex> lock(g_vulkanActivityMutex);
		g_vkComputeDevice->waitIdle();
		glfwSetWindowSize(m_window, m_pendingWidth, m_pendingHeight);
		return;
	}

	//If we're re-rendering after the window size changed, fix up the framebuffer before we worry about anything else
	if(m_resizeEventPending)
	{
		//If resize fails, wait a frame and try again. Don't redraw onto the incomplete framebuffer.
		if(!UpdateFramebuffer())
			return;
	}

	//Start frame
	{
		QueueLock qlock(m_renderQueue);
		ImGui_ImplVulkan_NewFrame();
	}
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	//Make sure the old frame has completed
	//Otherwise we risk modifying textures that last frame is still using
	(void)g_vkComputeDevice->waitForFences({**m_fences[m_frameIndex]}, VK_TRUE, UINT64_MAX);

	//Draw all of our application UI objects
	RenderUI();

	//Internal GUI rendering
	set<shared_ptr<Texture> > texturesToClear = m_texturesUsedThisFrame[m_lastFrameIndex];
	m_texturesUsedThisFrame[m_lastFrameIndex].clear();
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
			m_lastFrameIndex = m_frameIndex;
			m_frameIndex = result.second;
			if(result.first == vk::Result::eSuboptimalKHR)
			{
				// eSuboptimalKHR is actually a success code, meaning that although the image is suboptimal,
				// we *did* acquire the next image from the swapchain. Proceed to render the suboptimal frame
				// to avoid Vulkan validation error VUID-vkAcquireNextImageKHR-semaphore-01286.
				LogTrace("eSuboptimalKHR\n");
			}
		}
		catch(const vk::OutOfDateKHRError& err)
		{
			LogTrace("OutOfDateKHR\n");

			m_resizeEventPending = true;
			ImGui::UpdatePlatformWindows();
			{
				QueueLock qlock(m_renderQueue);
				ImGui::RenderPlatformWindowsDefault();
			}
			Render();

			return;
		}

		//Reset fences for next frame
		g_vkComputeDevice->resetFences({**m_fences[m_frameIndex]});
		(*QueueLock(m_renderQueue)).waitIdle();

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
		QueueLock qlock(m_renderQueue);
		(*qlock).submit(info, **m_fences[m_frameIndex]);
	}

	// if (!m_resizeEventPending)
	{
		//Handle any additional popup windows created by imgui
		ImGui::UpdatePlatformWindows();
		{
			QueueLock qlock(m_renderQueue);
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	//Present the main window
	if(!main_is_minimized)
	{
		vk::PresentInfoKHR presentInfo(**m_renderCompleteSemaphores[m_semaphoreIndex], **m_swapchain, m_frameIndex);
		m_semaphoreIndex = (m_semaphoreIndex + 1) % m_backBuffers.size();
		try
		{
			QueueLock qlock(m_renderQueue);
			(*qlock).waitIdle();
			if(vk::Result::eSuboptimalKHR == (*qlock).presentKHR(presentInfo))
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

	//We can now free references to last frame's textures
	//This will delete them if the containing object was destroyed that frame
	texturesToClear.clear();
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
		LogIndenter li;

		m_windowedWidth = m_width;
		m_windowedHeight = m_height;
		glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
		LogTrace("Our window is at (%d, %d)\n", m_windowedX, m_windowedY);

		//Find the centroid of our window
		int centerX = m_windowedX + m_width/2;
		int centerY = m_windowedY + m_height/2;

		//Which monitor are we on?
		int count;
		auto monitors = glfwGetMonitors(&count);
		for(int i=0; i<count; i++)
		{
			int xpos, ypos;
			glfwGetMonitorPos(monitors[i], &xpos, &ypos);
			auto mode = glfwGetVideoMode(monitors[i]);
			LogTrace("Monitor %d is at (%d, %d), (%d x %d)\n", i, xpos, ypos, mode->width, mode->height);
			LogIndenter li2;

			if( (centerX >= xpos) && (centerY >= ypos) &&
				(centerX < (xpos + mode->width)) && (centerY < (ypos + mode->height)) )
			{
				LogTrace("We are on this monitor\n");
				glfwSetWindowMonitor(m_window, monitors[i], 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
				break;
			}
		}
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ImGui hooks

static void Mutexed_ImGui_ImplVulkan_CreateWindow(ImGuiViewport* viewport)
{
	lock_guard<shared_mutex> lock(g_vulkanActivityMutex);
	g_vkComputeDevice->waitIdle();
	ImGui_ImplVulkan_CreateWindow(viewport);
}

static void Mutexed_ImGui_ImplVulkan_DestroyWindow(ImGuiViewport* viewport)
{
	lock_guard<shared_mutex> lock(g_vulkanActivityMutex);
	g_vkComputeDevice->waitIdle();
	ImGui_ImplVulkan_DestroyWindow(viewport);
}

static void Mutexed_ImGui_ImplVulkan_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	lock_guard<shared_mutex> lock(g_vulkanActivityMutex);
	g_vkComputeDevice->waitIdle();
	ImGui_ImplVulkan_SetWindowSize(viewport, size);
}
