
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

#include "util/w_utils.h"
#include "w_buffer.h"
#include "w_mesh.h"
#include "w_model.h"
#include "window/w_window.h"
#include "w_camera.h"
#include "w_texture.h"
#include "w_descriptor.h"
#include "util/w_color.h"

namespace Wiesel {
	class Renderer {
	public:
		explicit Renderer(Reference<AppWindow> window);
		~Renderer();

		static void Create(Reference<AppWindow> window);
		static void Destroy();
		WIESEL_GETTER_FN static Reference<Renderer> GetRenderer();

		void AddMesh(Reference<Mesh> mesh);
		void RemoveMesh(Reference<Mesh> mesh);

		void AddModel(Reference<Model> mesh);

		Reference<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex> vertices);
		void DestroyVertexBuffer(MemoryBuffer& buffer);

		Reference<MemoryBuffer> CreateIndexBuffer(std::vector<Index> indices);
		void DestroyIndexBuffer(MemoryBuffer& buffer);

		Reference<UniformBuffer> CreateUniformBuffer();
		void DestroyUniformBuffer(UniformBuffer& buffer);

		Reference<UniformBufferSet> CreateUniformBufferSet(uint32_t frames);
		void DestroyUniformBufferSet(UniformBufferSet& bufferSet);

		Reference<Texture> CreateBlankTexture();
		Reference<Texture> CreateTexture(const std::string& path, TextureProps props);
		void DestroyTexture(Texture& texture);

		Reference<Texture> CreateDepthStencil();
		void DestroyDepthStencil(Texture& texture);

		Reference<Texture> CreateColorImage();
		void DestroyColorImage(Texture& texture);

		Reference<DescriptorPool> CreateDescriptors(Reference<UniformBufferSet> uniformBufferSet, Reference<Texture> texture);
		void DestroyDescriptors(DescriptorPool& descriptorPool);

		WIESEL_GETTER_FN Reference<Camera> GetActiveCamera();
		void AddCamera(Reference<Camera> camera);
		void SetActiveCamera(uint64_t id);

		void SetClearColor(float r, float g, float b, float a = 1.0f);
		void SetClearColor(const Colorf& color);
		WIESEL_GETTER_FN Colorf& GetClearColor();

		void SetMsaaSamples(VkSampleCountFlagBits samples);
		WIESEL_GETTER_FN VkSampleCountFlagBits GetMsaaSamples();

		WIESEL_GETTER_FN VkDevice GetLogicalDevice();
		WIESEL_GETTER_FN float GetAspectRatio() const;
		WIESEL_GETTER_FN uint32_t GetCurrentFrame() const;
		WIESEL_GETTER_FN WindowSize GetWindowSize() const;

		void BeginFrame();
		void DrawMeshes();
		void DrawModels();

		void DrawMesh(Reference<Mesh> mesh);
		void DrawModel(Reference<Model> model);
		void EndFrame();

		void RecreateSwapChain();

		void PublishEvent(Event& event);

	private:
		friend class Mesh;

		static const int k_MaxFramesInFlight = 2;
		static Reference<Renderer> s_Renderer;

#ifdef DEBUG
        std::vector<const char*> validationLayers;
#endif
		std::vector<const char*> deviceExtensions;

		Reference<AppWindow> m_Window;
		VkInstance m_Instance{};
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_LogicalDevice{};
		VkSurfaceKHR m_Surface{};
		VkQueue m_GraphicsQueue{};
		VkQueue m_PresentQueue{};
		VkSwapchainKHR m_SwapChain{};

		VkDebugUtilsMessengerEXT m_DebugMessenger{};
		std::vector<VkImage> m_SwapChainImages;
		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent{};

		std::vector<VkImageView> m_SwapChainImageViews;
		VkRenderPass m_RenderPass{};
		VkDescriptorSetLayout m_DescriptorSetLayout{};
		uint32_t m_ImageIndex;
		Reference<Texture> m_DepthStencil;
		Reference<Texture> m_ColorImage;
		Reference<Texture> m_BlankTexture;

		VkPipelineLayout pipelineLayout{};
		VkPipeline graphicsPipeline{};
		std::vector<VkFramebuffer> swapChainFramebuffers;
		VkCommandPool commandPool{};

		std::vector<VkCommandBuffer> commandBuffers;
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;

		uint32_t m_CurrentFrame = 0;
		std::vector<Reference<Mesh>> m_Meshes;
		std::vector<Reference<Model>> m_Models;
		std::vector<Reference<Camera>> m_Cameras;
		uint64_t m_ActiveCameraId;
		float_t m_AspectRatio;
		WindowSize m_WindowSize;
		VkSampleCountFlagBits m_MsaaSamples;
		VkSampleCountFlagBits m_PreviousMsaaSamples;
		Colorf m_ClearColor;

		void Cleanup();
		void CreateVulkanInstance();
		void CreateSurface();
		void CreateImageViews();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSwapChain();
		void CreateRenderPass();
		void CreateDescriptorSetLayout();
		void CreateGraphicsPipeline();
		void CreateDepthResources();
		void CreateColorResources();
		void CreateFramebuffers();
		void CreateCommandPools();
		void CreateCommandBuffers();
		void CreatePermanentResources();
		void CreateSyncObjects();
		void CleanupSwapChain();
		VkShaderModule CreateShaderModule(const std::vector<char>& code);
		int32_t RateDeviceSuitability(VkPhysicalDevice device);
		bool IsDeviceSuitable(VkPhysicalDevice device);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
		VkCommandBuffer BeginSingleTimeCommands();
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

		std::vector<const char*> GetRequiredExtensions();
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
		void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
		VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkFormat FindDepthFormat();
		bool HasStencilComponent(VkFormat format);
		void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
		VkSampleCountFlagBits GetMaxUsableSampleCount();
#ifdef DEBUG
		bool CheckValidationLayerSupport();
		void SetupDebugMessenger();
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
#endif

	};

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
}