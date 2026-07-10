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
#include <set>
#include <string>

#include "animation/w_animation_clip_asset.h"
#include "asset/w_asset_handle.h"

namespace wiesel::editor {

// Which kind of timeline row is currently selected. Determines what fields
// the inline key editor / Curves / Keyframes tabs act on.
enum class AnimRowKind {
  None,
  PropertyCurve,   // source_index -> clip->property_curves[i]
  BonePosition,    // source_index -> clip->bone_channels[i].position_keys
  BoneRotation,    // source_index -> clip->bone_channels[i].rotation_keys
  BoneScale,       // source_index -> clip->bone_channels[i].scale_keys
};

struct AnimRowSelection {
  AnimRowKind kind = AnimRowKind::None;
  int source_index = -1;

  bool IsValid() const { return source_index >= 0 && kind != AnimRowKind::None; }
  bool IsProperty() const { return kind == AnimRowKind::PropertyCurve; }
  bool IsBone() const {
    return kind == AnimRowKind::BonePosition ||
           kind == AnimRowKind::BoneRotation ||
           kind == AnimRowKind::BoneScale;
  }
};

// Editor panel for .wanimclip assets. Authors the clip's property curves
// universally via the reflection facade - any component field tagged
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

  // Which row is currently selected in the unified timeline. Drives the
  // Curves/Keyframes tabs (they only populate when a property-curve row is
  // selected) and the inline key editor under the timeline.
  AnimRowSelection selected_row_;

  // Within the selected row's key array. -1 = no keyframe selected.
  int selected_key_ = -1;

  // Time cursor drawn on the timeline + curve views. Not bound to playback
  // yet; updated on ruler click.
  float playhead_time_ = 0.0f;

  // "Add Curve" popup state. `open_add_popup_next_` defers OpenPopup to the
  // outer window scope - calling OpenPopup inside the child list doesn't
  // match BeginPopupModal called later at the parent scope.
  std::string new_curve_component_;
  std::string new_curve_field_;
  bool open_add_popup_next_ = false;

  // Which top-level groups are expanded.
  bool group_bones_open_ = true;
  bool group_properties_open_ = true;

  // Which bone *groups* (by cleaned base name) have their Pos/Rot/Scale
  // sub-tracks expanded. Keying by name (not index) so the state survives
  // import-time re-ordering and aggregates the three assimp-pivot channels
  // ($AssimpFbx$_Translation / _Rotation / _Scaling) into one logical row.
  std::set<std::string> bone_expanded_;

  void RenderTitleBar();
  void RenderClipHeader();
  void RenderTimelineTab();
  void RenderCurvesTab();
  void RenderKeyframesTab();
  void RenderAddCurvePopup();
  void Save();
};

}  // namespace wiesel::editor
