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

#include "animation/w_animation_controller_asset.h"
#include "w_node_graph_editor.h"

#include <memory>

namespace wiesel::editor {

// INodeGraphDelegate implementation for AnimControllerAssetData.
// Maps states to nodes and transitions to links.
class AnimGraphDelegate : public INodeGraphDelegate {
 public:
  explicit AnimGraphDelegate(
      std::shared_ptr<AnimControllerAssetData> controller);

  // --- Node access ---
  int GetNodeCount() const override;
  int GetNodeId(int index) const override;
  const char* GetNodeTitle(int node_id) const override;
  glm::vec2 GetNodePosition(int node_id) const override;
  void SetNodePosition(int node_id, glm::vec2 pos) override;
  bool HasInputPin(int node_id) const override;
  void RenderNodeBody(int node_id) override;

  // --- Link access ---
  int GetLinkCount() const override;
  int GetLinkId(int index) const override;
  int GetLinkFromPin(int link_id) const override;
  int GetLinkToPin(int link_id) const override;

  // --- Editing ---
  void OnLinkCreated(int from_node_id, int to_node_id) override;
  void OnLinkDeleted(int link_id) override;
  void OnNodeDeleted(int node_id) override;
  void OnAddNode(const std::string& type, glm::vec2 position) override;

  int GetAddableNodeTypeCount() const override;
  const char* GetAddableNodeType(int index) const override;

  // --- Selection ---
  void OnNodeSelected(int node_id) override;
  void OnLinkSelected(int link_id) override;
  void OnSelectionCleared() override;

  // The currently selected node/link for the properties sidebar
  int selected_node_id = -1;
  int selected_link_id = -1;

  // Render the properties panel for the currently selected item
  void RenderProperties();

  bool is_dirty = false;

 private:
  std::shared_ptr<AnimControllerAssetData> controller_;
  int32_t next_id_ = 1;

  AnimControllerAssetData::State* FindStateById(int node_id);
  const AnimControllerAssetData::State* FindStateById(int node_id) const;
  AnimationTransition* FindTransitionById(int link_id);
  std::string FindStateNameById(int node_id) const;
  int FindNodeIdByName(const std::string& name) const;
  void EnsureIds();
};

}  // namespace wiesel::editor
