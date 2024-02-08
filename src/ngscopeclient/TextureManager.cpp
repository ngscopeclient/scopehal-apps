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
	@brief Implementation of TextureManager
 */

#include "ngscopeclient.h"
#include "TextureManager.h"
#include <png.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Texture

/**
	@brief Creates a texture from an externally supplied staging buffer
 */
Texture::Texture(
	const vk::raii::Device& device,
	const vk::ImageCreateInfo& imageInfo,
	const vk::raii::Buffer& srcBuf,
	int width,
	int height,
	TextureManager* mgr,
	const std::string& name
	)
	: m_image(device, imageInfo)
{
	auto req = m_image.getMemoryRequirements();

	//Figure out memory requirements of the buffer and decide what physical memory type to use
	uint32_t memType = 0;
	auto memProperties = g_vkComputePhysicalDevice->getMemoryProperties();
	for(uint32_t i=0; i<32; i++)
	{
		//Skip anything not device local, since we're optimizing this buffer for performance
		if(!(memProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))
			continue;

		//Stop if buffer is compatible
		if(req.memoryTypeBits & (1 << i) )
		{
			memType = i;
			break;
		}
	}
	LogTrace("Using memory type %u for texture buffer\n", memType);

	//Once the image is created, allocate device memory to back it
	vk::MemoryAllocateInfo info(req.size, memType);
	m_deviceMemory = make_unique<vk::raii::DeviceMemory>(*g_vkComputeDevice, info);
	m_image.bindMemory(**m_deviceMemory, 0);

	//Transfer our image data over from the staging buffer
	{
		vk::raii::CommandBuffer& cmdBuf = mgr->GetCmdBuffer();
		cmdBuf.begin({});

		//Initial image layout transition
		LayoutTransition(
			cmdBuf,
			vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal);

		//Copy the buffer to the image
		vk::ImageSubresourceLayers subresource(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
		vk::BufferImageCopy region(0, 0, 0, subresource, vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1) );
		cmdBuf.copyBufferToImage(*srcBuf, *m_image, vk::ImageLayout::eTransferDstOptimal, region);

		//Convert to something optimal for texture reads
		LayoutTransition(
			cmdBuf,
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		cmdBuf.end();

		//Submit the request and block until it completes
		mgr->GetQueue()->SubmitAndBlock(cmdBuf);
	}

	//Make a view for the image
	vk::ImageViewCreateInfo vinfo(
		{},
		*m_image,
		vk::ImageViewType::e2D,
		vk::Format::eR8G8B8A8Unorm,
		{},
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
	m_view = make_unique<vk::raii::ImageView>(*g_vkComputeDevice, vinfo);

	m_texture = ImGui_ImplVulkan_AddTexture(**mgr->GetSampler(), **m_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	SetName(name);
}

/**
	@brief Creates a blank texture, to be written to by a compute shader in the future
 */
Texture::Texture(
	const vk::raii::Device& device,
	const vk::ImageCreateInfo& imageInfo,
	TextureManager* mgr,
	const string& name)
	: m_image(device, imageInfo)
{
	auto req = m_image.getMemoryRequirements();

	//Figure out memory requirements of the buffer and decide what physical memory type to use
	uint32_t memType = 0;
	auto memProperties = g_vkComputePhysicalDevice->getMemoryProperties();
	for(uint32_t i=0; i<32; i++)
	{
		//Skip anything not device local, since we're optimizing this buffer for performance
		if(!(memProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))
			continue;

		//Stop if buffer is compatible
		if(req.memoryTypeBits & (1 << i) )
		{
			memType = i;
			break;
		}
	}
	LogTrace("Using memory type %u for texture buffer\n", memType);

	//Once the image is created, allocate device memory to back it
	vk::MemoryAllocateInfo info(req.size, memType);
	m_deviceMemory = make_unique<vk::raii::DeviceMemory>(*g_vkComputeDevice, info);
	m_image.bindMemory(**m_deviceMemory, 0);

	//Don't fill anything, we'll be writing in a shader later on when the time is right

	//Make a view for the image
	vk::ImageViewCreateInfo vinfo(
		{},
		*m_image,
		vk::ImageViewType::e2D,
		vk::Format::eR32G32B32A32Sfloat,
		{},
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
	m_view = make_unique<vk::raii::ImageView>(*g_vkComputeDevice, vinfo);

	m_texture = ImGui_ImplVulkan_AddTexture(**mgr->GetSampler(), **m_view, VK_IMAGE_LAYOUT_GENERAL);

	SetName(name);
}

void Texture::SetName(const string& name)
{
	if(g_hasDebugUtils && !name.empty())
	{
		//Not sure why this is being reported as a conflict by validation layers
		//but let's go ahead and lock to be safe for now
		//lock_guard<shared_mutex> lock(g_vulkanActivityMutex);

		string prefix = string("Texture.") + name;
		string texName = prefix + ".dset";
		string viewName = prefix + ".view";
		string imageName = prefix + ".image";
		string memName = prefix + ".mem";

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eDescriptorSet,
				reinterpret_cast<int64_t>(m_texture),
				texName.c_str()));

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eImage,
				reinterpret_cast<int64_t>(static_cast<VkImage>(*m_image)),
				imageName.c_str()));

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eImageView,
				reinterpret_cast<int64_t>(static_cast<VkImageView>(**m_view)),
				viewName.c_str()));

		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eDeviceMemory,
				reinterpret_cast<int64_t>(static_cast<VkDeviceMemory>(**m_deviceMemory)),
				memName.c_str()));
	}
}

void Texture::LayoutTransition(
	vk::raii::CommandBuffer& cmdBuf,
	vk::AccessFlags src,
	vk::AccessFlags dst,
	vk::ImageLayout from,
	vk::ImageLayout to)
{
	vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	vk::ImageMemoryBarrier barrier(
		src,
		dst,
		from,
		to,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		*m_image,
		range);

	if(dst == vk::AccessFlagBits::eShaderRead)
	{
		cmdBuf.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier);
	}
	else
	{
		cmdBuf.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			{},
			{},
			barrier);
	}
}

Texture::~Texture()
{
	ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(m_texture));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TextureManager::TextureManager(shared_ptr<QueueHandle> queue)
	: m_queue(queue)
{
	//Make a sampler using configuration that matches imgui
	vk::SamplerCreateInfo sinfo(
		{},
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		{},
		{},
		1.0,
		{},
		vk::CompareOp::eNever,
		-1000,
		1000
	);
	m_sampler = make_unique<vk::raii::Sampler>(*g_vkComputeDevice, sinfo);

	//Initialize command pool/buffer
	vk::CommandPoolCreateInfo poolInfo(
	vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		queue->m_family );
	m_cmdPool = make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, poolInfo);

	vk::CommandBufferAllocateInfo bufinfo(**m_cmdPool, vk::CommandBufferLevel::ePrimary, 1);
	m_cmdBuf = make_unique<vk::raii::CommandBuffer>(
		std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));
}

TextureManager::~TextureManager()
{
	m_cmdBuf = nullptr;
	m_cmdPool = nullptr;
	m_queue = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File loading

/**
	@brief Loads a texture from a file into a named resource

	If an existing texture by the same name already exists, it is overwritten.
 */
void TextureManager::LoadTexture(
	const string& name,
	const string& path)
{
	LogTrace("Loading texture \"%s\" from file \"%s\"\n", name.c_str(), path.c_str());
	LogIndenter li;

	//Initialize libpng
	auto png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(!png)
	{
		LogError("Failed to create PNG read struct\n");
		return;
	}
	auto info = png_create_info_struct(png);
	if(!info)
	{
		png_destroy_read_struct(&png, nullptr, nullptr);
		LogError("Failed to create PNG info struct\n");
		return;
	}
	auto end = png_create_info_struct(png);
	if(!end)
	{
		png_destroy_read_struct(&png, &info, nullptr);
		LogError("Failed to create PNG end info struct\n");
		return;
	}

	//Prepare to load the file (assume it's a PNG for now)
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
	{
		LogError("Failed to open texture file \"%s\"\n", path.c_str());
		return;
	}
	uint8_t sig[8];
	if(sizeof(sig) != fread(sig, 1, sizeof(sig), fp))
	{
		LogError("Failed to read signature of PNG file \"%s\"\n", path.c_str());
		fclose(fp);
		return;
	}
	if(0 != png_sig_cmp(sig, 0, sizeof(sig)))
	{
		LogError("Bad magic number in PNG file \"%s\"\n", path.c_str());
		fclose(fp);
		return;
	}
	png_init_io(png, fp);
	png_set_sig_bytes(png, sizeof(sig));

	//Read it
	png_read_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);
	auto rowPtrs = png_get_rows(png, info);

	//Figure out the file dimensions
	int width = png_get_image_width(png, info);
	int height = png_get_image_height(png, info);
	int depth = png_get_bit_depth(png, info);
	if(png_get_color_type(png, info) != PNG_COLOR_TYPE_RGBA)
	{
		LogError("Image \"%s\" is not RGBA color type, don't know how to load it\n", path.c_str());
		png_destroy_read_struct(&png, &info, &end);
		fclose(fp);
		return;
	}
	if(depth != 8)
	{
		LogError("Image \"%s\" is not 8 bits per channel, don't know how to load it\n", path.c_str());
		png_destroy_read_struct(&png, &info, &end);
		fclose(fp);
		return;
	}
	int bytesPerComponent = 1;
	int bytesPerPixel = 4*bytesPerComponent;
	LogTrace("Image is %d x %d pixels, RGBA8888\n", width, height);
	VkDeviceSize size = width * height * bytesPerPixel;

	//Allocate temporary staging buffer
	vk::BufferCreateInfo bufinfo({}, size, vk::BufferUsageFlagBits::eTransferSrc);
	vk::raii::Buffer stagingBuf(*g_vkComputeDevice, bufinfo);

	//Figure out memory requirements of the buffer and decide what physical memory type to use
	//For now, default to using the first type in the mask that is host visible
	auto req = stagingBuf.getMemoryRequirements();
	auto memProperties = g_vkComputePhysicalDevice->getMemoryProperties();
	uint32_t memType = 0;
	for(uint32_t i=0; i<32; i++)
	{
		//Skip anything not host visible since we have to be able to write to it
		if(!(memProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
			continue;

		//Stop if buffer is compatible
		if(req.memoryTypeBits & (1 << i) )
		{
			memType = i;
			break;
		}
	}
	LogTrace("Using memory type %u for staging buffer\n", memType);

	//Allocate the memory and bind to the buffer
	vk::MemoryAllocateInfo minfo(req.size, memType);
	vk::raii::DeviceMemory physMem(*g_vkComputeDevice, minfo);
	auto mappedPtr = reinterpret_cast<uint8_t*>(physMem.mapMemory(0, req.size));
	stagingBuf.bindMemory(*physMem, 0);

	//Fill the mapped buffer with image data from the PNG
	size_t rowSize = width * bytesPerPixel;
	for(int y=0; y<height; y++)
		memcpy(mappedPtr + (y*rowSize), rowPtrs[y], rowSize);
	physMem.unmapMemory();

	//Make the texture object
	vk::ImageCreateInfo imageInfo(
		{},
		vk::ImageType::e2D,
		vk::Format::eR8G8B8A8Unorm,
		vk::Extent3D(width, height, 1),
		1,
		1,
		VULKAN_HPP_NAMESPACE::SampleCountFlagBits::e1,
		VULKAN_HPP_NAMESPACE::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::SharingMode::eExclusive,
		{},
		vk::ImageLayout::eUndefined
		);
	m_textures[name] = make_shared<Texture>(*g_vkComputeDevice, imageInfo, stagingBuf, width, height, this, name);

	//Clean up
	png_destroy_read_struct(&png, &info, &end);
	fclose(fp);
}
