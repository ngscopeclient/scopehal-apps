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

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Texture

Texture::Texture(
	const vk::raii::Device& device,
	const vk::ImageCreateInfo& imageInfo,
	const vk::raii::Buffer& srcBuf,
	int width,
	int height,
	TextureManager* mgr
	)
	: m_image(device, imageInfo)
{
	//Once the image is created, allocate device memory to back it
	auto req = m_image.getMemoryRequirements();
	vk::MemoryAllocateInfo info(req.size, g_vkLocalMemoryType);
	m_deviceMemory = make_unique<vk::raii::DeviceMemory>(*g_vkComputeDevice, info);
	m_image.bindMemory(**m_deviceMemory, 0);

	//Transfer our image data over from the staging buffer
	{
		std::lock_guard<std::mutex> lock(g_vkTransferMutex);

		g_vkTransferCommandBuffer->begin({});

		//Initial image layout transition
		LayoutTransition(
			vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal);

		//Copy the buffer to the image
		vk::ImageSubresourceLayers subresource(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
		vk::BufferImageCopy region(0, 0, 0, subresource, vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1) );
		g_vkTransferCommandBuffer->copyBufferToImage(*srcBuf, *m_image, vk::ImageLayout::eTransferDstOptimal, region);

		//Convert to something optimal for texture reads
		LayoutTransition(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		g_vkTransferCommandBuffer->end();

		//Submit the request and block until it completes
		vk::raii::Fence fence(*g_vkComputeDevice, vk::FenceCreateInfo());
		vk::SubmitInfo sinfo({}, {}, **g_vkTransferCommandBuffer);
		g_vkTransferQueue->submit(sinfo, *fence);
		while(vk::Result::eTimeout == g_vkComputeDevice->waitForFences({*fence}, VK_TRUE, 1000 * 1000))
		{}
	}

	//Make a view for the image
	vk::ImageViewCreateInfo vinfo(
		{},
		*m_image,
		vk::ImageViewType::e2D,
		vk::Format::eR8G8B8A8Srgb,
		{},
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
	m_view = make_unique<vk::raii::ImageView>(*g_vkComputeDevice, vinfo);

	//Allocate a descriptor set from the texture manager, and bind our sampler etc to it
	m_descriptorSet = mgr->AllocateTextureDescriptor();
	vk::DescriptorImageInfo binfo(**mgr->GetSampler(), **m_view, vk::ImageLayout::eShaderReadOnlyOptimal);
	vector<vk::WriteDescriptorSet> writes;
	vk::WriteDescriptorSet wset(
		**m_descriptorSet,
		0,
		0,
		vk::DescriptorType::eCombinedImageSampler,
		binfo);
	writes.push_back(wset);
	g_vkComputeDevice->updateDescriptorSets(wset, nullptr);
}

void Texture::LayoutTransition(
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
		g_vkTransferCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			{},
			{},
			barrier);
	}
	else
	{
		g_vkTransferCommandBuffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			{},
			{},
			barrier);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

TextureManager::TextureManager()
{
	//Allocate descriptor pool
	vk::DescriptorPoolSize size(vk::DescriptorType::eSampler, 1000);
	vk::DescriptorPoolCreateInfo poolInfo(
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
		1,
		size);
	m_descriptorPool = make_unique<vk::raii::DescriptorPool>(*g_vkComputeDevice, poolInfo);

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

	//Set up descriptor set layout
	vector<vk::DescriptorSetLayoutBinding> bindings;
	bindings.push_back(vk::DescriptorSetLayoutBinding(
		0,
		vk::DescriptorType::eCombinedImageSampler,
		vk::ShaderStageFlagBits::eFragment,
		**m_sampler));
	vk::DescriptorSetLayoutCreateInfo linfo({}, bindings);
	m_descriptorLayout = make_unique<vk::raii::DescriptorSetLayout>(*g_vkComputeDevice, linfo);
}

TextureManager::~TextureManager()
{
	//Textures must be destroyed before we destroy the descriptor set they're allocated from
	m_textures.clear();
	m_descriptorLayout = nullptr;
	m_descriptorPool = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File loading

/**
	@brief Loads a texture from a file into a named resource

	If an existing texture by the same name already exists, it is overwritten.
 */
void TextureManager::LoadTexture(const string& name, const string& path)
{
	LogTrace("Loading texture \"%s\" from file \"%s\"\n", name.c_str(), path.c_str());

	//Load a texture
	int width = 256;
	int height = 256;
	VkDeviceSize size = width * height * 4;

	//Allocate temporary staging buffer
	vk::BufferCreateInfo bufinfo({}, size, vk::BufferUsageFlagBits::eTransferSrc);
	vk::raii::Buffer stagingBuf(*g_vkComputeDevice, bufinfo);

	//Figure out actual memory requirements of the buffer and allocate physical memory for it
	auto req = stagingBuf.getMemoryRequirements();
	vk::MemoryAllocateInfo info(req.size, g_vkPinnedMemoryType);
	vk::raii::DeviceMemory physMem(*g_vkComputeDevice, info);

	//Map it and bind to the buffer
	auto mappedPtr = reinterpret_cast<uint8_t*>(physMem.mapMemory(0, req.size));
	stagingBuf.bindMemory(*physMem, 0);

	//Fill the mapped buffer with image data
	for(int ipix=0; ipix<(width*height); ipix ++)
	{
		//full alpha magenta
		mappedPtr[ipix*4] = 0xff;
		mappedPtr[ipix*4 + 1] = 0x0;
		mappedPtr[ipix*4 + 2] = 0xff;
		mappedPtr[ipix*4 + 3] = 0xff;
	}
	physMem.unmapMemory();

	//Make the texture object
	vector<uint32_t> queueFamilies;
	vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
	queueFamilies.push_back(g_computeQueueType);	//FIXME: separate transfer queue?
	if(g_renderQueueType != g_computeQueueType)
	{
		queueFamilies.push_back(g_renderQueueType);
		sharingMode = vk::SharingMode::eConcurrent;
	}
	vk::ImageCreateInfo imageInfo(
		{},
		vk::ImageType::e2D,
		vk::Format::eR8G8B8A8Srgb,
		vk::Extent3D(width, height, 1),
		1,
		1,
		VULKAN_HPP_NAMESPACE::SampleCountFlagBits::e1,
		VULKAN_HPP_NAMESPACE::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		sharingMode,
		queueFamilies,
		vk::ImageLayout::eUndefined
		);
	m_textures[name] = make_shared<Texture>(*g_vkComputeDevice, imageInfo, stagingBuf, width, height, this);
}

/**
	@brief Allocates a new texture descriptor
 */
unique_ptr<vk::raii::DescriptorSet> TextureManager::AllocateTextureDescriptor()
{
	vk::DescriptorSetAllocateInfo dsinfo(**m_descriptorPool, **m_descriptorLayout);
	return make_unique<vk::raii::DescriptorSet>(move(vk::raii::DescriptorSets(*g_vkComputeDevice, dsinfo).front()));
}

ImTextureID TextureManager::GetTexture(const string& name)
{
	return m_textures[name]->GetTexture();
}
