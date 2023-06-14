
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

#include "layer/w_layer.hpp"
#include "util/imgui/w_imguiutil.hpp"
#include "w_pch.hpp"

namespace Wiesel {
  class ImGuiLayer : public Layer {
  public:
    ImGuiLayer();
    ~ImGuiLayer() override;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float_t deltaTime) override;
    void OnEvent(Event& event) override;

    void OnImGuiRender() override;
    void OnBeginFrame();
    void OnEndFrame();

  private:
    VkDescriptorPool m_ImGuiPool;
  };
}