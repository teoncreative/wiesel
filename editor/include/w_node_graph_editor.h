//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <glm/glm.hpp>
#include <string>

struct ImNodesEditorContext;

namespace wiesel::editor {

// Domain-specific interface for the graph editor.
// Implement this to use NodeGraphEditor with any node/link data structure.
class INodeGraphDelegate {
 public:
  virtual ~INodeGraphDelegate() = default;

  // --- Node access ---
  virtual int GetNodeCount() const = 0;
  virtual int GetNodeId(int index) const = 0;
  virtual const char* GetNodeTitle(int node_id) const = 0;
  virtual glm::vec2 GetNodePosition(int node_id) const = 0;
  virtual void SetNodePosition(int node_id, glm::vec2 pos) = 0;

  // Whether this node has an input pin (false for "Any State" style nodes)
  virtual bool HasInputPin(int node_id) const { return true; }

  // Render custom ImGui content inside a node body
  virtual void RenderNodeBody(int node_id) = 0;

  // --- Link access ---
  virtual int GetLinkCount() const = 0;
  virtual int GetLinkId(int index) const = 0;
  // Pin IDs: input = node_id * 2, output = node_id * 2 + 1
  virtual int GetLinkFromPin(int link_id) const = 0;
  virtual int GetLinkToPin(int link_id) const = 0;

  // --- Editing callbacks ---
  virtual void OnLinkCreated(int from_node_id, int to_node_id) = 0;
  virtual void OnLinkDeleted(int link_id) = 0;
  virtual void OnNodeDeleted(int node_id) = 0;
  virtual void OnAddNode(const std::string& type, glm::vec2 position) = 0;

  // What node types can be added via context menu
  virtual int GetAddableNodeTypeCount() const { return 0; }

  virtual const char* GetAddableNodeType(int index) const { return ""; }

  // --- Selection callbacks ---
  virtual void OnNodeSelected(int node_id) {}

  virtual void OnLinkSelected(int link_id) {}

  virtual void OnSelectionCleared() {}
};

// Generic node graph editor widget powered by imnodes.
// Call Render() inside an ImGui window. All domain logic is handled
// by the delegate - this class only drives the imnodes UI.
class NodeGraphEditor {
 public:
  NodeGraphEditor();
  ~NodeGraphEditor();

  void Render(INodeGraphDelegate& delegate);

 private:
  ImNodesEditorContext* context_ = nullptr;
  bool initialized_ = false;
  glm::vec2 context_menu_pos_ = {0.0f, 0.0f};
  int right_clicked_node_ = -1;
  int right_clicked_link_ = -1;
};

}  // namespace wiesel::editor
