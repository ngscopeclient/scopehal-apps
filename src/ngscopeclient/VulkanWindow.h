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
	@brief Declaration of VulkanWindow
 */
#ifndef VulkanWindow_h
#define VulkanWindow_h

/**
	@brief A GLFW window containing a Vulkan surface
 */
class VulkanWindow
{
public:
	VulkanWindow(const std::string& title, vk::raii::Queue& queue);
	virtual ~VulkanWindow();

	GLFWwindow* GetWindow()
	{ return m_window; }

	virtual void Render();

protected:
	void UpdateFramebuffer();
	virtual void DoRender();

	///@brief The underlying GLFW window object
	GLFWwindow* m_window;

	///@brief Surface for drawing onto
	std::shared_ptr<vk::raii::SurfaceKHR> m_surface;

	///@brief Descriptor pool for ImGui
	std::unique_ptr<vk::raii::DescriptorPool> m_imguiDescriptorPool;

	///@brief Queue for rendering to
	vk::raii::Queue& m_renderQueue;

	///@brief ImGui window data
	ImGui_ImplVulkanH_Window m_wdata;

	///@brief Set true if we have to handle a resize event
	bool m_resizeEventPending;
};

#endif
