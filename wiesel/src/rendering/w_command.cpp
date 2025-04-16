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
      vkCreateCommandPool(Engine::GetRenderer()->GetLogicalDevice(), &poolInfo, nullptr, &m_Handle));
}

CommandPool::~CommandPool() {
  vkDestroyCommandPool(Engine::GetRenderer()->GetLogicalDevice(), m_Handle, nullptr);
}

Ref<CommandBuffer> CommandPool::CreateBuffer() {
  if (!m_FreeBuffers.empty()) {
    VkCommandBuffer buffer = m_FreeBuffers.front();
    m_FreeBuffers.pop_front();
    return CreateReference<CommandBuffer>(*this, buffer);
  }
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_Handle;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer buffer;
  WIESEL_CHECK_VKRESULT(
      vkAllocateCommandBuffers(Engine::GetRenderer()->GetLogicalDevice(), &allocInfo, &buffer));
  return CreateReference<CommandBuffer>(*this, buffer);
}

void CommandPool::ReturnBuffer(VkCommandBuffer buffer) {
  m_FreeBuffers.push_back(buffer);
}

CommandBuffer::CommandBuffer(CommandPool& pool, VkCommandBuffer commandBuffer) : m_Pool(pool), m_Handle(commandBuffer) {
}

CommandBuffer::~CommandBuffer() {
  m_Pool.ReturnBuffer(m_Handle);
}

void CommandBuffer::Begin() {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;                   // Optional
  beginInfo.pInheritanceInfo = nullptr;  // Optional

  WIESEL_CHECK_VKRESULT(vkBeginCommandBuffer(m_Handle, &beginInfo));
}

void CommandBuffer::End() {
  WIESEL_CHECK_VKRESULT(vkEndCommandBuffer(m_Handle));
}

void CommandBuffer::Reset() {
  vkResetCommandBuffer(m_Handle, 0);
}
}
