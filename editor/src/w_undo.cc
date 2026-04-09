//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_undo.h"

#include "scene/w_component_serializer.h"
#include "scene/w_entity.h"
#include "scene/w_prefab.h"
#include "util/w_logger.h"

namespace Wiesel::Editor {

// --- CommandStack ---

void CommandStack::Execute(std::unique_ptr<IEditorCommand> cmd) {
  pending_.push_back({PendingAction::Execute, std::move(cmd)});
}

void CommandStack::Undo() {
  pending_.push_back({PendingAction::Undo, nullptr});
}

void CommandStack::Redo() {
  pending_.push_back({PendingAction::Redo, nullptr});
}

void CommandStack::Flush() {
  if (pending_.empty()) {
    return;
  }

  // Move to local to avoid issues if commands trigger more pending entries
  std::vector<PendingEntry> batch;
  batch.swap(pending_);

  for (auto& entry : batch) {
    switch (entry.action) {
      case PendingAction::Execute:
        DoExecute(std::move(entry.cmd));
        break;
      case PendingAction::Undo:
        DoUndo();
        break;
      case PendingAction::Redo:
        DoRedo();
        break;
    }
  }
}

void CommandStack::DoExecute(std::unique_ptr<IEditorCommand> cmd) {
  // Try merging with the current command
  if (current_ >= 0 && current_ < static_cast<int>(history_.size())) {
    if (history_[current_]->MergeWith(*cmd)) {
      return;
    }
  }

  cmd->Execute();

  int erase_start = current_ + 1;
  if (erase_start >= 0 && erase_start < static_cast<int>(history_.size())) {
    history_.erase(history_.begin() + erase_start, history_.end());
  }

  history_.push_back(std::move(cmd));
  current_ = static_cast<int>(history_.size()) - 1;
}

void CommandStack::DoUndo() {
  if (!CanUndo()) {
    return;
  }
  history_[current_]->Undo();
  current_--;
}

void CommandStack::DoRedo() {
  if (!CanRedo()) {
    return;
  }
  current_++;
  history_[current_]->Execute();
}

void CommandStack::Clear() {
  history_.clear();
  pending_.clear();
  current_ = -1;
}

IEditorCommand* CommandStack::GetCurrent() const {
  if (current_ >= 0 && current_ < static_cast<int>(history_.size())) {
    return history_[current_].get();
  }
  return nullptr;
}

// --- TransformCommand ---

TransformCommand::TransformCommand(std::shared_ptr<Scene> scene,
                                   entt::entity entity, glm::vec3 old_pos,
                                   glm::vec3 old_rot, glm::vec3 old_scale,
                                   glm::vec3 new_pos, glm::vec3 new_rot,
                                   glm::vec3 new_scale)
    : scene_(std::move(scene)),
      entity_(entity),
      old_pos_(old_pos),
      old_rot_(old_rot),
      old_scale_(old_scale),
      new_pos_(new_pos),
      new_rot_(new_rot),
      new_scale_(new_scale) {}

void TransformCommand::Execute() {
  if (!scene_->HasEntity(entity_)) {
    return;
  }
  auto& t = scene_->GetComponent<TransformComponent>(entity_);
  t.SetPosition(new_pos_);
  t.SetRotation(new_rot_);
  t.SetScale(new_scale_);
}

void TransformCommand::Undo() {
  if (!scene_->HasEntity(entity_)) {
    return;
  }
  auto& t = scene_->GetComponent<TransformComponent>(entity_);
  t.SetPosition(old_pos_);
  t.SetRotation(old_rot_);
  t.SetScale(old_scale_);
}

std::string TransformCommand::GetDescription() const {
  if (!scene_->HasEntity(entity_)) {
    return "Transform";
  }
  return "Transform " + scene_->GetComponent<TagComponent>(entity_).name;
}

bool TransformCommand::MergeWith(const IEditorCommand& other) {
  auto* o = dynamic_cast<const TransformCommand*>(&other);
  if (!o || o->entity_ != entity_ || o->scene_ != scene_) {
    return false;
  }
  new_pos_ = o->new_pos_;
  new_rot_ = o->new_rot_;
  new_scale_ = o->new_scale_;
  return true;
}

// --- EntityCreateCommand ---

EntityCreateCommand::EntityCreateCommand(std::shared_ptr<Scene> scene,
                                         entt::entity entity)
    : scene_(std::move(scene)), entity_(entity) {
  CaptureState();
}

void EntityCreateCommand::CaptureState() {
  if (!scene_->HasEntity(entity_)) {
    return;
  }
  Entity ent{entity_, scene_.get()};
  uuid_ = ent.GetUUID();
  name_ = ent.GetName();
  components_ = nlohmann::json::object();
  ComponentSerializerRegistry::SerializeAll(ent, components_);

  if (scene_->HasComponent<TreeComponent>(entity_)) {
    auto& tree = scene_->GetComponent<TreeComponent>(entity_);
    if (tree.parent != entt::null && scene_->HasEntity(tree.parent)) {
      parent_uuid_ = scene_->GetComponent<IdComponent>(tree.parent).Id;
    }
  }
}

void EntityCreateCommand::Execute() {
  if (first_execute_) {
    first_execute_ = false;
    return;  // Entity already exists on first execute
  }
  // Recreate the entity with the same UUID
  Entity ent = scene_->CreateEntityWithUUID(uuid_, name_);
  entity_ = ent.handle();
  ComponentSerializerRegistry::DeserializeAll(ent, components_, scene_.get());

  // Restore parent
  if (!parent_uuid_.IsNil()) {
    entt::entity parent = scene_->FindEntityByUUID(parent_uuid_);
    if (parent != entt::null) {
      scene_->LinkEntities(parent, entity_, false);
    }
  }
}

void EntityCreateCommand::Undo() {
  CaptureState();  // Re-capture in case components changed
  if (scene_->HasEntity(entity_)) {
    scene_->RemoveEntity(Entity{entity_, scene_.get()});
    scene_->ProcessDestroyQueue();
  }
}

std::string EntityCreateCommand::GetDescription() const {
  return "Create " + name_;
}

// --- EntityDeleteCommand ---

EntityDeleteCommand::EntityDeleteCommand(std::shared_ptr<Scene> scene,
                                         entt::entity entity)
    : scene_(std::move(scene)), entity_(entity) {
  Entity ent{entity_, scene_.get()};
  name_ = ent.GetName();

  // Capture parent UUID so we can re-link on undo
  if (scene_->HasComponent<TreeComponent>(entity_)) {
    auto& tree = scene_->GetComponent<TreeComponent>(entity_);
    if (tree.parent != entt::null && scene_->HasEntity(tree.parent)) {
      parent_uuid_ = scene_->GetComponent<IdComponent>(tree.parent).Id;
    }
  }

  // Serialize the entire entity subtree (entity + all children)
  subtree_json_ = Prefab::SerializeEntityTree(ent);
}

void EntityDeleteCommand::Execute() {
  if (scene_->HasEntity(entity_)) {
    // Re-capture before deletion in case state changed
    Entity ent{entity_, scene_.get()};
    subtree_json_ = Prefab::SerializeEntityTree(ent);
    scene_->RemoveEntity(ent);
    scene_->ProcessDestroyQueue();
  }
}

void EntityDeleteCommand::Undo() {
  Entity root = Prefab::DeserializeEntityTree(scene_, subtree_json_);
  entity_ = root.handle();

  if (!parent_uuid_.IsNil()) {
    entt::entity parent = scene_->FindEntityByUUID(parent_uuid_);
    if (parent != entt::null) {
      scene_->LinkEntities(parent, entity_, false);
    }
  }
}

std::string EntityDeleteCommand::GetDescription() const {
  return "Delete " + name_;
}

// --- ReparentCommand ---

ReparentCommand::ReparentCommand(std::shared_ptr<Scene> scene,
                                 entt::entity entity, entt::entity old_parent,
                                 entt::entity new_parent)
    : scene_(std::move(scene)),
      entity_(entity),
      old_parent_(old_parent),
      new_parent_(new_parent) {}

void ReparentCommand::Execute() {
  if (!scene_->HasEntity(entity_)) {
    return;
  }
  if (old_parent_ != entt::null && scene_->HasEntity(old_parent_)) {
    scene_->UnlinkEntities(old_parent_, entity_);
  }
  if (new_parent_ != entt::null && scene_->HasEntity(new_parent_)) {
    scene_->LinkEntities(new_parent_, entity_);
  }
}

void ReparentCommand::Undo() {
  if (!scene_->HasEntity(entity_)) {
    return;
  }
  if (new_parent_ != entt::null && scene_->HasEntity(new_parent_)) {
    scene_->UnlinkEntities(new_parent_, entity_);
  }
  if (old_parent_ != entt::null && scene_->HasEntity(old_parent_)) {
    scene_->LinkEntities(old_parent_, entity_);
  }
}

std::string ReparentCommand::GetDescription() const {
  return "Reparent Entity";
}

// --- CompoundCommand ---

CompoundCommand::CompoundCommand(std::string description)
    : description_(std::move(description)) {}

void CompoundCommand::Add(std::unique_ptr<IEditorCommand> cmd) {
  commands_.push_back(std::move(cmd));
}

void CompoundCommand::Execute() {
  for (auto& cmd : commands_) {
    cmd->Execute();
  }
}

void CompoundCommand::Undo() {
  for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
    (*it)->Undo();
  }
}

std::string CompoundCommand::GetDescription() const {
  return description_;
}

}  // namespace Wiesel::Editor
