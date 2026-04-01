//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_data_model.h"

#include <RmlUi/Core/Context.h>

#include "util/w_logger.h"
#include "w_pch.h"

namespace Wiesel {

UIDataModel::~UIDataModel() {
  Shutdown();
}

void UIDataModel::Init(Rml::Context* context,
                       const UIDocumentAssetProperties* properties) {
  if (context_) {
    Shutdown();
  }

  context_ = context;
  constructor_ = context_->CreateDataModel("model");
  if (!constructor_) {
    LOG_ERROR("Failed to create RmlUi data model");
    context_ = nullptr;
    return;
  }

  handle_ = constructor_.GetModelHandle();

  // Pre-register variables declared in asset properties
  if (properties) {
    for (const auto& decl : properties->variables) {
      auto& entry = EnsureEntry(decl.name);
      entry.mode = decl.mode == UIVariableMode::ReadOnly
                       ? UIBindingMode::ReadOnly
                       : UIBindingMode::TwoWay;
      switch (decl.type) {
        case UIVariableType::Int:
          entry.value = Rml::Variant(std::stoi(decl.default_value));
          break;
        case UIVariableType::Float:
          entry.value = Rml::Variant(std::stof(decl.default_value));
          break;
        case UIVariableType::String:
          entry.value = Rml::Variant(Rml::String(decl.default_value));
          break;
        case UIVariableType::Bool:
          entry.value = Rml::Variant(decl.default_value == "true" ||
                                     decl.default_value == "1");
          break;
      }
      entry.dirty = true;
    }
  }

  // Register all variables (both from properties and any set before Init)
  RegisterPendingVariables();
}

void UIDataModel::Shutdown() {
  if (context_) {
    context_->RemoveDataModel("model");
    constructor_ = Rml::DataModelConstructor();
    handle_ = Rml::DataModelHandle();
    context_ = nullptr;
  }

  // Mark all variables as unregistered so they re-register on next Init
  for (auto& [name, entry] : variables_) {
    entry.registered = false;
    entry.dirty = false;
  }
}

void UIDataModel::SetInt(const std::string& name, int value) {
  auto& entry = EnsureEntry(name);
  entry.value = Rml::Variant(value);
  entry.dirty = true;
  if (!entry.registered && context_) {
    RegisterVariable(name, entry);
  }
}

void UIDataModel::SetFloat(const std::string& name, float value) {
  auto& entry = EnsureEntry(name);
  entry.value = Rml::Variant(value);
  entry.dirty = true;
  if (!entry.registered && context_) {
    RegisterVariable(name, entry);
  }
}

void UIDataModel::SetString(const std::string& name, const std::string& value) {
  auto& entry = EnsureEntry(name);
  entry.value = Rml::Variant(Rml::String(value));
  entry.dirty = true;
  if (!entry.registered && context_) {
    RegisterVariable(name, entry);
  }
}

void UIDataModel::SetBool(const std::string& name, bool value) {
  auto& entry = EnsureEntry(name);
  entry.value = Rml::Variant(value);
  entry.dirty = true;
  if (!entry.registered && context_) {
    RegisterVariable(name, entry);
  }
}

int UIDataModel::GetInt(const std::string& name) const {
  auto it = variables_.find(name);
  if (it == variables_.end()) {
    return 0;
  }
  return it->second.value.Get<int>(0);
}

float UIDataModel::GetFloat(const std::string& name) const {
  auto it = variables_.find(name);
  if (it == variables_.end()) {
    return 0.0f;
  }
  return it->second.value.Get<float>(0.0f);
}

std::string UIDataModel::GetString(const std::string& name) const {
  auto it = variables_.find(name);
  if (it == variables_.end()) {
    return "";
  }
  return it->second.value.Get<Rml::String>("");
}

bool UIDataModel::GetBool(const std::string& name) const {
  auto it = variables_.find(name);
  if (it == variables_.end()) {
    return false;
  }
  return it->second.value.Get<bool>(false);
}

void UIDataModel::SetBindingMode(const std::string& name, UIBindingMode mode) {
  auto& entry = EnsureEntry(name);
  if (entry.registered) {
    LOG_WARN(
        "UIDataModel::SetBindingMode called after variable '{}' was already "
        "registered with RmlUi. Mode change will not take effect.",
        name);
    return;
  }
  entry.mode = mode;
}

void UIDataModel::OnChanged(const std::string& name,
                            std::function<void()> callback) {
  auto& entry = EnsureEntry(name);
  entry.on_changed = std::move(callback);
}

void UIDataModel::BindEvent(const std::string& name,
                            std::function<void(Rml::Event&)> callback) {
  if (!constructor_) {
    LOG_WARN(
        "UIDataModel::BindEvent called before Init. Event '{}' will not be "
        "bound.",
        name);
    return;
  }

  constructor_.BindEventCallback(
      name, [callback = std::move(callback)](
                Rml::DataModelHandle /*handle*/, Rml::Event& event,
                const Rml::VariantList& /*arguments*/) { callback(event); });
}

void UIDataModel::Flush() {
  if (!handle_) {
    return;
  }

  for (auto& [name, entry] : variables_) {
    if (entry.dirty) {
      handle_.DirtyVariable(name);
      entry.dirty = false;
    }
  }
}

UIDataModel::VariableEntry& UIDataModel::EnsureEntry(const std::string& name) {
  auto it = variables_.find(name);
  if (it == variables_.end()) {
    it = variables_.emplace(name, VariableEntry{}).first;
  }
  return it->second;
}

void UIDataModel::RegisterVariable(const std::string& name,
                                   VariableEntry& entry) {
  if (entry.registered) {
    return;
  }

  // Getter: read from our map
  Rml::DataGetFunc getter = [this, name](Rml::Variant& variant) {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
      variant = it->second.value;
    }
  };

  // Setter: write to our map and fire on_changed (TwoWay only)
  Rml::DataSetFunc setter;
  if (entry.mode == UIBindingMode::TwoWay) {
    setter = [this, name](const Rml::Variant& variant) {
      auto it = variables_.find(name);
      if (it != variables_.end()) {
        it->second.value = variant;
        if (it->second.on_changed) {
          it->second.on_changed();
        }
      }
    };
  }

  constructor_.BindFunc(name, std::move(getter), std::move(setter));
  entry.registered = true;
}

void UIDataModel::RegisterPendingVariables() {
  for (auto& [name, entry] : variables_) {
    if (!entry.registered) {
      RegisterVariable(name, entry);
      entry.dirty = true;  // Ensure initial values are pushed to UI
    }
  }
}

}  // namespace Wiesel
