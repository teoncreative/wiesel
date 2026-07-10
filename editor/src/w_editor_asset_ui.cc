//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_asset_ui.h"

#include <imgui.h>
#include <urkern/natural_sort.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "asset/w_asset_manager.h"
#include "asset/w_asset_properties.h"
#include "asset/w_asset_registry.h"
#include "cursor/w_cursor.h"
#include "physics/w_mesh_collider_asset.h"
#include "rendering/w_material.h"
#include "rendering/w_skybox.h"
#include "rendering/w_sprite_asset.h"
#include "rendering/w_texture.h"
#include "scene/w_scene.h"
#include "scene/w_scene_manager.h"
#include "ui/w_font.h"
#include "ui/w_ui_document.h"
#include "ui/w_ui_field.h"
#include "util/imgui/imgui_lucide.h"
#include "util/w_logger.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

namespace wiesel::editor {

using ui::field::PrefixLabel;
using ui::field::RenderTexturePreview;

// ---- AssetUiRegistry storage ----

static std::unordered_map<AssetType, AssetUiRegistry::RenderAssetFn>&
AssetRenderers() {
  static std::unordered_map<AssetType, AssetUiRegistry::RenderAssetFn> m;
  return m;
}

static std::unordered_map<AssetType, AssetUiRegistry::RenderPropertiesFn>&
PropertyRenderers() {
  static std::unordered_map<AssetType, AssetUiRegistry::RenderPropertiesFn> m;
  return m;
}

static std::unordered_map<AssetType, std::vector<AssetContextAction>>&
ContextActions() {
  static std::unordered_map<AssetType, std::vector<AssetContextAction>> m;
  return m;
}

void AssetUiRegistry::SetRenderAsset(AssetType type, RenderAssetFn fn) {
  AssetRenderers()[type] = std::move(fn);
}

void AssetUiRegistry::SetRenderProperties(AssetType type,
                                          RenderPropertiesFn fn) {
  PropertyRenderers()[type] = std::move(fn);
}

const AssetUiRegistry::RenderAssetFn* AssetUiRegistry::GetRenderAsset(
    AssetType type) {
  auto& m = AssetRenderers();
  auto it = m.find(type);
  return it == m.end() ? nullptr : &it->second;
}

const AssetUiRegistry::RenderPropertiesFn*
AssetUiRegistry::GetRenderProperties(AssetType type) {
  auto& m = PropertyRenderers();
  auto it = m.find(type);
  return it == m.end() ? nullptr : &it->second;
}

void AssetUiRegistry::AddContextAction(AssetType type,
                                       AssetContextAction action) {
  ContextActions()[type].push_back(std::move(action));
}

const std::vector<AssetContextAction>& AssetUiRegistry::GetContextActions(
    AssetType type) {
  static const std::vector<AssetContextAction> kEmpty;
  auto& m = ContextActions();
  auto it = m.find(type);
  return it == m.end() ? kEmpty : it->second;
}

// ---- AssetCombo ----

static ThumbnailEntry GetOrCreateThumbnail(AssetHandle handle,
                                           const AssetMetadata& meta) {
  return ThumbnailCache::Get()->GetOrCreate(handle, meta);
}

bool AssetCombo(const char* label,
                std::initializer_list<AssetType> types,
                AssetHandle& selected, bool allow_none,
                const char* none_label) {
  bool changed = false;
  std::string display;

  if (selected.IsValid()) {
    const auto* meta = Engine::asset_manager().GetMetadata(selected);
    display = meta ? meta->name : "(Unknown)";
  } else {
    display = allow_none ? none_label : "(Select...)";
  }

  if (ImGui::BeginCombo(label, display.c_str())) {
    if (allow_none) {
      if (ImGui::Selectable(none_label, !selected.IsValid())) {
        selected = {};
        changed = true;
      }
    }

    std::vector<AssetHandle> assets;
    for (AssetType type : types) {
      auto of_type = Engine::asset_manager().GetAllOfType(type);
      assets.insert(assets.end(), of_type.begin(), of_type.end());
    }
    std::sort(assets.begin(), assets.end(),
              [](const AssetHandle& a, const AssetHandle& b) {
                const auto* ma = Engine::asset_manager().GetMetadata(a);
                const auto* mb = Engine::asset_manager().GetMetadata(b);
                if (!ma || !mb) {
                  return false;
                }
                return urkern::NaturalLess(ma->name, mb->name);
              });
    for (const auto& handle : assets) {
      const auto* meta = Engine::asset_manager().GetMetadata(handle);
      if (!meta) {
        continue;
      }

      bool is_selected = (handle == selected);
      if (ImGui::Selectable(meta->name.c_str(), is_selected)) {
        selected = handle;
        changed = true;
      }

      if (ImGui::IsItemHovered()) {
        ThumbnailEntry thumb = GetOrCreateThumbnail(handle, *meta);
        if (thumb.texture_id) {
          ImGui::BeginTooltip();
          ImGui::Image(reinterpret_cast<ImTextureID>(thumb.texture_id),
                       thumb.FitSize(128), thumb.uv0, thumb.uv1);
          ImGui::EndTooltip();
        }
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (selected.IsValid()) {
    const auto* meta = Engine::asset_manager().GetMetadata(selected);
    if (meta) {
      ThumbnailEntry thumb = GetOrCreateThumbnail(selected, *meta);
      if (thumb.texture_id) {
        ImGui::SameLine();
        ImGui::Image(
            reinterpret_cast<ImTextureID>(thumb.texture_id),
            ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()),
            thumb.uv0, thumb.uv1);
      }
    }
  }

  return changed;
}

bool AssetCombo(const char* label, AssetType type, AssetHandle& selected,
                bool allow_none, const char* none_label) {
  return AssetCombo(label, {type}, selected, allow_none, none_label);
}

// ---- Built-in renderers: texture import properties ----

static bool RenderTextureProperties(void* p) {
  auto* props = static_cast<TextureAssetProperties*>(p);
  bool changed = false;

  const char* asset_types[] = {"Default", "Normal Map", "Sprite (UI)"};
  int at = static_cast<int>(props->asset_type);
  if (ImGui::Combo(PrefixLabel("Type").c_str(), &at, asset_types, 3)) {
    props->asset_type = static_cast<TextureAssetType>(at);
    changed = true;
  }

  const char* filter_modes[] = {"Nearest", "Linear"};
  int filter = static_cast<int>(props->filter_mode);
  if (ImGui::Combo(PrefixLabel("Filter Mode").c_str(), &filter, filter_modes,
                   2)) {
    props->filter_mode = static_cast<TextureFilterMode>(filter);
    changed = true;
  }

  const char* wrap_modes[] = {"Repeat", "Clamp", "Mirror"};
  int wrap = static_cast<int>(props->wrap_mode);
  if (ImGui::Combo(PrefixLabel("Wrap Mode").c_str(), &wrap, wrap_modes, 3)) {
    props->wrap_mode = static_cast<TextureWrapMode>(wrap);
    changed = true;
  }

  changed |= ImGui::Checkbox("Generate Mipmaps", &props->generate_mipmaps);

  ImGui::SeparatorText("9-Slice");
  changed |= ImGui::DragFloat4(PrefixLabel("Border (L,T,R,B)").c_str(),
                               reinterpret_cast<float*>(&props->slice_border),
                               1.0f, 0.0f, 500.0f);

  return changed;
}

// ---- Built-in renderers: font import properties ----

static bool RenderFontProperties(void* p) {
  auto* props = static_cast<FontAssetProperties*>(p);
  bool changed = false;

  const char* aa_modes[] = {"None", "Grayscale"};
  int aa = static_cast<int>(props->aa_mode);
  if (ImGui::Combo(PrefixLabel("Anti-Aliasing").c_str(), &aa, aa_modes, 2)) {
    props->aa_mode = static_cast<FontAAMode>(aa);
    changed = true;
  }

  return changed;
}

// ---- Built-in renderers: UIDocument import properties ----

static bool RenderUIDocumentProperties(void* p) {
  auto* props = static_cast<UIDocumentAssetProperties*>(p);
  bool changed = false;

  ImGui::SeparatorText("Data Variables");

  int remove_idx = -1;
  for (int i = 0; i < static_cast<int>(props->variables.size()); i++) {
    auto& v = props->variables[i];
    ImGui::PushID(i);

    char name_buf[128];
    std::strncpy(name_buf, v.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText("##name", name_buf, sizeof(name_buf))) {
      v.name = name_buf;
      changed = true;
    }
    ImGui::SameLine();

    const char* type_names[] = {"Int", "Float", "String", "Bool"};
    int type_idx = static_cast<int>(v.type);
    ImGui::SetNextItemWidth(70);
    if (ImGui::Combo("##type", &type_idx, type_names, 4)) {
      v.type = static_cast<UIVariableType>(type_idx);
      changed = true;
    }
    ImGui::SameLine();

    const char* mode_names[] = {"TwoWay", "ReadOnly"};
    int mode_idx = static_cast<int>(v.mode);
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("##mode", &mode_idx, mode_names, 2)) {
      v.mode = static_cast<UIVariableMode>(mode_idx);
      changed = true;
    }
    ImGui::SameLine();

    char def_buf[128];
    std::strncpy(def_buf, v.default_value.c_str(), sizeof(def_buf) - 1);
    def_buf[sizeof(def_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(80);
    if (ImGui::InputText("##default", def_buf, sizeof(def_buf))) {
      v.default_value = def_buf;
      changed = true;
    }
    ImGui::SameLine();

    if (ImGui::SmallButton("X")) {
      remove_idx = i;
      changed = true;
    }

    ImGui::PopID();
  }

  if (remove_idx >= 0) {
    props->variables.erase(props->variables.begin() + remove_idx);
  }

  if (ImGui::Button("+ Add Variable")) {
    UIVariableDecl decl;
    decl.name = "var_" + std::to_string(props->variables.size());
    props->variables.push_back(decl);
    changed = true;
  }

  ImGui::SeparatorText("Events");

  int ev_remove_idx = -1;
  for (int i = 0; i < static_cast<int>(props->events.size()); i++) {
    ImGui::PushID(1000 + i);
    char ev_buf[128];
    std::strncpy(ev_buf, props->events[i].c_str(), sizeof(ev_buf) - 1);
    ev_buf[sizeof(ev_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##event", ev_buf, sizeof(ev_buf))) {
      props->events[i] = ev_buf;
      changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("X")) {
      ev_remove_idx = i;
      changed = true;
    }
    ImGui::PopID();
  }

  if (ev_remove_idx >= 0) {
    props->events.erase(props->events.begin() + ev_remove_idx);
  }

  if (ImGui::Button("+ Add Event")) {
    props->events.push_back("on_event_" +
                            std::to_string(props->events.size()));
    changed = true;
  }

  return changed;
}

// ---- Built-in renderers: material asset ----

static bool RenderMaterialAsset(AssetHandle handle) {
  auto& mgr = Engine::asset_manager();
  mgr.LoadSync(handle);
  auto mat = mgr.Get<Material>(handle);
  if (!mat) {
    ImGui::TextDisabled("Material not loaded");
    return false;
  }

  bool changed = false;

  glm::vec4 color_tint = mat->GetColorTint();
  if (ImGui::ColorEdit4(PrefixLabel("Color Tint").c_str(), &color_tint.x)) {
    mat->SetColorTint(color_tint);
    changed = true;
  }

  float roughness = mat->GetRoughness();
  if (ImGui::SliderFloat(PrefixLabel("Roughness").c_str(), &roughness, 0.0f,
                         1.0f)) {
    mat->SetRoughness(roughness);
    changed = true;
  }

  float metallic = mat->GetMetallic();
  if (ImGui::SliderFloat(PrefixLabel("Metallic").c_str(), &metallic, 0.0f,
                         1.0f)) {
    mat->SetMetallic(metallic);
    changed = true;
  }

  float specular = mat->GetSpecular();
  if (ImGui::SliderFloat(PrefixLabel("Specular").c_str(), &specular, 0.0f,
                         1.0f)) {
    mat->SetSpecular(specular);
    changed = true;
  }

  if (ImGui::Checkbox("Double Sided", &mat->double_sided)) {
    changed = true;
  }

  ImGui::SeparatorText("Textures");

  auto render_slot = [](const char* label, TextureSlot& slot) {
    std::shared_ptr<Texture> tex;
    slot.Resolve(tex);
    RenderTexturePreview(label, tex.get());
  };
  render_slot("Diffuse", mat->base_texture);
  render_slot("Albedo", mat->albedo_map);
  render_slot("Normal", mat->normal_map);
  render_slot("Roughness", mat->roughness_map);
  render_slot("Metallic", mat->metallic_map);
  render_slot("Specular", mat->specular_map);
  render_slot("Height", mat->height_map);
  render_slot("Opacity", mat->opacity_map);

  if (changed) {
    mat->MarkDirty();
    AssetRegistry::Save(handle);
  }

  return changed;
}

// ---- Built-in renderers: cursor set ----

// CSS / RmlUi-style cursor state names. Matching these keys lets RmlUi
// cursor-change requests drive the cursor set automatically.
static constexpr const char* kCursorStateNames[] = {
    "default",     "pointer",           "crosshair",       "text",
    "move",        "wait",              "progress",        "help",
    "not-allowed", "grab",              "grabbing",        "resize-horizontal",
    "resize-vertical"};

static bool RenderCursorSetAsset(AssetHandle handle) {
  AssetManager& mgr = Engine::asset_manager();
  mgr.LoadSync(handle);
  auto data = mgr.Get<CursorSetData>(handle);
  if (!data) {
    ImGui::TextDisabled("Cursor set not loaded.");
    return false;
  }

  bool changed = false;

  const char* mode_names[] = {"Auto (Hardware)", "Force Software"};
  int mode = data->mode == CursorSetMode::ForceSoftware ? 1 : 0;
  if (ImGui::Combo(PrefixLabel("Mode").c_str(), &mode, mode_names, 2)) {
    data->mode =
        mode == 1 ? CursorSetMode::ForceSoftware : CursorSetMode::Auto;
    changed = true;
  }

  ImGui::SeparatorText("States");

  std::vector<std::string> names;
  names.reserve(data->states.size());
  for (const auto& [k, _] : data->states) {
    names.push_back(k);
  }
  std::ranges::sort(names);

  std::string remove_name;
  std::pair<std::string, std::string> rename;  // {old, new}

  for (size_t i = 0; i < names.size(); i++) {
    const std::string& state_name = names[i];
    auto it = data->states.find(state_name);
    if (it == data->states.end()) {
      continue;
    }
    CursorStateEntry& entry = it->second;

    ImGui::PushID(static_cast<int>(i));

    char name_buf[64];
    std::snprintf(name_buf, sizeof(name_buf), "%s", state_name.c_str());
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("##name", name_buf)) {
      for (const char* std_name : kCursorStateNames) {
        bool is_sel = state_name == std_name;
        if (ImGui::Selectable(std_name, is_sel) && state_name != std_name) {
          rename = {state_name, std_name};
        }
      }
      ImGui::Separator();
      ImGui::TextUnformatted("Custom");
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::InputText("##custom_name", name_buf, sizeof(name_buf),
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (name_buf[0] != '\0' && state_name != name_buf) {
          rename = {state_name, name_buf};
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove")) {
      remove_name = state_name;
    }

    bool animated = entry.frames.size() > 1;
    if (ImGui::Checkbox("Animated", &animated)) {
      if (animated) {
        while (entry.frames.size() < 2) {
          entry.frames.push_back({});
        }
      } else if (entry.frames.size() > 1) {
        entry.frames.resize(1);
      }
      changed = true;
    }

    if (!animated) {
      AssetHandle tex =
          entry.frames.empty() ? AssetHandle{} : entry.frames[0].texture;
      if (AssetCombo("Image", {AssetType::Texture, AssetType::Sprite}, tex)) {
        if (entry.frames.empty()) {
          entry.frames.push_back({});
        }
        entry.frames[0].texture = tex;
        changed = true;
      }
    } else {
      ImGui::Text("Frames: %d", static_cast<int>(entry.frames.size()));
      ImGui::SameLine();
      if (ImGui::SmallButton("+ Frame")) {
        entry.frames.push_back({});
        changed = true;
      }
      int remove_frame = -1;
      for (size_t f = 0; f < entry.frames.size(); f++) {
        ImGui::PushID(1000 + static_cast<int>(f));
        std::string lbl = "Frame " + std::to_string(f);
        if (AssetCombo(lbl.c_str(), {AssetType::Texture, AssetType::Sprite},
                       entry.frames[f].texture)) {
          changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
          remove_frame = static_cast<int>(f);
        }
        ImGui::PopID();
      }
      if (remove_frame >= 0) {
        entry.frames.erase(entry.frames.begin() + remove_frame);
        changed = true;
      }
      ImGui::SetNextItemWidth(80);
      if (ImGui::DragFloat("Frame Duration", &entry.frame_duration, 0.01f,
                           0.01f, 1.0f, "%.2fs")) {
        changed = true;
      }
    }

    ImGui::SetNextItemWidth(60);
    if (ImGui::DragInt("Hotspot X", &entry.hotspot.x, 1, 0, 256)) {
      changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::DragInt("Hotspot Y", &entry.hotspot.y, 1, 0, 256)) {
      changed = true;
    }

    ImGui::SetNextItemWidth(60);
    if (ImGui::DragInt("Scale", &entry.scale, 1, 1, 8)) {
      entry.scale = std::clamp(entry.scale, 1, 8);
      changed = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Nearest-neighbor upscale factor.\n"
          "Hotspot stays in source-pixel space.");
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (!rename.first.empty() && !rename.second.empty() &&
      !data->states.contains(rename.second)) {
    auto node = data->states.extract(rename.first);
    node.key() = rename.second;
    data->states.insert(std::move(node));
    changed = true;
  }
  if (!remove_name.empty()) {
    data->states.erase(remove_name);
    changed = true;
  }

  if (ImGui::Button("+ Add State")) {
    std::string new_name;
    for (const char* cand : kCursorStateNames) {
      if (!data->states.contains(cand)) {
        new_name = cand;
        break;
      }
    }
    if (new_name.empty()) {
      new_name = "state_" + std::to_string(data->states.size() + 1);
    }
    data->states[new_name] = {};
    changed = true;
  }

  if (changed) {
    AssetRegistry::Save(handle);
    Engine::cursor_manager().RefreshCursorSet(handle);
  }
  return changed;
}

// ---- Deferred reload queue ----

namespace {
std::vector<AssetHandle>& PendingReloads() {
  static std::vector<AssetHandle> v;
  return v;
}
}  // namespace

void QueueAssetReload(AssetHandle handle) {
  if (!handle.IsValid()) {
    return;
  }
  auto& q = PendingReloads();
  for (const AssetHandle& existing : q) {
    if (existing == handle) {
      return;
    }
  }
  q.push_back(handle);
}

void DrainPendingAssetReloads() {
  auto& q = PendingReloads();
  if (q.empty()) {
    return;
  }
  AssetManager& mgr = Engine::asset_manager();
  for (AssetHandle h : q) {
    mgr.Unload(h);  // fires AssetUnloadedEvent which drops the thumbnail cache
    mgr.LoadSync(h);
  }
  q.clear();
}

// ---- Built-in renderers: sprite ----

static bool RenderSpriteAsset(AssetHandle handle) {
  AssetManager& mgr = Engine::asset_manager();
  auto sprite = mgr.LoadAndGet<SpriteAssetData>(handle);
  if (!sprite) {
    ImGui::TextDisabled("Sprite not loaded.");
    return false;
  }

  bool changed = false;

  AssetHandle tex_handle = sprite->texture_handle;
  if (AssetCombo(PrefixLabel("Texture").c_str(), AssetType::Texture, tex_handle,
                 false)) {
    sprite->texture_handle = tex_handle;
    changed = true;
  }

  // Resolve dimensions for clamping + a preview.
  std::shared_ptr<Texture> tex;
  if (sprite->texture_handle.IsValid()) {
    tex = mgr.LoadAndGet<Texture>(sprite->texture_handle);
  }
  float tex_w = tex ? static_cast<float>(tex->width_) : 0.0f;
  float tex_h = tex ? static_cast<float>(tex->height_) : 0.0f;
  if (tex) {
    ImGui::TextDisabled("Texture: %dx%d", static_cast<int>(tex_w),
                        static_cast<int>(tex_h));
  }

  if (ImGui::DragFloat4(PrefixLabel("Rect (x,y,w,h)").c_str(),
                        &sprite->rect.x, 1.0f, 0.0f,
                        std::max(tex_w, tex_h))) {
    if (tex_w > 0.0f && tex_h > 0.0f) {
      // Leave at least 1px of room for w/h on each axis - otherwise the
      // hi value of clamp(z, 1, tex_w - rect.x) drops below the lo and crashesw
      sprite->rect.x = std::clamp(sprite->rect.x, 0.0f,
                                  std::max(0.0f, tex_w - 1.0f));
      sprite->rect.y = std::clamp(sprite->rect.y, 0.0f,
                                  std::max(0.0f, tex_h - 1.0f));
      sprite->rect.z =
          std::clamp(sprite->rect.z, 1.0f,
                     std::max(1.0f, tex_w - sprite->rect.x));
      sprite->rect.w =
          std::clamp(sprite->rect.w, 1.0f,
                     std::max(1.0f, tex_h - sprite->rect.y));
    }
    changed = true;
  }
  if (ImGui::DragFloat2(PrefixLabel("Pivot").c_str(), &sprite->pivot.x,
                        0.01f, 0.0f, 1.0f)) {
    changed = true;
  }
  // Region preview is intentionally omitted - the inspector header already
  // shows a thumbnail of the cropped sprite.

  if (changed) {
    AssetRegistry::Save(handle);
    // Defer the Unload+LoadSync until after ImGui presents - mid-frame
    // descriptor teardown corrupts the queued ImGui::Image draws.
    QueueAssetReload(handle);
  }
  return changed;
}

// ---- Built-in renderers: skybox ----

static bool RenderSkyboxAsset(AssetHandle handle) {
  AssetManager& mgr = Engine::asset_manager();
  auto data = mgr.LoadAndGet<SkyboxAssetData>(handle);
  if (!data) {
    ImGui::TextDisabled("Skybox not loaded.");
    return false;
  }

  bool changed = false;

  const char* type_names[] = {"Panorama (2:1)", "Cubemap (6 faces)",
                              "Cross (4x3)"};
  int type = static_cast<int>(data->type);
  if (ImGui::Combo(PrefixLabel("Type").c_str(), &type, type_names, 3)) {
    data->type = static_cast<SkyboxType>(type);
    changed = true;
  }

  if (data->type == SkyboxType::Panorama || data->type == SkyboxType::Cross) {
    if (AssetCombo(PrefixLabel("Source Image").c_str(), AssetType::Texture,
                   data->source_handle)) {
      changed = true;
    }
  } else {
    const char* face_labels[] = {"Right (+X)",  "Left (-X)",  "Top (+Y)",
                                 "Bottom (-Y)", "Front (+Z)", "Back (-Z)"};
    for (int i = 0; i < 6; i++) {
      ImGui::PushID(i);
      if (AssetCombo(PrefixLabel(face_labels[i]).c_str(), AssetType::Texture,
                     data->face_handles[i])) {
        changed = true;
      }
      ImGui::PopID();
    }
  }

  if (changed) {
    AssetRegistry::Save(handle);
    // If this skybox is on the active scene, re-set it so the renderer
    // rebuilds the cubemap GPU resources from the new source(s).
    Scene* s = Engine::scene_manager().GetActiveScene();
    if (s && s->GetSkyboxAsset() == handle) {
      s->SetSkyboxAsset(handle);
    }
  }
  return changed;
}

// ---- Built-in renderers: mesh collider ----

static bool RenderMeshColliderAsset(AssetHandle handle) {
  AssetManager& mgr = Engine::asset_manager();
  auto data = mgr.LoadAndGet<MeshColliderAssetData>(handle);
  if (!data) {
    ImGui::TextDisabled("Mesh collider not loaded.");
    return false;
  }

  bool changed = false;

  if (AssetCombo(PrefixLabel("Source Model").c_str(), AssetType::Model,
                 data->source_model)) {
    changed = true;
  }

  ImGui::TextDisabled("Vertices: %d  Indices: %d",
                      static_cast<int>(data->vertices.size()),
                      static_cast<int>(data->indices.size()));

  ImGui::Separator();
  bool can_bake = data->source_model.IsValid();
  if (!can_bake) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Bake from Source Model")) {
    auto baked = BakeMeshColliderFromModel(data->source_model);
    if (baked) {
      data->vertices = std::move(baked->vertices);
      data->indices = std::move(baked->indices);
      data->cached_shape = baked->cached_shape;
      AssetRegistry::Save(handle);
      // Defer reload until after present so dependent shape caches refresh
      // without ImGui's mid-frame descriptors getting clobbered.
      QueueAssetReload(handle);
    }
  }
  if (!can_bake) {
    ImGui::EndDisabled();
  }
  if (changed) {
    AssetRegistry::Save(handle);
  }
  return changed;
}

// ---- Install ----

void InstallEditorAssetUI() {
  AssetUiRegistry::SetRenderProperties(AssetType::Texture,
                                       RenderTextureProperties);
  AssetUiRegistry::SetRenderProperties(AssetType::Font, RenderFontProperties);
  AssetUiRegistry::SetRenderProperties(AssetType::UIDocument,
                                       RenderUIDocumentProperties);

  AssetUiRegistry::SetRenderAsset(AssetType::Material, RenderMaterialAsset);
  AssetUiRegistry::SetRenderAsset(AssetType::Sprite, RenderSpriteAsset);
  AssetUiRegistry::SetRenderAsset(AssetType::Skybox, RenderSkyboxAsset);
  AssetUiRegistry::SetRenderAsset(AssetType::MeshCollider,
                                  RenderMeshColliderAsset);
  AssetUiRegistry::SetRenderAsset(AssetType::CursorSet, RenderCursorSetAsset);
}

const char* AssetTypeIcon(AssetType type) {
  switch (type) {
    case AssetType::Texture:        return ICON_LC_IMAGE;
    case AssetType::Model:          return ICON_LC_BOX;
    case AssetType::Material:       return ICON_LC_PAINTBRUSH;
    case AssetType::Shader:         return ICON_LC_FILE_CODE;
    case AssetType::Sprite:         return ICON_LC_IMAGE;
    case AssetType::Skybox:         return ICON_LC_CLOUD;
    case AssetType::Font:           return ICON_LC_TYPE;
    case AssetType::Script:         return ICON_LC_FILE_CODE;
    case AssetType::Scene:          return ICON_LC_LAYERS_2;
    case AssetType::Prefab:         return ICON_LC_BOXES;
    case AssetType::Audio:          return ICON_LC_MUSIC;
    case AssetType::AnimClip:       return ICON_LC_FILM;
    case AssetType::AnimController: return ICON_LC_GIT_BRANCH;
    case AssetType::UIDocument:     return ICON_LC_LAYOUT_TEMPLATE;
    case AssetType::UIStylesheet:   return ICON_LC_BRUSH;
    case AssetType::CursorSet:      return ICON_LC_MOUSE_POINTER;
    case AssetType::MeshCollider:   return ICON_LC_BOX;
    default:                        return "";
  }
}

}  // namespace wiesel::editor
