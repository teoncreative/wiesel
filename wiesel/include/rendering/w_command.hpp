//
// Created by Metehan Gezer on 15/04/2025.
//

#ifndef WIESEL_COMMAND_CONTEXT_HPP
#define WIESEL_COMMAND_CONTEXT_HPP

#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class CommandBuffer;

class CommandPool {
 public:
  CommandPool();
  ~CommandPool();

  Ref<CommandBuffer> CreateBuffer();

  VkCommandPool handle_{};
 private:
  friend class CommandBuffer;
  void ReturnBuffer(VkCommandBuffer buffer);

  std::list<VkCommandBuffer> free_buffers_;
};

class CommandBuffer {
 public:
  CommandBuffer(CommandPool& pool, VkCommandBuffer m_CommandBuffer);
  ~CommandBuffer();

  void Reset();
  void Begin();
  void End();

  VkCommandBuffer handle_;
 private:
  CommandPool& pool_;

};
}

#endif  //WIESEL_COMMAND_CONTEXT_HPP
