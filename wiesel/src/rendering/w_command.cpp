//
// Created by Metehan Gezer on 15/04/2025.
//

#include "rendering/w_command.hpp"
#include "w_engine.hpp"
namespace Wiesel {

CommandPool::CommandPool() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = Engine::GetRenderer()->GetGraphicsQueueFamilyIndex();
  WIESEL_CHECK_VKRESULT(
      vkCreateCommandPool(Engine::GetRenderer()->GetLogicalDevice(), &poolInfo, nullptr, &handle_));
}

CommandPool::~CommandPool() {
  vkDestroyCommandPool(Engine::GetRenderer()->GetLogicalDevice(), handle_, nullptr);
}

Ref<CommandBuffer> CommandPool::CreateBuffer() {
  if (!free_buffers_.empty()) {
    VkCommandBuffer buffer = free_buffers_.front();
    free_buffers_.pop_front();
    return CreateReference<CommandBuffer>(*this, buffer);
  }
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = handle_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer buffer;
  WIESEL_CHECK_VKRESULT(
      vkAllocateCommandBuffers(Engine::GetRenderer()->GetLogicalDevice(), &allocInfo, &buffer));
  return CreateReference<CommandBuffer>(*this, buffer);
}

void CommandPool::ReturnBuffer(VkCommandBuffer buffer) {
  free_buffers_.push_back(buffer);
}

CommandBuffer::CommandBuffer(CommandPool& pool, VkCommandBuffer commandBuffer) : pool_(pool), handle_(commandBuffer) {
}

CommandBuffer::~CommandBuffer() {
  pool_.ReturnBuffer(handle_);
}

void CommandBuffer::Begin() {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;                   // Optional
  beginInfo.pInheritanceInfo = nullptr;  // Optional

  WIESEL_CHECK_VKRESULT(vkBeginCommandBuffer(handle_, &beginInfo));
}

void CommandBuffer::End() {
  WIESEL_CHECK_VKRESULT(vkEndCommandBuffer(handle_));
}

void CommandBuffer::Reset() {
  vkResetCommandBuffer(handle_, 0);
}
}
