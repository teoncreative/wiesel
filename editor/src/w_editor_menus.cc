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
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <cstring>
#include <cmath>

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
#include "ui/w_ui_layout.h"
#include "ui/w_ui_popup.h"
#include "ui/w_ui_row.h"
#include "ui/w_ui_section.h"
#include "ui/w_ui_style.h"
#include "util/imgui/imgui_theme.h"
#include "w_editor_input_ui.h"
#include <urkern/natural_sort.h>
#include <urkern/platform.h>
#include <urkern/thread_pool.h>
#include "ui/w_ui_field.h"
#include "w_editor_asset_factory.h"
#include "w_editor_asset_ui.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

namespace wiesel::editor {

namespace style = ui::style;
using ui::field::PrefixLabel;

Scene* scene();

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

void EditorLayer::RenderSliceSpritesPopup() {
  if (show_slice_sprites_) {
    ImGui::OpenPopup("Slice into Sprites");
    show_slice_sprites_ = false;
  }

  bool slice_open = true;
  if (ui::popup::Begin("Slice into Sprites", ICON_LC_GRID_2X2,
                            "Slice into Sprites", &slice_open,
                            ImVec2(700, 600))) {
    static char prefix_buf[128] = "sprite";
    static int columns = 6;
    static int rows = 1;
    static float pivot[2] = {0.5f, 0.5f};

    auto tex = slice_texture_handle_.IsValid()
                   ? Engine::asset_manager().Get<Texture>(slice_texture_handle_)
                   : nullptr;
    if (!tex) {
      ImGui::Text("Texture not loaded.");
      ui::popup::End();
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
    ui::popup::End();
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

  bool popup_open = true;
  if (ui::popup::Begin("Project Settings", ICON_LC_SETTINGS,
                            "Project Settings", &popup_open,
                            ImVec2(720, 500))) {
    auto& proj_settings = active_project_->GetSettings();
    auto& game_info = active_project_->GetGameInfo();
    bool changed = false;

    struct CategoryDef {
      const char* label;
      const char* icon;
    };
    const CategoryDef categories[] = {
        {"Scene", ICON_LC_LAYERS_2},
        {"Rendering", ICON_LC_MONITOR},
        {"Input", ICON_LC_GAMEPAD_2},
    };
    constexpr int kCategoryCount = IM_ARRAYSIZE(categories);

    ui::layout::BeginSidebarBody(160.0f);
    for (int i = 0; i < kCategoryCount; i++) {
      if (ui::row::CategoryRow(categories[i].label,
                            project_settings_category_ == i,
                            categories[i].icon)) {
        project_settings_category_ = i;
      }
    }
    ui::layout::BeginBody();

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
      // All of the input editor UI (mouse, contexts sidebar, actions/axes
      // cards, binding chips + capture popup) lives in w_editor_input_ui.cc
      // so this branch stays thin.
      if (RenderInputSettings(game_info.input, selected_input_context_)) {
        Engine::input().LoadFromSettings(game_info.input);
        changed = true;
      }
    }

    ui::layout::EndSidebarBody();

    if (changed) {
      active_project_->Save();
    }

    ui::popup::End();
  }
}

void EditorLayer::RenderMainMenuBar() {
  // Taller main menu bar for the editor toolbar. Reserves room on the
  // right for a command palette button (added after Help).
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
  const bool open = ImGui::BeginMainMenuBar();
  ImGui::PopStyleVar();
  if (open) {
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
        for (const auto& f : AssetFactoryRegistry::All()) {
          if (f.label == "Scene" && f.action) {
            f.action();
            break;
          }
        }
      }
      if (ImGui::MenuItem("Save", "Ctrl+S", false,
                          active_project_ != nullptr &&
                              editor_state_ == EditorState::Edit)) {
        SaveScene();
        SaveProject();
      }
      if (ImGui::MenuItem("Save As...", nullptr, false,
                          active_project_ != nullptr &&
                              editor_state_ == EditorState::Edit)) {
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
      ImGui::MenuItem(ICON_LC_EYE " Scene", nullptr, &panel_scene_view_);
      ImGui::MenuItem(ICON_LC_CAMERA " Game", nullptr, &panel_game_view_);
      ImGui::MenuItem(ICON_LC_LAYERS " Scene Hierarchy", nullptr,
                      &panel_scene_hierarchy_);
      ImGui::MenuItem(ICON_LC_SQUARE_MOUSE_POINTER " Entity Inspector", nullptr,
                      &panel_components_);
      ImGui::MenuItem(ICON_LC_FOLDER_OPEN " Asset Browser", nullptr,
                      &panel_asset_browser_);
      ImGui::MenuItem(ICON_LC_TERMINAL " Console", nullptr, &panel_console_);
      ImGui::MenuItem(ICON_LC_GAUGE " Render Stats", nullptr,
                      &panel_stats_);
      ImGui::MenuItem(ICON_LC_HISTORY " Undo History", nullptr,
                      &panel_undo_history_);
      ImGui::MenuItem(ICON_LC_INFO " LSP Debug", nullptr, &panel_lsp_debug_);
      ImGui::MenuItem(ICON_LC_GLOBE " Network", nullptr, &panel_network_);
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
      ImGui::PushItemWidth(320.0f);
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

      ImGui::SeparatorText("Windows");
      ImGui::MenuItem("Font Debug", nullptr, &panel_font_debug_);

      ImGui::PopItemWidth();
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        show_about_popup_ = true;
      }
      ImGui::EndMenu();
    }

    // Center: command-palette trigger, styled as an input.
    {
      ImGuiWindow* bar = ImGui::GetCurrentWindow();
      const float trigger_w = 360.0f;
      const float trigger_h = ImGui::GetFrameHeight();
      const float center_x =
          (ImGui::GetWindowWidth() - trigger_w) * 0.5f;
      ImGui::SameLine(center_x);
      // Vertically center inside the taller menu bar; the default cursor
      // sits at the line top.
      const float trigger_y =
          bar->Pos.y + (bar->Size.y - trigger_h) * 0.5f;
      ImGui::SetCursorScreenPos(
          ImVec2(ImGui::GetCursorScreenPos().x, trigger_y));
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      if (ImGui::InvisibleButton("##PaletteTrigger",
                                 ImVec2(trigger_w, trigger_h))) {
        command_palette_.Open();
      }
      const bool hovered = ImGui::IsItemHovered();
      ImDrawList* dl = ImGui::GetWindowDrawList();
      const ImGuiStyle& s = ImGui::GetStyle();
      const ImU32 bg = ImGui::GetColorU32(
          hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
      dl->AddRectFilled(pos,
                        ImVec2(pos.x + trigger_w, pos.y + trigger_h),
                        bg, s.FrameRounding);
      dl->AddRect(pos, ImVec2(pos.x + trigger_w, pos.y + trigger_h),
                  ImGui::GetColorU32(ImGuiCol_Border), s.FrameRounding,
                  0, 1.0f);
      const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
      const float cy = pos.y + trigger_h * 0.5f;
      const ImVec2 icon_sz = ImGui::CalcTextSize(ICON_LC_COMMAND);
      dl->AddText(ImVec2(pos.x + s.FramePadding.x,
                         cy - icon_sz.y * 0.5f),
                  muted, ICON_LC_COMMAND);
      const char* hint = "Run a command...";
      const ImVec2 hint_sz = ImGui::CalcTextSize(hint);
      dl->AddText(ImVec2(pos.x + s.FramePadding.x + icon_sz.x +
                             s.ItemInnerSpacing.x * 2.0f,
                         cy - hint_sz.y * 0.5f),
                  muted, hint);
      std::string shortcut = "Ctrl+Shift+K";
      const ImVec2 sc_sz = ImGui::CalcTextSize(shortcut.c_str());
      dl->AddText(ImVec2(pos.x + trigger_w - s.FramePadding.x - sc_sz.x,
                         cy - sc_sz.y * 0.5f),
                  muted, shortcut.c_str());
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
                          status_width + spacing + info_width + 16.0f;

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
  bool about_open = true;
  if (ui::popup::Begin("About Wiesel", ICON_LC_INFO, "About Wiesel",
                            &about_open, ImVec2(520, 0))) {
    ui::section::BeginSection("Engine", ICON_LC_CPU, /*fill=*/true);
    ImGui::Text("Version: %s", kEngineVersion);
    ImGui::Text("Git Branch: %s", WIESEL_GIT_BRANCH);
    ImGui::Text("Git Commit: %s", WIESEL_GIT_COMMIT);
    ImGui::Text("Build Type: %s", WIESEL_BUILD_TYPE);
    ImGui::Text("Window Backend: SDL3");
    ui::section::EndSection();

    ui::section::BeginSection("GPU", ICON_LC_MICROCHIP, /*fill=*/true);
    {
      auto props = Engine::renderer()->GetPhysicalDeviceProperties();
      uint32_t vk_major = VK_API_VERSION_MAJOR(props.apiVersion);
      uint32_t vk_minor = VK_API_VERSION_MINOR(props.apiVersion);
      uint32_t vk_patch = VK_API_VERSION_PATCH(props.apiVersion);
      ImGui::Text("GPU: %s", props.deviceName);
      ImGui::Text("Vulkan: %u.%u.%u", vk_major, vk_minor, vk_patch);
    }
    ui::section::EndSection();

    ui::section::BeginSection("Project", ICON_LC_LAYERS_2, /*fill=*/true);
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
    ui::section::EndSection();

    ui::section::BeginSection("Paths", ICON_LC_FOLDER, /*fill=*/true);
    ImGui::Text("Engine Assets: %s",
                Engine::properties().engine_assets_path.string().c_str());
    ImGui::Text("User Data: %s",
                Engine::properties().user_data_path.string().c_str());
    ImGui::Text("Working Dir: %s",
                std::filesystem::current_path().string().c_str());
    ui::section::EndSection();

    ui::popup::End();
  }
}

void EditorLayer::RenderStartupDialog() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 center = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f,
                         viewport->Pos.y + viewport->Size.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("##StartupDialog", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoDocking |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::PopStyleVar();

  // Spacing comes from the active ImGui theme (WindowPadding for outer
  // chrome, ItemSpacing.y for the natural gap between widgets) plus the
  // editor-wide kSeparatorPadY for breathing room around section dividers.
  const ImGuiStyle& s = ImGui::GetStyle();

  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  ImGui::Indent(s.WindowPadding.x);
  ImGui::SetWindowFontScale(style::kHeaderFontScale);
  ImGui::TextDisabled("%s", ICON_LC_LAYERS_2);
  ImGui::SameLine();
  ImGui::TextUnformatted("Welcome to Wiesel");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Unindent(s.WindowPadding.x);

  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));
  ui::layout::Separator();
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  ImGui::Indent(s.WindowPadding.x);
  ImGui::TextDisabled("Get started");
  float width = ImGui::GetContentRegionAvail().x - s.WindowPadding.x;
  if (ImGui::Button("New Project...", ImVec2(width, ImGui::GetFrameHeight()))) {
    NewProject();
  }
  if (ImGui::Button("Open Project...", ImVec2(width, ImGui::GetFrameHeight()))) {
    OpenProject();
  }
  ImGui::Unindent(s.WindowPadding.x);

  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));
  ui::layout::Separator();
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  ImGui::Indent(s.WindowPadding.x);
  const auto& recent = RecentProjects::Load();
  if (!recent.empty()) {
    ImGui::TextDisabled("Recent projects");

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    for (size_t i = 0; i < recent.size(); i++) {
      const std::string& path = recent[i];
      namespace fs = std::filesystem;
      std::string name = fs::path(path).stem().string();
      std::string dir = fs::path(path).parent_path().string();
      std::string label = name + "  " + dir;

      ImGui::PushID(static_cast<int>(i));
      ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf |
                                      ImGuiTreeNodeFlags_NoTreePushOnOpen;
      ui::row::HierarchyRow(label.c_str(), node_flags);
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
  ImGui::Unindent(s.WindowPadding.x);
  // Bottom edge: kSeparatorPadY of breathing PLUS the trailing ItemSpacing.y
  // because ImGui's CursorMaxPos calculation drops the spacing after the
  // last item (so a plain Dummy(kSeparatorPadY) here would land ~10 px
  // shorter than the matching top margin).
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY + s.ItemSpacing.y));

  ImGui::End();
}

void EditorLayer::RenderEditorSettingsPanel() {
  if (panel_editor_settings_) {
    ImGui::OpenPopup("Editor Settings");
    panel_editor_settings_ = false;
  }

  bool popup_open = true;
  if (ui::popup::Begin("Editor Settings", ICON_LC_SETTINGS_2,
                            "Editor Settings", &popup_open,
                            ImVec2(720, 500))) {
    struct CategoryDef {
      const char* label;
      const char* icon;
    };
    static const CategoryDef categories[] = {
        {"Appearance", ICON_LC_PAINTBRUSH},
        {"C# Language Server", ICON_LC_CODE_2},
    };
    static int selected_category = 0;

    ui::layout::BeginSidebarBody(180.0f);
    for (int i = 0; i < IM_ARRAYSIZE(categories); i++) {
      if (ui::row::CategoryRow(categories[i].label, selected_category == i,
                            categories[i].icon)) {
        selected_category = i;
      }
    }
    ui::layout::BeginBody();

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
        std::filesystem::path exe_dir = urkern::GetExecutableDirectory();
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

    ui::layout::EndSidebarBody();
    ui::popup::End();
  }
}

// Bottom info bar: git branch + build status + notification bell.
void EditorLayer::RenderInfoBar() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float bar_h = std::floor(ImGui::GetFrameHeight() * 0.9f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  // Use the host (dockspace) bg so the bar reads as a true footer.
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ImGui::GetStyleColorVec4(ImGuiCol_DockingEmptyBg));
  const bool open = ImGui::BeginViewportSideBar(
      "##InfoBar", viewport, ImGuiDir_Down, bar_h,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor();
  if (!open) {
    ImGui::End();
    return;
  }

  ImGui::SetWindowFontScale(0.82f);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 bar_pos = ImGui::GetCursorScreenPos();
  const float cy = bar_pos.y + bar_h * 0.5f;
  const float pad_x = ImGui::GetStyle().FramePadding.x;
  const float win_w = ImGui::GetWindowSize().x;
  const float side_margin = ImGui::GetStyle().WindowPadding.x;
  const float left_x = bar_pos.x + side_margin;
  const float right_x = bar_pos.x + win_w - side_margin;

  dl->AddLine(ImVec2(bar_pos.x, bar_pos.y),
              ImVec2(bar_pos.x + win_w, bar_pos.y),
              ImGui::GetColorU32(ImGuiCol_Border), 1.0f);

  // Left: git branch.
  const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  if (!git_branch_.empty()) {
    std::string label = std::string(ICON_LC_GIT_BRANCH) + "  " + git_branch_;
    const ImVec2 sz = ImGui::CalcTextSize(label.c_str());
    dl->AddText(ImVec2(left_x, cy - sz.y * 0.5f), muted, label.c_str());
  }

  // Right edge: notification bell.
  const size_t unread = notifications_.UnreadCount();
  std::string bell_label;
  if (unread > 0) {
    bell_label = std::to_string(unread) + "  ";
  }
  bell_label.append(ICON_LC_BELL);
  const ImVec2 bell_sz = ImGui::CalcTextSize(bell_label.c_str());
  const float bell_w = bell_sz.x + pad_x * 2.0f;

  ImGui::SetCursorScreenPos(ImVec2(right_x - bell_w, bar_pos.y));
  const bool bell_clicked =
      ImGui::InvisibleButton("##InfoBarBell", ImVec2(bell_w, bar_h));
  const bool bell_hovered = ImGui::IsItemHovered();
  if (bell_clicked) {
    notifications_.ToggleHistoryPanel();
  }
  if (bell_hovered) {
    dl->AddRectFilled(ImVec2(right_x - bell_w, bar_pos.y),
                      ImVec2(right_x, bar_pos.y + bar_h),
                      ImGui::GetColorU32(ImGuiCol_HeaderHovered));
  }
  const ImU32 bell_col = (unread > 0 || bell_hovered)
                             ? ImGui::GetColorU32(ImGuiCol_Text)
                             : ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const ImVec2 bell_pos(right_x - bell_w + pad_x,
                        cy - bell_sz.y * 0.5f);
  dl->AddText(bell_pos, bell_col, bell_label.c_str());

  auto& script_mgr = Engine::script_manager();
  const bool compiling = script_mgr.IsCompiling();
  const auto& compile = script_mgr.last_compile_result();
  const char* state_text;
  ImVec4 state_col;
  if (compiling) {
    state_text = "build: compiling";
    state_col = ImVec4(1.0f, 0.65f, 0.15f, 1.0f);  // orange
  } else if (!compile.success && !compile.output.empty()) {
    state_text = "build: error";
    state_col = ImVec4(0.9f, 0.25f, 0.25f, 1.0f);  // red
  } else {
    state_text = "build: ready";
    state_col = ImVec4(0.35f, 0.78f, 0.40f, 1.0f);  // green
  }
  const float dot_r = ImGui::GetFontSize() * 0.28f;
  const float sep_r = ImGui::GetFontSize() * 0.14f;

  // Walk leftward from the bell: asset counter | dot | build status.
  float rx = right_x - bell_w - pad_x;

  const auto asset_stats = Engine::asset_manager().GetStats();
  std::string asset_label =
      "[" + std::to_string(asset_stats.loaded) + "/" +
      std::to_string(asset_stats.total) + "]";
  const ImVec2 asset_sz = ImGui::CalcTextSize(asset_label.c_str());
  rx -= asset_sz.x;
  dl->AddText(ImVec2(rx, cy - asset_sz.y * 0.5f), muted, asset_label.c_str());

  rx -= pad_x + sep_r;
  dl->AddCircleFilled(ImVec2(rx, cy), sep_r, muted);
  rx -= sep_r + pad_x;

  const ImVec2 text_sz = ImGui::CalcTextSize(state_text);
  rx -= text_sz.x;
  const ImVec2 text_pos(rx, cy - text_sz.y * 0.5f);
  const ImVec2 dot_center(rx - pad_x - dot_r, cy);
  dl->AddCircleFilled(dot_center, dot_r,
                      ImGui::ColorConvertFloat4ToU32(state_col));
  dl->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(state_col),
              state_text);

  ImGui::SetWindowFontScale(1.0f);
  ImGui::End();
}

void EditorLayer::RefreshGitBranch() {
  git_branch_.clear();
  if (!active_project_) {
    return;
  }
  const std::string dir = active_project_->GetProjectDirectory().string();
  std::string cmd =
      "git -C \"" + dir + "\" branch --show-current 2>/dev/null";
  FILE* pipe = ::popen(cmd.c_str(), "r");
  if (!pipe) {
    return;
  }
  char buf[256];
  if (std::fgets(buf, sizeof(buf), pipe)) {
    std::string line(buf);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    if (!line.empty()) {
      git_branch_ = std::move(line);
    }
  }
  ::pclose(pipe);
}

}  // namespace wiesel::editor
