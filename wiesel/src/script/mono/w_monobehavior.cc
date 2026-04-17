//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/mono/w_monobehavior.h"

#include "events/w_network_events.h"
#include "input/w_input.h"
#include "networking/w_replication_types.h"
#include "mono_wrappers.h"
#include "script/w_script_field_registry.h"
#include "script/w_scriptmanager.h"
#include "w_engine.h"

namespace wiesel {

MonoBehavior::MonoBehavior(Entity entity, const std::string& script_name)
    : IBehavior(script_name, entity) {
  script_instance_ = nullptr;
  InstantiateScript();
}

MonoBehavior::~MonoBehavior() {}

void MonoBehavior::OnUpdate(float_t delta_time) {
  if (unset_ || !enabled_) {
    return;
  }
  script_instance_->OnUpdate(delta_time);
}

void MonoBehavior::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};

  dispatcher.Dispatch<ScriptsReloadedEvent>(WIESEL_BIND_FN(OnReloadScripts));
  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_FN(OnKeyPressed));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
  dispatcher.Dispatch<NetworkClientConnectedEvent>(
      WIESEL_BIND_FN(OnNetworkClientConnected));
  dispatcher.Dispatch<NetworkClientDisconnectedEvent>(
      WIESEL_BIND_FN(OnNetworkClientDisconnected));
  dispatcher.Dispatch<NetworkConnectedToServerEvent>(
      WIESEL_BIND_FN(OnNetworkConnectedToServer));
  dispatcher.Dispatch<NetworkDisconnectedFromServerEvent>(
      WIESEL_BIND_FN(OnNetworkDisconnectedFromServer));
}

void MonoBehavior::InstantiateScript() {
  if (name_.empty()) {
    return;
  }
  script_instance_ = Engine::script_manager().CreateScriptInstance(this);
  if (script_instance_) {
    unset_ = false;
    enabled_ = true;
    ApplyPendingFields();
  }
}

void MonoBehavior::SetPendingFields(nlohmann::json fields) {
  pending_fields_ = std::move(fields);
  if (script_instance_) {
    ApplyPendingFields();
  }
}

void MonoBehavior::ApplyPendingFields() {
  if (pending_fields_.empty() || !script_instance_) {
    return;
  }
  for (auto& [field_name, field_data] :
       script_instance_->script_data().fields()) {
    if (!pending_fields_.contains(field_name)) {
      continue;
    }
    std::string type_name =
        mono_type_get_name(mono_field_get_type(field_data.field()));
    ScriptFieldTypeRegistry::DeserializeField(
        type_name, script_instance_->handle(), field_data.field(),
        pending_fields_[field_name], scene_);
  }
  pending_fields_.clear();
}

bool MonoBehavior::OnReloadScripts(ScriptsReloadedEvent& event) {
  std::map<std::string, std::function<MonoObject*()>> copy;
  if (script_instance_) {
    copy = script_instance_->attached_variables_;
    // Detach before destroying so the destructor doesn't touch the dead domain
    script_instance_->Detach();
  }
  script_instance_ = nullptr;
  InstantiateScript();
  script_instance_->attached_variables_ = copy;
  return false;
}

bool MonoBehavior::OnKeyPressed(KeyPressedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  return script_instance_->OnKeyPressed(event);
}

bool MonoBehavior::OnKeyReleased(KeyReleasedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  return script_instance_->OnKeyReleased(event);
}

bool MonoBehavior::OnMouseMoved(MouseMovedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  return script_instance_->OnMouseMoved(event);
}

bool MonoBehavior::OnNetworkClientConnected(
    NetworkClientConnectedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  script_instance_->OnClientConnected(event.session_id());
  return false;
}

bool MonoBehavior::OnNetworkClientDisconnected(
    NetworkClientDisconnectedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  script_instance_->OnClientDisconnected(event.session_id());
  return false;
}

bool MonoBehavior::OnNetworkConnectedToServer(
    NetworkConnectedToServerEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  script_instance_->OnConnectedToServer();
  return false;
}

bool MonoBehavior::OnNetworkDisconnectedFromServer(
    NetworkDisconnectedFromServerEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  script_instance_->OnDisconnectedFromServer();
  return false;
}

bool MonoBehavior::OnPointerClick(float x, float y) {
  if (script_instance_) {
    return script_instance_->OnPointerClick(x, y);
  }
  return false;
}

bool MonoBehavior::OnPointerDown(float x, float y) {
  if (script_instance_) {
    return script_instance_->OnPointerDown(x, y);
  }
  return false;
}

bool MonoBehavior::OnPointerUp(float x, float y) {
  if (script_instance_) {
    return script_instance_->OnPointerUp(x, y);
  }
  return false;
}

void MonoBehavior::OnPointerEnter() {
  if (script_instance_) {
    script_instance_->OnPointerEnter();
  }
}

void MonoBehavior::OnPointerExit() {
  if (script_instance_) {
    script_instance_->OnPointerExit();
  }
}

void MonoBehavior::OnSelect() {
  if (script_instance_) {
    script_instance_->OnSelect();
  }
}

void MonoBehavior::OnDeselect() {
  if (script_instance_) {
    script_instance_->OnDeselect();
  }
}

bool MonoBehavior::OnSubmit() {
  if (script_instance_) {
    return script_instance_->OnSubmit();
  }
  return false;
}

bool MonoBehavior::OnCancel() {
  if (script_instance_) {
    return script_instance_->OnCancel();
  }
  return false;
}

void MonoBehavior::OnSyncVarChanged(const std::string& var_name) {
  if (!script_instance_) {
    return;
  }

  auto& fields = script_instance_->script_data().fields();
  auto it = fields.find(var_name);
  if (it == fields.end() || !it->second.is_network_var()) {
    return;
  }

  MonoObject* net_var = nullptr;
  mono_field_get_value(script_instance_->handle(), it->second.field(),
                       &net_var);
  if (!net_var) {
    return;
  }

  if (!scene_ || !scene_->HasComponent<NetworkIdentityComponent>(handle())) {
    return;
  }
  auto& net_id = scene_->GetComponent<NetworkIdentityComponent>(handle());
  auto var_it = net_id.sync_vars.find(var_name);
  if (var_it == net_id.sync_vars.end()) {
    return;
  }

  MonoClass* net_var_class = mono_object_get_class(net_var);
  MonoClassField* val_field =
      mono_class_get_field_from_name(net_var_class, "value");
  if (!val_field) {
    return;
  }

  ScriptFieldTypeRegistry::DeserializeField(
      it->second.inner_type_name(), net_var, val_field, var_it->second,
      scene_);

  script_instance_->OnSyncVarChanged(var_name);
}

}  // namespace wiesel