/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of VulkanWindow
 */
#ifndef VulkanWindow_h
#define VulkanWindow_h

class Texture;

/**
	@brief A GLFW window containing a Vulkan surface
 */
class VulkanWindow
{
public:
	VulkanWindow(const std::string& title, std::shared_ptr<QueueHandle> queue);
	virtual ~VulkanWindow();

	GLFWwindow* GetWindow()
	{ return m_window; }

	///@brief Scale factor for UI elements (icons, scrollbars, etc); typically an integer number.
	float GetUIScale();

	///@brief Scale factor for fonts, applied on top of UI scale factor; often a fractional number.
	float GetFontScale();

	virtual void Render();

	std::shared_ptr<QueueHandle> GetRenderQueue()
	{ return m_renderQueue; }

	void AddTextureUsedThisFrame(std::shared_ptr<Texture> tex)
	{ m_texturesUsedThisFrame[m_frameIndex].emplace(tex); }

	bool IsFullscreen()
	{ return m_fullscreen; }

protected:
	bool UpdateFramebuffer();
	void SetFullscreen(bool fullscreen);

	virtual void DoRender(vk::raii::CommandBuffer& cmdBuf);
	virtual void RenderUI();

	///@brief The underlying GLFW window object
	GLFWwindow* m_window;

	///@brief ImGui context for GUI objects
	ImGuiContext* m_context;

	///@brief Surface for drawing onto
	std::shared_ptr<vk::raii::SurfaceKHR> m_surface;

	///@brief Descriptor pool for ImGui
	std::shared_ptr<vk::raii::DescriptorPool> m_imguiDescriptorPool;

	///@brief Queue for rendering to
	std::shared_ptr<QueueHandle> m_renderQueue;

	///@brief Set true if we have to handle a resize event
	bool m_resizeEventPending;

	///@brief Set true if a resize was requested by software (i.e. we need to resize to m_pendingWidth / m_pendingHeight)
	bool m_softwareResizeRequested;

	///@brief Requested width for software resize
	int m_pendingWidth;

	///@brief Requested height for software resize
	int m_pendingHeight;

	///@brief Frame command pool
	std::unique_ptr<vk::raii::CommandPool> m_cmdPool;

	///@brief Frame command buffers
	std::vector<std::unique_ptr<vk::raii::CommandBuffer> > m_cmdBuffers;

	///@brief Semaphore indicating framebuffer is ready
	std::vector<std::unique_ptr<vk::raii::Semaphore> > m_imageAcquiredSemaphores;

	///@brief Semaphore indicating frame is complete
	std::vector<std::unique_ptr<vk::raii::Semaphore> > m_renderCompleteSemaphores;

	///@brief Frame semaphore number for double buffering
	uint32_t m_semaphoreIndex;

	///@brief Frame number for double buffering
	uint32_t m_frameIndex;

	///@brief Frame number for double buffering
	uint32_t m_lastFrameIndex;

	///@brief Frame fences
	std::vector<std::unique_ptr<vk::raii::Fence> > m_fences;

	///@brief Back buffer view
	std::vector<std::unique_ptr<vk::raii::ImageView> > m_backBufferViews;

	///@brief Framebuffer
	std::vector<std::unique_ptr<vk::raii::Framebuffer> > m_framebuffers;

	///@brief Render pass for drawing everything
	std::unique_ptr<vk::raii::RenderPass> m_renderPass;

	///@brief Swapchain for presenting to the screen
	std::unique_ptr<vk::raii::SwapchainKHR> m_swapchain;

	/**
		@brief Back buffer images

		Note that the return value of SwapchainKHR::getImages() changed in the 1.3.229 Vulkan SDK
		See https://github.com/KhronosGroup/Vulkan-Hpp/issues/1417
	 */
	#if VK_HEADER_VERSION >= 229
		std::vector<vk::Image> m_backBuffers;
	#else
		std::vector<VkImage> m_backBuffers;
	#endif

	///@brief The minimum image count for the backbuffer allowed by this GPU
	int m_minImageCount;

	///@brief Current window width
	int m_width;

	///@brief Current window height
	int m_height;

	///@brief Fullscreen flag
	bool m_fullscreen;

	///@brief Saved position before we went fullscreen
	int m_windowedX;

	///@brief Saved position before we went fullscreen
	int m_windowedY;

	///@brief Saved size before we went fullscreen
	int m_windowedWidth;

	///@brief Saved size before we went fullscreen
	int m_windowedHeight;

	///@brief Textures used this frame
	std::vector< std::set<std::shared_ptr<Texture> > > m_texturesUsedThisFrame;

};

#endif
