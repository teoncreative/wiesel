//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_anim_graph_delegate.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "asset/w_asset_manager.h"
#include "w_engine.h"

namespace wiesel::editor {

AnimGraphDelegate::AnimGraphDelegate(
    std::shared_ptr<AnimControllerAssetData> controller)
    : controller_(std::move(controller)) {
  EnsureIds();
}

void AnimGraphDelegate::EnsureIds() {
  // Assign IDs to states and transitions that don't have them yet
  for (auto& state : controller_->states) {
    if (state.editor_id < 0) {
      state.editor_id = next_id_++;
    } else if (state.editor_id >= next_id_) {
      next_id_ = state.editor_id + 1;
    }
  }
  for (auto& trans : controller_->transitions) {
    if (trans.editor_id < 0) {
      trans.editor_id = next_id_++;
    } else if (trans.editor_id >= next_id_) {
      next_id_ = trans.editor_id + 1;
    }
  }
}

AnimControllerAssetData::State* AnimGraphDelegate::FindStateById(int node_id) {
  for (auto& s : controller_->states) {
    if (s.editor_id == node_id) {
      return &s;
    }
  }
  return nullptr;
}

const AnimControllerAssetData::State* AnimGraphDelegate::FindStateById(
    int node_id) const {
  for (const auto& s : controller_->states) {
    if (s.editor_id == node_id) {
      return &s;
    }
  }
  return nullptr;
}

AnimationTransition* AnimGraphDelegate::FindTransitionById(int link_id) {
  for (auto& t : controller_->transitions) {
    if (t.editor_id == link_id) {
      return &t;
    }
  }
  return nullptr;
}

std::string AnimGraphDelegate::FindStateNameById(int node_id) const {
  const auto* state = FindStateById(node_id);
  return state ? state->name : "";
}

int AnimGraphDelegate::FindNodeIdByName(const std::string& name) const {
  for (const auto& s : controller_->states) {
    if (s.name == name) {
      return s.editor_id;
    }
  }
  return -1;
}

// --- Node access ---

int AnimGraphDelegate::GetNodeCount() const {
  return static_cast<int>(controller_->states.size());
}

int AnimGraphDelegate::GetNodeId(int index) const {
  return controller_->states[index].editor_id;
}

const char* AnimGraphDelegate::GetNodeTitle(int node_id) const {
  const auto* state = FindStateById(node_id);
  return state ? state->name.c_str() : "???";
}

glm::vec2 AnimGraphDelegate::GetNodePosition(int node_id) const {
  const auto* state = FindStateById(node_id);
  return state ? state->editor_pos : glm::vec2{0, 0};
}

void AnimGraphDelegate::SetNodePosition(int node_id, glm::vec2 pos) {
  auto* state = FindStateById(node_id);
  if (state) {
    state->editor_pos = pos;
  }
}

bool AnimGraphDelegate::HasInputPin(int /*node_id*/) const {
  return true;
}

void AnimGraphDelegate::RenderNodeBody(int node_id) {
  const auto* state = FindStateById(node_id);
  if (!state) {
    return;
  }
  ImGui::PushItemWidth(100);
  if (controller_->default_state == state->name) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Default");
  }
  ImGui::TextDisabled("Speed: %.2f", state->speed);
  ImGui::PopItemWidth();
}

// --- Link access ---

int AnimGraphDelegate::GetLinkCount() const {
  return static_cast<int>(controller_->transitions.size());
}

int AnimGraphDelegate::GetLinkId(int index) const {
  return controller_->transitions[index].editor_id;
}

int AnimGraphDelegate::GetLinkFromPin(int link_id) const {
  const auto* trans =
      const_cast<AnimGraphDelegate*>(this)->FindTransitionById(link_id);
  if (!trans) {
    return -1;
  }
  int from_node = FindNodeIdByName(trans->from_state);
  return from_node >= 0 ? from_node * 2 + 1 : -1;  // output pin
}

int AnimGraphDelegate::GetLinkToPin(int link_id) const {
  const auto* trans =
      const_cast<AnimGraphDelegate*>(this)->FindTransitionById(link_id);
  if (!trans) {
    return -1;
  }
  int to_node = FindNodeIdByName(trans->to_state);
  return to_node >= 0 ? to_node * 2 : -1;  // input pin
}

// --- Editing ---

void AnimGraphDelegate::OnLinkCreated(int from_node_id, int to_node_id) {
  AnimationTransition trans;
  trans.from_state = FindStateNameById(from_node_id);
  trans.to_state = FindStateNameById(to_node_id);
  trans.editor_id = next_id_++;
  controller_->transitions.push_back(std::move(trans));
  is_dirty = true;
}

void AnimGraphDelegate::OnLinkDeleted(int link_id) {
  auto& transitions = controller_->transitions;
  transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
                                   [link_id](const AnimationTransition& t) {
                                     return t.editor_id == link_id;
                                   }),
                    transitions.end());
  if (selected_link_id == link_id) {
    selected_link_id = -1;
  }
  is_dirty = true;
}

void AnimGraphDelegate::OnNodeDeleted(int node_id) {
  std::string name = FindStateNameById(node_id);
  if (name.empty()) {
    return;
  }

  // Remove all transitions referencing this state
  auto& transitions = controller_->transitions;
  transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
                                   [&name](const AnimationTransition& t) {
                                     return t.from_state == name ||
                                            t.to_state == name;
                                   }),
                    transitions.end());

  // Remove the state
  auto& states = controller_->states;
  states.erase(
      std::remove_if(states.begin(), states.end(),
                     [node_id](const AnimControllerAssetData::State& s) {
                       return s.editor_id == node_id;
                     }),
      states.end());

  if (selected_node_id == node_id) {
    selected_node_id = -1;
  }
  is_dirty = true;
}

void AnimGraphDelegate::OnAddNode(const std::string& /*type*/,
                                  glm::vec2 position) {
  AnimControllerAssetData::State state;
  state.name = "State" + std::to_string(controller_->states.size());
  state.editor_pos = position;
  state.editor_id = next_id_++;
  controller_->states.push_back(std::move(state));

  if (controller_->default_state.empty()) {
    controller_->default_state = controller_->states.back().name;
  }
  is_dirty = true;
}

int AnimGraphDelegate::GetAddableNodeTypeCount() const {
  return 1;
}

const char* AnimGraphDelegate::GetAddableNodeType(int /*index*/) const {
  return "Add State";
}

// --- Selection ---

void AnimGraphDelegate::OnNodeSelected(int node_id) {
  selected_node_id = node_id;
  selected_link_id = -1;
}

void AnimGraphDelegate::OnLinkSelected(int link_id) {
  selected_link_id = link_id;
  selected_node_id = -1;
}

void AnimGraphDelegate::OnSelectionCleared() {
  selected_node_id = -1;
  selected_link_id = -1;
}

// --- Properties sidebar ---

void AnimGraphDelegate::RenderProperties() {
  if (selected_node_id >= 0) {
    auto* state = FindStateById(selected_node_id);
    if (state) {
      ImGui::TextColored(ImVec4(0.86f, 0.26f, 0.26f, 1.0f), "State");
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::InputText("Name", &state->name);
      // Animation clip picker
      {
        std::string clip_name = "(None)";
        if (state->clip_handle.IsValid()) {
          const auto* meta =
              Engine::asset_manager().GetMetadata(state->clip_handle);
          if (meta) {
            clip_name = meta->name;
          }
        }
        if (ImGui::BeginCombo("Clip", clip_name.c_str())) {
          for (AssetHandle h :
               Engine::asset_manager().GetAllOfType(AssetType::AnimClip)) {
            const auto* m = Engine::asset_manager().GetMetadata(h);
            if (!m) {
              continue;
            }
            bool selected = (state->clip_handle == h);
            if (ImGui::Selectable(m->name.c_str(), selected)) {
              state->clip_handle = h;
              is_dirty = true;
            }
          }
          ImGui::EndCombo();
        }
      }
      ImGui::DragFloat("Speed", &state->speed, 0.01f, 0.0f, 10.0f);

      bool is_default = (controller_->default_state == state->name);
      if (ImGui::Checkbox("Default State", &is_default)) {
        if (is_default) {
          controller_->default_state = state->name;
        }
      }
      is_dirty = true;
    }
  } else if (selected_link_id >= 0) {
    auto* trans = FindTransitionById(selected_link_id);
    if (trans) {
      ImGui::TextColored(ImVec4(0.86f, 0.26f, 0.26f, 1.0f), "Transition");
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::Text("From: %s", trans->from_state.empty()
                                  ? "(Any)"
                                  : trans->from_state.c_str());
      ImGui::Text("To: %s", trans->to_state.c_str());
      ImGui::DragFloat("Blend Duration", &trans->blend_duration, 0.01f, 0.0f,
                       5.0f);

      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.86f, 0.26f, 0.26f, 1.0f), "Conditions");
      ImGui::Spacing();
      auto& params = controller_->default_parameters;

      for (size_t i = 0; i < trans->conditions.size(); i++) {
        auto& cond = trans->conditions[i];
        ImGui::PushID(static_cast<int>(i));

        // Dark box around each condition
        ImVec2 region = ImGui::GetContentRegionAvail();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            cursor, ImVec2(cursor.x + region.x, cursor.y + 4.0f),
            IM_COL32(0, 0, 0, 0));  // spacer

        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
        ImGui::BeginChild(
            ("##cond" + std::to_string(i)).c_str(), ImVec2(-1, 0),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        // Parameter dropdown + remove button
        std::string param_label =
            cond.param_name.empty() ? "(Select Parameter)" : cond.param_name;
        ImGui::SetNextItemWidth(-30);
        if (ImGui::BeginCombo("##param", param_label.c_str())) {
          for (const auto& [name, param] : params) {
            const char* type_suffix = "";
            switch (param.type) {
              case AnimParamType::Bool:
                type_suffix = " (Bool)";
                break;
              case AnimParamType::Int:
                type_suffix = " (Int)";
                break;
              case AnimParamType::Float:
                type_suffix = " (Float)";
                break;
              case AnimParamType::Trigger:
                type_suffix = " (Trigger)";
                break;
            }
            std::string display = name + type_suffix;
            if (ImGui::Selectable(display.c_str(), cond.param_name == name)) {
              cond.param_name = name;
              cond.param_type = param.type;
            }
          }
          ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
          ImGui::EndChild();
          ImGui::PopStyleColor();
          ImGui::EndGroup();
          trans->conditions.erase(trans->conditions.begin() +
                                  static_cast<ptrdiff_t>(i));
          ImGui::PopID();
          is_dirty = true;
          break;
        }

        // Operator and value on the same line where possible
        if (cond.param_type == AnimParamType::Trigger) {
          ImGui::TextDisabled("Fires when triggered");
        } else if (cond.param_type == AnimParamType::Bool) {
          const char* bool_ops[] = {"==", "!="};
          int op_idx = (cond.op == ConditionOp::NotEquals) ? 1 : 0;
          ImGui::SetNextItemWidth(60);
          if (ImGui::Combo("##op", &op_idx, bool_ops, 2)) {
            cond.op =
                (op_idx == 1) ? ConditionOp::NotEquals : ConditionOp::Equals;
          }
          ImGui::SameLine();
          ImGui::Checkbox("##val", &cond.value.b);
        } else {
          const char* num_ops[] = {"==", "!=", ">", "<"};
          int op_idx = static_cast<int>(cond.op);
          ImGui::SetNextItemWidth(60);
          if (ImGui::Combo("##op", &op_idx, num_ops, 4)) {
            cond.op = static_cast<ConditionOp>(op_idx);
          }
          ImGui::SameLine();
          ImGui::SetNextItemWidth(-30);
          if (cond.param_type == AnimParamType::Int) {
            ImGui::InputInt("##val", &cond.value.i);
          } else {
            ImGui::DragFloat("##val", &cond.value.f, 0.01f);
          }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::PopID();
      }
      if (ImGui::Button("+ Condition")) {
        trans->conditions.push_back({});
      }
      is_dirty = true;
    }
  } else {
    ImGui::TextDisabled("Select a node or link to view properties");
  }
}

}  // namespace wiesel::editor
