//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_anim_clip_editor.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "asset/w_asset_registry.h"
#include "core/w_reflect_facade.h"
#include "w_engine.h"

namespace wiesel::editor {

namespace {

// Kinds of keyframe storage a PropertyCurve uses. Derived once per field
// from the field's reflected C++ type — the UI dispatches on this instead
// of hashing per frame.
enum class CurveValueKind {
  Unsupported,
  Float,
  Int,
  Bool,
  Vec2,
  Vec3,
  Vec4,
  Quat,
  AssetHandle,
};

CurveValueKind KindFromTypeId(uint32_t type_id) {
  if (type_id == reflect::TypeHashOf<float>()) {
    return CurveValueKind::Float;
  }
  if (type_id == reflect::TypeHashOf<int32_t>() ||
      type_id == reflect::TypeHashOf<int>()) {
    return CurveValueKind::Int;
  }
  if (type_id == reflect::TypeHashOf<bool>()) {
    return CurveValueKind::Bool;
  }
  if (type_id == reflect::TypeHashOf<glm::vec2>()) {
    return CurveValueKind::Vec2;
  }
  if (type_id == reflect::TypeHashOf<glm::vec3>()) {
    return CurveValueKind::Vec3;
  }
  if (type_id == reflect::TypeHashOf<glm::vec4>()) {
    return CurveValueKind::Vec4;
  }
  if (type_id == reflect::TypeHashOf<glm::quat>()) {
    return CurveValueKind::Quat;
  }
  if (type_id == reflect::TypeHashOf<wiesel::AssetHandle>()) {
    return CurveValueKind::AssetHandle;
  }
  return CurveValueKind::Unsupported;
}

CurveValueKind KindOf(const PropertyCurve& curve) {
  if (!curve.float_keys.empty()) {
    return CurveValueKind::Float;
  }
  if (!curve.vec2_keys.empty()) {
    return CurveValueKind::Vec2;
  }
  if (!curve.vec3_keys.empty()) {
    return CurveValueKind::Vec3;
  }
  if (!curve.vec4_keys.empty()) {
    return CurveValueKind::Vec4;
  }
  if (!curve.quat_keys.empty()) {
    return CurveValueKind::Quat;
  }
  if (!curve.int_keys.empty()) {
    return CurveValueKind::Int;
  }
  if (!curve.bool_keys.empty()) {
    return CurveValueKind::Bool;
  }
  if (!curve.asset_keys.empty()) {
    return CurveValueKind::AssetHandle;
  }
  return CurveValueKind::Unsupported;
}

const char* KindLabel(CurveValueKind k) {
  switch (k) {
    case CurveValueKind::Float:
      return "float";
    case CurveValueKind::Int:
      return "int";
    case CurveValueKind::Bool:
      return "bool";
    case CurveValueKind::Vec2:
      return "vec2";
    case CurveValueKind::Vec3:
      return "vec3";
    case CurveValueKind::Vec4:
      return "vec4";
    case CurveValueKind::Quat:
      return "quat";
    case CurveValueKind::AssetHandle:
      return "asset";
    default:
      return "?";
  }
}

// Resize the matching key vector to `size`, clearing the others.
void ResizeFor(PropertyCurve& curve, CurveValueKind kind, size_t size) {
  curve.float_keys.clear();
  curve.vec2_keys.clear();
  curve.vec3_keys.clear();
  curve.vec4_keys.clear();
  curve.quat_keys.clear();
  curve.int_keys.clear();
  curve.bool_keys.clear();
  curve.asset_keys.clear();
  switch (kind) {
    case CurveValueKind::Float:
      curve.float_keys.resize(size);
      break;
    case CurveValueKind::Int:
      curve.int_keys.resize(size);
      break;
    case CurveValueKind::Bool:
      curve.bool_keys.resize(size);
      break;
    case CurveValueKind::Vec2:
      curve.vec2_keys.resize(size);
      break;
    case CurveValueKind::Vec3:
      curve.vec3_keys.resize(size);
      break;
    case CurveValueKind::Vec4:
      curve.vec4_keys.resize(size);
      break;
    case CurveValueKind::Quat:
      curve.quat_keys.resize(size);
      break;
    case CurveValueKind::AssetHandle:
      curve.asset_keys.resize(size);
      break;
    default:
      break;
  }
}

// Count of keyframes regardless of which vector is populated.
size_t KeyCount(const PropertyCurve& curve) {
  return curve.float_keys.size() + curve.vec2_keys.size() +
         curve.vec3_keys.size() + curve.vec4_keys.size() +
         curve.quat_keys.size() + curve.int_keys.size() +
         curve.bool_keys.size() + curve.asset_keys.size();
}

// --- Per-kind key editors ---

template <typename Vec>
std::vector<AnimationKey<Vec>>& KeysFor(PropertyCurve& c);

template <>
std::vector<AnimationKey<float>>& KeysFor<float>(PropertyCurve& c) {
  return c.float_keys;
}
template <>
std::vector<AnimationKey<glm::vec2>>& KeysFor<glm::vec2>(PropertyCurve& c) {
  return c.vec2_keys;
}
template <>
std::vector<AnimationKey<glm::vec3>>& KeysFor<glm::vec3>(PropertyCurve& c) {
  return c.vec3_keys;
}
template <>
std::vector<AnimationKey<glm::vec4>>& KeysFor<glm::vec4>(PropertyCurve& c) {
  return c.vec4_keys;
}
template <>
std::vector<AnimationKey<glm::quat>>& KeysFor<glm::quat>(PropertyCurve& c) {
  return c.quat_keys;
}
template <>
std::vector<AnimationKey<int>>& KeysFor<int>(PropertyCurve& c) {
  return c.int_keys;
}
template <>
std::vector<AnimationKey<bool>>& KeysFor<bool>(PropertyCurve& c) {
  return c.bool_keys;
}
template <>
std::vector<AnimationKey<wiesel::AssetHandle>>& KeysFor<wiesel::AssetHandle>(
    PropertyCurve& c) {
  return c.asset_keys;
}

// Render a single keyframe's value editor for each supported type. Returns
// true if the value changed.
bool EditValue(const char* id, float& v) {
  return ImGui::DragFloat(id, &v, 0.01f);
}
bool EditValue(const char* id, int& v) {
  return ImGui::DragInt(id, &v, 1);
}
bool EditValue(const char* id, bool& v) {
  return ImGui::Checkbox(id, &v);
}
bool EditValue(const char* id, glm::vec2& v) {
  return ImGui::DragFloat2(id, &v.x, 0.01f);
}
bool EditValue(const char* id, glm::vec3& v) {
  return ImGui::DragFloat3(id, &v.x, 0.01f);
}
bool EditValue(const char* id, glm::vec4& v) {
  return ImGui::DragFloat4(id, &v.x, 0.01f);
}
bool EditValue(const char* id, glm::quat& v) {
  float arr[4] = {v.x, v.y, v.z, v.w};
  bool changed = ImGui::DragFloat4(id, arr, 0.01f);
  if (changed) {
    v = glm::quat(arr[3], arr[0], arr[1], arr[2]);
  }
  return changed;
}
bool EditValue(const char* id, wiesel::AssetHandle& v) {
  char buf[64] = {};
  std::string s = v.IsValid() ? v.ToString() : std::string{};
  std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
  bool changed = ImGui::InputText(id, buf, sizeof(buf));
  if (changed) {
    v = (buf[0] == '\0') ? wiesel::AssetHandle{}
                         : wiesel::AssetHandle::FromString(buf);
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("AssetHandle")) {
      v = *static_cast<const wiesel::AssetHandle*>(p->Data);
      changed = true;
    }
    ImGui::EndDragDropTarget();
  }
  return changed;
}

template <typename T>
bool RenderTypedKeyTable(PropertyCurve& curve) {
  auto& keys = KeysFor<T>(curve);
  bool changed = false;
  int to_remove = -1;

  if (ImGui::BeginTable("##Keys", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed,
                            32.0f);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < keys.size(); ++i) {
      ImGui::TableNextRow();
      ImGui::PushID(static_cast<int>(i));

      ImGui::TableSetColumnIndex(0);
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::DragFloat("##t", &keys[i].time, 0.01f, 0.0f, 0.0f, "%.3f")) {
        changed = true;
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (EditValue("##v", keys[i].value)) {
        changed = true;
      }

      ImGui::TableSetColumnIndex(2);
      if (ImGui::SmallButton("X")) {
        to_remove = static_cast<int>(i);
      }

      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  if (to_remove >= 0) {
    keys.erase(keys.begin() + to_remove);
    changed = true;
  }

  if (ImGui::Button("+ Add Keyframe")) {
    float last_time = keys.empty() ? 0.0f : keys.back().time;
    keys.push_back({last_time + 0.1f, T{}});
    changed = true;
  }

  // Sort by time on edit so the evaluator's linear scan stays correct.
  if (changed) {
    std::sort(keys.begin(), keys.end(),
              [](const auto& a, const auto& b) { return a.time < b.time; });
  }
  return changed;
}

bool RenderKeyTable(PropertyCurve& curve, CurveValueKind kind) {
  switch (kind) {
    case CurveValueKind::Float:
      return RenderTypedKeyTable<float>(curve);
    case CurveValueKind::Int:
      return RenderTypedKeyTable<int>(curve);
    case CurveValueKind::Bool:
      return RenderTypedKeyTable<bool>(curve);
    case CurveValueKind::Vec2:
      return RenderTypedKeyTable<glm::vec2>(curve);
    case CurveValueKind::Vec3:
      return RenderTypedKeyTable<glm::vec3>(curve);
    case CurveValueKind::Vec4:
      return RenderTypedKeyTable<glm::vec4>(curve);
    case CurveValueKind::Quat:
      return RenderTypedKeyTable<glm::quat>(curve);
    case CurveValueKind::AssetHandle:
      return RenderTypedKeyTable<wiesel::AssetHandle>(curve);
    default:
      ImGui::TextDisabled("Unsupported value type for keyframes");
      return false;
  }
}

}  // namespace

void AnimClipEditor::Open(AssetHandle handle,
                          std::shared_ptr<AnimClipAssetData> data) {
  asset_handle_ = handle;
  clip_ = std::move(data);
  selected_curve_ = clip_ && !clip_->property_curves.empty() ? 0 : -1;
  dirty_ = false;
  open_ = true;
}

void AnimClipEditor::Close() {
  open_ = false;
  clip_.reset();
  asset_handle_ = {};
  selected_curve_ = -1;
  dirty_ = false;
}

void AnimClipEditor::Render() {
  if (!open_ || !clip_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Animation Clip Editor", &open_)) {
    ImGui::End();
    if (!open_) {
      Close();
    }
    return;
  }

  // Toolbar
  if (ImGui::Button("Save")) {
    Save();
  }
  ImGui::SameLine();
  const auto* meta = Engine::asset_manager().GetMetadata(asset_handle_);
  if (meta) {
    ImGui::TextDisabled("%s%s", meta->name.c_str(), dirty_ ? " *" : "");
  }

  ImGui::Separator();
  RenderClipHeader();
  ImGui::Separator();

  float list_width = 260.0f;
  ImGui::BeginChild("##CurveList", ImVec2(list_width, 0), true);
  RenderCurveList();
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("##CurveEditor", ImVec2(0, 0), true);
  RenderSelectedCurve();
  ImGui::EndChild();

  RenderAddCurvePopup();

  ImGui::End();

  if (!open_) {
    Close();
  }
}

void AnimClipEditor::RenderClipHeader() {
  if (ImGui::DragFloat("Duration (s)", &clip_->duration, 0.01f, 0.0f, 0.0f,
                       "%.3f")) {
    dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::DragFloat("Ticks/sec", &clip_->ticks_per_second, 0.1f, 0.0f,
                       0.0f, "%.2f")) {
    dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("Loop", &clip_->loop)) {
    dirty_ = true;
  }
  if (clip_->HasBoneChannels()) {
    ImGui::TextDisabled("Bone channels: %zu (read-only here)",
                        clip_->bone_channels.size());
  }
}

void AnimClipEditor::RenderCurveList() {
  ImGui::Text("Property Curves (%zu)", clip_->property_curves.size());
  ImGui::Separator();

  int to_remove = -1;
  for (size_t i = 0; i < clip_->property_curves.size(); ++i) {
    const auto& c = clip_->property_curves[i];
    ImGui::PushID(static_cast<int>(i));
    std::string label = c.target_component + "." + c.target_field;
    if (label.size() == 1) {
      label = "<empty>";
    }
    bool selected = (selected_curve_ == static_cast<int>(i));
    if (ImGui::Selectable(label.c_str(), selected)) {
      selected_curve_ = static_cast<int>(i);
    }
    if (ImGui::BeginPopupContextItem("curve_ctx")) {
      if (ImGui::MenuItem("Remove")) {
        to_remove = static_cast<int>(i);
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }

  if (to_remove >= 0) {
    clip_->property_curves.erase(clip_->property_curves.begin() + to_remove);
    if (selected_curve_ >= static_cast<int>(clip_->property_curves.size())) {
      selected_curve_ =
          clip_->property_curves.empty()
              ? -1
              : static_cast<int>(clip_->property_curves.size()) - 1;
    }
    dirty_ = true;
  }

  ImGui::Separator();
  if (ImGui::Button("+ Add Curve", ImVec2(-FLT_MIN, 0))) {
    new_curve_component_.clear();
    new_curve_field_.clear();
    ImGui::OpenPopup("AddCurve");
  }
}

void AnimClipEditor::RenderSelectedCurve() {
  if (selected_curve_ < 0 ||
      selected_curve_ >= static_cast<int>(clip_->property_curves.size())) {
    ImGui::TextDisabled(
        "Select a curve on the left, or click + Add Curve to create one.");
    return;
  }

  PropertyCurve& curve = clip_->property_curves[selected_curve_];

  ImGui::Text("Target: %s.%s", curve.target_component.c_str(),
              curve.target_field.c_str());
  CurveValueKind kind = KindOf(curve);
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", KindLabel(kind));

  const char* interp_items[] = {"Linear", "Step"};
  int interp_idx = curve.interp == CurveInterp::Step ? 1 : 0;
  if (ImGui::Combo("Interpolation", &interp_idx, interp_items, 2)) {
    curve.interp = interp_idx == 1 ? CurveInterp::Step : CurveInterp::Linear;
    dirty_ = true;
  }

  ImGui::Separator();
  if (RenderKeyTable(curve, kind)) {
    dirty_ = true;
  }
}

void AnimClipEditor::RenderAddCurvePopup() {
  if (!ImGui::BeginPopupModal("AddCurve", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  // Component picker — any reflected type with at least one Animatable field.
  if (ImGui::BeginCombo("Component", new_curve_component_.empty()
                                         ? "<select>"
                                         : new_curve_component_.c_str())) {
    reflect::ForEachType([&](reflect::TypeHandle t) {
      bool has_animatable = false;
      reflect::ForEachField(
          t, reflect::AttrFlag::Animatable,
          [&](reflect::FieldHandle) { has_animatable = true; });
      if (!has_animatable) {
        return;
      }
      std::string name{t.Name()};
      bool selected = (name == new_curve_component_);
      if (ImGui::Selectable(name.c_str(), selected)) {
        new_curve_component_ = name;
        new_curve_field_.clear();
      }
    });
    ImGui::EndCombo();
  }

  // Field picker — only Animatable fields with a supported keyframe type.
  reflect::TypeHandle component_type = reflect::FindType(new_curve_component_);
  if (component_type) {
    if (ImGui::BeginCombo("Field", new_curve_field_.empty()
                                       ? "<select>"
                                       : new_curve_field_.c_str())) {
      reflect::ForEachField(
          component_type, reflect::AttrFlag::Animatable,
          [&](reflect::FieldHandle f) {
            CurveValueKind k = KindFromTypeId(f.TypeId());
            if (k == CurveValueKind::Unsupported) {
              return;
            }
            std::string fname{f.Name()};
            std::string label = fname + "  (" + KindLabel(k) + ")";
            bool selected = (fname == new_curve_field_);
            if (ImGui::Selectable(label.c_str(), selected)) {
              new_curve_field_ = fname;
            }
          });
      ImGui::EndCombo();
    }
  } else {
    ImGui::TextDisabled("Pick a component first");
  }

  ImGui::Separator();

  bool can_add = component_type && !new_curve_field_.empty();
  if (!can_add) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Add")) {
    reflect::FieldHandle field =
        reflect::FindField(component_type, new_curve_field_);
    CurveValueKind kind = KindFromTypeId(field.TypeId());
    PropertyCurve c;
    c.target_component = new_curve_component_;
    c.target_field = new_curve_field_;
    c.interp = (kind == CurveValueKind::Bool || kind == CurveValueKind::Int ||
                kind == CurveValueKind::AssetHandle)
                   ? CurveInterp::Step
                   : CurveInterp::Linear;
    ResizeFor(c, kind, 0);
    clip_->property_curves.push_back(std::move(c));
    selected_curve_ = static_cast<int>(clip_->property_curves.size()) - 1;
    dirty_ = true;
    ImGui::CloseCurrentPopup();
  }
  if (!can_add) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void AnimClipEditor::Save() {
  if (!asset_handle_.IsValid() || !clip_) {
    return;
  }
  Engine::asset_manager().Store(asset_handle_, clip_);
  AssetRegistry::Save(asset_handle_);
  dirty_ = false;
}

}  // namespace wiesel::editor
