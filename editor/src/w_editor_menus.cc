//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "asset/w_asset_manager.h"
#include "asset/w_asset_registry.h"
#include "cursor/w_cursor.h"
#include "game/w_game_loader.h"
#include "input/w_input.h"
#include "physics/w_mesh_collider_asset.h"
#include "rendering/w_renderer.h"
#include "rendering/w_skybox.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "rendering/w_texture.h"
#include "scene/w_prefab.h"
#include "scene/w_scene_manager.h"
#include "script/w_scriptmanager.h"
#include "util/imgui/imgui_theme.h"
#include "util/imgui/w_imguiutil.h"
#include "util/w_gamepadcodes.h"
#include "util/w_keycodes.h"
#include "util/w_natural_sort.h"
#include "util/w_platform.h"
#include "util/w_thread_pool.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

namespace Wiesel::Editor {

// Defined in w_editor.cc
std::shared_ptr<Scene> scene();

static ThumbnailEntry GetOrCreateThumbnail(AssetHandle handle,
                                           const AssetMetadata& meta) {
  return ThumbnailCache::Get()->GetOrCreate(handle, meta);
}

// Reusable asset picker combo. Shows all assets of the given type with
// thumbnail previews on hover. Returns true if selection changed.
// `selected` is updated to the chosen handle (or null if "(None)" picked).
// `allow_none` adds a "(None)" option at the top.
static bool AssetCombo(const char* label, AssetType type, AssetHandle& selected,
                       bool allow_none = true,
                       const char* none_label = "(None)") {
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

    auto assets = Engine::asset_manager().GetAllOfType(type);
    // Sort by name naturally
    std::sort(assets.begin(), assets.end(),
              [](const AssetHandle& a, const AssetHandle& b) {
                const auto* ma = Engine::asset_manager().GetMetadata(a);
                const auto* mb = Engine::asset_manager().GetMetadata(b);
                if (!ma || !mb) {
                  return false;
                }
                return NaturalLess(ma->name, mb->name);
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

      // Thumbnail preview on hover
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

  // Also show thumbnail next to the combo for the current selection
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

static AssetHandle CreateSpriteAsset(const std::string& vfs_path,
                                     const AssetHandle& texture_handle, float x,
                                     float y, float w, float h, float pivot_x,
                                     float pivot_y) {
  auto data = std::make_shared<SpriteAssetData>();
  data->texture_handle = texture_handle;
  data->rect = {x, y, w, h};
  data->pivot = {pivot_x, pivot_y};
  std::string name = VirtualFileSystem::Stem(vfs_path);
  return AssetRegistry::Create<SpriteAssetData>(name, AssetType::Sprite,
                                                vfs_path, data);
}

void EditorLayer::RenderCreateSkyboxPopup() {
  if (show_create_skybox_) {
    ImGui::OpenPopup("Create Skybox");
    show_create_skybox_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Skybox", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name_buf[128] = "skybox";
    static int skybox_type = 0;
    static AssetHandle source_handle;
    static std::array<AssetHandle, 6> face_handles;

    ImGui::InputText("Name", name_buf, sizeof(name_buf));

    const char* type_names[] = {"Panorama (2:1)", "Cubemap (6 faces)",
                                "Cross (4x3)"};
    ImGui::Combo("Type", &skybox_type, type_names, 3);

    if (skybox_type == 0 || skybox_type == 2) {
      AssetCombo("Source Image", AssetType::Texture, source_handle);
    } else {
      const char* face_labels[] = {"Right (+X)",  "Left (-X)",  "Top (+Y)",
                                   "Bottom (-Y)", "Front (+Z)", "Back (-Z)"};
      for (int i = 0; i < 6; i++) {
        ImGui::PushID(i);
        AssetCombo(face_labels[i], AssetType::Texture, face_handles[i]);
        ImGui::PopID();
      }
    }

    ImGui::Separator();

    bool can_create = name_buf[0] != '\0';
    if (skybox_type == 0 || skybox_type == 2) {
      can_create = can_create && source_handle.IsValid();
    } else {
      for (int i = 0; i < 6; i++) {
        if (!face_handles[i].IsValid()) {
          can_create = false;
          break;
        }
      }
    }

    if (ImGui::Button("Create") && can_create) {
      auto data = std::make_shared<SkyboxAssetData>();
      if (skybox_type == 0) {
        data->type = SkyboxType::Panorama;
        data->source_handle = source_handle;
      } else if (skybox_type == 1) {
        data->type = SkyboxType::Cubemap;
        for (int i = 0; i < 6; i++) {
          data->face_handles[i] = face_handles[i];
        }
      } else {
        data->type = SkyboxType::Cross;
        data->source_handle = source_handle;
      }

      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wskybox";
      AssetHandle new_handle = AssetRegistry::Create<SkyboxAssetData>(
          name_buf, AssetType::Skybox, vfs_path, data);
      if (new_handle.IsValid()) {
        ScanProjectAssets();
        scene()->SetSkyboxAsset(new_handle);
        scene_dirty_ = true;
      }

      name_buf[0] = '\0';
      source_handle = {};
      face_handles = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      source_handle = {};
      face_handles = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderCreateCursorSetPopup() {
  if (show_create_cursorset_) {
    ImGui::OpenPopup("Create Cursor Set");
    show_create_cursorset_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Cursor Set", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name_buf[128] = "cursors";
    static int mode = 0;

    struct StateEntry {
      char name[64] = "";
      AssetHandle texture;
      int hotspot_x = 0;
      int hotspot_y = 0;
      float frame_duration = 0.1f;
      std::vector<AssetHandle> frames;
      bool use_animation = false;
    };

    static std::vector<StateEntry> states;

    if (states.empty()) {
      StateEntry default_state;
      strncpy(default_state.name, "default", sizeof(default_state.name));
      states.push_back(default_state);
    }

    ImGui::InputText("Name", name_buf, sizeof(name_buf));

    const char* mode_names[] = {"Auto (Hardware)", "Force Software"};
    ImGui::Combo("Mode", &mode, mode_names, 2);

    ImGui::SeparatorText("Cursor States");

    int remove_idx = -1;
    for (int i = 0; i < static_cast<int>(states.size()); i++) {
      auto& s = states[i];
      ImGui::PushID(i);

      ImGui::SetNextItemWidth(120);
      ImGui::InputText("##name", s.name, sizeof(s.name));
      ImGui::SameLine();

      if (!s.use_animation) {
        AssetCombo("Texture", AssetType::Texture, s.texture);
      } else {
        ImGui::Text("Frames: %d", static_cast<int>(s.frames.size()));
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Frame")) {
          s.frames.push_back({});
        }
        for (int f = 0; f < static_cast<int>(s.frames.size()); f++) {
          ImGui::PushID(1000 + f);
          AssetCombo(("Frame " + std::to_string(f)).c_str(), AssetType::Texture,
                     s.frames[f]);
          ImGui::PopID();
        }
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("Frame Duration", &s.frame_duration, 0.01f, 0.01f,
                         1.0f, "%.2fs");
      }

      ImGui::Checkbox("Animated", &s.use_animation);

      ImGui::SetNextItemWidth(60);
      ImGui::DragInt("Hotspot X", &s.hotspot_x, 1, 0, 256);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      ImGui::DragInt("Hotspot Y", &s.hotspot_y, 1, 0, 256);

      ImGui::SameLine();
      if (i > 0 && ImGui::SmallButton("X")) {
        remove_idx = i;
      }

      ImGui::Separator();
      ImGui::PopID();
    }

    if (remove_idx >= 0) {
      states.erase(states.begin() + remove_idx);
    }

    if (ImGui::Button("+ Add State")) {
      StateEntry new_state;
      snprintf(new_state.name, sizeof(new_state.name), "state_%d",
               static_cast<int>(states.size()));
      states.push_back(new_state);
    }

    ImGui::Separator();

    bool can_create = name_buf[0] != '\0' && !states.empty();
    if (ImGui::Button("Create") && can_create) {
      auto data = std::make_shared<CursorSetData>();
      data->mode =
          (mode == 0) ? CursorSetMode::Auto : CursorSetMode::ForceSoftware;

      for (const auto& s : states) {
        if (s.name[0] == '\0') {
          continue;
        }
        CursorStateEntry entry;
        entry.hotspot = {s.hotspot_x, s.hotspot_y};
        entry.frame_duration = s.frame_duration;

        if (s.use_animation && !s.frames.empty()) {
          for (const auto& f : s.frames) {
            CursorFrame frame;
            frame.texture = f;
            entry.frames.push_back(frame);
          }
        } else if (s.texture.IsValid()) {
          CursorFrame frame;
          frame.texture = s.texture;
          entry.frames.push_back(frame);
        }

        data->states[s.name] = std::move(entry);
      }

      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wcursorset";
      AssetHandle new_handle = AssetRegistry::Create<CursorSetData>(
          name_buf, AssetType::CursorSet, vfs_path, data);
      if (new_handle.IsValid()) {
        ScanProjectAssets();
        if (scene()) {
          scene()->SetCursorSetAsset(new_handle);
          scene_dirty_ = true;
        }
      }

      name_buf[0] = '\0';
      states.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      states.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderCreateMeshColliderPopup() {
  if (show_create_meshcollider_) {
    ImGui::OpenPopup("Create Mesh Collider");
    show_create_meshcollider_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Mesh Collider", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name_buf[128] = "mesh_collider";
    static AssetHandle source_model;

    ImGui::InputText("Name", name_buf, sizeof(name_buf));
    AssetCombo("Source Model", AssetType::Model, source_model);

    ImGui::Separator();

    bool can_create = name_buf[0] != '\0' && source_model.IsValid();
    if (ImGui::Button("Bake") && can_create) {
      auto data = BakeMeshColliderFromModel(source_model);
      if (data) {
        std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                               std::string(name_buf) + ".wmeshcol";
        AssetHandle new_handle = AssetRegistry::Create<MeshColliderAssetData>(
            name_buf, AssetType::MeshCollider, vfs_path, data);
        if (new_handle.IsValid()) {
          AssetRegistry::Save(new_handle);
          ScanProjectAssets();
        }
      }

      name_buf[0] = '\0';
      source_model = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      source_model = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderCreateSpritePopup() {
  if (show_create_sprite_) {
    ImGui::OpenPopup("Create Sprite");
    show_create_sprite_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Create Sprite", nullptr, ImGuiWindowFlags_None)) {
    static char name_buf[128] = "sprite";
    static AssetHandle texture_handle;
    static float rect[4] = {0, 0, 64, 64};  // x, y, w, h in pixels
    static float pivot[2] = {0.5f, 0.5f};

    ImGui::InputText("Name", name_buf, sizeof(name_buf));
    AssetCombo("Texture", AssetType::Texture, texture_handle, false);

    // Show texture preview with rect overlay
    if (texture_handle.IsValid()) {
      auto tex = Engine::asset_manager().Get<Texture>(texture_handle);
      if (!tex) {
        Engine::asset_manager().LoadSync(texture_handle);
        tex = Engine::asset_manager().Get<Texture>(texture_handle);
      }
      if (tex) {
        float tex_w = static_cast<float>(tex->width_);
        float tex_h = static_cast<float>(tex->height_);

        ImGui::Text("Texture: %dx%d", static_cast<int>(tex_w),
                    static_cast<int>(tex_h));

        ImGui::DragFloat4("Rect (x, y, w, h)", rect, 1.0f, 0.0f,
                          std::max(tex_w, tex_h));
        ImGui::DragFloat2("Pivot", pivot, 0.01f, 0.0f, 1.0f);

        // Clamp rect to texture bounds
        rect[0] = std::clamp(rect[0], 0.0f, tex_w);
        rect[1] = std::clamp(rect[1], 0.0f, tex_h);
        rect[2] = std::clamp(rect[2], 1.0f, tex_w - rect[0]);
        rect[3] = std::clamp(rect[3], 1.0f, tex_h - rect[1]);

        // Preview
        ImGui::Separator();
        ImGui::Text("Preview:");
        VkDescriptorSet desc = tex->GetImGuiDescriptor();
        if (desc) {
          // Show just the selected sub-region
          ImVec2 uv0(rect[0] / tex_w, rect[1] / tex_h);
          ImVec2 uv1((rect[0] + rect[2]) / tex_w, (rect[1] + rect[3]) / tex_h);
          float aspect = rect[2] / rect[3];
          float preview_h = 128.0f;
          float preview_w = preview_h * aspect;
          ImGui::Image(reinterpret_cast<ImTextureID>(desc),
                       ImVec2(preview_w, preview_h), uv0, uv1);
        }
      }
    }

    ImGui::Separator();
    bool can_create = name_buf[0] != '\0' && texture_handle.IsValid();
    if (ImGui::Button("Create") && can_create) {
      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wsprite";
      CreateSpriteAsset(vfs_path, texture_handle, rect[0], rect[1], rect[2],
                        rect[3], pivot[0], pivot[1]);
      ScanProjectAssets();
      name_buf[0] = '\0';
      texture_handle = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      texture_handle = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorLayer::RenderSliceSpritesPopup() {
  if (show_slice_sprites_) {
    ImGui::OpenPopup("Slice into Sprites");
    show_slice_sprites_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Slice into Sprites", nullptr,
                             ImGuiWindowFlags_None)) {
    static char prefix_buf[128] = "sprite";
    static int columns = 6;
    static int rows = 1;
    static float pivot[2] = {0.5f, 0.5f};

    auto tex = slice_texture_handle_.IsValid()
                   ? Engine::asset_manager().Get<Texture>(slice_texture_handle_)
                   : nullptr;
    if (!tex) {
      ImGui::Text("Texture not loaded.");
      if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
      return;
    }

    float tex_w = static_cast<float>(tex->width_);
    float tex_h = static_cast<float>(tex->height_);
    const auto* meta =
        Engine::asset_manager().GetMetadata(slice_texture_handle_);
    ImGui::Text("Texture: %s (%dx%d)", meta ? meta->name.c_str() : "?",
                static_cast<int>(tex_w), static_cast<int>(tex_h));

    ImGui::InputText("Name Prefix", prefix_buf, sizeof(prefix_buf));
    ImGui::InputInt("Columns", &columns);
    ImGui::InputInt("Rows", &rows);
    ImGui::DragFloat2("Pivot", pivot, 0.01f, 0.0f, 1.0f);

    columns = std::max(1, columns);
    rows = std::max(1, rows);

    float cell_w = tex_w / static_cast<float>(columns);
    float cell_h = tex_h / static_cast<float>(rows);
    int total = columns * rows;

    ImGui::Text("Cell size: %.0fx%.0f | Total sprites: %d", cell_w, cell_h,
                total);

    // Texture preview with grid overlay
    ImGui::Separator();
    VkDescriptorSet desc = tex->GetImGuiDescriptor();
    if (desc) {
      // Fit preview to available width
      float avail_w = ImGui::GetContentRegionAvail().x;
      float avail_h = ImGui::GetContentRegionAvail().y - 50.0f;
      float scale = std::min(avail_w / tex_w, avail_h / tex_h);
      scale = std::min(scale, 1.0f);  // don't upscale
      float preview_w = tex_w * scale;
      float preview_h = tex_h * scale;

      ImVec2 cursor = ImGui::GetCursorScreenPos();
      ImGui::Image(reinterpret_cast<ImTextureID>(desc),
                   ImVec2(preview_w, preview_h));

      // Draw grid lines
      ImDrawList* dl = ImGui::GetWindowDrawList();
      ImU32 grid_col = IM_COL32(255, 255, 0, 180);

      for (int c = 1; c < columns; c++) {
        float x = cursor.x + (static_cast<float>(c) / columns) * preview_w;
        dl->AddLine(ImVec2(x, cursor.y), ImVec2(x, cursor.y + preview_h),
                    grid_col);
      }
      for (int r = 1; r < rows; r++) {
        float y = cursor.y + (static_cast<float>(r) / rows) * preview_h;
        dl->AddLine(ImVec2(cursor.x, y), ImVec2(cursor.x + preview_w, y),
                    grid_col);
      }

      // Draw border
      dl->AddRect(cursor, ImVec2(cursor.x + preview_w, cursor.y + preview_h),
                  grid_col);

      // Draw cell index labels
      for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
          int idx = r * columns + c;
          float cx = cursor.x + (c + 0.5f) / columns * preview_w;
          float cy = cursor.y + (r + 0.5f) / rows * preview_h;
          std::string label = std::to_string(idx);
          ImVec2 text_sz = ImGui::CalcTextSize(label.c_str());
          dl->AddText(ImVec2(cx - text_sz.x * 0.5f, cy - text_sz.y * 0.5f),
                      IM_COL32(255, 255, 255, 200), label.c_str());
        }
      }
    }

    ImGui::Separator();
    bool can_create = prefix_buf[0] != '\0';
    if (ImGui::Button("Slice") && can_create) {
      std::string base_vfs = asset_browser_panel_.browser().CurrentVfsDir();
      for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
          int idx = r * columns + c;
          std::string name =
              std::string(prefix_buf) + "_" + std::to_string(idx);
          std::string vfs_path = base_vfs + name + ".wsprite";
          CreateSpriteAsset(vfs_path, slice_texture_handle_, c * cell_w,
                            r * cell_h, cell_w, cell_h, pivot[0], pivot[1]);
        }
      }
      ScanProjectAssets();
      prefix_buf[0] = '\0';
      slice_texture_handle_ = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      prefix_buf[0] = '\0';
      slice_texture_handle_ = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorLayer::RenderCreateAnimControllerPopup() {
  if (show_create_animcontroller_) {
    ImGui::OpenPopup("Create Animation Controller");
    show_create_animcontroller_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Animation Controller", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name_buf[128] = "controller";
    ImGui::InputText("Name", name_buf, sizeof(name_buf));

    if (ImGui::Button("Create") && name_buf[0] != '\0') {
      auto data = std::make_shared<AnimControllerAssetData>();
      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wanimcontroller";
      AssetHandle handle = AssetRegistry::Create<AnimControllerAssetData>(
          name_buf, AssetType::AnimController, vfs_path, data);
      if (handle.IsValid()) {
        anim_controller_editor_.Open(handle, data);
        ScanProjectAssets();
      }
      std::strcpy(name_buf, "controller");
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorLayer::RenderProjectSettingsPopup() {
  if (!active_project_) {
    return;
  }

  if (show_project_settings_) {
    ImGui::OpenPopup("Project Settings");
    show_project_settings_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(720, 500), ImGuiCond_Appearing);

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Project Settings", &popup_open,
                             ImGuiWindowFlags_NoScrollbar)) {
    auto& proj_settings = active_project_->GetSettings();
    auto& game_info = active_project_->GetGameInfo();
    bool changed = false;

    const char* categories[] = {"Scene", "Rendering", "Input"};
    constexpr int kCategoryCount = 3;

    // Left panel: category list
    // ItemSpacing.x = 2*WindowPadding so selectable highlight extends to child edges,
    // while text remains indented by WindowPadding (the cursor offset).
    float pad = ImGui::GetStyle().WindowPadding.x;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(pad * 2.0f, ImGui::GetStyle().ItemSpacing.y));
    ImGui::BeginChild("##categories", ImVec2(140, 0), ImGuiChildFlags_Borders);
    ImGui::PopStyleVar();
    for (int i = 0; i < kCategoryCount; i++) {
      if (ImGui::Selectable(categories[i], project_settings_category_ == i)) {
        project_settings_category_ = i;
      }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: settings
    ImGui::BeginChild("##settings", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (project_settings_category_ == 0) {
      // ---- Scene ----
      ImGui::SeparatorText("Project");
      char name_buf[128];
      strncpy(name_buf, proj_settings.name.c_str(), sizeof(name_buf) - 1);
      name_buf[sizeof(name_buf) - 1] = '\0';
      if (ImGui::InputText(PrefixLabel("Project Name").c_str(), name_buf,
                           sizeof(name_buf))) {
        proj_settings.name = name_buf;
        changed = true;
      }

      ImGui::SeparatorText("Game");
      {
        char game_name_buf[128];
        strncpy(game_name_buf, game_info.name.c_str(),
                sizeof(game_name_buf) - 1);
        game_name_buf[sizeof(game_name_buf) - 1] = '\0';
        if (ImGui::InputText(PrefixLabel("Game Name").c_str(), game_name_buf,
                             sizeof(game_name_buf))) {
          game_info.name = game_name_buf;
          changed = true;
        }
      }
      {
        char version_buf[64];
        strncpy(version_buf, game_info.version.c_str(),
                sizeof(version_buf) - 1);
        version_buf[sizeof(version_buf) - 1] = '\0';
        if (ImGui::InputText(PrefixLabel("Version").c_str(), version_buf,
                             sizeof(version_buf))) {
          game_info.version = version_buf;
          changed = true;
        }
      }
      if (AssetCombo("Icon", AssetType::Texture, game_info.icon, true,
                     "(None)")) {
        changed = true;
      }

      // Start scene selector
      if (AssetCombo(PrefixLabel("Start Scene").c_str(), AssetType::Scene,
                     game_info.start_scene, true, "(None)")) {
        changed = true;
      }

      ImGui::SeparatorText("Skybox");
      {
        AssetHandle skybox_handle = scene()->GetSkyboxAsset();
        if (AssetCombo(PrefixLabel("Skybox").c_str(), AssetType::Skybox,
                       skybox_handle, true, "(Default)")) {
          scene()->SetSkyboxAsset(skybox_handle);
          scene_dirty_ = true;
        }
      }

      ImGui::SeparatorText("Cursor");
      {
        AssetHandle cursor_handle = scene()->GetCursorSetAsset();
        if (AssetCombo(PrefixLabel("Cursor Set").c_str(), AssetType::CursorSet,
                       cursor_handle, true, "(None)")) {
          scene()->SetCursorSetAsset(cursor_handle);
          scene_dirty_ = true;
        }
      }

      ImGui::SeparatorText("Asset Loading");
      {
        bool keep_loaded = scene()->GetKeepAssetsLoaded();
        if (ImGui::Checkbox(PrefixLabel("Keep Assets Loaded").c_str(),
                            &keep_loaded)) {
          scene()->SetKeepAssetsLoaded(keep_loaded);
          scene_dirty_ = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "When enabled, assets from this scene are not unloaded\n"
              "when switching to another scene. Useful for loading screens.");
        }

        bool preload = scene()->GetPreloadAssets();
        if (ImGui::Checkbox(PrefixLabel("Preload Assets").c_str(), &preload)) {
          scene()->SetPreloadAssets(preload);
          scene_dirty_ = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "When enabled, assets for this scene are loaded\n"
              "when the project opens, even if the scene is not active.\n"
              "Useful for scenes that need instant transitions.");
        }
      }

      ImGui::SeparatorText("Physics");
      {
        auto& physics = scene()->GetPhysicsWorld();
        glm::vec3 gravity = physics.GetGravity();
        if (ImGui::DragFloat3(PrefixLabel("Gravity").c_str(), &gravity.x,
                              0.1f)) {
          physics.SetGravity(gravity);
        }
      }

    } else if (project_settings_category_ == 1) {
      // ---- Rendering ----
      Renderer* renderer = Engine::renderer().get();
      auto& settings = renderer->options();

      ImGui::SeparatorText("Ambient");
      ImGui::ColorEdit3(PrefixLabel("Ambient Color").c_str(),
                        &settings.ambient_color.x);
      ImGui::SliderFloat(PrefixLabel("Ambient Intensity").c_str(),
                         &settings.ambient_intensity, 0.0f, 1.0f);

      ImGui::SeparatorText("General");
      ImGui::Checkbox(PrefixLabel("Enable SSAO").c_str(),
                      &settings.ssao_enabled);
      ImGui::Checkbox(PrefixLabel("Enable IBL").c_str(), &settings.ibl_enabled);
      ImGui::Checkbox(PrefixLabel("Enable Vsync").c_str(), &settings.vsync);
      ImGui::Checkbox(PrefixLabel("Shadows").c_str(),
                      &settings.shadows_enabled);
      if (settings.shadows_enabled && renderer->IsRayTracingSupported()) {
        ImGui::Checkbox(PrefixLabel("RT Shadows").c_str(),
                        &settings.rt_shadows_enabled);
      }

      {
        const char* aa_labels[] = {"None", "FXAA", "TAA"};
        int aa_current =
            static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));
        if (ImGui::Combo(PrefixLabel("Anti-Aliasing").c_str(), &aa_current,
                         aa_labels, IM_ARRAYSIZE(aa_labels))) {
          settings.aa_mode = static_cast<AntiAliasingMode>(aa_current);
        }
      }

      {
        std::vector<SamplingMode> supported_values =
            renderer->GetSupportedSamplingModes();
        std::vector<const char*> labels;
        SamplingMode current_mode = settings.msaa_mode;
        int selected_index = 0;
        for (size_t i = 0; i < supported_values.size(); i++) {
          labels.push_back(ToString(supported_values[i]));
          if (supported_values[i] == current_mode) {
            selected_index = static_cast<int>(i);
          }
        }
        if (ImGui::Combo(PrefixLabel("MSAA Samples").c_str(), &selected_index,
                         labels.data(), labels.size())) {
          settings.msaa_mode = supported_values[selected_index];
        }
      }

      ImGui::SeparatorText("Quality");
      {
        const char* shadow_labels[] = {"Low (512)", "Medium (1024)",
                                       "High (2048)", "Ultra (4096)"};
        int shadow_res = settings.shadow_map_resolution;
        int shadow_idx = 3;
        if (shadow_res <= 512) {
          shadow_idx = 0;
        } else if (shadow_res <= 1024) {
          shadow_idx = 1;
        } else if (shadow_res <= 2048) {
          shadow_idx = 2;
        }
        if (ImGui::Combo(PrefixLabel("Shadow Quality").c_str(), &shadow_idx,
                         shadow_labels, IM_ARRAYSIZE(shadow_labels))) {
          int resolutions[] = {512, 1024, 2048, 4096};
          settings.shadow_map_resolution = resolutions[shadow_idx];
        }
      }

      {
        const char* aniso_labels[] = {"Off", "2x", "4x", "8x", "16x"};
        int aniso_values[] = {1, 2, 4, 8, 16};
        int aniso_current = settings.anisotropic_filtering;
        int aniso_idx = 4;
        for (int i = 0; i < 5; i++) {
          if (aniso_values[i] == aniso_current) {
            aniso_idx = i;
            break;
          }
        }
        if (ImGui::Combo(PrefixLabel("Anisotropic Filtering").c_str(),
                         &aniso_idx, aniso_labels,
                         IM_ARRAYSIZE(aniso_labels))) {
          settings.anisotropic_filtering = aniso_values[aniso_idx];
        }
      }

      {
        const char* tex_labels[] = {"Full", "High (1/2)", "Medium (1/4)",
                                    "Low (1/8)"};
        int tex_quality = settings.texture_quality;
        if (ImGui::Combo(PrefixLabel("Texture Quality").c_str(), &tex_quality,
                         tex_labels, IM_ARRAYSIZE(tex_labels))) {
          int old_quality = settings.texture_quality;
          settings.texture_quality = tex_quality;
          if (old_quality != tex_quality) {
            Engine::app().SubmitToMainThread([]() {
              Engine::asset_manager().ReloadAllOfType(AssetType::Texture);
            });
          }
        }
      }

      ImGui::SeparatorText("Post Processing");
      ImGui::Checkbox(PrefixLabel("Enable Bloom").c_str(),
                      &settings.bloom_enabled);
      if (settings.bloom_enabled) {
        ImGui::SliderFloat(PrefixLabel("Threshold").c_str(),
                           &settings.bloom_threshold, 0.0f, 2.0f);
        ImGui::SliderFloat(PrefixLabel("Intensity").c_str(),
                           &settings.bloom_intensity, 0.0f, 5.0f);
        ImGui::SliderFloat(PrefixLabel("Scatter").c_str(),
                           &settings.bloom_scatter, 0.0f, 1.0f);
        glm::vec3 tint = settings.bloom_tint;
        if (ImGui::ColorEdit3(PrefixLabel("Tint").c_str(), &tint.x)) {
          settings.bloom_tint = tint;
        }
        ImGui::SliderFloat(PrefixLabel("Clamp").c_str(), &settings.bloom_clamp,
                           0.0f, 65535.0f, "%.0f");
        ImGui::Checkbox(PrefixLabel("High Quality").c_str(),
                        &settings.bloom_high_quality);
      }
      ImGui::Checkbox(PrefixLabel("Enable Motion Blur").c_str(),
                      &settings.motion_blur_enabled);
      if (settings.motion_blur_enabled) {
        ImGui::SliderFloat(PrefixLabel("MB Strength").c_str(),
                           &settings.motion_blur_strength, 0.0f, 3.0f);
        ImGui::SliderInt(PrefixLabel("MB Samples").c_str(),
                         &settings.motion_blur_samples, 2, 16);
      }

      // Sync live renderer options back to project for saving
      GameLoader::CaptureRenderOptions(game_info.render_options);
      changed = true;

    } else if (project_settings_category_ == 2) {
      // ---- Input ----
      auto& input = game_info.input;
      bool input_changed = false;

      ImGui::SeparatorText("Mouse");
      input_changed |=
          ImGui::DragFloat(PrefixLabel("Sensitivity X").c_str(),
                           &input.mouse_sensitivity_x, 1.0f, 1.0f, 500.0f);
      input_changed |=
          ImGui::DragFloat(PrefixLabel("Sensitivity Y").c_str(),
                           &input.mouse_sensitivity_y, 1.0f, 1.0f, 500.0f);

      int gp_count = Engine::input().GetConnectedGamepadCount();
      if (gp_count > 0) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                           "Gamepads: %d connected", gp_count);
      }

      ImGui::SeparatorText("Contexts");

      // Validate selected context still exists
      if (!selected_input_context_.empty() &&
          input.contexts.find(selected_input_context_) ==
              input.contexts.end()) {
        selected_input_context_.clear();
        selected_input_item_ = -1;
      }

      // Left: context list
      ImGui::BeginChild("##ctx_list", ImVec2(130, 0), ImGuiChildFlags_Borders);
      for (auto& [ctx_name, ctx] : input.contexts) {
        if (ImGui::Selectable(ctx_name.c_str(),
                              selected_input_context_ == ctx_name)) {
          selected_input_context_ = ctx_name;
          selected_input_item_ = -1;
        }
      }
      ImGui::Spacing();
      static char new_ctx_name[64] = "";
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##newctx", "new context...", new_ctx_name,
                               sizeof(new_ctx_name));
      if (ImGui::Button("Add", ImVec2(-1, 0)) && new_ctx_name[0] != '\0') {
        std::string cname = new_ctx_name;
        if (input.contexts.find(cname) == input.contexts.end()) {
          InputContext nc;
          nc.name = cname;
          input.contexts[cname] = std::move(nc);
          selected_input_context_ = cname;
          input_changed = true;
          new_ctx_name[0] = '\0';
        }
      }
      ImGui::EndChild();

      ImGui::SameLine();

      // Right: selected context content
      ImGui::BeginChild("##ctx_content", ImVec2(0, 0));
      if (!selected_input_context_.empty() &&
          input.contexts.find(selected_input_context_) !=
              input.contexts.end()) {
        auto& ctx = input.contexts[selected_input_context_];

        // Context header with delete
        ImGui::Text("Context: %s", selected_input_context_.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
        if (ImGui::SmallButton("Delete")) {
          input.contexts.erase(selected_input_context_);
          selected_input_context_.clear();
          selected_input_item_ = -1;
          input_changed = true;
          // Skip rendering rest since ctx is gone
          ImGui::EndChild();
          if (input_changed) {
            Engine::input().LoadFromSettings(input);
            changed = true;
          }
          // Early out handled below
          goto input_done;
        }
        ImGui::Separator();

        static int input_tab = 0;  // 0 = Actions, 1 = Axes
        if (ImGui::BeginTabBar("##input_tabs")) {
          if (ImGui::BeginTabItem("Actions")) {
            input_tab = 0;

            // Table: Name | Keys | Buttons | Delete
            if (ImGui::BeginTable("##actions_table", 4,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                      100);
              ImGui::TableSetupColumn("Keys");
              ImGui::TableSetupColumn("Buttons");
              ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed,
                                      20);
              ImGui::TableHeadersRow();

              int action_to_remove = -1;
              for (int i = 0; i < (int)ctx.actions.size(); i++) {
                ImGui::PushID(i);
                auto& action = ctx.actions[i];

                ImGui::TableNextRow();

                // Name
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                char abuf[128];
                strncpy(abuf, action.name.c_str(), sizeof(abuf) - 1);
                abuf[sizeof(abuf) - 1] = '\0';
                if (ImGui::InputText("##name", abuf, sizeof(abuf))) {
                  action.name = abuf;
                  input_changed = true;
                }

                // Keys (tags + add combo)
                ImGui::TableNextColumn();
                int key_rm = -1;
                for (int k = 0; k < (int)action.keys.size(); k++) {
                  if (k > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(k);
                  ImGui::SmallButton(KeyCodeToString(action.keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    key_rm = k;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (key_rm >= 0) {
                  action.keys.erase(action.keys.begin() + key_rm);
                  input_changed = true;
                }
                if (!action.keys.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addkey");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addkey", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      action.keys.push_back(code);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Buttons (tags + add combo)
                ImGui::TableNextColumn();
                int btn_rm = -1;
                for (int b = 0; b < (int)action.buttons.size(); b++) {
                  if (b > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(b + 200);
                  ImGui::SmallButton(GamepadButtonToString(action.buttons[b]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    btn_rm = b;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (btn_rm >= 0) {
                  action.buttons.erase(action.buttons.begin() + btn_rm);
                  input_changed = true;
                }
                if (!action.buttons.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addbtn");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addbtn", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto btn : GetAllGamepadButtons()) {
                    if (ImGui::Selectable(GamepadButtonToString(btn))) {
                      action.buttons.push_back(btn);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Delete
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) {
                  action_to_remove = i;
                }

                ImGui::PopID();
              }
              ImGui::EndTable();

              if (action_to_remove >= 0) {
                ctx.actions.erase(ctx.actions.begin() + action_to_remove);
                input_changed = true;
              }
            }
            if (ImGui::Button("+ Add Action")) {
              ctx.actions.push_back({"New Action", {}, {}});
              input_changed = true;
            }

            ImGui::EndTabItem();
          }

          if (ImGui::BeginTabItem("Axes")) {
            input_tab = 1;

            // Table: Name | +Keys | -Keys | Stick | Smooth | Delete
            if (ImGui::BeginTable("##axes_table", 6,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                      90);
              ImGui::TableSetupColumn("+Keys");
              ImGui::TableSetupColumn("-Keys");
              ImGui::TableSetupColumn("Stick", ImGuiTableColumnFlags_WidthFixed,
                                      100);
              ImGui::TableSetupColumn("Smooth",
                                      ImGuiTableColumnFlags_WidthFixed, 80);
              ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed,
                                      20);
              ImGui::TableHeadersRow();

              int axis_to_remove = -1;
              for (int i = 0; i < (int)ctx.axes.size(); i++) {
                ImGui::PushID(i + 1000);
                auto& axis = ctx.axes[i];

                ImGui::TableNextRow();

                // Name
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                char abuf[128];
                strncpy(abuf, axis.name.c_str(), sizeof(abuf) - 1);
                abuf[sizeof(abuf) - 1] = '\0';
                if (ImGui::InputText("##name", abuf, sizeof(abuf))) {
                  axis.name = abuf;
                  input_changed = true;
                }

                // Positive keys
                ImGui::TableNextColumn();
                int pk_rm = -1;
                for (int k = 0; k < (int)axis.positive_keys.size(); k++) {
                  if (k > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(k);
                  ImGui::SmallButton(KeyCodeToString(axis.positive_keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    pk_rm = k;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (pk_rm >= 0) {
                  axis.positive_keys.erase(axis.positive_keys.begin() + pk_rm);
                  input_changed = true;
                }
                if (!axis.positive_keys.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addpos");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addpos", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      axis.positive_keys.push_back(code);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Negative keys
                ImGui::TableNextColumn();
                int nk_rm = -1;
                for (int k = 0; k < (int)axis.negative_keys.size(); k++) {
                  if (k > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(k + 500);
                  ImGui::SmallButton(KeyCodeToString(axis.negative_keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    nk_rm = k;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (nk_rm >= 0) {
                  axis.negative_keys.erase(axis.negative_keys.begin() + nk_rm);
                  input_changed = true;
                }
                if (!axis.negative_keys.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addneg");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addneg", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      axis.negative_keys.push_back(code);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Stick
                ImGui::TableNextColumn();
                const char* stick_label =
                    axis.gamepad_axis >= 0
                        ? GamepadAxisToString(axis.gamepad_axis)
                        : "None";
                ImGui::SetNextItemWidth(-1);
                ImGui::PushID("gpaxis");
                if (ImGui::BeginCombo("##stick", stick_label)) {
                  if (ImGui::Selectable("None", axis.gamepad_axis < 0)) {
                    axis.gamepad_axis = -1;
                    input_changed = true;
                  }
                  for (auto ga : GetAllGamepadAxes()) {
                    if (ImGui::Selectable(GamepadAxisToString(ga),
                                          axis.gamepad_axis == ga)) {
                      axis.gamepad_axis = ga;
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Smooth
                ImGui::TableNextColumn();
                ImGui::PushID("smooth");
                if (ImGui::Checkbox("##smooth", &axis.smooth)) {
                  input_changed = true;
                }
                if (axis.smooth) {
                  ImGui::SetNextItemWidth(60);
                  if (ImGui::DragFloat("Grav", &axis.gravity, 0.1f, 0.1f,
                                       50.0f)) {
                    input_changed = true;
                  }
                  ImGui::SetNextItemWidth(60);
                  if (ImGui::DragFloat("Sens", &axis.sensitivity, 0.1f, 0.1f,
                                       50.0f)) {
                    input_changed = true;
                  }
                }
                ImGui::PopID();

                // Delete
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) {
                  axis_to_remove = i;
                }

                ImGui::PopID();
              }
              ImGui::EndTable();

              if (axis_to_remove >= 0) {
                ctx.axes.erase(ctx.axes.begin() + axis_to_remove);
                input_changed = true;
              }
            }
            if (ImGui::Button("+ Add Axis")) {
              ctx.axes.push_back({"New Axis", {}, {}});
              input_changed = true;
            }

            ImGui::EndTabItem();
          }
          ImGui::EndTabBar();
        }
      } else {
        ImGui::TextDisabled("Select a context from the list");
      }
      ImGui::EndChild();

    input_done:
      if (input_changed) {
        Engine::input().LoadFromSettings(input);
        changed = true;
      }
    }

    ImGui::EndChild();

    if (changed) {
      active_project_->Save();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Project...")) {
        NewProject();
      }
      if (ImGui::MenuItem("Open Project...")) {
        OpenProject();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("New Scene", nullptr, false,
                          active_project_ != nullptr)) {
        NewScene();
      }
      if (ImGui::MenuItem("Save", "Ctrl+S", false,
                          active_project_ != nullptr)) {
        SaveScene();
        SaveProject();
      }
      if (ImGui::MenuItem("Save As...", nullptr, false,
                          active_project_ != nullptr)) {
        SaveSceneAs();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Export Game...", nullptr, false,
                          active_project_ != nullptr)) {
        ExportGame();
      }

      ImGui::Separator();

      // Recent Projects
      const auto& recent = RecentProjects::Load();
      if (!recent.empty()) {
        ImGui::TextDisabled("Recent Projects");
        for (size_t i = 0; i < recent.size(); i++) {
          const std::string& path = recent[i];
          std::string label = std::filesystem::path(path).stem().string();
          ImGui::PushID(static_cast<int>(i));
          if (ImGui::MenuItem(label.c_str())) {
            if (std::filesystem::exists(path)) {
              deferred_action_ = DeferredAction::OpenProject;
              deferred_path_ = path;
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", path.c_str());
          }
          ImGui::PopID();
        }
        ImGui::Separator();
      }

      if (ImGui::MenuItem("Close Project", nullptr, false,
                          active_project_ != nullptr)) {
        deferred_action_ = DeferredAction::CloseProject;
      }

      if (ImGui::MenuItem("Exit")) {
        app_.Close();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, command_stack_.CanUndo())) {
        PerformUndo();
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, command_stack_.CanRedo())) {
        PerformRedo();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Clear Scene")) {
        ClearScene();
      }
      if (ImGui::MenuItem("Project Settings", nullptr, false,
                          active_project_ != nullptr)) {
        show_project_settings_ = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Editor Settings")) {
        panel_editor_settings_ = true;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
      ImGui::MenuItem(CODICON_PREVIEW " Scene", nullptr, &panel_scene_view_);
      ImGui::MenuItem(CODICON_CAMERA_VIDEO " Game", nullptr, &panel_game_view_);
      ImGui::MenuItem(CODICON_SYMBOL_RULER " Scene Hierarchy", nullptr,
                      &panel_scene_hierarchy_);
      ImGui::MenuItem(CODICON_INSPECT " Entity Inspector", nullptr,
                      &panel_components_);
      ImGui::MenuItem(CODICON_FOLDER_OPENED " Asset Browser", nullptr,
                      &panel_asset_browser_);
      ImGui::MenuItem(CODICON_TERMINAL " Console", nullptr, &panel_console_);
      ImGui::MenuItem(CODICON_DASHBOARD " Render Stats", nullptr,
                      &panel_stats_);
      ImGui::MenuItem(CODICON_HISTORY " Undo History", nullptr,
                      &panel_undo_history_);
      ImGui::MenuItem(CODICON_INFO " LSP Debug", nullptr, &panel_lsp_debug_);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Layout")) {
        panel_scene_hierarchy_ = true;
        panel_components_ = true;
        panel_asset_browser_ = true;
        panel_console_ = true;
        panel_stats_ = true;
        panel_scene_view_ = true;
        panel_game_view_ = true;
        layout_initialized_ = false;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug")) {
      auto& settings = Engine::renderer()->options();

      ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(),
                      &settings.wireframe_enabled);
      ImGui::Checkbox(PrefixLabel("Only SSAO").c_str(), &settings.only_ssao);
      {
        const char* debug_modes[] = {"Off",     "Cascades",  "Material",
                                     "Normals", "World Pos", "Raw Normal",
                                     "Albedo",  "Depth",     "Vertex Normal"};
        int debug_mode = settings.debug_cascades;
        if (ImGui::Combo(PrefixLabel("Debug View").c_str(), &debug_mode,
                         debug_modes, 9)) {
          settings.debug_cascades = debug_mode;
        }
      }

      ImGui::SeparatorText("Overlays");
      ImGui::Checkbox(PrefixLabel("Colliders").c_str(),
                      &settings.show_colliders);
      ImGui::Checkbox(PrefixLabel("Bounds").c_str(), &settings.show_bounds);
      ImGui::Checkbox(PrefixLabel("Triggers").c_str(), &settings.show_triggers);
      ImGui::Checkbox(PrefixLabel("Reverb Zones").c_str(),
                      &settings.show_reverb_zones);
      ImGui::Checkbox(PrefixLabel("Cameras").c_str(), &settings.show_cameras);

      ImGui::SeparatorText("Actions");
      if (ImGui::MenuItem("Reload Scripts")) {
        Engine::script_manager().ReloadAsync();
      }
      if (ImGui::MenuItem("Reload All (+ Core)")) {
        Engine::script_manager().ReloadAsync(true);
      }
      if (ImGui::MenuItem("Recreate Pipeline")) {
        Engine::renderer()->SetRecreatePipeline(true);
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        show_about_popup_ = true;
      }
      ImGui::EndMenu();
    }

    // Right-aligned status bar: asset stats + project info
    {
      auto asset_stats = Engine::asset_manager().GetStats();

      std::string status_text;
      bool has_activity =
          asset_stats.loading > 0 || Engine::script_manager().IsCompiling();
      bool has_compile_error = false;
      if (Engine::script_manager().IsCompiling()) {
        status_text = "Compiling scripts...";
      } else if (!Engine::script_manager()
                      .last_compile_result()
                      .output.empty() &&
                 !Engine::script_manager().last_compile_result().success) {
        has_compile_error = true;
      }

      if (status_text.empty() && asset_stats.loading > 0) {
        status_text = std::format("Loading {} asset{}...", asset_stats.loading,
                                  asset_stats.loading > 1 ? "s" : "");
      } else if (status_text.empty() && asset_stats.failed > 0) {
        status_text = std::format("{} failed", asset_stats.failed);
      }
      std::string asset_summary =
          std::format("[{}/{}]", asset_stats.loaded, asset_stats.total);

      std::string info;
      if (active_project_) {
        info = active_project_->GetSettings().name;
        if (!current_scene_path_.empty()) {
          info += " - " + VirtualFileSystem::Stem(current_scene_path_);
        }
        if (scene_dirty_) {
          info += " *";
        }
      } else {
        info = "No Project";
      }

      float spacing = 12.0f;
      float info_width = ImGui::CalcTextSize(info.c_str()).x;
      float summary_width = ImGui::CalcTextSize(asset_summary.c_str()).x;
      float status_width =
          status_text.empty()
              ? 0.0f
              : ImGui::CalcTextSize(status_text.c_str()).x + spacing;
      float error_width = has_compile_error
                              ? ImGui::CalcTextSize("Compile Error").x + spacing
                              : 0.0f;
      bool has_pending_reload =
          script_reload_pending_ && editor_state_ == EditorState::Playing;
      float pending_width =
          has_pending_reload ? ImGui::CalcTextSize("Reload Pending").x + spacing
                             : 0.0f;
      size_t unread = notifications_.UnreadCount();
      float notif_width =
          unread > 0
              ? ImGui::CalcTextSize((std::to_string(unread) + " notification" +
                                     (unread > 1 ? "s" : ""))
                                        .c_str())
                        .x +
                    spacing
              : 0.0f;
      float total_right = notif_width + pending_width + error_width +
                          status_width + summary_width + spacing + info_width +
                          16.0f;

      ImGui::SameLine(ImGui::GetWindowWidth() - total_right);

      if (unread > 0) {
        notifications_.RenderHistoryButton();
        ImGui::SameLine(0, spacing);
      }

      if (has_pending_reload) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Reload Pending");
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Script changes detected.\n"
              "Reload will happen when you stop playing.");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, spacing);
      }

      if (has_compile_error) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        if (ImGui::SmallButton("Compile Error")) {
          ImGui::OpenPopup("compile_error_popup");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, spacing);
      }

      if (!status_text.empty()) {
        if (has_activity) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s",
                             status_text.c_str());
        } else {
          ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                             status_text.c_str());
        }
        ImGui::SameLine(0, spacing);
      }

      ImGui::TextDisabled("%s", asset_summary.c_str());
      ImGui::SameLine(0, spacing);
      ImGui::TextDisabled("%s", info.c_str());
    }

    // Compile error popup
    ImGui::SetNextWindowSize(ImVec2(1200, 500));
    if (ImGui::BeginPopup("compile_error_popup", ImGuiWindowFlags_NoResize |
                                                     ImGuiWindowFlags_NoMove)) {
      const auto& result = Engine::script_manager().last_compile_result();
      if (result.success) {
        ImGui::CloseCurrentPopup();
      } else {
        ImGui::Text("Compilation failed (exit code %d)", result.exit_code);
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        std::string output_copy = result.output;
        ImGui::InputTextMultiline(
            "##compile_output", output_copy.data(), output_copy.size() + 1,
            ImVec2(-1, -ImGui::GetFrameHeightWithSpacing()),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(2);
        if (!result.command.empty()) {
          ImGui::TextWrapped("Command: %s", result.command.c_str());
        }
      }
      ImGui::EndPopup();
    }

    ImGui::EndMainMenuBar();
  }

  // About popup
  if (show_about_popup_) {
    ImGui::OpenPopup("About Wiesel");
    show_about_popup_ = false;
  }
  if (ImGui::BeginPopupModal("About Wiesel", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Engine info
    ImGui::SeparatorText("Engine");
    ImGui::Text("Git Branch: %s", WIESEL_GIT_BRANCH);
    ImGui::Text("Git Commit: %s", WIESEL_GIT_COMMIT);
    ImGui::Text("Build Type: %s", WIESEL_BUILD_TYPE);
    ImGui::Text("Window Backend: SDL3");

    // GPU info
    ImGui::SeparatorText("GPU");
    auto props = Engine::renderer()->GetPhysicalDeviceProperties();
    uint32_t vk_major = VK_API_VERSION_MAJOR(props.apiVersion);
    uint32_t vk_minor = VK_API_VERSION_MINOR(props.apiVersion);
    uint32_t vk_patch = VK_API_VERSION_PATCH(props.apiVersion);
    ImGui::Text("GPU: %s", props.deviceName);
    ImGui::Text("Vulkan: %u.%u.%u", vk_major, vk_minor, vk_patch);

    // Project info
    ImGui::SeparatorText("Project");
    if (active_project_) {
      ImGui::Text("Name: %s", active_project_->GetSettings().name.c_str());
      ImGui::Text("Path: %s",
                  active_project_->GetProjectDirectory().string().c_str());
      ImGui::Text("Assets: %s",
                  active_project_->GetAssetsDirectory().string().c_str());
      if (!current_scene_path_.empty()) {
        ImGui::Text("Scene: %s", current_scene_path_.c_str());
      }
      auto asset_stats = Engine::asset_manager().GetStats();
      ImGui::Text("Assets: %zu loaded, %zu total", asset_stats.loaded,
                  asset_stats.total);
    } else {
      ImGui::TextDisabled("No project open");
    }

    // Paths
    ImGui::SeparatorText("Paths");
    ImGui::Text("Engine Assets: %s",
                Engine::properties().engine_assets_path.string().c_str());
    ImGui::Text("User Data: %s",
                Engine::properties().user_data_path.string().c_str());
    ImGui::Text("Working Dir: %s",
                std::filesystem::current_path().string().c_str());

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Keyboard shortcuts (skip when code editor or other text input has focus)
  ImGuiIO& io = ImGui::GetIO();
  bool text_input_active = code_editor_focused_ || io.WantTextInput;
  if (!text_input_active && io.KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    if (!current_scene_path_.empty()) {
      SaveScene();
      SaveProject();
    } else {
      SaveSceneAs();
    }
  }

  // Entity copy (Ctrl+C) - copies full entity tree including children
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false) &&
      has_selected_entity_ && !ImGui::GetIO().WantTextInput) {
    Entity entity{selected_entity_, selected_entity_scene_.get()};
    nlohmann::json j = Prefab::SerializeEntityTree(entity);
    entity_clipboard_ = j.dump();
  }

  // Entity paste (Ctrl+V) - pastes full entity tree with new UUIDs
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false) &&
      !entity_clipboard_.empty() && !ImGui::GetIO().WantTextInput) {
    try {
      nlohmann::json j = nlohmann::json::parse(entity_clipboard_);
      Entity new_entity = Prefab::DeserializeEntityTree(scene(), j);
      if (new_entity) {
        selected_entity_ = new_entity.handle();
        selected_entity_scene_ = scene();
        has_selected_entity_ = true;
        scroll_to_selected_ = true;
        scene_dirty_ = true;
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to paste entity: {}", e.what());
    }
  }

  // Entity duplicate (Ctrl+D) - duplicates full entity tree
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) &&
      has_selected_entity_ && !ImGui::GetIO().WantTextInput) {
    Entity entity{selected_entity_, selected_entity_scene_.get()};
    nlohmann::json j = Prefab::SerializeEntityTree(entity);
    Entity new_entity =
        Prefab::DeserializeEntityTree(selected_entity_scene_, j);
    if (new_entity) {
      // Parent to same parent as original
      Entity parent = entity.GetParent();
      if (parent) {
        selected_entity_scene_->LinkEntities(parent.handle(), new_entity);
      }
      selected_entity_ = new_entity.handle();
      has_selected_entity_ = true;
      scroll_to_selected_ = true;
      scene_dirty_ = true;
    }
  }

  // Delete selected entity
  if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && has_selected_entity_ &&
      !ImGui::GetIO().WantTextInput) {
    command_stack_.Execute(std::make_unique<EntityDeleteCommand>(
        selected_entity_scene_, selected_entity_));
    has_selected_entity_ = false;
    scene_dirty_ = true;
  }
}

void EditorLayer::RenderStartupDialog() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 center = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f,
                         viewport->Pos.y + viewport->Size.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520, 0));
  ImGui::Begin("Welcome to Wiesel", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);

  ImGui::Text("Get started:");
  ImGui::Spacing();

  float width = ImGui::GetContentRegionAvail().x;
  if (ImGui::Button("New Project...", ImVec2(width, 30))) {
    NewProject();
  }
  if (ImGui::Button("Open Project...", ImVec2(width, 30))) {
    OpenProject();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const auto& recent = RecentProjects::Load();
  if (!recent.empty()) {
    ImGui::Text("Recent Projects:");
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));
    for (size_t i = 0; i < recent.size(); i++) {
      const std::string& path = recent[i];
      namespace fs = std::filesystem;
      std::string name = fs::path(path).stem().string();
      std::string dir = fs::path(path).parent_path().string();
      std::string label = name + "  " + dir;

      ImGui::PushID(static_cast<int>(i));
      ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf |
                                      ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                      ImGuiTreeNodeFlags_SpanAvailWidth;
      ImGui::PaddedTreeNodeEx(label.c_str(), node_flags);
      if (ImGui::IsItemClicked()) {
        if (fs::exists(path)) {
          deferred_action_ = DeferredAction::OpenProject;
          deferred_path_ = path;
        }
      }
      ImGui::PopID();
    }
    ImGui::PopStyleVar();
  } else {
    ImGui::TextDisabled("No recent projects.");
  }

  ImGui::End();
}

void EditorLayer::RenderEditorSettingsPanel() {
  if (panel_editor_settings_) {
    ImGui::OpenPopup("Editor Settings");
    panel_editor_settings_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 420), ImGuiCond_Appearing);

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Editor Settings", &popup_open,
                             ImGuiWindowFlags_NoScrollbar)) {
    static const char* categories[] = {"Appearance", "C# Language Server"};
    static int selected_category = 0;

    // Left panel: category list
    ImGui::BeginChild("categories", ImVec2(160, 0), ImGuiChildFlags_Borders);
    for (int i = 0; i < 2; i++) {
      if (ImGui::Selectable(categories[i], selected_category == i)) {
        selected_category = i;
      }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: settings
    ImGui::BeginChild("settings_content", ImVec2(0, 0),
                      ImGuiChildFlags_Borders);

    if (selected_category == 0) {
      // --- Appearance ---
      auto current = ImGui::Moonlight::GetCurrentTheme();
      const char* theme_name = ImGui::Moonlight::GetThemeName(current);
      if (ImGui::BeginCombo("Theme", theme_name)) {
        for (int i = 0; i < static_cast<int>(ImGui::Moonlight::Theme::Count);
             i++) {
          auto theme = static_cast<ImGui::Moonlight::Theme>(i);
          bool selected = (current == theme);
          if (ImGui::Selectable(ImGui::Moonlight::GetThemeName(theme),
                                selected)) {
            ImGui::Moonlight::ApplyTheme(theme);
            editor_config_->Set("editor.theme", static_cast<int>(theme));
            editor_config_->Save();
          }
        }
        ImGui::EndCombo();
      }
    } else if (selected_category == 1) {
      // --- C# LSP ---
      static char lsp_cmd_buf[512] = {};
      static bool lsp_buf_initialized = false;
      if (!lsp_buf_initialized) {
        std::string saved =
            editor_config_->Get<std::string>("lsp.csharp.command", "");
        strncpy(lsp_cmd_buf, saved.c_str(), sizeof(lsp_cmd_buf) - 1);
        lsp_buf_initialized = true;
      }

      ImGui::TextWrapped(
          "Command to launch the C# LSP server. Examples:\n"
          "  csharp-ls\n"
          "  \"path/to/OmniSharp.exe\" --languageserver");
      ImGui::Spacing();
      ImGui::InputText("Command", lsp_cmd_buf, sizeof(lsp_cmd_buf));

      ImGui::Spacing();
      if (ImGui::Button("Auto-Detect")) {
        std::string detected;
        std::filesystem::path exe_dir = GetExecutableDirectory();
#ifdef _WIN32
        std::filesystem::path omnisharp =
            exe_dir / "omnisharp" / "OmniSharp.exe";
#else
        std::filesystem::path omnisharp = exe_dir / "omnisharp" / "run";
#endif
        if (std::filesystem::exists(omnisharp)) {
          detected = "\"" + omnisharp.string() + "\" --languageserver";
        }
        if (detected.empty()) {
#ifdef _WIN32
          if (std::system("where csharp-ls >nul 2>nul") == 0) {
            detected = "csharp-ls";
          }
#else
          if (std::system("which csharp-ls >/dev/null 2>&1") == 0) {
            detected = "csharp-ls";
          }
#endif
        }
        if (!detected.empty()) {
          strncpy(lsp_cmd_buf, detected.c_str(), sizeof(lsp_cmd_buf) - 1);
          notifications_.PushInfo("Auto-detected: " + detected);
        } else {
          notifications_.PushWarning("No C# LSP server found on this system.");
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Install csharp-ls")) {
        notifications_.PushInfo("Installing csharp-ls...");
        Engine::thread_pool().Submit([this]() {
          int result = std::system(
              "dotnet tool install -g csharp-ls "
              "--add-source https://api.nuget.org/v3/index.json");
          if (result != 0) {
            result = std::system(
                "dotnet tool update -g csharp-ls "
                "--add-source https://api.nuget.org/v3/index.json");
          }
          if (result == 0) {
            notifications_.PushInfo("csharp-ls installed successfully.");
          } else {
            notifications_.PushError("Failed to install csharp-ls.");
          }
        });
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      if (ImGui::Button("Save & Restart LSP")) {
        std::string cmd(lsp_cmd_buf);
        editor_config_->Set("lsp.csharp.command", cmd);
        editor_config_->Save();
        StopLsp();
        if (active_project_) {
          StartLsp();
        }
      }
      ImGui::SameLine();
      ImGui::TextDisabled(
          "Status: %s", lsp_initialized_
                            ? (lsp_client_.IsRunning() ? "Running" : "Stopped")
                            : "Not started");
    }

    ImGui::EndChild();
    ImGui::EndPopup();
  }
}

}  // namespace Wiesel::Editor
