
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "layer/w_layerimgui.hpp"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <ImGuizmo.h>

#include "rendering/w_renderer.hpp"
#include "util/imgui/imgui_spectrum.hpp"
#include "w_engine.hpp"

namespace Wiesel {

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

  WIESEL_CHECK_VKRESULT(
      vkCreateDescriptorPool(Engine::GetRenderer()->logical_device_, &pool_info,
                             nullptr, &m_ImGuiPool));

  // 2: initialize imgui library

  //this initializes the core structures of imgui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  //ImGui::StyleColorsDark(&ImGui::GetStyle());
  ImGui::Spectrum::StyleColorsSpectrum();
  ImGui::Spectrum::LoadFont();

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  Engine::GetRenderer()->window_->ImGuiInit();

  //this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = Engine::GetRenderer()->instance_;
  init_info.PhysicalDevice = Engine::GetRenderer()->physical_device_;
  init_info.Device = Engine::GetRenderer()->logical_device_;
  init_info.Queue = Engine::GetRenderer()->graphics_queue_;
  init_info.DescriptorPool = m_ImGuiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.MSAASamples = Engine::GetRenderer()->msaa_samples_;
  init_info.RenderPass = Engine::GetRenderer()->composite_render_pass_->GetVulkanHandle();

  ImGui_ImplVulkan_Init(&init_info);
}

void ImGuiLayer::OnDetach() {
  LOG_DEBUG("Destroying imgui pool");
  vkDeviceWaitIdle(Engine::GetRenderer()->logical_device_);
  // Vulkan does this
  /*vkDestroyDescriptorPool(Engine::GetRenderer()->m_LogicalDevice, m_ImGuiPool, nullptr);*/
  ImGui_ImplVulkan_Shutdown();
}

void ImGuiLayer::OnUpdate(float_t deltaTime) {}

void ImGuiLayer::OnEvent(Event& event) {}

void ImGuiLayer::OnBeginPresent() {
  PROFILE_ZONE_SCOPED();
  ImGui_ImplVulkan_NewFrame();
  Engine::GetRenderer()->window_->ImGuiNewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
}

void ImGuiLayer::OnPresent() {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(
      ImGui::GetDrawData(),
      Engine::GetRenderer()->command_buffer_->handle_);
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    // move this to window handle!!
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

}  // namespace Wiesel
