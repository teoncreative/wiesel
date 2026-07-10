
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

#include <nlohmann/json.hpp>

#include "behavior/w_behavior.h"
#include "script/w_scriptmanager.h"

namespace wiesel {

class ScriptInstance;

class MonoBehavior : public IBehavior {
 public:
  MonoBehavior(Entity entity, const std::string& script_name);
  ~MonoBehavior() override;

  void OnUpdate(float_t delta_time) override;
  void OnEvent(Event& event) override;

  template <class T>
  void AttachExternComponent(std::string variable, entt::entity entity) {
    if (unset_ || !enabled_) {
      return;
    }
    script_instance_->AttachExternComponent<T>(variable, entity);
  }

  ScriptInstance* script_instance() const { return script_instance_.get(); }

  // Store field values to apply when the script instance becomes available.
  // If the script instance already exists, apply them immediately.
  void SetPendingFields(nlohmann::json fields);
  void ApplyPendingFields();

  bool OnPointerClick(float x, float y) override;

  bool OnPointerDown(float x, float y) override;

  bool OnPointerUp(float x, float y) override;

  void OnPointerEnter() override;

  void OnPointerExit() override;

  void OnSelect() override;

  void OnDeselect() override;

  bool OnSubmit() override;

  bool OnCancel() override;

  void OnSyncVarChanged(const std::string& var_name) override;
  void OnServerRpc(const std::string& rpc_name,
                   const std::string& args_json) override;
  void OnClientRpc(const std::string& rpc_name,
                   const std::string& args_json) override;

 private:
  void InstantiateScript();
  bool OnReloadScripts(ScriptsReloadedEvent& event);
  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);
  bool OnNetworkClientConnected(class NetworkClientConnectedEvent& event);
  bool OnNetworkClientDisconnected(class NetworkClientDisconnectedEvent& event);
  bool OnNetworkConnectedToServer(class NetworkConnectedToServerEvent& event);
  bool OnNetworkDisconnectedFromServer(
      class NetworkDisconnectedFromServerEvent& event);

  std::unique_ptr<ScriptInstance> script_instance_;
  nlohmann::json pending_fields_;
};

}  // namespace wiesel