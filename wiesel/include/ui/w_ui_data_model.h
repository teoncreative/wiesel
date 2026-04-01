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

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Variant.h>

#include "asset/w_asset_properties.h"
#include "w_pch.h"

namespace Rml {
class Context;
class Event;
}  // namespace Rml

namespace Wiesel {

enum class UIBindingMode {
  TwoWay,    // Code and UI can both read/write (default)
  ReadOnly,  // Code sets, UI only reads
};

class UIDataModel {
 public:
  UIDataModel() = default;
  ~UIDataModel();

  // Initialize the data model on the given RmlUi context.
  // Creates an RmlUi data model named "model".
  // If properties is provided, pre-registers all declared variables.
  void Init(Rml::Context* context,
            const UIDocumentAssetProperties* properties = nullptr);

  // Shut down and remove the data model from the context.
  void Shutdown();

  // Set typed values (code -> UI). Registers the variable on first use.
  void SetInt(const std::string& name, int value);
  void SetFloat(const std::string& name, float value);
  void SetString(const std::string& name, const std::string& value);
  void SetBool(const std::string& name, bool value);

  // Get typed values (UI -> code).
  int GetInt(const std::string& name) const;
  float GetFloat(const std::string& name) const;
  std::string GetString(const std::string& name) const;
  bool GetBool(const std::string& name) const;

  // Set binding mode for a variable. Must be called before the first Set/Get
  // that registers the variable with RmlUi.
  void SetBindingMode(const std::string& name, UIBindingMode mode);

  // Register a callback for when the UI changes a variable (TwoWay only).
  void OnChanged(const std::string& name, std::function<void()> callback);

  // Bind an event callback (data-event-click="name" in RML).
  void BindEvent(const std::string& name,
                 std::function<void(Rml::Event&)> callback);

  // Mark all code-side changes as dirty so context->Update() picks them up.
  // Call this before context->Update().
  void Flush();

  bool IsInitialized() const { return context_ != nullptr; }

 private:
  struct VariableEntry {
    Rml::Variant value;
    UIBindingMode mode = UIBindingMode::TwoWay;
    bool dirty = false;
    bool registered = false;  // Whether BindFunc has been called
    std::function<void()> on_changed;
  };

  // Ensure a variable entry exists, creating it if needed.
  VariableEntry& EnsureEntry(const std::string& name);

  // Register a variable with RmlUi via BindFunc. Called on first Set.
  void RegisterVariable(const std::string& name, VariableEntry& entry);

  // Register all pending (unregistered) variables. Called during Init if
  // variables were set before the context was available.
  void RegisterPendingVariables();

  std::unordered_map<std::string, VariableEntry> variables_;
  Rml::DataModelConstructor constructor_;
  Rml::DataModelHandle handle_;
  Rml::Context* context_ = nullptr;
};

}  // namespace Wiesel
