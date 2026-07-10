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
#include <imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "asset/w_asset_registry.h"
#include "core/w_reflect_facade.h"
#include "ui/w_ui_button.h"
#include "ui/w_ui_chip.h"
#include "ui/w_ui_draw.h"
#include "ui/w_ui_field.h"
#include "ui/w_ui_layout.h"
#include "ui/w_ui_row.h"
#include "ui/w_ui_section.h"
#include "ui/w_ui_style.h"
#include "util/imgui/imgui_lucide.h"
#include "w_engine.h"

namespace wiesel::editor {

namespace {

// Kinds of keyframe storage a PropertyCurve uses. Derived once per field
// from the field's reflected C++ type - the UI dispatches on this instead
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

// Prefer resolving through reflection on target_component.target_field so
// empty (just-added) curves still report the correct kind. Fall back to
// whichever key vector is populated if the field can't be resolved (e.g.
// the component was removed from the codebase since the clip was saved).
CurveValueKind KindOf(const PropertyCurve& curve) {
  reflect::TypeHandle type = reflect::FindType(curve.target_component);
  if (type) {
    reflect::FieldHandle field = reflect::FindField(type, curve.target_field);
    if (field) {
      CurveValueKind k = KindFromTypeId(field.TypeId());
      if (k != CurveValueKind::Unsupported) {
        return k;
      }
    }
  }
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

// ---- Curve key-vector dispatch ------------------------------------------

// Call `f(keys)` with a reference to the matching typed key vector. Lets
// generic code (timeline, curves graph) work against PropertyCurve without
// hard-coding per-type branches at every use site.
template <typename F>
void VisitKeys(PropertyCurve& c, CurveValueKind kind, F&& f) {
  switch (kind) {
    case CurveValueKind::Float:
      f(c.float_keys);
      break;
    case CurveValueKind::Int:
      f(c.int_keys);
      break;
    case CurveValueKind::Bool:
      f(c.bool_keys);
      break;
    case CurveValueKind::Vec2:
      f(c.vec2_keys);
      break;
    case CurveValueKind::Vec3:
      f(c.vec3_keys);
      break;
    case CurveValueKind::Vec4:
      f(c.vec4_keys);
      break;
    case CurveValueKind::Quat:
      f(c.quat_keys);
      break;
    case CurveValueKind::AssetHandle:
      f(c.asset_keys);
      break;
    default:
      break;
  }
}

size_t KeyCountOf(const PropertyCurve& c, CurveValueKind kind) {
  switch (kind) {
    case CurveValueKind::Float:
      return c.float_keys.size();
    case CurveValueKind::Int:
      return c.int_keys.size();
    case CurveValueKind::Bool:
      return c.bool_keys.size();
    case CurveValueKind::Vec2:
      return c.vec2_keys.size();
    case CurveValueKind::Vec3:
      return c.vec3_keys.size();
    case CurveValueKind::Vec4:
      return c.vec4_keys.size();
    case CurveValueKind::Quat:
      return c.quat_keys.size();
    case CurveValueKind::AssetHandle:
      return c.asset_keys.size();
    default:
      return 0;
  }
}

float KeyTimeAt(const PropertyCurve& c, CurveValueKind kind, size_t idx) {
  switch (kind) {
    case CurveValueKind::Float:
      return c.float_keys[idx].time;
    case CurveValueKind::Int:
      return c.int_keys[idx].time;
    case CurveValueKind::Bool:
      return c.bool_keys[idx].time;
    case CurveValueKind::Vec2:
      return c.vec2_keys[idx].time;
    case CurveValueKind::Vec3:
      return c.vec3_keys[idx].time;
    case CurveValueKind::Vec4:
      return c.vec4_keys[idx].time;
    case CurveValueKind::Quat:
      return c.quat_keys[idx].time;
    case CurveValueKind::AssetHandle:
      return c.asset_keys[idx].time;
    default:
      return 0.0f;
  }
}

void SetKeyTime(PropertyCurve& c, CurveValueKind kind, size_t idx, float t) {
  switch (kind) {
    case CurveValueKind::Float:
      c.float_keys[idx].time = t;
      break;
    case CurveValueKind::Int:
      c.int_keys[idx].time = t;
      break;
    case CurveValueKind::Bool:
      c.bool_keys[idx].time = t;
      break;
    case CurveValueKind::Vec2:
      c.vec2_keys[idx].time = t;
      break;
    case CurveValueKind::Vec3:
      c.vec3_keys[idx].time = t;
      break;
    case CurveValueKind::Vec4:
      c.vec4_keys[idx].time = t;
      break;
    case CurveValueKind::Quat:
      c.quat_keys[idx].time = t;
      break;
    case CurveValueKind::AssetHandle:
      c.asset_keys[idx].time = t;
      break;
    default:
      break;
  }
}

int InsertKeyframeAt(PropertyCurve& c, CurveValueKind kind, float t) {
  int new_idx = -1;
  VisitKeys(c, kind, [&](auto& keys) {
    using KeyT = typename std::remove_reference_t<decltype(keys)>::value_type;
    KeyT k{};
    k.time = t;
    keys.push_back(k);
    std::sort(keys.begin(), keys.end(),
              [](const auto& a, const auto& b) { return a.time < b.time; });
    for (size_t i = 0; i < keys.size(); ++i) {
      if (keys[i].time == t) {
        new_idx = static_cast<int>(i);
        break;
      }
    }
  });
  return new_idx;
}

std::string CurveLabel(const PropertyCurve& c) {
  if (c.target_component.empty() && c.target_field.empty()) {
    return "<empty>";
  }
  return c.target_component + "." + c.target_field;
}

// ---- Timeline track widget ----------------------------------------------

namespace style_ns = wiesel::editor::ui::style;

constexpr float kTimelineRulerHeight = 22.0f;
constexpr float kTimelineTrackHeight = 32.0f;
constexpr float kKeyHalfSize = 5.0f;

struct TimelineResult {
  bool changed = false;
  bool key_added = false;
  int new_key_index = -1;
};

// Horizontal timeline: ruler on top, single track below with each keyframe
// drawn as a diamond. Click ruler -> playhead. Click track empty area ->
// insert keyframe. Click diamond -> select. Drag diamond -> move in time.
TimelineResult RenderTimelineTrack(PropertyCurve& curve, CurveValueKind kind,
                                   float duration, int* selected_key,
                                   float* playhead) {
  TimelineResult result;

  const float avail_w = ImMax(120.0f, ImGui::GetContentRegionAvail().x);
  const float total_h = kTimelineRulerHeight + kTimelineTrackHeight;

  const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
  const ImVec2 canvas_max(canvas_min.x + avail_w, canvas_min.y + total_h);
  const float ruler_bottom_y = canvas_min.y + kTimelineRulerHeight;
  const float track_mid_y = ruler_bottom_y + kTimelineTrackHeight * 0.5f;

  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(canvas_min, canvas_max, style_ns::kDrawerBg);
  dl->AddRect(canvas_min, canvas_max, ImGui::GetColorU32(ImGuiCol_Border));
  dl->AddLine(ImVec2(canvas_min.x, ruler_bottom_y),
              ImVec2(canvas_max.x, ruler_bottom_y),
              ImGui::GetColorU32(ImGuiCol_Border));

  const float eff_duration = ImMax(duration, 0.001f);
  const float px_per_sec = avail_w / eff_duration;
  auto time_to_x = [&](float t) { return canvas_min.x + t * px_per_sec; };
  auto x_to_time = [&](float x) { return (x - canvas_min.x) / px_per_sec; };

  // Nice-number ruler ticks: pick the first step that yields <=8 labels.
  auto pick_step = [](float total) {
    const float target = total / 8.0f;
    const float steps[] = {0.01f, 0.02f, 0.05f, 0.1f,  0.2f,
                           0.25f, 0.5f,  1.0f,  2.0f,  5.0f,
                           10.0f};
    for (float s : steps) {
      if (s >= target) {
        return s;
      }
    }
    return 10.0f;
  };
  const float tick_step = pick_step(eff_duration);
  const ImU32 tick_col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const ImU32 label_col = ImGui::GetColorU32(ImGuiCol_Text);
  for (float t = 0.0f; t <= eff_duration + 1e-4f; t += tick_step) {
    const float x = time_to_x(t);
    dl->AddLine(ImVec2(x, ruler_bottom_y - 6.0f),
                ImVec2(x, ruler_bottom_y - 1.0f), tick_col);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.2fs", t);
    dl->AddText(ImVec2(x + 2.0f, canvas_min.y + 2.0f), label_col, buf);
    dl->AddLine(ImVec2(x, ruler_bottom_y), ImVec2(x, canvas_max.y),
                IM_COL32(255, 255, 255, 14));
  }

  // Area click handler (ruler / empty track). The keyframe buttons sit on
  // top; SetNextItemAllowOverlap tells ImGui the area button may be
  // overlapped by later items so those later items can win the hit test.
  ImGui::SetCursorScreenPos(canvas_min);
  ImGui::SetNextItemAllowOverlap();
  ImGui::InvisibleButton("##timeline_area", ImVec2(avail_w, total_h));
  const bool area_hovered = ImGui::IsItemHovered();
  const bool area_clicked = ImGui::IsItemClicked();

  int hovered_key = -1;
  int pressed_key = -1;

  const size_t key_count = KeyCountOf(curve, kind);
  for (size_t i = 0; i < key_count; ++i) {
    const float t = KeyTimeAt(curve, kind, i);
    const float x = time_to_x(t);
    const ImVec2 btn_min(x - kKeyHalfSize - 1.0f,
                         track_mid_y - kKeyHalfSize - 1.0f);
    const ImVec2 btn_size((kKeyHalfSize + 1.0f) * 2.0f,
                          (kKeyHalfSize + 1.0f) * 2.0f);

    ImGui::SetCursorScreenPos(btn_min);
    ImGui::PushID(static_cast<int>(i));
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##key", btn_size);
    const bool hv = ImGui::IsItemHovered();
    const bool act = ImGui::IsItemActive();
    const bool clk = ImGui::IsItemClicked();
    ImGui::PopID();

    if (hv) {
      hovered_key = static_cast<int>(i);
    }
    if (clk) {
      pressed_key = static_cast<int>(i);
    }
    if (act && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
      const float delta = ImGui::GetIO().MouseDelta.x / px_per_sec;
      const float nt = ImClamp(t + delta, 0.0f, eff_duration);
      if (nt != t) {
        SetKeyTime(curve, kind, i, nt);
        result.changed = true;
      }
    }

    const bool selected = (*selected_key == static_cast<int>(i));
    const ImU32 fill =
        selected ? ImGui::GetColorU32(ImGuiCol_CheckMark)
                 : (hv ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                       : IM_COL32(0xd8, 0xaa, 0x3a, 0xff));
    const ImU32 outline = ImGui::GetColorU32(ImGuiCol_Border);
    const ImVec2 c{x, track_mid_y};
    const ImVec2 top{c.x, c.y - kKeyHalfSize};
    const ImVec2 right{c.x + kKeyHalfSize, c.y};
    const ImVec2 bot{c.x, c.y + kKeyHalfSize};
    const ImVec2 left{c.x - kKeyHalfSize, c.y};
    dl->AddQuadFilled(top, right, bot, left, fill);
    dl->AddQuad(top, right, bot, left, outline, 1.0f);
  }

  if (pressed_key >= 0) {
    *selected_key = pressed_key;
  } else if (area_clicked && hovered_key < 0 && area_hovered) {
    const ImVec2 mp = ImGui::GetIO().MousePos;
    if (mp.y >= ruler_bottom_y && mp.y <= canvas_max.y) {
      const float t = ImClamp(x_to_time(mp.x), 0.0f, eff_duration);
      const int new_idx = InsertKeyframeAt(curve, kind, t);
      result.key_added = true;
      result.new_key_index = new_idx;
      *selected_key = new_idx;
    } else if (mp.y >= canvas_min.y && mp.y < ruler_bottom_y) {
      *playhead = ImClamp(x_to_time(mp.x), 0.0f, eff_duration);
    }
  }

  if (*playhead >= 0.0f && *playhead <= eff_duration) {
    const float x = time_to_x(*playhead);
    dl->AddLine(ImVec2(x, canvas_min.y), ImVec2(x, canvas_max.y),
                IM_COL32(0xff, 0x6e, 0x40, 0xff), 1.5f);
  }

  if (hovered_key >= 0) {
    ImGui::SetTooltip("Key %d  @ %.3fs", hovered_key,
                      KeyTimeAt(curve, kind, hovered_key));
  }

  // Leave the cursor past the canvas so the next widget lays out below.
  ImGui::SetCursorScreenPos(ImVec2(canvas_min.x, canvas_max.y));
  ImGui::Dummy(ImVec2(avail_w, 0.0f));

  return result;
}

// ---- Curves graph -------------------------------------------------------

constexpr float kCurvesGraphHeight = 200.0f;

ImU32 ComponentColor(int c) {
  switch (c) {
    case 0:
      return IM_COL32(0xe6, 0x3f, 0x3f, 0xff);
    case 1:
      return IM_COL32(0x45, 0xc4, 0x55, 0xff);
    case 2:
      return IM_COL32(0x4e, 0x92, 0xff, 0xff);
    case 3:
      return IM_COL32(0xd8, 0xaa, 0x3a, 0xff);
    default:
      return IM_COL32(0xee, 0xee, 0xee, 0xff);
  }
}

// Component count (1..4) for each numeric kind. 0 signals "not graphable".
int ComponentCount(CurveValueKind kind) {
  switch (kind) {
    case CurveValueKind::Float:
    case CurveValueKind::Int:
      return 1;
    case CurveValueKind::Vec2:
      return 2;
    case CurveValueKind::Vec3:
      return 3;
    case CurveValueKind::Vec4:
    case CurveValueKind::Quat:
      return 4;
    default:
      return 0;
  }
}

// Extract one component of a key's value as float. Only valid for numeric
// kinds with ci < ComponentCount(kind).
float GetComponentFloat(const PropertyCurve& c, CurveValueKind kind,
                        size_t idx, int ci) {
  switch (kind) {
    case CurveValueKind::Float:
      return c.float_keys[idx].value;
    case CurveValueKind::Int:
      return static_cast<float>(c.int_keys[idx].value);
    case CurveValueKind::Vec2:
      return c.vec2_keys[idx].value[ci];
    case CurveValueKind::Vec3:
      return c.vec3_keys[idx].value[ci];
    case CurveValueKind::Vec4:
      return c.vec4_keys[idx].value[ci];
    case CurveValueKind::Quat: {
      const glm::quat& q = c.quat_keys[idx].value;
      if (ci == 0) {
        return q.x;
      }
      if (ci == 1) {
        return q.y;
      }
      if (ci == 2) {
        return q.z;
      }
      return q.w;
    }
    default:
      return 0.0f;
  }
}

// Write back one component. Int rounds; quat updates the named field.
void SetComponentFloat(PropertyCurve& c, CurveValueKind kind, size_t idx,
                       int ci, float v) {
  switch (kind) {
    case CurveValueKind::Float:
      c.float_keys[idx].value = v;
      break;
    case CurveValueKind::Int:
      c.int_keys[idx].value = static_cast<int>(std::round(v));
      break;
    case CurveValueKind::Vec2:
      c.vec2_keys[idx].value[ci] = v;
      break;
    case CurveValueKind::Vec3:
      c.vec3_keys[idx].value[ci] = v;
      break;
    case CurveValueKind::Vec4:
      c.vec4_keys[idx].value[ci] = v;
      break;
    case CurveValueKind::Quat: {
      glm::quat& q = c.quat_keys[idx].value;
      if (ci == 0) {
        q.x = v;
      } else if (ci == 1) {
        q.y = v;
      } else if (ci == 2) {
        q.z = v;
      } else {
        q.w = v;
      }
      break;
    }
    default:
      break;
  }
}

// Value-over-time graph for numeric curve kinds. Step interp draws as
// horizontal + vertical segments; linear as straight lines. Multi-component
// curves render one trace per component (x/y/z/w). Each control point is
// draggable - X drag moves the key's time (shared across components), Y
// drag moves only the dragged component's value. Returns true if curve
// was mutated.
bool RenderCurveGraph(PropertyCurve& c, CurveValueKind kind, float duration,
                      float playhead) {
  const float avail_w = ImMax(120.0f, ImGui::GetContentRegionAvail().x);
  const ImVec2 lo = ImGui::GetCursorScreenPos();
  const ImVec2 hi(lo.x + avail_w, lo.y + kCurvesGraphHeight);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(lo, hi, style_ns::kDrawerBg);
  dl->AddRect(lo, hi, ImGui::GetColorU32(ImGuiCol_Border));

  const int comp_count = ComponentCount(kind);
  const size_t key_count = KeyCountOf(c, kind);
  if (comp_count == 0) {
    dl->AddText(ImVec2(lo.x + 10.0f, lo.y + 10.0f),
                ImGui::GetColorU32(ImGuiCol_TextDisabled),
                "No graph for this type - use the Keyframes tab.");
    ImGui::Dummy(ImVec2(avail_w, kCurvesGraphHeight));
    return false;
  }

  // Collect (time, value) pairs per component for range computation + line
  // drawing. Held as vector<vector<pair>> indexed by (ci, key_i).
  std::vector<std::vector<std::pair<float, float>>> series(comp_count);
  for (size_t i = 0; i < key_count; ++i) {
    const float t = KeyTimeAt(c, kind, i);
    for (int ci = 0; ci < comp_count; ++ci) {
      series[ci].emplace_back(t, GetComponentFloat(c, kind, i, ci));
    }
  }

  float y_min = +FLT_MAX;
  float y_max = -FLT_MAX;
  for (auto& s : series) {
    for (auto& p : s) {
      y_min = ImMin(y_min, p.second);
      y_max = ImMax(y_max, p.second);
    }
  }
  if (y_min == FLT_MAX) {
    y_min = -1.0f;
    y_max = 1.0f;
  }
  if (y_max - y_min < 1e-4f) {
    y_min -= 0.5f;
    y_max += 0.5f;
  }
  const float y_pad = (y_max - y_min) * 0.1f;
  y_min -= y_pad;
  y_max += y_pad;

  const float eff = ImMax(duration, 0.001f);
  const float w = hi.x - lo.x;
  const float h = hi.y - lo.y;
  const float px_per_sec = w / eff;
  const float px_per_val = h / (y_max - y_min);

  auto time_to_x = [&](float t) { return lo.x + t * px_per_sec; };
  auto value_to_y = [&](float v) {
    return hi.y - (v - y_min) * px_per_val;
  };

  if (y_min < 0.0f && y_max > 0.0f) {
    const float zy = value_to_y(0.0f);
    dl->AddLine(ImVec2(lo.x, zy), ImVec2(hi.x, zy),
                IM_COL32(255, 255, 255, 30), 1.0f);
  }

  const bool step = (c.interp == CurveInterp::Step);
  for (int ci = 0; ci < comp_count; ++ci) {
    const auto& pts = series[ci];
    const ImU32 col = ComponentColor(ci);
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
      const ImVec2 a(time_to_x(pts[i].first), value_to_y(pts[i].second));
      const ImVec2 b(time_to_x(pts[i + 1].first),
                     value_to_y(pts[i + 1].second));
      if (step) {
        const ImVec2 mid(b.x, a.y);
        dl->AddLine(a, mid, col, 1.5f);
        dl->AddLine(mid, b, col, 1.5f);
      } else {
        dl->AddLine(a, b, col, 1.5f);
      }
    }
  }

  // Interactive control points. One InvisibleButton per (key, component).
  // Drag X -> move shared key time; drag Y -> move only this component's
  // value. We apply edits directly to the curve.
  bool changed = false;
  float target_time_after_drag = -1.0f;
  for (size_t i = 0; i < key_count; ++i) {
    const float t = KeyTimeAt(c, kind, i);
    for (int ci = 0; ci < comp_count; ++ci) {
      const float v = GetComponentFloat(c, kind, i, ci);
      const float cx = time_to_x(t);
      const float cy = value_to_y(v);
      const float hs = 5.0f;
      const ImVec2 bmin(cx - hs - 1.0f, cy - hs - 1.0f);
      const ImVec2 bsz((hs + 1.0f) * 2.0f, (hs + 1.0f) * 2.0f);

      ImGui::SetCursorScreenPos(bmin);
      ImGui::PushID(static_cast<int>(i * 4 + ci));
      ImGui::SetNextItemAllowOverlap();
      ImGui::InvisibleButton("##cp", bsz);
      const bool hv = ImGui::IsItemHovered();
      const bool act = ImGui::IsItemActive();
      ImGui::PopID();

      if (act && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        const ImVec2 md = ImGui::GetIO().MouseDelta;
        const float dt = md.x / px_per_sec;
        const float dv = -md.y / px_per_val;  // screen-y is inverted
        if (dt != 0.0f) {
          const float nt = ImClamp(t + dt, 0.0f, eff);
          SetKeyTime(c, kind, i, nt);
          changed = true;
          target_time_after_drag = nt;
        }
        if (dv != 0.0f) {
          SetComponentFloat(c, kind, i, ci, v + dv);
          changed = true;
        }
      }

      const ImU32 outline = ImGui::GetColorU32(ImGuiCol_Border);
      const ImU32 fill =
          hv ? ImGui::GetColorU32(ImGuiCol_Text) : ComponentColor(ci);
      dl->AddCircleFilled(ImVec2(cx, cy), hs, fill);
      dl->AddCircle(ImVec2(cx, cy), hs, outline, 0, 1.0f);
      if (hv) {
        ImGui::SetTooltip("Key %zu  component %d\n  t=%.3fs  v=%.3f", i, ci,
                          t, v);
      }
    }
  }

  // Re-sort keys if a drag changed times.
  if (changed && target_time_after_drag >= 0.0f) {
    VisitKeys(c, kind, [](auto& keys) {
      std::sort(keys.begin(), keys.end(),
                [](const auto& a, const auto& b) { return a.time < b.time; });
    });
  }

  if (playhead >= 0.0f && playhead <= eff) {
    const float x = time_to_x(playhead);
    dl->AddLine(ImVec2(x, lo.y), ImVec2(x, hi.y),
                IM_COL32(0xff, 0x6e, 0x40, 0xff), 1.5f);
  }

  // Reserve layout space + push cursor past the graph.
  ImGui::SetCursorScreenPos(ImVec2(lo.x, hi.y));
  ImGui::Dummy(ImVec2(avail_w, 0.0f));

  return changed;
}

// ---- Track-view abstraction ---------------------------------------------

// Lightweight per-frame handle over a keyframe time list. Lets the timeline
// renderer operate uniformly on property curves and bone sub-tracks without
// knowing about the underlying types.
struct TrackView {
  size_t count = 0;
  std::function<float(size_t)> GetTime;
  std::function<void(size_t, float)> SetTime;
  // Returns new index, or -1 if insert unsupported / duplicate.
  std::function<int(float)> InsertAt;
  std::function<void(size_t)> EraseAt;
  // Sort keys by time. Called after a time-drag edit.
  std::function<void()> Sort;
};

TrackView MakePropertyTrack(PropertyCurve& curve, CurveValueKind kind) {
  TrackView v;
  v.count = KeyCountOf(curve, kind);
  v.GetTime = [c = &curve, k = kind](size_t i) {
    return KeyTimeAt(*c, k, i);
  };
  v.SetTime = [c = &curve, k = kind](size_t i, float t) {
    SetKeyTime(*c, k, i, t);
  };
  v.InsertAt = [c = &curve, k = kind](float t) {
    return InsertKeyframeAt(*c, k, t);
  };
  v.EraseAt = [c = &curve, k = kind](size_t i) {
    VisitKeys(*c, k, [&](auto& keys) { keys.erase(keys.begin() + i); });
  };
  v.Sort = [c = &curve, k = kind]() {
    VisitKeys(*c, k, [](auto& keys) {
      std::sort(keys.begin(), keys.end(),
                [](const auto& a, const auto& b) { return a.time < b.time; });
    });
  };
  return v;
}

// Bone sub-tracks. Each bone channel has three separate key vectors; this
// picks one by AnimRowKind. Insert uses a zeroed default value.
TrackView MakeBoneTrack(AnimationChannel& channel, AnimRowKind sub) {
  TrackView v;
  switch (sub) {
    case AnimRowKind::BonePosition: {
      auto& keys = channel.position_keys;
      v.count = keys.size();
      v.GetTime = [k = &keys](size_t i) { return (*k)[i].time; };
      v.SetTime = [k = &keys](size_t i, float t) { (*k)[i].time = t; };
      v.InsertAt = [k = &keys](float t) {
        k->push_back({t, glm::vec3(0.0f)});
        std::sort(k->begin(), k->end(),
                  [](const auto& a, const auto& b) { return a.time < b.time; });
        for (size_t i = 0; i < k->size(); ++i) {
          if ((*k)[i].time == t) {
            return static_cast<int>(i);
          }
        }
        return -1;
      };
      v.EraseAt = [k = &keys](size_t i) { k->erase(k->begin() + i); };
      v.Sort = [k = &keys]() {
        std::sort(k->begin(), k->end(),
                  [](const auto& a, const auto& b) { return a.time < b.time; });
      };
      break;
    }
    case AnimRowKind::BoneRotation: {
      auto& keys = channel.rotation_keys;
      v.count = keys.size();
      v.GetTime = [k = &keys](size_t i) { return (*k)[i].time; };
      v.SetTime = [k = &keys](size_t i, float t) { (*k)[i].time = t; };
      v.InsertAt = [k = &keys](float t) {
        k->push_back({t, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});
        std::sort(k->begin(), k->end(),
                  [](const auto& a, const auto& b) { return a.time < b.time; });
        for (size_t i = 0; i < k->size(); ++i) {
          if ((*k)[i].time == t) {
            return static_cast<int>(i);
          }
        }
        return -1;
      };
      v.EraseAt = [k = &keys](size_t i) { k->erase(k->begin() + i); };
      v.Sort = [k = &keys]() {
        std::sort(k->begin(), k->end(),
                  [](const auto& a, const auto& b) { return a.time < b.time; });
      };
      break;
    }
    case AnimRowKind::BoneScale: {
      auto& keys = channel.scale_keys;
      v.count = keys.size();
      v.GetTime = [k = &keys](size_t i) { return (*k)[i].time; };
      v.SetTime = [k = &keys](size_t i, float t) { (*k)[i].time = t; };
      v.InsertAt = [k = &keys](float t) {
        k->push_back({t, glm::vec3(1.0f)});
        std::sort(k->begin(), k->end(),
                  [](const auto& a, const auto& b) { return a.time < b.time; });
        for (size_t i = 0; i < k->size(); ++i) {
          if ((*k)[i].time == t) {
            return static_cast<int>(i);
          }
        }
        return -1;
      };
      v.EraseAt = [k = &keys](size_t i) { k->erase(k->begin() + i); };
      v.Sort = [k = &keys]() {
        std::sort(k->begin(), k->end(),
                  [](const auto& a, const auto& b) { return a.time < b.time; });
      };
      break;
    }
    default:
      break;
  }
  return v;
}

// ---- Unified timeline rendering -----------------------------------------

constexpr float kRowH = 22.0f;
constexpr float kGroupRowH = 26.0f;
constexpr float kLabelColW = 260.0f;
constexpr float kRulerH = 28.0f;
constexpr float kIndentStep = 14.0f;

// One entry in the flat row list the timeline renders each frame. Built from
// clip bone channels + property curves, respecting expanded/collapsed state.
struct TimelineRow {
  AnimRowKind kind = AnimRowKind::None;
  int source_index = -1;  // -1 for header rows; otherwise index into
                          // bone_channels (for bone sub-rows) or
                          // property_curves (for PropertyCurve rows)
  int indent_level = 0;
  bool is_header = false;       // group-header or bone-name row (no keyframes)
  bool is_bone_name = false;
  bool is_group = false;
  bool* toggle_target = nullptr;     // for top-level group rows
  std::string bone_group_key;        // for bone-name rows: key into
                                     // bone_expanded_
  std::string label;
};

// One "logical bone" view aggregating the three assimp pivot channels
// ($AssimpFbx$_Translation / _Rotation / _Scaling) under a single cleaned
// base name. A regular (non-split) channel populates whichever of the three
// sub-sources have keys in it.
struct BoneGroup {
  std::string base_name;
  int pos_source = -1;    // index into clip->bone_channels
  int rot_source = -1;
  int scale_source = -1;
  int first_appear = 0;   // original order in bone_channels, for stable sort
};

// Parse an Assimp-pivot-decomposed bone name ("Root_$AssimpFbx$_Translation")
// into base + sub-kind. Returns false when the name is a regular bone;
// caller should treat that channel as contributing all three sub-tracks.
bool SplitAssimpBoneName(const std::string& name, std::string& out_base,
                         AnimRowKind& out_kind) {
  static constexpr const char* kMarker = "_$AssimpFbx$_";
  const auto marker_pos = name.find(kMarker);
  if (marker_pos == std::string::npos) {
    return false;
  }
  out_base = name.substr(0, marker_pos);
  const std::string suffix = name.substr(marker_pos + std::strlen(kMarker));
  if (suffix == "Translation") {
    out_kind = AnimRowKind::BonePosition;
  } else if (suffix == "Rotation") {
    out_kind = AnimRowKind::BoneRotation;
  } else if (suffix == "Scaling") {
    out_kind = AnimRowKind::BoneScale;
  } else {
    // Unknown pivot suffix — treat as a regular bone with the full name.
    out_base = name;
    return false;
  }
  return true;
}

std::vector<BoneGroup> BuildBoneGroups(const AnimClipAssetData& clip) {
  std::vector<BoneGroup> groups;
  std::unordered_map<std::string, size_t> by_name;
  for (size_t i = 0; i < clip.bone_channels.size(); ++i) {
    const auto& ch = clip.bone_channels[i];
    std::string base;
    AnimRowKind pivot_kind = AnimRowKind::None;
    const bool is_pivot = SplitAssimpBoneName(ch.node_name, base, pivot_kind);
    auto [it, inserted] = by_name.try_emplace(base, groups.size());
    if (inserted) {
      groups.push_back({});
      groups.back().base_name = base;
      groups.back().first_appear = static_cast<int>(i);
    }
    BoneGroup& g = groups[it->second];
    if (is_pivot) {
      if (pivot_kind == AnimRowKind::BonePosition) {
        g.pos_source = static_cast<int>(i);
      } else if (pivot_kind == AnimRowKind::BoneRotation) {
        g.rot_source = static_cast<int>(i);
      } else if (pivot_kind == AnimRowKind::BoneScale) {
        g.scale_source = static_cast<int>(i);
      }
    } else {
      // Regular channel: its three key vectors map to their sub-tracks.
      if (!ch.position_keys.empty() && g.pos_source < 0) {
        g.pos_source = static_cast<int>(i);
      }
      if (!ch.rotation_keys.empty() && g.rot_source < 0) {
        g.rot_source = static_cast<int>(i);
      }
      if (!ch.scale_keys.empty() && g.scale_source < 0) {
        g.scale_source = static_cast<int>(i);
      }
    }
  }
  std::sort(groups.begin(), groups.end(),
            [](const BoneGroup& a, const BoneGroup& b) {
              return a.first_appear < b.first_appear;
            });
  return groups;
}

// Draw a single diamond centered on (cx, cy).
void DrawDiamond(ImDrawList* dl, float cx, float cy, float half_size,
                 ImU32 fill, ImU32 outline) {
  const ImVec2 top{cx, cy - half_size};
  const ImVec2 right{cx + half_size, cy};
  const ImVec2 bot{cx, cy + half_size};
  const ImVec2 left{cx - half_size, cy};
  dl->AddQuadFilled(top, right, bot, left, fill);
  dl->AddQuad(top, right, bot, left, outline, 1.0f);
}

}  // namespace

// ---- Public entry ---------------------------------------------------------

void AnimClipEditor::Open(AssetHandle handle,
                          std::shared_ptr<AnimClipAssetData> data) {
  asset_handle_ = handle;
  clip_ = std::move(data);
  if (clip_ && !clip_->property_curves.empty()) {
    selected_row_ = {AnimRowKind::PropertyCurve, 0};
  } else {
    selected_row_ = {};
  }
  selected_key_ = -1;
  dirty_ = false;
  open_ = true;
}

void AnimClipEditor::Close() {
  open_ = false;
  clip_.reset();
  asset_handle_ = {};
  selected_row_ = {};
  selected_key_ = -1;
  bone_expanded_.clear();
  dirty_ = false;
}

void AnimClipEditor::Render() {
  if (!open_ || !clip_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(1200, 720), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(ICON_LC_FILM "  Animation Clip", &open_)) {
    ImGui::End();
    if (!open_) {
      Close();
    }
    return;
  }

  RenderTitleBar();
  ui::layout::Separator();
  RenderClipHeader();
  ui::layout::Separator();

  if (ImGui::BeginTabBar("##anim_tabs")) {
    if (ImGui::BeginTabItem(ICON_LC_ACTIVITY "  Timeline")) {
      RenderTimelineTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(ICON_LC_LINE_CHART "  Curves")) {
      RenderCurvesTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(ICON_LC_LIST "  Keyframes")) {
      RenderKeyframesTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  RenderAddCurvePopup();
  ImGui::End();

  if (!open_) {
    Close();
  }
}

void AnimClipEditor::RenderTitleBar() {
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled(ICON_LC_FILM);
  ImGui::SameLine();
  const auto* meta = Engine::asset_manager().GetMetadata(asset_handle_);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(meta ? meta->name.c_str() : "Animation Clip");
  if (dirty_) {
    ImGui::SameLine();
    ImGui::TextDisabled("*");
  }

  // Right-aligned toolbar: [+ Add Property] [Save]
  const ImGuiStyle& s = ImGui::GetStyle();
  const float add_w = ImGui::CalcTextSize(ICON_LC_PLUS "  Add Property").x +
                      s.FramePadding.x * 2.0f;
  const float save_w =
      ImGui::CalcTextSize("Save").x + s.FramePadding.x * 2.0f;
  const float tools_w = add_w + s.ItemSpacing.x + save_w;
  const float avail = ImGui::GetContentRegionAvail().x;
  if (avail > tools_w) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - tools_w);
  }
  if (ImGui::Button(ICON_LC_PLUS "  Add Property")) {
    new_curve_component_.clear();
    new_curve_field_.clear();
    open_add_popup_next_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    Save();
  }
}

void AnimClipEditor::RenderClipHeader() {
  const float field_w = 90.0f;
  const ImGuiStyle& s = ImGui::GetStyle();

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Duration");
  ImGui::SameLine(0, s.ItemInnerSpacing.x);
  ImGui::SetNextItemWidth(field_w);
  if (ImGui::DragFloat("##Duration", &clip_->duration, 0.01f, 0.0f, 0.0f,
                       "%.2fs")) {
    dirty_ = true;
  }

  ImGui::SameLine();
  ImGui::TextUnformatted("Ticks/s");
  ImGui::SameLine(0, s.ItemInnerSpacing.x);
  ImGui::SetNextItemWidth(field_w);
  if (ImGui::DragFloat("##Ticks", &clip_->ticks_per_second, 0.1f, 0.0f, 0.0f,
                       "%.2f")) {
    dirty_ = true;
  }

  ImGui::SameLine();
  if (ImGui::Checkbox("Loop", &clip_->loop)) {
    dirty_ = true;
  }

  char ph_buf[32];
  std::snprintf(ph_buf, sizeof(ph_buf), "t = %.3fs", playhead_time_);
  const ImVec2 ph_sz = ImGui::CalcTextSize(ph_buf);
  const float remaining = ImGui::GetContentRegionAvail().x;
  if (remaining > ph_sz.x + s.ItemSpacing.x) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining - ph_sz.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", ph_buf);
  }
}

void AnimClipEditor::RenderTimelineTab() {
  // --- Build the flat row list from current expanded state ---------------
  std::vector<TimelineRow> rows;
  rows.reserve(16);

  if (clip_->HasBoneChannels()) {
    TimelineRow hdr;
    hdr.is_header = true;
    hdr.is_group = true;
    hdr.indent_level = 0;
    hdr.label = std::string(ICON_LC_BONE) + "  Bones";
    hdr.toggle_target = &group_bones_open_;
    rows.push_back(std::move(hdr));
    if (group_bones_open_) {
      const auto groups = BuildBoneGroups(*clip_);
      for (const auto& g : groups) {
        TimelineRow br;
        br.is_header = true;
        br.is_bone_name = true;
        br.indent_level = 1;
        br.label = g.base_name;
        br.bone_group_key = g.base_name;
        rows.push_back(std::move(br));
        if (bone_expanded_.count(g.base_name)) {
          auto add_sub = [&](AnimRowKind sub, int src, const char* lbl) {
            if (src < 0) {
              return;
            }
            TimelineRow r;
            r.kind = sub;
            r.source_index = src;
            r.indent_level = 2;
            r.label = lbl;
            rows.push_back(std::move(r));
          };
          add_sub(AnimRowKind::BonePosition, g.pos_source, "Position");
          add_sub(AnimRowKind::BoneRotation, g.rot_source, "Rotation");
          add_sub(AnimRowKind::BoneScale, g.scale_source, "Scale");
        }
      }
    }
  }

  {
    TimelineRow hdr;
    hdr.is_header = true;
    hdr.is_group = true;
    hdr.indent_level = 0;
    hdr.label =
        std::string(ICON_LC_SLIDERS_HORIZONTAL) + "  Properties";
    hdr.toggle_target = &group_properties_open_;
    rows.push_back(std::move(hdr));
    if (group_properties_open_) {
      for (size_t c = 0; c < clip_->property_curves.size(); ++c) {
        const auto& pc = clip_->property_curves[c];
        CurveValueKind kind = KindOf(pc);
        TimelineRow pr;
        pr.kind = AnimRowKind::PropertyCurve;
        pr.source_index = static_cast<int>(c);
        pr.indent_level = 1;
        pr.label =
            CurveLabel(pc) + "  (" + std::string(KindLabel(kind)) + ")";
        rows.push_back(std::move(pr));
      }
    }
  }

  // --- Sizing: reserve inline key editor height at the bottom -----------
  const float avail_w = ImGui::GetContentRegionAvail().x;
  const float avail_h = ImGui::GetContentRegionAvail().y;
  const bool editor_visible =
      selected_row_.IsValid() && selected_key_ >= 0;
  const float editor_h = editor_visible ? 110.0f : 0.0f;
  const float timeline_h =
      ImMax(80.0f, avail_h - editor_h - ImGui::GetStyle().ItemSpacing.y);

  const float eff_duration = ImMax(clip_->duration, 0.001f);

  // Flat scroll child (no border) fills the container; vertical scrollbar
  // appears automatically when row count exceeds the height.
  ImGui::BeginChild("##anim_timeline", ImVec2(avail_w, timeline_h), 0, 0);

  const float inner_w = ImGui::GetContentRegionAvail().x;
  const float label_w = kLabelColW;
  const float track_w = ImMax(60.0f, inner_w - label_w);
  const float px_per_sec = track_w / eff_duration;

  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Pixel-driven tick picker: aim ~70 px between labels, snap to nice
  // numeric steps. Keeps labels readable regardless of zoom / window size.
  auto pick_step = [&](float total, float track_px) {
    const float desired_labels = ImMax(1.0f, track_px / 70.0f);
    const float target = total / desired_labels;
    static const float steps[] = {0.01f, 0.02f, 0.05f, 0.1f,  0.2f,
                                  0.25f, 0.5f,  1.0f,  2.0f,  5.0f,
                                  10.0f, 30.0f, 60.0f};
    for (float s : steps) {
      if (s >= target) {
        return s;
      }
    }
    return 60.0f;
  };
  const float tick_step = pick_step(eff_duration, track_w);

  // Precompute tick times so we can draw both the ruler labels and the
  // full-height grid lines against the same set.
  std::vector<float> tick_times;
  for (float t = 0.0f; t <= eff_duration + 1e-4f; t += tick_step) {
    tick_times.push_back(t);
  }

  // --- Ruler row --------------------------------------------------------
  const ImVec2 ruler_pos = ImGui::GetCursorScreenPos();
  const float track_left = ruler_pos.x + label_w;
  auto time_to_x = [&](float t) { return track_left + t * px_per_sec; };
  auto x_to_time = [&](float x) {
    return ImClamp((x - track_left) / px_per_sec, 0.0f, eff_duration);
  };

  ImGui::Dummy(ImVec2(inner_w, kRulerH));
  const ImVec2 cursor_after_ruler = ImGui::GetCursorScreenPos();

  // Ruler hit area (click to move playhead).
  ImGui::SetCursorScreenPos(ImVec2(track_left, ruler_pos.y));
  ImGui::InvisibleButton("##ruler", ImVec2(track_w, kRulerH));
  if (ImGui::IsItemClicked()) {
    playhead_time_ = x_to_time(ImGui::GetIO().MousePos.x);
  }
  ImGui::SetCursorScreenPos(cursor_after_ruler);

  // Ruler visuals
  const ImVec2 ruler_end(ruler_pos.x + inner_w, ruler_pos.y + kRulerH);
  dl->AddRectFilled(ruler_pos,
                    ImVec2(ruler_pos.x + label_w, ruler_end.y),
                    ImGui::GetColorU32(ImGuiCol_WindowBg));
  dl->AddRectFilled(ImVec2(track_left, ruler_pos.y), ruler_end,
                    ui::style::kDrawerBg);
  dl->AddLine(ImVec2(track_left, ruler_pos.y),
              ImVec2(track_left, ruler_end.y),
              ImGui::GetColorU32(ImGuiCol_Border));
  const ImU32 tick_col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const ImU32 label_col = ImGui::GetColorU32(ImGuiCol_Text);
  for (float t : tick_times) {
    const float x = time_to_x(t);
    dl->AddLine(ImVec2(x, ruler_end.y - 6.0f),
                ImVec2(x, ruler_end.y - 1.0f), tick_col);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.4gs", t);
    dl->AddText(ImVec2(x + 3.0f, ruler_pos.y + 4.0f), label_col, buf);
  }

  // --- Rows ------------------------------------------------------------
  struct DragCtx {
    bool changed = false;
    AnimRowKind kind = AnimRowKind::None;
    int source = -1;
    float target_time = -1.0f;
  } drag;

  const float rows_top_y = cursor_after_ruler.y;

  for (size_t ri = 0; ri < rows.size(); ++ri) {
    TimelineRow& row = rows[ri];
    const float row_h = row.is_header ? kGroupRowH : kRowH;
    const ImVec2 row_pos = ImGui::GetCursorScreenPos();

    // Reserve the row in layout so the scroll child measures correctly.
    ImGui::Dummy(ImVec2(inner_w, row_h));
    const ImVec2 cursor_after_row = ImGui::GetCursorScreenPos();

    // Label hit-test overlay
    ImGui::SetCursorScreenPos(row_pos);
    ImGui::PushID(static_cast<int>(ri));
    ImGui::InvisibleButton("##label_hit", ImVec2(label_w, row_h));
    const bool label_clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    // --- Track-side backgrounds (always drawn, even for header rows) ----
    const ImVec2 row_end(row_pos.x + inner_w, row_pos.y + row_h);
    dl->AddRectFilled(ImVec2(track_left, row_pos.y),
                      ImVec2(row_end.x, row_end.y), ui::style::kDrawerBg);
    const bool is_selected = (selected_row_.kind == row.kind &&
                              selected_row_.source_index == row.source_index &&
                              !row.is_header);
    if (is_selected) {
      dl->AddRectFilled(ImVec2(row_pos.x, row_pos.y),
                        ImVec2(track_left, row_end.y),
                        ImGui::GetColorU32(ImGuiCol_Header));
    }
    dl->AddLine(ImVec2(track_left, row_pos.y),
                ImVec2(track_left, row_end.y),
                ImGui::GetColorU32(ImGuiCol_Border));
    dl->AddLine(ImVec2(row_pos.x, row_end.y), ImVec2(row_end.x, row_end.y),
                IM_COL32(255, 255, 255, 16));

    // Label side: indent + chevron + label text
    const float indent_px = row.indent_level * kIndentStep + 6.0f;
    const float cy_row = row_pos.y + row_h * 0.5f;
    float cursor_x = row_pos.x + indent_px;
    if (row.is_group || row.is_bone_name) {
      const bool open = row.is_group
                            ? (row.toggle_target && *row.toggle_target)
                            : (bone_expanded_.count(row.bone_group_key) > 0);
      const char* chev_glyph =
          open ? ICON_LC_CHEVRON_DOWN : ICON_LC_CHEVRON_RIGHT;
      const ImVec2 chev_sz = ImGui::CalcTextSize(chev_glyph);
      dl->AddText(ImVec2(cursor_x, cy_row - chev_sz.y * 0.5f),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), chev_glyph);
      cursor_x += chev_sz.x + 4.0f;
      if (label_clicked) {
        if (row.is_group && row.toggle_target) {
          *row.toggle_target = !(*row.toggle_target);
        } else if (row.is_bone_name) {
          if (bone_expanded_.count(row.bone_group_key)) {
            bone_expanded_.erase(row.bone_group_key);
          } else {
            bone_expanded_.insert(row.bone_group_key);
          }
        }
      }
    }
    const ImVec2 lbl_sz = ImGui::CalcTextSize(row.label.c_str());
    // Clip the label to the label column so long bone names don't spill
    // into the track area.
    dl->PushClipRect(ImVec2(row_pos.x, row_pos.y),
                     ImVec2(track_left - 2.0f, row_end.y), true);
    dl->AddText(ImVec2(cursor_x, cy_row - lbl_sz.y * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), row.label.c_str());
    dl->PopClipRect();

    // --- Leaf rows: keyframe strip --------------------------------------
    if (!row.is_header) {
      TrackView tv;
      if (row.kind == AnimRowKind::PropertyCurve) {
        PropertyCurve& pc = clip_->property_curves[row.source_index];
        tv = MakePropertyTrack(pc, KindOf(pc));
      } else {
        AnimationChannel& bc = clip_->bone_channels[row.source_index];
        tv = MakeBoneTrack(bc, row.kind);
      }

      const float track_mid_y = row_pos.y + row_h * 0.5f;

      // Empty-strip click -> add key.
      ImGui::SetCursorScreenPos(ImVec2(track_left, row_pos.y));
      ImGui::PushID(static_cast<int>(ri + 0x10000));
      ImGui::SetNextItemAllowOverlap();
      ImGui::InvisibleButton("##strip", ImVec2(track_w, row_h));
      const bool strip_hovered = ImGui::IsItemHovered();
      const bool strip_clicked = ImGui::IsItemClicked();
      ImGui::PopID();

      int hovered_key = -1;
      int pressed_key = -1;
      for (size_t i = 0; i < tv.count; ++i) {
        const float t = tv.GetTime(i);
        const float x = time_to_x(t);
        const float hs = 5.0f;
        const ImVec2 bmin(x - hs - 1.0f, track_mid_y - hs - 1.0f);
        const ImVec2 bsz((hs + 1.0f) * 2.0f, (hs + 1.0f) * 2.0f);
        ImGui::SetCursorScreenPos(bmin);
        ImGui::PushID(static_cast<int>(i + 0x20000 * ri));
        ImGui::InvisibleButton("##key", bsz);
        const bool hv = ImGui::IsItemHovered();
        const bool act = ImGui::IsItemActive();
        const bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        if (hv) {
          hovered_key = static_cast<int>(i);
        }
        if (clk) {
          pressed_key = static_cast<int>(i);
        }
        if (act && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
          const float delta = ImGui::GetIO().MouseDelta.x / px_per_sec;
          const float nt = ImClamp(t + delta, 0.0f, eff_duration);
          if (nt != t) {
            tv.SetTime(i, nt);
            drag.changed = true;
            drag.kind = row.kind;
            drag.source = row.source_index;
            drag.target_time = nt;
          }
        }

        const bool key_selected =
            is_selected && selected_key_ == static_cast<int>(i);
        const ImU32 fill =
            key_selected ? ImGui::GetColorU32(ImGuiCol_CheckMark)
                         : (hv ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                               : IM_COL32(0xd8, 0xaa, 0x3a, 0xff));
        const ImU32 outline = ImGui::GetColorU32(ImGuiCol_Border);
        DrawDiamond(dl, x, track_mid_y, hs, fill, outline);
      }

      if (pressed_key >= 0) {
        selected_row_ = {row.kind, row.source_index};
        selected_key_ = pressed_key;
      } else if (strip_clicked && hovered_key < 0 && strip_hovered) {
        const float t = x_to_time(ImGui::GetIO().MousePos.x);
        const int new_idx = tv.InsertAt ? tv.InsertAt(t) : -1;
        if (new_idx >= 0) {
          selected_row_ = {row.kind, row.source_index};
          selected_key_ = new_idx;
          dirty_ = true;
        }
      }
      if (hovered_key >= 0) {
        ImGui::SetTooltip("Key %d  @ %.3fs", hovered_key,
                          tv.GetTime(hovered_key));
      }
    }

    // Restore cursor for next row.
    ImGui::SetCursorScreenPos(cursor_after_row);
  }

  const float rows_bottom_y = ImGui::GetCursorScreenPos().y;

  // --- Grid lines across the track area (after rows so we draw on top) -
  const ImU32 grid_col = IM_COL32(255, 255, 255, 18);
  for (float t : tick_times) {
    const float x = time_to_x(t);
    dl->AddLine(ImVec2(x, rows_top_y), ImVec2(x, rows_bottom_y), grid_col,
                1.0f);
  }

  // --- Playhead ----------------------------------------------------------
  if (playhead_time_ >= 0.0f && playhead_time_ <= eff_duration) {
    const float px = time_to_x(playhead_time_);
    dl->AddLine(ImVec2(px, ruler_pos.y),
                ImVec2(px, ImMax(rows_bottom_y, rows_top_y + 1.0f)),
                IM_COL32(0xff, 0x6e, 0x40, 0xff), 1.5f);
  }

  ImGui::EndChild();

  // --- Apply pending drag re-sort + remap selection ----------------------
  if (drag.changed) {
    TrackView tv;
    if (drag.kind == AnimRowKind::PropertyCurve) {
      PropertyCurve& pc = clip_->property_curves[drag.source];
      tv = MakePropertyTrack(pc, KindOf(pc));
    } else {
      AnimationChannel& bc = clip_->bone_channels[drag.source];
      tv = MakeBoneTrack(bc, drag.kind);
    }
    if (tv.Sort) {
      tv.Sort();
    }
    if (selected_row_.kind == drag.kind &&
        selected_row_.source_index == drag.source &&
        drag.target_time >= 0.0f) {
      for (size_t i = 0; i < tv.count; ++i) {
        if (tv.GetTime(i) == drag.target_time) {
          selected_key_ = static_cast<int>(i);
          break;
        }
      }
    }
    dirty_ = true;
  }

  // --- Inline key editor -------------------------------------------------
  if (editor_visible) {
    ImGui::Spacing();
    ui::layout::Separator();
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(ICON_LC_DIAMOND);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted("Selected Key");
    ImGui::SameLine();
    ImGui::TextDisabled("#%d", selected_key_);

    if (selected_row_.IsProperty()) {
      PropertyCurve& curve =
          clip_->property_curves[selected_row_.source_index];
      CurveValueKind kind = KindOf(curve);
      bool changed = false;
      bool delete_requested = false;
      VisitKeys(curve, kind, [&](auto& keys) {
        if (selected_key_ >= static_cast<int>(keys.size())) {
          return;
        }
        auto& k = keys[static_cast<size_t>(selected_key_)];
        if (ImGui::DragFloat(ui::field::PrefixLabel("Time").c_str(), &k.time,
                             0.01f, 0.0f, clip_->duration, "%.3fs")) {
          changed = true;
        }
        if (EditValue(ui::field::PrefixLabel("Value").c_str(), k.value)) {
          changed = true;
        }
      });
      ImGui::SameLine();
      if (ImGui::Button(ICON_LC_TRASH_2 "##del_key")) {
        delete_requested = true;
      }
      if (delete_requested) {
        VisitKeys(curve, kind, [&](auto& keys) {
          if (selected_key_ < static_cast<int>(keys.size())) {
            keys.erase(keys.begin() + selected_key_);
          }
        });
        selected_key_ = -1;
        dirty_ = true;
      } else if (changed) {
        VisitKeys(curve, kind, [](auto& keys) {
          std::sort(keys.begin(), keys.end(),
                    [](const auto& a, const auto& b) {
                      return a.time < b.time;
                    });
        });
        dirty_ = true;
      }
    } else if (selected_row_.IsBone()) {
      AnimationChannel& bc =
          clip_->bone_channels[selected_row_.source_index];
      TrackView tv = MakeBoneTrack(bc, selected_row_.kind);
      if (selected_key_ < static_cast<int>(tv.count)) {
        float t = tv.GetTime(selected_key_);
        if (ImGui::DragFloat(ui::field::PrefixLabel("Time").c_str(), &t,
                             0.01f, 0.0f, clip_->duration, "%.3fs")) {
          tv.SetTime(selected_key_, t);
          tv.Sort();
          for (size_t i = 0; i < tv.count; ++i) {
            if (tv.GetTime(i) == t) {
              selected_key_ = static_cast<int>(i);
              break;
            }
          }
          dirty_ = true;
        }
        ImGui::TextDisabled(
            "Bone values are read-only for now; edit via the source "
            "model.");
        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_TRASH_2 "##del_bone_key")) {
          tv.EraseAt(selected_key_);
          selected_key_ = -1;
          dirty_ = true;
        }
      }
    }
  }
}

void AnimClipEditor::RenderCurvesTab() {
  if (!selected_row_.IsProperty()) {
    ImGui::TextDisabled(
        "Select a property curve in the Timeline tab to edit its graph.");
    return;
  }
  if (selected_row_.source_index >=
      static_cast<int>(clip_->property_curves.size())) {
    return;
  }
  PropertyCurve& curve = clip_->property_curves[selected_row_.source_index];
  CurveValueKind kind = KindOf(curve);

  // Target + interp selector
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(ICON_LC_TARGET);
  ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::TextUnformatted(curve.target_component.c_str());
  ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::TextDisabled(".");
  ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::TextUnformatted(curve.target_field.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", KindLabel(kind));

  const char* interp_items[] = {"Linear", "Step"};
  int interp_idx = curve.interp == CurveInterp::Step ? 1 : 0;
  const float combo_w = 110.0f;
  const float remaining = ImGui::GetContentRegionAvail().x;
  if (remaining > combo_w) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining - combo_w);
  }
  ImGui::SetNextItemWidth(combo_w);
  if (ImGui::Combo("##interp", &interp_idx, interp_items, 2)) {
    curve.interp = interp_idx == 1 ? CurveInterp::Step : CurveInterp::Linear;
    dirty_ = true;
  }

  if (RenderCurveGraph(curve, kind, clip_->duration, playhead_time_)) {
    dirty_ = true;
  }

  ImGui::TextDisabled(
      "Drag a control point: X moves the key's time (shared across "
      "components); Y moves this component's value.");
}

void AnimClipEditor::RenderKeyframesTab() {
  if (!selected_row_.IsProperty()) {
    ImGui::TextDisabled(
        "Select a property curve in the Timeline tab to edit its keys.");
    return;
  }
  if (selected_row_.source_index >=
      static_cast<int>(clip_->property_curves.size())) {
    return;
  }
  PropertyCurve& curve = clip_->property_curves[selected_row_.source_index];
  CurveValueKind kind = KindOf(curve);
  if (RenderKeyTable(curve, kind)) {
    dirty_ = true;
  }
}

void AnimClipEditor::RenderAddCurvePopup() {
  // Deferred from the "+ Add Curve" button so OpenPopup runs in the same
  // window scope as BeginPopupModal below.
  if (open_add_popup_next_) {
    ImGui::OpenPopup("AddCurve");
    open_add_popup_next_ = false;
  }

  if (!ImGui::BeginPopupModal("AddCurve", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  // Component picker - any reflected type with at least one Animatable field.
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

  // Field picker - only Animatable fields with a supported keyframe type.
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
    selected_row_ = {
        AnimRowKind::PropertyCurve,
        static_cast<int>(clip_->property_curves.size()) - 1};
    selected_key_ = -1;
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
