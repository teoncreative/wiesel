//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "animation/w_animation_controller_asset.h"
#include "asset/w_asset_handle.h"
#include "w_anim_graph_delegate.h"
#include "w_node_graph_editor.h"

#include <memory>

namespace wiesel::editor {

// Editor panel for visually editing .wanimcontroller assets.
// Opens when double-clicking a controller asset in the asset browser.
class AnimControllerEditor {
 public:
  // Open the editor for the given controller asset.
  void Open(AssetHandle handle, std::shared_ptr<AnimControllerAssetData> data);
  void Close();

  bool IsOpen() const { return open_; }

  // Render the full panel (graph + properties sidebar + parameters).
  void Render();

 private:
  bool open_ = false;
  AssetHandle asset_handle_;
  std::shared_ptr<AnimControllerAssetData> controller_;
  std::unique_ptr<AnimGraphDelegate> delegate_;
  NodeGraphEditor graph_editor_;

  void RenderParametersPanel();
  void Save();
};

}  // namespace wiesel::editor
