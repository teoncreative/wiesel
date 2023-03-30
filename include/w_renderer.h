
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "util/w_utils.h"
#include "w_buffer.h"
#include "w_mesh.h"
#include "window/w_window.h"
#include "w_camera.h"

namespace Wiesel {
	class Renderer {
	public:
		explicit Renderer(Reference<AppWindow> window);
		~Renderer();

		static void Create(Reference<AppWindow> window);
		WIESEL_GETTER_FN static Reference<Renderer> GetRenderer();

		void AddMesh(Reference<Mesh> mesh);
		void RemoveMesh(Reference<Mesh> mesh);

		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
		VkImageView CreateImageView(VkImage image, VkFormat format);

		WIESEL_GETTER_FN VkDevice GetLogicalDevice();

		Reference<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex> vertices);
		void DestroyVertexBuffer(MemoryBuffer& buffer);

		Reference<MemoryBuffer> CreateIndexBuffer(std::vector<Index> indices);
		void DestroyIndexBuffer(MemoryBuffer& buffer);

		Reference<UniformBuffer> CreateUniformBuffer(VkDescriptorPool pool);
		void DestroyUniformBuffer(UniformBuffer& buffer);

		Reference<UniformBufferSet> CreateUniformBufferSet(uint32_t frames);
		void DestroyUniformBufferSet(UniformBufferSet& bufferSet);

		WIESEL_GETTER_FN Reference<Camera> GetActiveCamera();
		void AddCamera(Reference<Camera> camera);
		void SetActiveCamera(uint64_t id);

		WIESEL_GETTER_FN float GetAspectRatio() const;

		void BeginFrame();
		void DrawMeshes();
		void DrawMesh(Reference<Mesh> mesh);
		void EndFrame();

		WIESEL_GETTER_FN uint32_t GetCurrentFrame() const;

		static void Destroy();

	private:
		friend class Mesh;

		static const int k_MaxFramesInFlight = 2;
		static Reference<Renderer> s_Renderer;

#ifdef DEBUG
		const std::vector<const char*> validationLayers = {
				"VK_LAYER_KHRONOS_validation"
		};
#endif

		const std::vector<const char*> deviceExtensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				"VK_KHR_portability_subset"
		};

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
		std::vector<Reference<Camera>> m_Cameras;
		uint64_t m_ActiveCameraId;
		float_t m_AspectRatio;

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
		void CreateFramebuffers();
		void CreateCommandPools();
		void CreateCommandBuffers();
		void CreateSyncObjects();
		void CleanupSwapChain();
		void RecreateSwapChain();
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
		bool CheckValidationLayerSupport();

		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		std::vector<const char*> GetRequiredExtensions();


		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
#ifdef DEBUG
		void SetupDebugMessenger();
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
#endif

	};

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
}