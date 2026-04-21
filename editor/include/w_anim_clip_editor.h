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

#include <memory>
#include <string>

#include "animation/w_animation_clip_asset.h"
#include "asset/w_asset_handle.h"

namespace wiesel::editor {

// Editor panel for .wanimclip assets. Authors the clip's property curves
// universally via the reflection facade — any component field tagged
// WPROPERTY(Animatable) can be driven.
//
// Opens when double-clicking a .wanimclip in the asset browser.
class AnimClipEditor {
 public:
  void Open(AssetHandle handle, std::shared_ptr<AnimClipAssetData> data);
  void Close();

  bool IsOpen() const { return open_; }

  void Render();

 private:
  bool open_ = false;
  bool dirty_ = false;
  AssetHandle asset_handle_;
  std::shared_ptr<AnimClipAssetData> clip_;

  // -1 = none; otherwise index into clip_->property_curves.
  int selected_curve_ = -1;

  // "Add Curve" popup state.
  std::string new_curve_component_;
  std::string new_curve_field_;

  void RenderClipHeader();
  void RenderCurveList();
  void RenderSelectedCurve();
  void RenderAddCurvePopup();
  void Save();
};

}  // namespace wiesel::editor
