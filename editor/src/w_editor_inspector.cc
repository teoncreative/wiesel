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

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "asset/w_asset_registry.h"
#include "scene/w_scene.h"
#include "scene/w_scene_manager.h"
#include "ui/w_font.h"
#include "util/imgui/w_imguiutil.h"
#include "w_editor_asset_ui.h"
#include "w_editor_components.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

namespace wiesel::editor {

void EditorLayer::RenderInspectorPanel() {
  bool& open = panel_components_;
  if (!open) {
    return;
  }
  if (ImGui::Begin(ICON_LC_SQUARE_MOUSE_POINTER " Inspector", &open)) {
    Entity selected_entity = selected_entity_.Resolve();
    if (selected_entity) {
      inspector_mode_ = InspectorMode::Entity;
    }

    if (inspector_mode_ == InspectorMode::Entity) {
      if (selected_entity) {
        RenderEntityInspector(selected_entity);
      } else {
        ImGui::TextDisabled("No entity selected");
      }
    } else if (inspector_mode_ == InspectorMode::Asset) {
      RenderAssetPropertiesPanel();
    }
  }
  ImGui::End();
}

void EditorLayer::RenderAssetPropertiesPanel() {
  const AssetMetadata* meta =
      Engine::asset_manager().GetMetadata(inspector_asset_handle_);
  if (!meta || !inspector_asset_handle_.IsValid()) {
    ImGui::TextDisabled("No asset selected");
    return;
  }
  ImGui::Text("Name: %s", meta->name.c_str());
  if (inspector_asset_read_only_) {
    ImVec2 tag_size = ImGui::CalcTextSize("Read-only");
    float x = ImGui::GetContentRegionAvail().x - tag_size.x;
    ImGui::SameLine(x + ImGui::GetCursorStartPos().x);
    ImGui::TextDisabled("Read-only");
  }
  ImGui::TextDisabled("Path: %s", meta->virtual_source_path.c_str());
  ImGui::Separator();

  if (meta->type == AssetType::Texture || meta->type == AssetType::Sprite) {
    ThumbnailEntry thumb =
        ThumbnailCache::Get()->GetOrCreate(inspector_asset_handle_, *meta);
    if (thumb.texture_id) {
      float avail_width = ImGui::GetContentRegionAvail().x;
      float max_preview = std::min(avail_width, 256.0f);
      ImVec2 preview_size = thumb.FitSize(max_preview);
      float indent = (avail_width - preview_size.x) * 0.5f;
      if (indent > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
      }
      ImGui::Image(reinterpret_cast<ImTextureID>(thumb.texture_id),
                   preview_size, thumb.uv0, thumb.uv1);
      if (thumb.width > 0 && thumb.height > 0) {
        uint32_t display_w =
            static_cast<uint32_t>(thumb.width * (thumb.uv1.x - thumb.uv0.x));
        uint32_t display_h =
            static_cast<uint32_t>(thumb.height * (thumb.uv1.y - thumb.uv0.y));
        ImGui::TextDisabled("%u x %u", display_w, display_h);
      }
      ImGui::Separator();
    }
  }

  if (const auto* render_asset =
          AssetUiRegistry::GetRenderAsset(meta->type)) {
    if (inspector_asset_read_only_) {
      ImGui::BeginDisabled();
    }
    (*render_asset)(inspector_asset_handle_);
    if (inspector_asset_read_only_) {
      ImGui::EndDisabled();
    }
  }

  const AssetTypeDesc* desc = AssetRegistry::Get(meta->type);
  const auto* render_props = AssetUiRegistry::GetRenderProperties(meta->type);

  if (inspector_asset_read_only_) {
    if (render_props && meta->properties) {
      ImGui::BeginDisabled();
      (*render_props)(meta->properties.get());
      ImGui::EndDisabled();
    }
  } else {
    if (render_props && meta->properties) {
      bool changed = (*render_props)(meta->properties.get());
      if (changed) {
        auto physical =
            Engine::vfs()->GetPhysicalPath(meta->virtual_source_path);
        if (physical.has_value()) {
          std::filesystem::path meta_path = physical->string() + ".meta";
          AssetRegistry::WriteMetaFile(meta_path, meta->handle, meta->type,
                                       meta->properties.get());
        }
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Reimport")) {
      if (meta->type == AssetType::Font) {
        FontCache::Invalidate(inspector_asset_handle_);
      }
      Engine::asset_manager().Unload(inspector_asset_handle_);
      Engine::asset_manager().LoadSync(inspector_asset_handle_);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Reload this asset with the current properties.");
    }
    (void)desc;
  }
}

void EditorLayer::RenderEntityInspector(Entity selected_entity) {
  TagComponent& tag = selected_entity.GetComponent<TagComponent>();
  if (ImGui::InputText("##", &tag.name, ImGuiInputTextFlags_AutoSelectAll)) {
    if (tag.name[0] == ' ') {
      TrimLeft(tag.name);
    }

    if (tag.name.empty()) {
      tag.name = "Entity";
    }
  }
  // Game tags
  {
    std::string tags_display;
    for (size_t i = 0; i < tag.tags.size(); i++) {
      if (i > 0) {
        tags_display += ", ";
      }
      tags_display += tag.tags[i];
    }
    if (tags_display.empty()) {
      tags_display = "(no tags)";
    }
    ImGui::TextDisabled("Tags: %s", tags_display.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("+##addtag")) {
      ImGui::OpenPopup("add_tag_popup");
    }
    if (ImGui::BeginPopup("add_tag_popup")) {
      static char tag_buf[64] = "";
      ImGui::InputText("##tagname", tag_buf, sizeof(tag_buf));
      ImGui::SameLine();
      if (ImGui::Button("Add") && tag_buf[0] != '\0') {
        tag.AddTag(tag_buf);
        tag_buf[0] = '\0';
        ImGui::CloseCurrentPopup();
      }
      // Show existing tags with remove buttons
      for (size_t i = 0; i < tag.tags.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%s", tag.tags[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
          tag.RemoveTag(tag.tags[i]);
          ImGui::PopID();
          break;
        }
        ImGui::PopID();
      }
      ImGui::EndPopup();
    }
  }

  RenderModals(selected_entity);
  SetInspectorCommandStack(&command_stack_);
  RenderExistingComponents(selected_entity);
  SetInspectorCommandStack(nullptr);

  // "+ Add Component" button at the bottom: full content width, dashed
  // border by default, solid border + highlight + brighter text on hover.
  ImGui::Spacing();
  const ImVec2 btn_size(
      ImGui::GetContentRegionAvail().x,
      ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y);
  const ImVec2 btn_pos = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##AddComponentBtn", btn_size);
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 p_max(btn_pos.x + btn_size.x, btn_pos.y + btn_size.y);
  const float rounding = ImGui::GetStyle().FrameRounding;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (hovered) {
    dl->AddRectFilled(btn_pos, p_max,
                      ImGui::GetColorU32(ImGuiCol_ButtonHovered), rounding);
    dl->AddRect(btn_pos, p_max, ImGui::GetColorU32(ImGuiCol_Border), rounding,
                0, 1.0f);
  } else {
    ImGui::DashedRectRounded(dl, btn_pos, p_max,
                             ImGui::GetColorU32(ImGuiCol_Border), rounding,
                             1.0f, 4.0f, 3.0f);
  }
  const char* label = ICON_LC_PLUS "  Add Component";
  const ImVec2 text_sz = ImGui::CalcTextSize(label);
  const ImVec2 text_pos(btn_pos.x + (btn_size.x - text_sz.x) * 0.5f,
                        btn_pos.y + (btn_size.y - text_sz.y) * 0.5f);
  const ImU32 text_col =
      hovered ? ImGui::GetColorU32(ImGuiCol_Text)
              : ImGui::GetColorU32(ImGuiCol_TextDisabled);
  dl->AddText(text_pos, text_col, label);
  if (clicked) {
    ImGui::OpenPopup("add_component_popup");
  }
  if (ImGui::BeginPopup("add_component_popup")) {
    RenderAddPopup(selected_entity);
    ImGui::EndPopup();
  }
}

}  // namespace wiesel::editor
