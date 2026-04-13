//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 15/04/2025.
//

#ifndef WIESEL_COMMAND_CONTEXT_HPP
#define WIESEL_COMMAND_CONTEXT_HPP

#include "util/w_utils.h"
#include "w_pch.h"

namespace wiesel {

class CommandBuffer;

class CommandPool {
 public:
  CommandPool();
  ~CommandPool();

  std::shared_ptr<CommandBuffer> CreateBuffer();

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
}  // namespace wiesel

#endif  //WIESEL_COMMAND_CONTEXT_HPP
