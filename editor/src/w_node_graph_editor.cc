//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_node_graph_editor.h"

#include <imgui.h>
#include <imnodes.h>

namespace Wiesel::Editor {

NodeGraphEditor::NodeGraphEditor() {
  context_ = ImNodes::EditorContextCreate();
}

NodeGraphEditor::~NodeGraphEditor() {
  if (context_) {
    ImNodes::EditorContextFree(context_);
  }
}

void NodeGraphEditor::Render(INodeGraphDelegate& delegate) {
  ImNodes::EditorContextSet(context_);

  // Apply theme colors matching the editor
  ImVec4* imgui_colors = ImGui::GetStyle().Colors;
  ImGuiStyle& style = ImGui::GetStyle();

  // Node colors - match editor panel backgrounds
  ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(20, 20, 20, 230));
  ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered,
                          IM_COL32(28, 28, 28, 230));
  ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected,
                          IM_COL32(28, 28, 28, 230));
  ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(55, 55, 55, 200));

  // Title bar - use accent color
  ImU32 accent =
      ImGui::ColorConvertFloat4ToU32(imgui_colors[ImGuiCol_CheckMark]);
  ImU32 accent_dim =
      IM_COL32((accent & 0xFF) * 3 / 4, ((accent >> 8) & 0xFF) * 3 / 4,
               ((accent >> 16) & 0xFF) * 3 / 4, 200);
  ImNodes::PushColorStyle(ImNodesCol_TitleBar, accent_dim);
  ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, accent);
  ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, accent);

  // Links
  ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(160, 160, 160, 200));
  ImNodes::PushColorStyle(ImNodesCol_LinkHovered, accent);
  ImNodes::PushColorStyle(ImNodesCol_LinkSelected, accent);

  // Pins
  ImNodes::PushColorStyle(ImNodesCol_Pin, IM_COL32(180, 180, 180, 255));
  ImNodes::PushColorStyle(ImNodesCol_PinHovered, accent);

  // Grid
  ImNodes::PushColorStyle(ImNodesCol_GridBackground, IM_COL32(14, 14, 14, 255));
  ImNodes::PushColorStyle(ImNodesCol_GridLine, IM_COL32(30, 30, 30, 255));
  ImNodes::PushColorStyle(ImNodesCol_GridLinePrimary,
                          IM_COL32(40, 40, 40, 255));

  // Style vars - match editor rounding
  ImNodes::PushStyleVar(ImNodesStyleVar_NodeCornerRounding,
                        style.FrameRounding);
  ImNodes::PushStyleVar(ImNodesStyleVar_NodePadding, ImVec2(10.0f, 6.0f));
  ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 1.5f);
  ImNodes::PushStyleVar(ImNodesStyleVar_LinkThickness, 2.5f);
  ImNodes::PushStyleVar(ImNodesStyleVar_PinCircleRadius, 4.5f);

  // Set initial node positions on first render
  if (!initialized_) {
    for (int i = 0; i < delegate.GetNodeCount(); i++) {
      int node_id = delegate.GetNodeId(i);
      glm::vec2 pos = delegate.GetNodePosition(node_id);
      ImNodes::SetNodeEditorSpacePos(node_id, ImVec2(pos.x, pos.y));
    }
    initialized_ = true;
  }

  ImNodes::BeginNodeEditor();

  // Render nodes
  for (int i = 0; i < delegate.GetNodeCount(); i++) {
    int node_id = delegate.GetNodeId(i);

    ImNodes::BeginNode(node_id);

    // Title bar
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(delegate.GetNodeTitle(node_id));
    ImNodes::EndNodeTitleBar();

    // Input pin
    if (delegate.HasInputPin(node_id)) {
      ImNodes::BeginInputAttribute(node_id * 2);
      ImGui::TextUnformatted("In");
      ImNodes::EndInputAttribute();
    }

    // Custom body content
    delegate.RenderNodeBody(node_id);

    // Output pin
    ImNodes::BeginOutputAttribute(node_id * 2 + 1);
    ImGui::TextUnformatted("Out");
    ImNodes::EndOutputAttribute();

    ImNodes::EndNode();
  }

  // Render links
  for (int i = 0; i < delegate.GetLinkCount(); i++) {
    int link_id = delegate.GetLinkId(i);
    int from_pin = delegate.GetLinkFromPin(link_id);
    int to_pin = delegate.GetLinkToPin(link_id);
    ImNodes::Link(link_id, from_pin, to_pin);
  }

  ImNodes::EndNodeEditor();

  // Detect right-click targets (must be after EndNodeEditor)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    int hovered_node = -1;
    int hovered_link = -1;

    if (ImNodes::IsNodeHovered(&hovered_node)) {
      right_clicked_node_ = hovered_node;
      right_clicked_link_ = -1;
      ImGui::OpenPopup("##NodeContextMenu");
    } else if (ImNodes::IsLinkHovered(&hovered_link)) {
      right_clicked_link_ = hovered_link;
      right_clicked_node_ = -1;
      ImGui::OpenPopup("##LinkContextMenu");
    } else if (ImNodes::IsEditorHovered()) {
      right_clicked_node_ = -1;
      right_clicked_link_ = -1;
      ImVec2 mouse = ImGui::GetMousePos();
      context_menu_pos_ = glm::vec2(mouse.x, mouse.y);
      ImGui::OpenPopup("##GraphContextMenu");
    }
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

  if (ImGui::BeginPopup("##GraphContextMenu")) {
    int type_count = delegate.GetAddableNodeTypeCount();
    for (int i = 0; i < type_count; i++) {
      const char* type_name = delegate.GetAddableNodeType(i);
      if (ImGui::MenuItem(type_name)) {
        delegate.OnAddNode(type_name, context_menu_pos_);
      }
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("##NodeContextMenu")) {
    if (right_clicked_node_ >= 0) {
      ImGui::TextDisabled("%s", delegate.GetNodeTitle(right_clicked_node_));
      ImGui::Separator();
      if (ImGui::MenuItem("Remove State")) {
        delegate.OnNodeDeleted(right_clicked_node_);
        right_clicked_node_ = -1;
      }
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("##LinkContextMenu")) {
    if (right_clicked_link_ >= 0) {
      ImGui::TextDisabled("Transition");
      ImGui::Separator();
      if (ImGui::MenuItem("Remove Transition")) {
        delegate.OnLinkDeleted(right_clicked_link_);
        right_clicked_link_ = -1;
      }
    }
    ImGui::EndPopup();
  }

  ImGui::PopStyleVar();

  // Pop style
  ImNodes::PopStyleVar(5);
  ImNodes::PopColorStyle();  // PinHovered
  ImNodes::PopColorStyle();  // Pin
  ImNodes::PopColorStyle();  // LinkSelected
  ImNodes::PopColorStyle();  // LinkHovered
  ImNodes::PopColorStyle();  // Link
  ImNodes::PopColorStyle();  // TitleBarSelected
  ImNodes::PopColorStyle();  // TitleBarHovered
  ImNodes::PopColorStyle();  // TitleBar
  ImNodes::PopColorStyle();  // NodeOutline
  ImNodes::PopColorStyle();  // NodeBackgroundSelected
  ImNodes::PopColorStyle();  // NodeBackgroundHovered
  ImNodes::PopColorStyle();  // NodeBackground
  ImNodes::PopColorStyle();  // GridLinePrimary
  ImNodes::PopColorStyle();  // GridLine
  ImNodes::PopColorStyle();  // GridBackground

  // Read back node positions
  for (int i = 0; i < delegate.GetNodeCount(); i++) {
    int node_id = delegate.GetNodeId(i);
    ImVec2 pos = ImNodes::GetNodeEditorSpacePos(node_id);
    delegate.SetNodePosition(node_id, {pos.x, pos.y});
  }

  // Handle new link creation
  int from_pin, to_pin;
  if (ImNodes::IsLinkCreated(&from_pin, &to_pin)) {
    // Output pins are odd (node_id * 2 + 1), input pins are even (node_id * 2)
    int from_node = from_pin / 2;
    int to_node = to_pin / 2;
    if (from_node != to_node) {
      delegate.OnLinkCreated(from_node, to_node);
    }
  }

  // Handle link deletion
  int destroyed_link;
  if (ImNodes::IsLinkDestroyed(&destroyed_link)) {
    delegate.OnLinkDeleted(destroyed_link);
  }

  // Handle selection
  int num_selected_nodes = ImNodes::NumSelectedNodes();
  int num_selected_links = ImNodes::NumSelectedLinks();

  if (num_selected_nodes == 1) {
    int selected;
    ImNodes::GetSelectedNodes(&selected);
    delegate.OnNodeSelected(selected);
  } else if (num_selected_links == 1) {
    int selected;
    ImNodes::GetSelectedLinks(&selected);
    delegate.OnLinkSelected(selected);
  } else if (num_selected_nodes == 0 && num_selected_links == 0) {
    delegate.OnSelectionCleared();
  }

  // Handle delete key
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    if (num_selected_links > 0) {
      std::vector<int> selected(num_selected_links);
      ImNodes::GetSelectedLinks(selected.data());
      for (int link_id : selected) {
        delegate.OnLinkDeleted(link_id);
      }
      ImNodes::ClearLinkSelection();
    }
    if (num_selected_nodes > 0) {
      std::vector<int> selected(num_selected_nodes);
      ImNodes::GetSelectedNodes(selected.data());
      for (int node_id : selected) {
        delegate.OnNodeDeleted(node_id);
      }
      ImNodes::ClearNodeSelection();
    }
  }
}

}  // namespace Wiesel::Editor
