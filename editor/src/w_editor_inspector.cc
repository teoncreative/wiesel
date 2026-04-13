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
  if (ImGui::Begin(CODICON_INSPECT " Inspector", &open)) {
    if (has_selected_entity_ && selected_entity_scene_) {
      inspector_mode_ = InspectorMode::Entity;
    }

    if (inspector_mode_ == InspectorMode::Entity) {
      if (has_selected_entity_) {
        RenderEntityInspector(selected_entity_);
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

  const AssetTypeDesc* desc = AssetRegistry::Get(meta->type);
  if (desc && desc->RenderAssetImGui) {
    desc->RenderAssetImGui(inspector_asset_handle_);
  }

  if (inspector_asset_read_only_) {
    // Show properties as read-only text
    if (desc && desc->RenderPropertiesImGui && meta->properties) {
      ImGui::BeginDisabled();
      desc->RenderPropertiesImGui(meta->properties.get());
      ImGui::EndDisabled();
    }
  } else {
    if (desc && desc->RenderPropertiesImGui && meta->properties) {
      bool changed = desc->RenderPropertiesImGui(meta->properties.get());
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
  }
}

void EditorLayer::RenderEntityInspector(entt::entity handle) {
  Entity entity = {handle, selected_entity_scene_.get()};
  TagComponent& tag = entity.GetComponent<TagComponent>();
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

  ImGui::SameLine();
  if (ImGui::Button("Add")) {
    ImGui::OpenPopup("add_component_popup");
  }
  RenderModals(entity);
  if (ImGui::BeginPopup("add_component_popup")) {
    RenderAddPopup(entity);
    ImGui::EndPopup();
  }
  SetInspectorCommandStack(&command_stack_);
  RenderExistingComponents(entity);
  SetInspectorCommandStack(nullptr);
}

}  // namespace wiesel::editor
