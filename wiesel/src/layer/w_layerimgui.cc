
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "layer/w_layerimgui.h"

// clang-format off
// Import order important
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imnodes.h>
// clang-format on

#include <backends/imgui_impl_sdl3.h>
#include "events/w_engineevents.h"
#include "rendering/w_renderer.h"
#include "util/imgui/imgui_theme.h"
#include "w_engine.h"

namespace wiesel {

ImGuiLayer::ImGuiLayer() : Layer("ImGui") {}

ImGuiLayer::~ImGuiLayer() = default;

void ImGuiLayer::OnAttach() {
  LOG_DEBUG("Creating imgui");
  //1: create descriptor pool for IMGUI
  // the size of the pool is very oversize, but it's copied from imgui demo itself.
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      Engine::renderer()->logical_device_, &pool_info, nullptr, &m_ImGuiPool));

  // 2: initialize imgui library

  //this initializes the core structures of imgui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImNodes::CreateContext();
  //ImGui::StyleColorsDark(&ImGui::GetStyle());
  ImGui::Moonlight::ApplyTheme();
  ImGui::Moonlight::LoadFont();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  Engine::renderer()->window_->ImGuiInit();

  //this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = Engine::renderer()->instance_;
  init_info.PhysicalDevice = Engine::renderer()->physical_device_;
  init_info.Device = Engine::renderer()->logical_device_;
  init_info.Queue = Engine::renderer()->graphics_queue_;
  init_info.DescriptorPool = m_ImGuiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.PipelineInfoMain.MSAASamples =
      ToVkSampleCountFlagBits(Engine::renderer()->options().msaa_mode);
  init_info.PipelineInfoMain.RenderPass =
      Engine::renderer()->present_render_pass_->GetVulkanHandle();

  ImGui_ImplVulkan_Init(&init_info);
}

void ImGuiLayer::OnDetach() {
  LOG_DEBUG("Destroying imgui pool");
  vkDeviceWaitIdle(Engine::renderer()->logical_device_);
  ImNodes::DestroyContext();
  ImGui_ImplVulkan_Shutdown();
  vkDestroyDescriptorPool(Engine::renderer()->logical_device_, m_ImGuiPool,
                          nullptr);
  m_ImGuiPool = VK_NULL_HANDLE;
}

void ImGuiLayer::OnUpdate(float_t deltaTime) {}

void ImGuiLayer::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};
  dispatcher.Dispatch<PipelineRecreatedEvent>(
      [this](PipelineRecreatedEvent& e) {
        // Defer reinitialization until next frame to avoid mid-frame issues
        needs_reinitialization_ = true;
        return false;
      });
}

void ImGuiLayer::ReinitializeImGuiVulkan() {
  LOG_INFO("Reinitializing ImGui backends");

  // No need for vkDeviceWaitIdle here - RecreateSwapChain already did that
  // and we're between frames, so it's safe to reinitialize

  // Shutdown both backends
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();

  // Reinitialize window backend (must be done before Vulkan backend)
  Engine::renderer()->window_->ImGuiInit();

  // Reinitialize Vulkan backend with new settings
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = Engine::renderer()->instance_;
  init_info.PhysicalDevice = Engine::renderer()->physical_device_;
  init_info.Device = Engine::renderer()->logical_device_;
  init_info.Queue = Engine::renderer()->graphics_queue_;
  init_info.DescriptorPool = m_ImGuiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.PipelineInfoMain.MSAASamples =
      ToVkSampleCountFlagBits(Engine::renderer()->options().msaa_mode);
  init_info.PipelineInfoMain.RenderPass =
      Engine::renderer()->present_render_pass_->GetVulkanHandle();

  ImGui_ImplVulkan_Init(&init_info);

  needs_reinitialization_ = false;
}

void ImGuiLayer::OnBeginPresent() {
  PROFILE_ZONE_SCOPED();

  // Handle deferred reinitialization at the start of a new frame
  if (needs_reinitialization_) {
    ReinitializeImGuiVulkan();
  }

  ImGui_ImplVulkan_NewFrame();
  Engine::renderer()->window_->ImGuiNewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
}

void ImGuiLayer::OnPresent() {
  PROFILE_ZONE_SCOPED_N("ImGuiLayer::OnPresent");
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(
      ImGui::GetDrawData(), Engine::renderer()->GetCommandBuffer().handle_);
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    // move this to window handle!!
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

}  // namespace wiesel
