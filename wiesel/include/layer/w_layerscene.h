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
// Created by Metehan Gezer on 09/10/2025.
//

#ifndef W_LAYERSCENE_H
#define W_LAYERSCENE_H

#include "layer/w_layer.h"
#include "w_pch.h"

namespace Wiesel {
class SceneLayer : public Layer {
 public:
  SceneLayer();
  ~SceneLayer() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float_t delta_time) override;
  void OnEvent(Event& event) override;

  void OnPrePresent() override;
  void OnBeginPresent() override;
  void OnPresent() override;
  void OnPostPresent() override;
};
}  // namespace Wiesel

#endif  //W_LAYERSCENE_H