
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_componentutil.hpp"

#include "asset/w_asset_manager.hpp"
#include "behavior/w_behavior.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_lights.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "util/imgui/w_imguiutil.hpp"
#include "util/w_dialogs.hpp"
#include "util/w_logger.hpp"
#include "w_application.hpp"
#include "w_engine.hpp"
#include "mono_util.h"
#include <typeindex>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <ranges>

namespace Wiesel {

void RenderComponentImGui(TransformComponent& component, Entity entity) {
  if (ImGui::ClosableTreeNode("Transform", nullptr)) {
    bool changed = false;
    changed |=
        ImGui::DragFloat3(PrefixLabel("Position").c_str(),
                          reinterpret_cast<float*>(&component.position), 0.1f);
    changed |=
        ImGui::DragFloat3(PrefixLabel("Rotation").c_str(),
                          reinterpret_cast<float*>(&component.rotation), 0.1f);
    changed |=
        ImGui::DragFloat3(PrefixLabel("Scale").c_str(),
                          reinterpret_cast<float*>(&component.scale), 0.1f);
    if (changed) {
      component.is_changed = true;
    }
    ImGui::TreePop();
  }
}

// Accepts an AssetHandle drag-drop payload, filtered by type.
// Returns true and writes the handle if accepted; false otherwise.
static bool AcceptAssetDragDrop(AssetType required_type, AssetHandle& out_handle) {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetHandle")) {
      AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
      const AssetMetadata* meta = AssetManager::Get().GetMetadata(dropped);
      if (meta && meta->type == required_type) {
        out_handle = dropped;
        ImGui::EndDragDropTarget();
        return true;
      }
    }
    ImGui::EndDragDropTarget();
  }
  return false;
}

void RenderComponentImGui(ModelComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Model", &visible)) {
    auto& model = entity.GetComponent<ModelComponent>();
    auto& assets = AssetManager::Get();

    // Asset selector: show current asset name + dropdown to pick from registered model assets
    const AssetMetadata* currentMeta = model.model_handle.IsValid()
        ? assets.GetMetadata(model.model_handle)
        : nullptr;
    std::string currentName = currentMeta ? currentMeta->name : "(None)";

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 4);
    if (ImGui::BeginCombo(PrefixLabel("Asset").c_str(), currentName.c_str())) {
      // "None" option
      if (ImGui::Selectable("(None)", !model.model_handle.IsValid())) {
        model.model_handle = kNullAssetHandle;
      }

      // List all registered Model assets
      auto modelAssets = assets.GetAllOfType(AssetType::Model);
      for (auto& handle : modelAssets) {
        const auto* meta = assets.GetMetadata(handle);
        if (!meta) continue;
        bool is_selected = model.model_handle == handle;
        if (ImGui::Selectable(meta->name.c_str(), is_selected)) {
          if (model.model_handle != handle) {
            model.model_handle = handle;
          }
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Drop target: drag a Model asset from the Asset Browser onto the combo
    AssetHandle dropped;
    if (AcceptAssetDragDrop(AssetType::Model, dropped)) {
      if (model.model_handle != dropped) {
        model.model_handle = dropped;
      }
    }

    // Show load state
    if (model.model_handle.IsValid()) {
      AssetLoadState state = assets.GetLoadState(model.model_handle);
      const char* stateStr = "Unknown";
      switch (state) {
        case AssetLoadState::Unloaded: stateStr = "Unloaded"; break;
        case AssetLoadState::Loading:  stateStr = "Loading..."; break;
        case AssetLoadState::Loaded:   stateStr = "Loaded"; break;
        case AssetLoadState::Failed:   stateStr = "Failed"; break;
      }
      ImGui::TextDisabled("Status: %s", stateStr);
    }

    ImGui::Checkbox("Receive Shadows", &model.receive_shadows);
    ImGui::Checkbox("Render", &model.enable_rendering);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<ModelComponent>();
    visible = true;
  }
}

void RenderComponentImGui(LightDirectComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Directional Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.light_data.base.ambient, 0.01f);
    ImGui::DragFloat(PrefixLabel("Diffuse").c_str(),
                     &component.light_data.base.diffuse, 0.1f);
    ImGui::DragFloat(PrefixLabel("Specular").c_str(),
                     &component.light_data.base.specular, 0.1f);
    ImGui::DragFloat(PrefixLabel("Density").c_str(),
                     &component.light_data.base.density, 0.1f);
    ImGui::ColorPicker3(
        PrefixLabel("Color").c_str(),
        reinterpret_cast<float*>(&component.light_data.base.color));
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<LightDirectComponent>();
    visible = true;
  }
}

void RenderComponentImGui(LightPointComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Point Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.light_data.base.ambient, 0.01f);
    ImGui::DragFloat(PrefixLabel("Diffuse").c_str(),
                     &component.light_data.base.diffuse, 0.1f);
    ImGui::DragFloat(PrefixLabel("Specular").c_str(),
                     &component.light_data.base.specular, 0.1f);
    ImGui::DragFloat(PrefixLabel("Density").c_str(),
                     &component.light_data.base.density, 0.1f);
    if (ImGui::TreeNode("Attenuation")) {
      ImGui::DragFloat(PrefixLabel("Constant").c_str(),
                       &component.light_data.constant, 0.1f);
      ImGui::DragFloat(PrefixLabel("Linear").c_str(),
                       &component.light_data.linear, 0.1f);
      ImGui::DragFloat(PrefixLabel("Exp").c_str(),
                       &component.light_data.exp, 0.1f);
      ImGui::TreePop();
    }
    ImGui::ColorPicker3(
        "Color", reinterpret_cast<float*>(&component.light_data.base.color));
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<LightPointComponent>();
    visible = true;
  }
}

void RenderComponentImGui(CameraComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Camera", &visible)) {
    bool changed = false;
    changed |= ImGui::DragFloat(PrefixLabel("FOV").c_str(),
                                &component.field_of_view, 1.0f);
    changed |= ImGui::DragFloat(PrefixLabel("Near Plane").c_str(),
                                &component.near_plane, 0.1f);
    changed |= ImGui::DragFloat(PrefixLabel("Far Plane").c_str(),
                                &component.far_plane, 0.1f);
    int width = component.viewport_size.x;
    changed |= ImGui::InputInt(PrefixLabel("Width").c_str(), &width);
    if (width > 16) {
      component.viewport_size.x = width;
    }
    int height = component.viewport_size.y;
    changed |= ImGui::InputInt(PrefixLabel("Height").c_str(), &height);
    if (height > 9) {
      component.viewport_size.y = height;
    }

    if (changed) {
      component.aspect_ratio = component.viewport_size.x / component.viewport_size.y;
      component.view_changed = true;
      component.resources_dirty = true;
    }

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CameraComponent>();
    visible = true;
  }
}

void RenderScriptVariables(ScriptInstance* instance) {
  if (!instance) {
    return;
  }
  ScriptData& data = instance->script_data();
  for (FieldData& value : data.fields() | std::views::values) {
    if (value.GetFieldType() == FieldType::Float) {
      float_t val = value.Get<float_t>(instance->handle());
      if (ImGui::DragFloat(
              PrefixLabel(value.GetFormattedName().c_str()).c_str(), &val,
              0.1f)) {
        value.Set(instance->handle(), &val);
      }
    } else if (value.GetFieldType() == FieldType::Integer) {
      int32_t val = value.Get<int32_t>(instance->handle());
      if (ImGui::DragInt(PrefixLabel(value.GetFormattedName().c_str()).c_str(),
                         &val, 1)) {
        value.Set(instance->handle(), &val);
      }
    } else if (value.GetFieldType() == FieldType::Boolean) {
      bool val = value.Get<bool>(instance->handle());
      if (ImGui::Checkbox(PrefixLabel(value.GetFormattedName().c_str()).c_str(),
                          &val)) {
        value.Set(instance->handle(), &val);
      }
    } else if (value.GetFieldType() == FieldType::String) {
      MonoObject* val = value.Get<MonoObject*>(instance->handle());
      MonoObjectWrapper wrapper{val};
      std::string str = wrapper.AsString();
      if (ImGui::InputText(
              PrefixLabel(value.GetFormattedName().c_str()).c_str(), &str)) {
        MonoString* newVal =
            mono_string_new(ScriptManager::app_domain(), str.c_str());
        value.Set(instance->handle(), newVal);
      }
    }
    // todo objects, long and unsigned numbers
  }
}

bool RenderBehaviorComponentImGui(BehaviorsComponent& component,
                                  IBehavior& behavior,
                                  Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode(behavior.GetName().c_str(), &visible)) {
    bool enabled = behavior.IsEnabled();
    if (ImGui::Checkbox(PrefixLabel("Enabled").c_str(), &enabled)) {
      behavior.SetEnabled(enabled);
    }
    /*ImGui::InputText("##", behavior.GetFilePtr(),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (!behavior.IsInternalBehavior()) {
      if (ImGui::Button("...")) {
        std::string name = behavior->GetName();
        Dialogs::OpenFileDialog(
            {{"Lua Script", "lua"}}, [&entity, &name](const std::string& file) {
              Scene* engineScene = entity.GetScene();
              entt::entity entityHandle = entity.GetHandle();
              Application::Get()->SubmitToMainThread([engineScene, entityHandle,
                                                      name, file]() {
                Entity entity{entityHandle, engineScene};
                if (entity.HasComponent<BehaviorsComponent>()) {
                  auto& component = entity.GetComponent<BehaviorsComponent>();
                  component.m_Behaviors.erase(name);
                  auto newBehavior = CreateReference<LuaBehavior>(entity, file);
                  component.m_Behaviors[newBehavior->GetName()] = newBehavior;
                }
              });
            });
      }
      ImGui::SameLine();
      if (ImGui::Button("Reload")) {
        std::string name = behavior.GetName();
        std::string file = behavior.GetFile();
        bool wasEnabled = behavior.IsEnabled();
        delete component.m_Behaviors[name];
        auto* newBehavior = new MonoBehavior(entity, file);
        newBehavior->SetEnabled(wasEnabled);
        component.m_Behaviors[name] = newBehavior;

        ImGui::TreePop();
        return true;
      }
    }*/
    if (MonoBehavior* mono = dynamic_cast<MonoBehavior*>(&behavior)) {
      RenderScriptVariables(mono->script_instance());
    }

    ImGui::TreePop();
  }
  if (!visible) {
    component.behaviors_.erase(behavior.GetName());
    delete &behavior;
    visible = true;
    return true;
  }
  return false;
}

void RenderComponentImGui(BehaviorsComponent& component, Entity entity) {
  for (const auto& entry : component.behaviors_) {
    if (RenderBehaviorComponentImGui(component, *entry.second, entity)) {
      break;
    }
  }
}

void RenderAddComponentImGui_ModelComponent(Entity entity) {
  if (ImGui::MenuItem("Model")) {
    entity.AddComponent<ModelComponent>();
  }
}

void RenderAddComponentImGui_LightPointComponent(Entity entity) {
  if (ImGui::MenuItem("Point Light")) {
    entity.AddComponent<LightPointComponent>();
  }
}

void RenderAddComponentImGui_LightDirectComponent(Entity entity) {
  if (ImGui::MenuItem("Directional Light")) {
    entity.AddComponent<LightDirectComponent>();
  }
}

void RenderAddComponentImGui_CameraComponent(Entity entity) {
  if (ImGui::MenuItem("Camera")) {
    auto& component = entity.AddComponent<CameraComponent>();
    Engine::GetRenderer()->SetupCameraComponent(component);
  }
}
static entt::entity addMonoScriptEntityId = entt::null;
static bool shouldOpenMonoScriptPopup = false;

void RenderAddComponentImGui_BehaviorsComponent(Entity entity) {
  if (ImGui::MenuItem("C# Script")) {
    addMonoScriptEntityId = entity.handle();
    shouldOpenMonoScriptPopup = true;
  }
}

void RenderModalComponentImGui_BehaviorsComponent(Entity entity) {
  if (entity.handle() != addMonoScriptEntityId)
    return;

  if (shouldOpenMonoScriptPopup) {
    ImGui::OpenPopup("Add C# Script");
    shouldOpenMonoScriptPopup = false;
  }

  static int currentScriptIndex = 0;
  bool open = true;
  if (ImGui::BeginPopupModal("Add C# Script", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
    const std::vector<std::string> scriptNames = ScriptManager::script_names();
    if (!scriptNames.empty()) {
      ImGui::Combo("Script Name", &currentScriptIndex,
                   [](void* data, int idx, const char** out_text) {
                     const auto& names = *static_cast<const std::vector<std::string>*>(data);
                     if (idx < 0 || idx >= static_cast<int>(names.size())) return false;
                     *out_text = names[idx].c_str();
                     return true;
                   }, (void*)&scriptNames, scriptNames.size());
    } else {
      ImGui::TextDisabled("No scripts found.");
    }

    if (ImGui::Button("Add") && !scriptNames.empty()) {
      if (!entity.HasComponent<BehaviorsComponent>())
        entity.AddComponent<BehaviorsComponent>();

      entity.GetComponent<BehaviorsComponent>().AddBehavior<MonoBehavior>(entity, scriptNames[currentScriptIndex]);
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (!open) {
    addMonoScriptEntityId = entt::null;
  }
}


struct ComponentDesc {
  std::function<void(Entity)> RenderSelf;
  std::function<void(Entity)> RenderAdd;
  std::function<void(Entity)> RenderModal;
  std::function<bool(Entity)> HasComponent;
};

std::unordered_map<std::type_index, ComponentDesc> kRegistry;

template<typename T>
void RegisterComponentType(
    void (*renderSelf)(T&, Entity),
    void (*renderAdd)(Entity),
    void (*renderModal)(Entity)
) {
  auto ti = std::type_index(typeid(T));
  kRegistry[ti] = ComponentDesc{
      [renderSelf](Entity e) {
        if (renderSelf) {
          renderSelf(e.GetComponent<T>(), e);
        }
      },
      [renderAdd](Entity e) {
        if (renderAdd) {
          renderAdd(e);
        }
      },
      [renderModal](Entity e) {
        if (renderModal) {
          renderModal(e);
        }
      },
      [](Entity entity) {
        return entity.HasComponent<T>();
      }
  };
}

void InitializeComponents() {
  RegisterComponentType<IdComponent>(nullptr, nullptr, nullptr);
  RegisterComponentType<TagComponent>(nullptr, nullptr, nullptr);
  RegisterComponentType<TransformComponent>(RenderComponentImGui, nullptr, nullptr);
  RegisterComponentType<ModelComponent>(RenderComponentImGui, RenderAddComponentImGui_ModelComponent, nullptr);
  RegisterComponentType<LightDirectComponent>(RenderComponentImGui, RenderAddComponentImGui_LightDirectComponent, nullptr);
  RegisterComponentType<LightPointComponent>(RenderComponentImGui, RenderAddComponentImGui_LightPointComponent, nullptr);
  RegisterComponentType<CameraComponent>(RenderComponentImGui, RenderAddComponentImGui_CameraComponent, nullptr);
  RegisterComponentType<BehaviorsComponent>(RenderComponentImGui, RenderAddComponentImGui_BehaviorsComponent, RenderModalComponentImGui_BehaviorsComponent);
}

void RenderExistingComponents(Entity entity) {
  for (const auto& item : kRegistry) {
    if (item.second.HasComponent(entity)) {
      item.second.RenderSelf(entity);
    }
  }
}

void RenderModals(Entity entity) {
  for (const auto& item : kRegistry) {
    item.second.RenderModal(entity);
  }
}

void RenderAddPopup(Entity entity) {
  for (const auto& item : kRegistry) {
    item.second.RenderAdd(entity);
  }
}
}  // namespace Wiesel