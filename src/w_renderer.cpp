
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_renderer.h"

namespace Wiesel {
	Reference<Renderer> Renderer::s_Renderer;

	Renderer::Renderer(Reference<AppWindow> window) : m_Window(window) {
#ifdef DEBUG
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef __APPLE__
        deviceExtensions.push_back("VK_KHR_portability_subset");
#endif
		m_ImageIndex = 0;
		m_CurrentFrame = 0;
		CreateVulkanInstance();
#ifdef DEBUG
		SetupDebugMessenger();
#endif
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateDescriptorSetLayout();
		CreateGraphicsPipeline();
		CreateCommandPools();
		CreateCommandBuffers();
		CreateDepthResources();
		CreateFramebuffers();
		CreateSyncObjects();

		Reference<Camera> camera = CreateReference<Camera>(glm::vec3(1.0f), glm::quat(), m_AspectRatio);
		AddCamera(camera);
		SetActiveCamera(camera->GetObjectId());
	}

	Renderer::~Renderer() {

	}

	void Renderer::Create(Reference<AppWindow> window) {
		s_Renderer = CreateReference<Renderer>(window);
	}

	void Renderer::Destroy() {
		s_Renderer->Cleanup();
		s_Renderer = nullptr;
	}

	Reference<Renderer> Renderer::GetRenderer() {
		return s_Renderer;
	}

	VkDevice Renderer::GetLogicalDevice() {
		return m_LogicalDevice;
	}

	Reference<MemoryBuffer> Renderer::CreateVertexBuffer(std::vector<Vertex> vertices) {
		Reference<MemoryBuffer> memoryBuffer = CreateReference<MemoryBuffer>(MemoryTypeVertexBuffer);

		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Renderer::GetRenderer()->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t) bufferSize);
		vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->m_Buffer, memoryBuffer->m_BufferMemory);

		CopyBuffer(stagingBuffer, memoryBuffer->m_Buffer, bufferSize);

		vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
		return memoryBuffer;
	}

	void Renderer::DestroyVertexBuffer(MemoryBuffer& buffer) {
		vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
		vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
	}

	Reference<MemoryBuffer> Renderer::CreateIndexBuffer(std::vector<Index> indices) {
		Reference<MemoryBuffer> memoryBuffer = CreateReference<MemoryBuffer>(MemoryTypeIndexBuffer);

		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t) bufferSize);
		vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->m_Buffer, memoryBuffer->m_BufferMemory);

		CopyBuffer(stagingBuffer, memoryBuffer->m_Buffer, bufferSize);

		vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);

		return memoryBuffer;
	}

	Reference<UniformBuffer> Renderer::CreateUniformBuffer() {
		VkDeviceSize bufferSize = sizeof(Wiesel::UniformBufferObject);

		Reference<UniformBuffer> uniformBuffer = CreateReference<UniformBuffer>();

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffer->m_Buffer, uniformBuffer->m_BufferMemory);

		WIESEL_CHECK_VKRESULT(vkMapMemory(m_LogicalDevice, uniformBuffer->m_BufferMemory, 0, bufferSize, 0, &uniformBuffer->m_Data));

		return uniformBuffer;
	}

	Reference<UniformBufferSet> Renderer::CreateUniformBufferSet(uint32_t frames) {
		Reference<UniformBufferSet> bufferSet = CreateReference<UniformBufferSet>(frames);

		for (int frame = 0; frame < frames; ++frame) {
			bufferSet->m_Buffers.emplace_back(CreateUniformBuffer());
		}

		return bufferSet;
	}

	void Renderer::DestroyIndexBuffer(MemoryBuffer& buffer) {
		vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
		vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
	}

	void Renderer::DestroyUniformBuffer(UniformBuffer& buffer) {
		vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
		vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
	}

	void Renderer::DestroyUniformBufferSet(UniformBufferSet& bufferSet) {
		// todo reuse pools
		bufferSet.m_Buffers.clear();
	}

	Reference<Texture> Renderer::CreateTexture(const std::string& path) {
		Reference<Texture> texture = CreateReference<Texture>(TextureTypeTexture, path);

		stbi_uc* pixels = stbi_load(path.c_str(), &texture->m_Width, &texture->m_Height, &texture->m_Channels, STBI_rgb_alpha);
		texture->m_Size = texture->m_Width * texture->m_Height * STBI_rgb_alpha;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image: " + path);
		}

		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(texture->m_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, texture->m_Size, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(texture->m_Size));
		vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

		stbi_image_free(pixels);

		CreateImage(texture->m_Width, texture->m_Height, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image, texture->m_DeviceMemory);

		TransitionImageLayout(texture->m_Image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(stagingBuffer, texture->m_Image, static_cast<uint32_t>(texture->m_Width), static_cast<uint32_t>(texture->m_Height));

		TransitionImageLayout(texture->m_Image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);

		// todo move this to a function
		// Sampler
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		// * VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
		// * VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
		// * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: Take the color of the edge closest to the coordinate beyond the image dimensions.
		// * VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: Like clamp to edge, but instead uses the edge opposite to the closest edge.
		// * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: Return a solid color when sampling beyond the dimensions of the image.
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		WIESEL_CHECK_VKRESULT(vkCreateSampler(m_LogicalDevice, &samplerInfo, nullptr, &texture->m_Sampler));

		texture->m_ImageView = CreateImageView(texture->m_Image, format, VK_IMAGE_ASPECT_COLOR_BIT);

		texture->m_Allocated = true;
		return texture;
	}

	Reference<Texture> Renderer::CreateDepthStencil() {
		Reference<Texture> texture = CreateReference<Texture>(TextureTypeDepthStencil, "");

		VkFormat depthFormat = FindDepthFormat();

		CreateImage(m_SwapChainExtent.width, m_SwapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image, texture->m_DeviceMemory);
		texture->m_ImageView = CreateImageView(texture->m_Image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
		TransitionImageLayout(texture->m_Image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		texture->m_Allocated = true;
		return texture;
	}

	void Renderer::DestroyTexture(Texture& texture) {
        if (!texture.m_Allocated) {
            return;
        }

		vkDestroySampler(m_LogicalDevice, texture.m_Sampler, nullptr);
		vkDestroyImageView(m_LogicalDevice, texture.m_ImageView, nullptr);
		vkDestroyImage(m_LogicalDevice, texture.m_Image, nullptr);
		vkFreeMemory(m_LogicalDevice, texture.m_DeviceMemory, nullptr);

		texture.m_Allocated = false;
	}

	void Renderer::DestroyDepthStencil(Texture& texture) {
        if (!texture.m_Allocated) {
            return;
        }

		vkDestroyImageView(m_LogicalDevice, texture.m_ImageView, nullptr);
		vkDestroyImage(m_LogicalDevice, texture.m_Image, nullptr);
		vkFreeMemory(m_LogicalDevice, texture.m_DeviceMemory, nullptr);

		texture.m_Allocated = false;
	}

	Reference<DescriptorPool> Renderer::CreateDescriptors(Reference<UniformBufferSet> uniformBufferSet, Reference<Texture> texture) {
		Reference<DescriptorPool> object = CreateReference<DescriptorPool>(uniformBufferSet->m_BufferCount);

		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(uniformBufferSet->m_BufferCount);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(uniformBufferSet->m_BufferCount);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(uniformBufferSet->m_BufferCount);

		WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

		std::vector<VkDescriptorSetLayout> layouts(uniformBufferSet->m_BufferCount, m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = object->m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(uniformBufferSet->m_BufferCount);
		allocInfo.pSetLayouts = layouts.data();

		WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, object->m_Descriptors.data()));

		for (size_t i = 0; i < uniformBufferSet->m_BufferCount; i++) {
			std::vector<VkWriteDescriptorSet> writes{};

			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBufferSet->m_Buffers[i]->m_Buffer;
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(Wiesel::UniformBufferObject);

				VkWriteDescriptorSet set;
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.dstSet = object->m_Descriptors[i];
				set.dstBinding = 0;
				set.dstArrayElement = 0;
				set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				set.descriptorCount = 1;
				set.pBufferInfo = &bufferInfo;
				set.pNext = nullptr;

				writes.push_back(set);
			}

			if (texture != nullptr) {
				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texture->m_ImageView;
				imageInfo.sampler = texture->m_Sampler;

				VkWriteDescriptorSet set;
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.dstSet = object->m_Descriptors[i];
				set.dstBinding = writes.size();
				set.dstArrayElement = 0;
				set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				set.descriptorCount = 1;
				set.pImageInfo = &imageInfo;
				set.pNext = nullptr;

				writes.push_back(set);
			}

			vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}

		return object;
	}

	void Renderer::DestroyDescriptors(DescriptorPool& descriptorPool) {
		// Destroying the pool is enough to destroy all descriptor set objects.
		vkDestroyDescriptorPool(m_LogicalDevice, descriptorPool.m_DescriptorPool, nullptr);
	}

	void Renderer::AddMesh(Reference<Mesh> mesh) {
		m_Meshes.emplace_back(std::move(mesh));
	}

	void Renderer::RemoveMesh(Reference<Mesh> mesh) {
		auto value = std::remove(m_Meshes.begin(), m_Meshes.end(), mesh);
		m_Meshes.erase(value, m_Meshes.end());
	}

	void Renderer::AddModel(Reference<Model> mesh) {
		m_Models.emplace_back(std::move(mesh));
	}

	uint32_t Renderer::GetCurrentFrame() const {
		return m_CurrentFrame;
	}

	Reference<Camera> Renderer::GetActiveCamera() {
		return m_Cameras[m_ActiveCameraId];
	}

	void Renderer::AddCamera(Reference<Camera> camera) {
		camera->SetObjectId(m_Cameras.size());
		m_Cameras.emplace_back(std::move(camera));
	}

	void Renderer::SetActiveCamera(uint64_t id) {
		m_ActiveCameraId = id;
	}

	float Renderer::GetAspectRatio() const {
		return m_AspectRatio;
	}

	WindowSize Renderer::GetWindowSize() const {
		return m_WindowSize;
	}

	void Renderer::Cleanup() {
		vkDeviceWaitIdle(m_LogicalDevice);
        LogDebug("Destroying Renderer");
		CleanupSwapChain();

        LogDebug("Destroying cameras");
		m_Cameras.clear();

        LogDebug("Destroying models");
		m_Models.clear();

        LogDebug("Destroying meshes");
		m_Meshes.clear();

        LogDebug("Destroying descriptor set layout");
		vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayout, nullptr);
        LogDebug("Destroying graphics pipeline");
		vkDestroyPipeline(m_LogicalDevice, graphicsPipeline, nullptr);
        LogDebug("Destroying pipeline layout");
		vkDestroyPipelineLayout(m_LogicalDevice, pipelineLayout, nullptr);

        LogDebug("Destroying render pass");
		vkDestroyRenderPass(m_LogicalDevice, m_RenderPass, nullptr);

        LogDebug("Destroying semaphores and fences");
		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			vkDestroySemaphore(m_LogicalDevice, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_LogicalDevice, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(m_LogicalDevice, inFlightFences[i], nullptr);
		}

        LogDebug("Destroying command pool");
		vkDestroyCommandPool(m_LogicalDevice, commandPool, nullptr);

        LogDebug("Destroying device");
		vkDestroyDevice(m_LogicalDevice, nullptr);

#ifdef DEBUG
        LogDebug("Destroying debug messanger");
		DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif

        LogDebug("Destroying surface khr");
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        LogDebug("Destroying vulkan instance");
		vkDestroyInstance(m_Instance, nullptr);

		LogDebug("Renderer destroyed");
	}

	void Renderer::CreateVulkanInstance() {
#ifdef DEBUG
		if (!CheckValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}
#endif

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Wiesel";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = GetRequiredExtensions();
        extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef __APPLE__
        extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef DEBUG
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#else
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
#endif

		WIESEL_CHECK_VKRESULT(vkCreateInstance(&createInfo, nullptr, &m_Instance));
	}

	void Renderer::CreateSurface() {
		m_Window->CreateWindowSurface(m_Instance, &m_Surface);
	}

	void Renderer::PickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
		Wiesel::LogDebug(std::to_string(deviceCount) + " devices found!");
		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

		// Use an ordered map to automatically sort candidates by increasing score
		std::multimap<int, VkPhysicalDevice> candidates;

		for (const auto& device : devices) {
			if (!IsDeviceSuitable(device)) {
				continue;
			}
			int32_t score = RateDeviceSuitability(device);
			candidates.insert(std::make_pair(score, device));
		}

		// Check if the best candidate is suitable at all
		if (candidates.rbegin()->first > 0) {
			m_PhysicalDevice = candidates.rbegin()->second;
		} else {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	void Renderer::CreateLogicalDevice() {
		LogDebug("Creating logical device");
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		//deviceFeatures.fillModeNonSolid = true;
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		createInfo.enabledLayerCount = 0;

		if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_LogicalDevice) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(m_LogicalDevice, indices.presentFamily.value(), 0, &m_PresentQueue);
		vkGetDeviceQueue(m_LogicalDevice, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
	}

	void Renderer::CreateSwapChain() {
		LogDebug("Creating swap chain");
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
		uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		// The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.
		// You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		// If it's clipped, obscured pixels will be ignored hence increasing the performance.
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount, m_SwapChainImages.data());
		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = extent;

		m_AspectRatio = m_SwapChainExtent.width / (float) m_SwapChainExtent.height;
		m_WindowSize.Width = m_SwapChainExtent.width;
		m_WindowSize.Height = m_SwapChainExtent.height;
	}

	void Renderer::CreateImageViews() {
		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (uint32_t i = 0; i < m_SwapChainImages.size(); i++) {
			m_SwapChainImageViews[i] = CreateImageView(m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	void Renderer::CreateRenderPass() {
		LogDebug("Creating render pass");
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_SwapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(m_LogicalDevice, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void Renderer::CreateDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = nullptr;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		WIESEL_CHECK_VKRESULT(vkCreateDescriptorSetLayout(m_LogicalDevice, &layoutInfo, nullptr, &m_DescriptorSetLayout));
	}

	void Renderer::CreateGraphicsPipeline() {
		auto vertShaderCode = Wiesel::ReadFile("assets/shaders/test.vert.spv");
		auto fragShaderCode = Wiesel::ReadFile("assets/shaders/test.frag.spv");

		VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";
		VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		std::vector<VkDynamicState> dynamicStates = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		auto bindingDescription = Wiesel::Vertex::GetBindingDescription();
		auto attributeDescriptions = Wiesel::Vertex::GetAttributeDescriptions();

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();


		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) m_SwapChainExtent.width;
		viewport.height = (float) m_SwapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_SwapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		/*
		 * VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
		 * VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
		 * VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
		 */
		//rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		//rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
		/*colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;*/

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		WIESEL_CHECK_VKRESULT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout));


		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;

		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = m_RenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		WIESEL_CHECK_VKRESULT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

		// Cleanup
		vkDestroyShaderModule(m_LogicalDevice, fragShaderModule, nullptr);
		vkDestroyShaderModule(m_LogicalDevice, vertShaderModule, nullptr);
	}

	void Renderer::CreateDepthResources() {
        LogDebug("Creating depth stencil");
		m_DepthStencil = CreateDepthStencil();
	}

	void Renderer::CreateFramebuffers() {
		swapChainFramebuffers.resize(m_SwapChainImageViews.size());
		for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) {
			std::array<VkImageView, 2> attachments = {
					m_SwapChainImageViews[i],
					m_DepthStencil->m_ImageView
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = m_SwapChainExtent.width;
			framebufferInfo.height = m_SwapChainExtent.height;
			framebufferInfo.layers = 1;

			WIESEL_CHECK_VKRESULT(vkCreateFramebuffer(m_LogicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]));
		}
	}

	void Renderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		WIESEL_CHECK_VKRESULT(vkCreateBuffer(m_LogicalDevice, &bufferInfo, nullptr, &buffer));

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_LogicalDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

		WIESEL_CHECK_VKRESULT(vkAllocateMemory(m_LogicalDevice, &allocInfo, nullptr, &bufferMemory));
		WIESEL_CHECK_VKRESULT(vkBindBufferMemory(m_LogicalDevice, buffer, bufferMemory, 0));
	}

	void Renderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(commandBuffer);
	}

	void Renderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = {0, 0, 0};
		region.imageExtent = {
				width,
				height,
				1
		};

		vkCmdCopyBufferToImage(
				commandBuffer,
				buffer,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&region
		);

		EndSingleTimeCommands(commandBuffer);
	}

	void Renderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent(format)) {
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		} else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
				commandBuffer,
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
		);

		EndSingleTimeCommands(commandBuffer);
	}

	void Renderer::CreateCommandPools() {
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
		WIESEL_CHECK_VKRESULT(vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &commandPool));
	}

	void Renderer::CreateCommandBuffers() {
		commandBuffers.resize(k_MaxFramesInFlight);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

		WIESEL_CHECK_VKRESULT(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, commandBuffers.data()));
	}

	void Renderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(width);
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		/*
		 * VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order like our pixels array
		 * VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
		 */
		imageInfo.tiling = tiling;
		/*
		 * VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
		 * VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels.
		 */
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional
		WIESEL_CHECK_VKRESULT(vkCreateImage(m_LogicalDevice, &imageInfo, nullptr, &image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_LogicalDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

		WIESEL_CHECK_VKRESULT(vkAllocateMemory(m_LogicalDevice, &allocInfo, nullptr, &imageMemory));

		vkBindImageMemory(m_LogicalDevice, image, imageMemory, 0);
	}

	VkImageView Renderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		WIESEL_CHECK_VKRESULT(vkCreateImageView(m_LogicalDevice, &viewInfo, nullptr, &imageView));

		return imageView;
	}

	VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat Renderer::FindDepthFormat() {
		return FindSupportedFormat(
				{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
				VK_IMAGE_TILING_OPTIMAL,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool Renderer::HasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void Renderer::CreateSyncObjects() {
		imageAvailableSemaphores.resize(k_MaxFramesInFlight);
		renderFinishedSemaphores.resize(k_MaxFramesInFlight);
		inFlightFences.resize(k_MaxFramesInFlight);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			WIESEL_CHECK_VKRESULT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
			WIESEL_CHECK_VKRESULT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
			WIESEL_CHECK_VKRESULT(vkCreateFence(m_LogicalDevice, &fenceInfo, nullptr, &inFlightFences[i]));
		}
	}

	void Renderer::CleanupSwapChain() {
        LogDebug("Cleanup swap chain");
		m_DepthStencil = nullptr;

        LogDebug("Destroying swap chain framebuffers");
		for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(m_LogicalDevice, swapChainFramebuffers[i], nullptr);
		}

        LogDebug("Destroying swap chain imageviews");
		for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) {
			vkDestroyImageView(m_LogicalDevice, m_SwapChainImageViews[i], nullptr);
		}

        LogDebug("Destroying swap chain");
		vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);
	}

	void Renderer::RecreateSwapChain() {
		Wiesel::LogInfo("Recreating swap chains...");
		Wiesel::WindowSize size{};
		m_Window->GetWindowFramebufferSize(size);
        while(size.Width == 0 || size.Height == 0) {
            m_Window->GetWindowFramebufferSize(size);
            m_Window->OnUpdate();
        }

		vkDeviceWaitIdle(m_LogicalDevice);

		CleanupSwapChain();

		CreateSwapChain();
		CreateImageViews();
		CreateDepthResources();
		CreateFramebuffers();

		AppRecreateSwapChainsEvent event(size, m_AspectRatio);
		PublishEvent(event);
	}

	void Renderer::PublishEvent(Event& event) {
		for (const auto& camera : m_Cameras) {
			if (event.m_Handled) {
				return;
			}
			camera->OnEvent(event);
		}
	}

	void Renderer::BeginFrame() {
		vkWaitForFences(m_LogicalDevice, 1, &inFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		VkResult result = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX, imageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_ImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapChain();
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		// Setup
		vkResetFences(m_LogicalDevice, 1, &inFlightFences[m_CurrentFrame]);
		vkResetCommandBuffer(commandBuffers[m_CurrentFrame], 0);

		// Actual drawing
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		WIESEL_CHECK_VKRESULT(vkBeginCommandBuffer(commandBuffers[m_CurrentFrame], &beginInfo));

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[m_ImageIndex];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = m_SwapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
		clearValues[1].depthStencil = {1.0f, 0};

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();
		vkCmdBeginRenderPass(commandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_SwapChainExtent.width);
		viewport.height = static_cast<float>(m_SwapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[m_CurrentFrame], 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = m_SwapChainExtent;
		vkCmdSetScissor(commandBuffers[m_CurrentFrame], 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	}

	void Renderer::DrawMeshes() {
		for (const auto& mesh : m_Meshes) {
			DrawMesh(mesh);
		}
	}

	void Renderer::DrawModels() {
		for (const auto& model : m_Models) {
			DrawModel(model);
		}
	}

	void Renderer::DrawMesh(Reference<Mesh> mesh) {
		if (!mesh->IsAllocated()) {
			return;
		}

		mesh->UpdateUniformBuffer();

		VkBuffer vertexBuffers[] = {mesh->GetVertexBuffer()->m_Buffer};
		std::vector<Index> indices = mesh->GetIndices();
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffers[m_CurrentFrame], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffers[m_CurrentFrame], mesh->GetIndexBuffer()->m_Buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(commandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &mesh->GetDescriptors()->m_Descriptors[m_CurrentFrame], 0, nullptr);
		vkCmdDrawIndexed(commandBuffers[m_CurrentFrame], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	}

	void Renderer::DrawModel(Reference<Model> model) {
		for (const auto& mesh : model->GetMeshes()) {
			DrawMesh(mesh);
		}
	}

	void Renderer::EndFrame() {
		// Finish command buffer
		vkCmdEndRenderPass(commandBuffers[m_CurrentFrame]);
		WIESEL_CHECK_VKRESULT(vkEndCommandBuffer(commandBuffers[m_CurrentFrame]));

		// Presentation
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[m_CurrentFrame];

		VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[m_CurrentFrame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[m_CurrentFrame]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		WIESEL_CHECK_VKRESULT(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, inFlightFences[m_CurrentFrame]));

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = {m_SwapChain};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &m_ImageIndex;
		presentInfo.pResults = nullptr; // Optional

		VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			RecreateSwapChain();
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		m_CurrentFrame = (m_CurrentFrame + 1) % k_MaxFramesInFlight;
	}

	std::vector<const char*> Renderer::GetRequiredExtensions() {
		uint32_t extensionsCount = 0;
		const char** windowExtensions;
		windowExtensions = m_Window->GetRequiredInstanceExtensions(&extensionsCount);

		std::vector<const char*> extensions(windowExtensions, windowExtensions + extensionsCount);
#ifdef DEBUG
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
		return extensions;
	}

	int32_t Renderer::RateDeviceSuitability(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		int32_t score = 0;

		// Discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1000;
		}

		// Maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;
		return score;
	}

	VkCommandBuffer Renderer::BeginSingleTimeCommands() {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_GraphicsQueue);

		vkFreeCommandBuffers(m_LogicalDevice, commandPool, 1, &commandBuffer);
	}

	bool Renderer::IsDeviceSuitable(VkPhysicalDevice device) {
		QueueFamilyIndices indices = FindQueueFamilies(device);

		bool extensionsSupported = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
	}

	VkSurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR Renderer::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		/*
		 * VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
		 * VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
		 * VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
		 * VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
		 */
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;

	}

	VkExtent2D Renderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		} else {
			Wiesel::WindowSize size{};
			m_Window->GetWindowFramebufferSize(size);

			VkExtent2D actualExtent = {
					static_cast<uint32_t>(size.Width),
					static_cast<uint32_t>(size.Height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	QueueFamilyIndices Renderer::FindQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;
		// Logic to find queue family indices to populate struct with

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		uint32_t i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.IsComplete()) {
				break;
			}

			i++;
		}
		return indices;
	}

	SwapChainSupportDetails Renderer::QuerySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
		}


		return details;
	}

	VkShaderModule Renderer::CreateShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
		VkShaderModule shaderModule;
		WIESEL_CHECK_VKRESULT(vkCreateShaderModule(m_LogicalDevice, &createInfo, nullptr, &shaderModule));
		return shaderModule;
	}

	uint32_t Renderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

#ifdef DEBUG
	void Renderer::SetupDebugMessenger() {
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		WIESEL_CHECK_VKRESULT(CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger));
	}

	VkResult Renderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		} else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void Renderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

	bool Renderer::CheckValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	void Renderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			Wiesel::LogDebug("Validation layer: " + std::string(pCallbackData->pMessage));
		} else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			Wiesel::LogWarn("Validation layer: " + std::string(pCallbackData->pMessage));
		} else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			Wiesel::LogError("Validation layer: " + std::string(pCallbackData->pMessage));
		} else {
			Wiesel::LogInfo("Validation layer: " + std::string(pCallbackData->pMessage));
		}
		return VK_FALSE;
	}
#endif

}