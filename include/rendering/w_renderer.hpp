
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

#include "util/w_utils.hpp"
#include "util/w_color.hpp"
#include "window/w_window.hpp"
#include "rendering/w_buffer.hpp"
#include "rendering/w_mesh.hpp"
#include "rendering/w_camera.hpp"
#include "rendering/w_texture.hpp"
#include "rendering/w_descriptor.hpp"
#include "scene/w_lights.hpp"
#include "scene/w_components.hpp"
#include "w_pipeline.hpp"
#include "w_renderpass.hpp"
#include "w_shader.hpp"

#ifdef DEBUG
#define VULKAN_VALIDATION
#endif

namespace Wiesel {
	class Renderer {
	public:
		explicit Renderer(Reference<AppWindow> window);
		~Renderer();

		Reference<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex> vertices);
		void DestroyVertexBuffer(MemoryBuffer& buffer);

		Reference<MemoryBuffer> CreateIndexBuffer(std::vector<Index> indices);
		void DestroyIndexBuffer(MemoryBuffer& buffer);

		Reference<UniformBuffer> CreateUniformBuffer(VkDeviceSize size);
		void DestroyUniformBuffer(UniformBuffer& buffer);

		Reference<UniformBufferSet> CreateUniformBufferSet(uint32_t frames, VkDeviceSize size);
		void DestroyUniformBufferSet(UniformBufferSet& bufferSet);

		Reference<Texture> CreateBlankTexture();
		Reference<Texture> CreateTexture(const std::string& path, TextureProps textureProps, SamplerProps samplerProps);
		void DestroyTexture(Texture& texture);
		VkSampler CreateTextureSampler(uint32_t mipLevels, SamplerProps samplerProps);

		Reference<DepthStencil> CreateDepthStencil();
		void DestroyDepthStencil(DepthStencil& texture);

		Reference<ColorImage> CreateColorImage();
		void DestroyColorImage(ColorImage& texture);

		Reference<DescriptorData> CreateDescriptors(Reference<UniformBufferSet> uniformBufferSet, Reference<Material> material) __attribute__ ((optnone)); // optimization for this function is disabled because compiler does something weird
		void DestroyDescriptors(DescriptorData& descriptorPool);

		// todo properties
		Reference<DescriptorLayout> CreateDescriptorLayout();
		void DestroyDescriptorLayout(DescriptorLayout& layout);

		Reference<GraphicsPipeline> CreateGraphicsPipeline(PipelineProperties properties);
		void AllocateGraphicsPipeline(PipelineProperties properties, Reference<GraphicsPipeline> pipeline);
		void DestroyGraphicsPipeline(GraphicsPipeline& pipeline);
		void RecreateGraphicsPipeline(Reference<GraphicsPipeline> pipeline);
		void RecreateShader(Reference<Shader> shader);

		Reference<RenderPass> CreateRenderPass(RenderPassProperties properties);
		void AllocateRenderPass(Reference<RenderPass> renderPass);
		void DestroyRenderPass(RenderPass& renderPass);
		void RecreateRenderPass(Reference<RenderPass> renderPass);

		Reference<Shader> CreateShader(ShaderProperties properties);
		Reference<Shader> CreateShader(const std::vector<uint32_t>& code, ShaderProperties properties);
		void DestroyShader(Shader& shader);

		WIESEL_GETTER_FN Reference<CameraData> GetCameraData();

		void SetClearColor(float r, float g, float b, float a = 1.0f);
		void SetClearColor(const Colorf& color);
		WIESEL_GETTER_FN Colorf& GetClearColor();

		void SetMsaaSamples(VkSampleCountFlagBits samples);
		WIESEL_GETTER_FN VkSampleCountFlagBits GetMsaaSamples();

		void SetVsync(bool vsync);
		WIESEL_GETTER_FN bool IsVsync();

		void SetWireframeEnabled(bool value);
		WIESEL_GETTER_FN bool IsWireframeEnabled();
		WIESEL_GETTER_FN bool* IsWireframeEnabledPtr();

		void SetRecreateGraphicsPipeline(bool value);
		WIESEL_GETTER_FN bool IsRecreateGraphicsPipeline();

		void SetRecreateShaders(bool value);
		WIESEL_GETTER_FN bool IsRecreateShaders();

		WIESEL_GETTER_FN VkDevice GetLogicalDevice();
		WIESEL_GETTER_FN float GetAspectRatio() const;
		WIESEL_GETTER_FN uint32_t GetCurrentFrame() const;
		WIESEL_GETTER_FN const WindowSize& GetWindowSize() const;
		WIESEL_GETTER_FN LightsUniformBufferObject& GetLightsBufferObject();

		bool BeginFrame(Reference<CameraData> data);
		void DrawModel(ModelComponent& model, TransformComponent& transform);
		void DrawMesh(Reference<Mesh> mesh, TransformComponent& transform);
		void EndFrame();

		void RecreateSwapChain();
		void Cleanup();
	private:
		friend class Mesh;
		friend class ImGuiLayer;

		static const uint32_t k_MaxFramesInFlight;
		static Reference<Renderer> s_Renderer;

#ifdef VULKAN_VALIDATION
        std::vector<const char*> validationLayers;
		VkDebugUtilsMessengerEXT m_DebugMessenger{};
#endif
		std::vector<const char*> m_DeviceExtensions;

		Reference<AppWindow> m_Window;
		VkInstance m_Instance{};
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_LogicalDevice{};
		VkSurfaceKHR m_Surface{};
		VkQueue m_GraphicsQueue{};
		VkQueue m_PresentQueue{};
		VkSwapchainKHR m_SwapChain{};
		bool m_SwapChainCreated;

		uint32_t m_ImageIndex;
		// make this Wiesel Texture
		std::vector<VkImage> m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
		VkFormat m_SwapChainImageFormat;

		VkExtent2D m_SwapChainExtent{};
		Reference<DescriptorLayout> m_DefaultDescriptorLayout{};
		Reference<DepthStencil> m_DepthStencil;
		Reference<ColorImage> m_ColorImage;
		Reference<Texture> m_BlankTexture;

		std::vector<VkFramebuffer> m_SwapChainFramebuffers;
		VkCommandPool m_CommandPool{};

		std::vector<VkCommandBuffer> m_CommandBuffers;
		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;

		uint32_t m_CurrentFrame = 0;
		float_t m_AspectRatio;
		WindowSize m_WindowSize;
		VkSampleCountFlagBits m_MsaaSamples;
		VkSampleCountFlagBits m_PreviousMsaaSamples;
		Colorf m_ClearColor;
		bool m_Vsync;
		bool m_RecreateSwapChain;
		Reference<UniformBufferSet> m_LightsGlobalUboSet;
		LightsUniformBufferObject m_LightsGlobalUbo;
		bool m_EnableWireframe;
		bool m_RecreateGraphicsPipeline;
		bool m_RecreateShaders;
		Reference<GraphicsPipeline> m_DefaultGraphicsPipeline;
		Reference<GraphicsPipeline> m_CurrentGraphicsPipeline;
		Reference<RenderPass> m_DefaultRenderPass;
		Reference<CameraData> m_CameraData;

		void CreateVulkanInstance();
		void CreateSurface();
		void CreateImageViews();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSwapChain();
		void CreateDefaultRenderPass();
		void CreateDefaultDescriptorSetLayout();
		void CreateDefaultGraphicsPipeline();
		void CreateDepthResources();
		void CreateColorResources();
		void CreateFramebuffers();
		void CreateCommandPools();
		void CreateCommandBuffers();
		void CreatePermanentResources();
		void CreateSyncObjects();
		void CreateGlobalUniformBuffers();
		void CleanupSwapChain();
		void CleanupDefaultRenderPass();
		void CleanupGlobalUniformBuffers();
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
#ifdef VULKAN_VALIDATION
		bool CheckValidationLayerSupport();
		void SetupDebugMessenger();
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
#endif

	};

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
}