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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "util/w_uuid.h"

namespace Wiesel {
class Scene;
}

namespace Wiesel::Editor {

// Base class for all editor commands (undo/redo).
class IEditorCommand {
 public:
  virtual ~IEditorCommand() = default;
  virtual void Execute() = 0;
  virtual void Undo() = 0;
  virtual std::string GetDescription() const = 0;

  // If true, consecutive commands of the same type can be merged
  // (e.g., continuous gizmo drag collapses into one entry).
  virtual bool MergeWith(const IEditorCommand& other) { return false; }
};

// Undo/redo stack with cursor-based navigation.
class CommandStack {
 public:
  // Queue a command for execution. All commands are deferred and
  // processed between frames via Flush().
  void Execute(std::unique_ptr<IEditorCommand> cmd);

  // Queue undo/redo for deferred processing.
  void Undo();
  void Redo();

  // Process all pending operations. Call once between frames.
  void Flush();

  bool CanUndo() const { return current_ >= 0; }

  bool CanRedo() const {
    return current_ < static_cast<int>(history_.size()) - 1;
  }

  void Clear();

  // Get the command at the current position (for description in toast).
  // Returns nullptr if stack is empty.
  IEditorCommand* GetCurrent() const;

  // For the history panel.
  const std::vector<std::unique_ptr<IEditorCommand>>& GetHistory() const {
    return history_;
  }

  int GetCurrentIndex() const { return current_; }

 private:
  enum class PendingAction { Execute, Undo, Redo };

  struct PendingEntry {
    PendingAction action;
    std::unique_ptr<IEditorCommand> cmd;  // only for Execute
  };

  void DoExecute(std::unique_ptr<IEditorCommand> cmd);
  void DoUndo();
  void DoRedo();

  std::vector<std::unique_ptr<IEditorCommand>> history_;
  std::vector<PendingEntry> pending_;
  int current_ = -1;
};

// --- Built-in command types ---

// Generic property change command.
// Captures getter/setter lambdas and old/new values.
template <typename T>
class PropertyCommand : public IEditorCommand {
 public:
  PropertyCommand(std::string description, std::function<void(const T&)> setter,
                  T old_value, T new_value)
      : description_(std::move(description)),
        setter_(std::move(setter)),
        old_value_(std::move(old_value)),
        new_value_(std::move(new_value)) {}

  void Execute() override { setter_(new_value_); }

  void Undo() override { setter_(old_value_); }

  std::string GetDescription() const override { return description_; }

  bool MergeWith(const IEditorCommand& other) override {
    auto* o = dynamic_cast<const PropertyCommand<T>*>(&other);
    if (!o || o->description_ != description_) {
      return false;
    }
    new_value_ = o->new_value_;
    return true;
  }

 private:
  std::string description_;
  std::function<void(const T&)> setter_;
  T old_value_;
  T new_value_;
};

// Transform change (position + rotation + scale as one unit).
class TransformCommand : public IEditorCommand {
 public:
  TransformCommand(std::shared_ptr<Scene> scene, entt::entity entity,
                   glm::vec3 old_pos, glm::vec3 old_rot, glm::vec3 old_scale,
                   glm::vec3 new_pos, glm::vec3 new_rot, glm::vec3 new_scale);

  void Execute() override;
  void Undo() override;
  std::string GetDescription() const override;
  bool MergeWith(const IEditorCommand& other) override;

 private:
  std::shared_ptr<Scene> scene_;
  entt::entity entity_;
  glm::vec3 old_pos_, old_rot_, old_scale_;
  glm::vec3 new_pos_, new_rot_, new_scale_;
};

// Entity creation. Undo deletes. Redo recreates with same UUID.
class EntityCreateCommand : public IEditorCommand {
 public:
  EntityCreateCommand(std::shared_ptr<Scene> scene, entt::entity entity);

  void Execute() override;
  void Undo() override;
  std::string GetDescription() const override;

 private:
  void CaptureState();

  std::shared_ptr<Scene> scene_;
  entt::entity entity_;
  UUID uuid_;
  std::string name_;
  UUID parent_uuid_;
  nlohmann::json components_;
  bool first_execute_ = true;
};

// Entity deletion. Execute deletes. Undo recreates.
class EntityDeleteCommand : public IEditorCommand {
 public:
  EntityDeleteCommand(std::shared_ptr<Scene> scene, entt::entity entity);

  void Execute() override;
  void Undo() override;
  std::string GetDescription() const override;

 private:
  std::shared_ptr<Scene> scene_;
  entt::entity entity_;
  std::string name_;
  UUID parent_uuid_;
  nlohmann::json subtree_json_;  // serialized entity subtree
};

// Entity reparenting.
class ReparentCommand : public IEditorCommand {
 public:
  ReparentCommand(std::shared_ptr<Scene> scene, entt::entity entity,
                  entt::entity old_parent, entt::entity new_parent);

  void Execute() override;
  void Undo() override;
  std::string GetDescription() const override;

 private:
  std::shared_ptr<Scene> scene_;
  entt::entity entity_;
  entt::entity old_parent_;
  entt::entity new_parent_;
};

// Groups multiple commands as one undo step.
class CompoundCommand : public IEditorCommand {
 public:
  explicit CompoundCommand(std::string description);

  void Add(std::unique_ptr<IEditorCommand> cmd);

  void Execute() override;
  void Undo() override;
  std::string GetDescription() const override;

 private:
  std::string description_;
  std::vector<std::unique_ptr<IEditorCommand>> commands_;
};

}  // namespace Wiesel::Editor
