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

#include "w_editor_components.h"
#include "w_engine.h"

namespace Wiesel::Editor {

void EditorLayer::RenderEntityInspectorPanel() {
  bool& components_open = panel_components_;
  if (components_open) {
    if (ImGui::Begin("Entity Inspector", &components_open)) {
      if (!has_selected_entity_) {
        ImGui::TextDisabled("No entity selected");
      } else {
        RenderEntityInspector(selected_entity_);
      }
    }
    ImGui::End();
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

}  // namespace Wiesel::Editor
