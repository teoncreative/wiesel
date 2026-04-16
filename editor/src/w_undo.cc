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

namespace wiesel::editor {

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

TransformCommand::TransformCommand(Scene* scene,
                                   entt::entity entity, glm::vec3 old_pos,
                                   glm::vec3 old_rot, glm::vec3 old_scale,
                                   glm::vec3 new_pos, glm::vec3 new_rot,
                                   glm::vec3 new_scale)
    : scene_(scene->GetHandle()),
      entity_(entity),
      old_pos_(old_pos),
      old_rot_(old_rot),
      old_scale_(old_scale),
      new_pos_(new_pos),
      new_rot_(new_rot),
      new_scale_(new_scale) {}

void TransformCommand::Execute() {
  Scene* s = scene_.Resolve();
  if (!s || !s->HasEntity(entity_)) {
    return;
  }
  auto& t = s->GetComponent<TransformComponent>(entity_);
  t.SetPosition(new_pos_);
  t.SetRotation(new_rot_);
  t.SetScale(new_scale_);
}

void TransformCommand::Undo() {
  Scene* s = scene_.Resolve();
  if (!s || !s->HasEntity(entity_)) {
    return;
  }
  auto& t = s->GetComponent<TransformComponent>(entity_);
  t.SetPosition(old_pos_);
  t.SetRotation(old_rot_);
  t.SetScale(old_scale_);
}

std::string TransformCommand::GetDescription() const {
  Scene* s = scene_.Resolve();
  if (!s || !s->HasEntity(entity_)) {
    return "Transform";
  }
  return "Transform " + s->GetComponent<TagComponent>(entity_).name;
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

EntityCreateCommand::EntityCreateCommand(EntityRef ref)
    : entity_ref_(ref) {
  CaptureState();
}

void EntityCreateCommand::CaptureState() {
  Entity entity = entity_ref_.Resolve();
  if (!entity) {
    // Scene no longer contains the entity or the scene itself is gone
    return;
  }
  uuid_ = entity.GetUUID();
  name_ = entity.GetName();
  components_json_ = nlohmann::json::object();
  ComponentSerializerRegistry::SerializeAll(entity, components_json_);

  Entity parent = entity.GetParent();
  if (parent) {
    parent_uuid_ = entity.GetUUID();
  }
}

void EntityCreateCommand::Execute() {
  if (first_execute_) {
    first_execute_ = false;
    // This command is designed to be added after the entity was created
    // Check prevents adding the entity twice
    return;
  }
  Scene* scene = entity_ref_.ResolveScene();
  if (!scene) {
    // Scene is gone
    return;
  }
  // Create a new entity and update the ref as well
  Entity entity = scene->CreateEntityWithUUID(uuid_, name_);
  entity_ref_ = entity.ToRef();
  ComponentSerializerRegistry::DeserializeAll(entity, components_json_, scene);

  // Attach to parent if given
  if (!parent_uuid_.IsNil()) {
    entt::entity parent = scene->FindEntityByUUID(parent_uuid_);
    if (parent != entt::null) {
      scene->LinkEntities(parent, entity, false);
    }
  }
}

void EntityCreateCommand::Undo() {
  CaptureState();
  Entity entity = entity_ref_.Resolve();
  if (entity) {
    Scene* scene = entity.GetScene();
    scene->RemoveEntity(entity);
    scene->ProcessDestroyQueue();
  }
}

std::string EntityCreateCommand::GetDescription() const {
  return "Create " + name_;
}

// --- EntityDeleteCommand ---

EntityDeleteCommand::EntityDeleteCommand(Entity entity)
    : ref_(entity.ToRef()), scene_handle_(entity.GetSceneHandle()) {
  name_ = entity.GetName();
  if (Entity parent = entity.GetParent()) {
      parent_uuid_ = parent.GetUUID();
  }
}

void EntityDeleteCommand::Execute() {
  Scene* s = scene_handle_.Resolve();
  if (!s || !s->HasEntity(ref_)) {
    return;
  }
  Entity entity = ref_.Resolve();
  subtree_json_ = Prefab::SerializeEntityTree(entity);
  s->RemoveEntity(entity);
  s->ProcessDestroyQueue();
}

void EntityDeleteCommand::Undo() {
  Scene* s = scene_handle_.Resolve();
  if (!s) {
    return;
  }
  Entity root = Prefab::DeserializeEntityTree(*s, subtree_json_);
  if (!parent_uuid_.IsNil()) {
    entt::entity parent = s->FindEntityByUUID(parent_uuid_);
    if (parent != entt::null) {
      s->LinkEntities(parent, root, false);
    }
  }
}

std::string EntityDeleteCommand::GetDescription() const {
  return "Delete " + name_;
}

// --- ReparentCommand ---

ReparentCommand::ReparentCommand(Scene* scene,
                                 entt::entity entity, entt::entity old_parent,
                                 entt::entity new_parent)
    : scene_handle_(scene->GetHandle()),
      entity_(entity),
      old_parent_(old_parent),
      new_parent_(new_parent) {}

void ReparentCommand::Execute() {
  Scene* s = scene_handle_.Resolve();
  if (!s || !s->HasEntity(entity_)) {
    return;
  }
  if (old_parent_ != entt::null && s->HasEntity(old_parent_)) {
    s->UnlinkEntities(old_parent_, entity_);
  }
  if (new_parent_ != entt::null && s->HasEntity(new_parent_)) {
    s->LinkEntities(new_parent_, entity_);
  }
}

void ReparentCommand::Undo() {
  Scene* s = scene_handle_.Resolve();
  if (!s || !s->HasEntity(entity_)) {
    return;
  }
  if (new_parent_ != entt::null && s->HasEntity(new_parent_)) {
    s->UnlinkEntities(new_parent_, entity_);
  }
  if (old_parent_ != entt::null && s->HasEntity(old_parent_)) {
    s->LinkEntities(old_parent_, entity_);
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

}  // namespace wiesel::editor
