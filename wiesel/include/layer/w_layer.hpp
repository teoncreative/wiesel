
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

#include "events/w_events.hpp"
#include "rendering/w_renderer.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class Layer {
 public:
  Layer(const std::string& layerName);
  virtual ~Layer();

  virtual void OnAttach() = 0;
  virtual void OnDetach() = 0;
  virtual void OnUpdate(float_t deltaTime) = 0;
  virtual void OnEvent(Event& event) = 0;

  virtual void OnImGuiRender();
  virtual void OnPostRender();

 protected:
  friend class Application;

  std::string m_LayerName;  // used to debug
  uint32_t m_Id;
};
}  // namespace Wiesel