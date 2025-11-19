//
// Created by Metehan Gezer on 09/10/2025.
//

#ifndef W_LAYERSCENE_H
#define W_LAYERSCENE_H


#include "layer/w_layer.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class SceneLayer : public Layer {
public:
  SceneLayer(std::shared_ptr<Scene> scene);
  ~SceneLayer() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float_t delta_time) override;
  void OnEvent(Event& event) override;

  void OnPrePresent() override;
  void OnBeginPresent() override;
  void OnPresent() override;
  void OnPostPresent() override;
private:
  std::shared_ptr<Scene> scene_;

};
}  // namespace Wiesel

#endif  //W_LAYERSCENE_H
