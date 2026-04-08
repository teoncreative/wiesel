//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_anim_controller_editor.h"

#include <imgui.h>

#include "asset/w_asset_serializer.h"
#include "w_engine.h"

namespace Wiesel::Editor {

void AnimControllerEditor::Open(AssetHandle handle,
                                std::shared_ptr<AnimControllerAssetData> data) {
  asset_handle_ = handle;
  controller_ = std::move(data);
  delegate_ = std::make_unique<AnimGraphDelegate>(controller_);
  open_ = true;
}

void AnimControllerEditor::Close() {
  open_ = false;
  delegate_.reset();
  controller_.reset();
  asset_handle_ = {};
}

void AnimControllerEditor::Render() {
  if (!open_ || !controller_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Animation Controller Editor", &open_)) {
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
    ImGui::TextDisabled("%s", meta->name.c_str());
  }

  // Split: left = graph, right = properties
  float properties_width = 280.0f;
  float avail_width = ImGui::GetContentRegionAvail().x;
  float graph_width = avail_width - properties_width - 8.0f;

  // Graph area
  ImGui::BeginChild(
      "##GraphArea",
      ImVec2(graph_width, -ImGui::GetFrameHeightWithSpacing() * 4));
  graph_editor_.Render(*delegate_);
  ImGui::EndChild();

  ImGui::SameLine();

  // Properties sidebar
  ImGui::BeginChild(
      "##Properties",
      ImVec2(properties_width, -ImGui::GetFrameHeightWithSpacing() * 4));
  delegate_->RenderProperties();
  ImGui::EndChild();

  // Parameters panel at bottom
  ImGui::Separator();
  RenderParametersPanel();

  ImGui::End();

  if (!open_) {
    Close();
  }
}

void AnimControllerEditor::RenderParametersPanel() {
  if (!ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& params = controller_->default_parameters;
  std::string to_remove;

  for (auto& [name, param] : params) {
    ImGui::PushID(name.c_str());

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
    ImGui::BeginChild(("##p_" + name).c_str(), ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    const char* type_label = "";
    switch (param.type) {
      case AnimParamType::Bool:
        type_label = "Bool";
        break;
      case AnimParamType::Int:
        type_label = "Int";
        break;
      case AnimParamType::Float:
        type_label = "Float";
        break;
      case AnimParamType::Trigger:
        type_label = "Trigger";
        break;
    }
    ImGui::TextDisabled("%s", type_label);
    ImGui::SameLine();
    ImGui::Text("%s", name.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    if (ImGui::SmallButton("X")) {
      to_remove = name;
    }

    switch (param.type) {
      case AnimParamType::Bool:
        ImGui::Checkbox("Default", &param.b);
        break;
      case AnimParamType::Int:
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##v", &param.i);
        break;
      case AnimParamType::Float:
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##v", &param.f, 0.01f);
        break;
      case AnimParamType::Trigger:
        break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::PopID();
  }

  if (!to_remove.empty()) {
    params.erase(to_remove);
    delegate_->is_dirty = true;
  }

  // Add parameter
  static char new_name[64] = "";
  static int new_type = 0;
  ImGui::SetNextItemWidth(120);
  ImGui::InputText("##pn", new_name, sizeof(new_name));
  ImGui::SameLine();
  const char* types[] = {"Bool", "Int", "Float", "Trigger"};
  ImGui::SetNextItemWidth(80);
  ImGui::Combo("##pt", &new_type, types, 4);
  ImGui::SameLine();
  if (ImGui::Button("+ Param") && new_name[0] != '\0' &&
      !params.contains(new_name)) {
    switch (new_type) {
      case 0:
        params[new_name] = AnimParam::MakeBool(false);
        break;
      case 1:
        params[new_name] = AnimParam::MakeInt(0);
        break;
      case 2:
        params[new_name] = AnimParam::MakeFloat(0.0f);
        break;
      case 3:
        params[new_name] = AnimParam::MakeTrigger();
        break;
    }
    new_name[0] = '\0';
    delegate_->is_dirty = true;
  }
}

void AnimControllerEditor::Save() {
  if (!asset_handle_.IsValid() || !controller_) {
    return;
  }
  Engine::asset_manager().Store(asset_handle_, controller_);
  AssetSerializerRegistry::Save(asset_handle_);
  delegate_->is_dirty = false;
}

}  // namespace Wiesel::Editor
