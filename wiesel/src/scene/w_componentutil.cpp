
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

namespace Wiesel {
void InitializeComponents() {

}

template <>
void RenderComponentImGui(TransformComponent& component, Entity entity) {
  if (ImGui::ClosableTreeNode("Transform", nullptr)) {
    bool changed = false;
    changed |=
        ImGui::DragFloat3(PrefixLabel("Position").c_str(),
                          reinterpret_cast<float*>(&component.Position), 0.1f);
    changed |=
        ImGui::DragFloat3(PrefixLabel("Rotation").c_str(),
                          reinterpret_cast<float*>(&component.Rotation), 0.1f);
    changed |=
        ImGui::DragFloat3(PrefixLabel("Scale").c_str(),
                          reinterpret_cast<float*>(&component.Scale), 0.1f);
    if (changed) {
      component.IsChanged = true;
    }
    ImGui::TreePop();
  }
}

template <>
void RenderComponentImGui(ModelComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Model", &visible)) {
    auto& model = entity.GetComponent<ModelComponent>();
    ImGui::InputText("##", &model.Data.ModelPath, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("...")) {
      Dialogs::OpenFileDialog(
          {{"Model file", "obj,gltf"}}, [&entity](const std::string& file) {
            auto& model = entity.GetComponent<ModelComponent>();
            aiScene* aiScene = Engine::LoadAssimpModel(model, file);
            if (aiScene == nullptr) {
              return;  // todo alert
            }
            Scene* engineScene = entity.GetScene();
            entt::entity entityHandle = entity.GetHandle();
            Application::Get()->SubmitToMainThread(
                [aiScene, entityHandle, engineScene, file]() {
                  Entity entity{entityHandle, engineScene};
                  if (entity.HasComponent<ModelComponent>()) {
                    auto& transform = entity.GetComponent<TransformComponent>();
                    auto& model = entity.GetComponent<ModelComponent>();
                    Engine::LoadModel(aiScene, transform, model, file);
                  }
                });
          });
    }
    ImGui::Checkbox("Receive Shadows", &model.Data.ReceiveShadows);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<ModelComponent>();
    visible = true;
  }
}

template <>
void RenderComponentImGui(LightDirectComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Directional Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.LightData.Base.Ambient, 0.01f);
    ImGui::DragFloat(PrefixLabel("Diffuse").c_str(),
                     &component.LightData.Base.Diffuse, 0.1f);
    ImGui::DragFloat(PrefixLabel("Specular").c_str(),
                     &component.LightData.Base.Specular, 0.1f);
    ImGui::DragFloat(PrefixLabel("Density").c_str(),
                     &component.LightData.Base.Density, 0.1f);
    ImGui::ColorPicker3(
        PrefixLabel("Color").c_str(),
        reinterpret_cast<float*>(&component.LightData.Base.Color));
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<LightDirectComponent>();
    visible = true;
  }
}

template <>
void RenderComponentImGui(LightPointComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Point Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.LightData.Base.Ambient, 0.01f);
    ImGui::DragFloat(PrefixLabel("Diffuse").c_str(),
                     &component.LightData.Base.Diffuse, 0.1f);
    ImGui::DragFloat(PrefixLabel("Specular").c_str(),
                     &component.LightData.Base.Specular, 0.1f);
    ImGui::DragFloat(PrefixLabel("Density").c_str(),
                     &component.LightData.Base.Density, 0.1f);
    if (ImGui::TreeNode("Attenuation")) {
      ImGui::DragFloat(PrefixLabel("Constant").c_str(),
                       &component.LightData.Constant, 0.1f);
      ImGui::DragFloat(PrefixLabel("Linear").c_str(),
                       &component.LightData.Linear, 0.1f);
      ImGui::DragFloat(PrefixLabel("Exp").c_str(),
                       &component.LightData.Exp, 0.1f);
      ImGui::TreePop();
    }
    ImGui::ColorPicker3(
        "Color", reinterpret_cast<float*>(&component.LightData.Base.Color));
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<LightPointComponent>();
    visible = true;
  }
}

template <>
void RenderComponentImGui(CameraComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Camera", &visible)) {
    bool changed = false;
    changed |= ImGui::DragFloat(PrefixLabel("FOV").c_str(),
                                &component.m_Camera.m_FieldOfView, 1.0f);
    changed |= ImGui::DragFloat(PrefixLabel("Near Plane").c_str(),
                                &component.m_Camera.m_NearPlane, 0.1f);
    changed |= ImGui::DragFloat(PrefixLabel("Far Plane").c_str(),
                                &component.m_Camera.m_FarPlane, 0.1f);
    if (changed) {
      component.m_Camera.m_IsChanged = true;
    }

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CameraComponent>();
    visible = true;
  }
}

template <>
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
    if (auto mono = dynamic_cast<MonoBehavior*>(&behavior)) {
      auto* instance = mono->GetScriptInstance();
      auto* data = instance->GetScriptData();
      for (auto& [key, value] : data->GetFields()) {
        if (value.GetFieldType() == FieldType::Float) {
          float_t val = value.Get<float_t>(instance->GetInstance());
          if (ImGui::DragFloat(PrefixLabel(value.GetFormattedName().c_str()).c_str(), &val, 0.1f)) {
            value.Set(instance->GetInstance(), &val);
          }
        } else if (value.GetFieldType() == FieldType::Integer) {
          int32_t val = value.Get<int32_t>(instance->GetInstance());
          if (ImGui::DragInt(PrefixLabel(value.GetFormattedName().c_str()).c_str(), &val, 1)) {
            value.Set(instance->GetInstance(), &val);
          }
        } else if (value.GetFieldType() == FieldType::Boolean) {
          bool val = value.Get<bool>(instance->GetInstance());
          if (ImGui::Checkbox(PrefixLabel(value.GetFormattedName().c_str()).c_str(), &val)) {
            value.Set(instance->GetInstance(), &val);
          }
        } else if (value.GetFieldType() == FieldType::String) {
          MonoObject* val = value.Get<MonoObject*>(instance->GetInstance());
          MonoObjectWrapper wrapper{val};
          std::string str = wrapper.AsString();
          if (ImGui::InputText(PrefixLabel(value.GetFormattedName().c_str()).c_str(), &str)) {
            MonoString* newVal = mono_string_new(ScriptManager::GetAppDomain(), str.c_str());
            value.Set(instance->GetInstance(), newVal);
          }
        }
        // todo objects, long and unsigned numbers
      }
    }

    ImGui::TreePop();
  }
  if (!visible) {
    component.m_Behaviors.erase(behavior.GetName());
    delete &behavior;
    visible = true;
    return true;
  }
  return false;
}

template <>
void RenderComponentImGui(BehaviorsComponent& component, Entity entity) {
  for (const auto& entry : component.m_Behaviors) {
    if (RenderBehaviorComponentImGui(component, *entry.second, entity)) {
      break;
    }
  }
}

template <>
void RenderAddComponentImGui<ModelComponent>(Entity entity) {
  if (ImGui::MenuItem("Model")) {
    entity.AddComponent<ModelComponent>();
  }
}

template <>
void RenderAddComponentImGui<LightPointComponent>(Entity entity) {
  if (ImGui::MenuItem("Point Light")) {
    entity.AddComponent<LightPointComponent>();
  }
}

template <>
void RenderAddComponentImGui<LightDirectComponent>(Entity entity) {
  if (ImGui::MenuItem("Directional Light")) {
    entity.AddComponent<LightDirectComponent>();
  }
}

template <>
void RenderAddComponentImGui<CameraComponent>(Entity entity) {
  if (ImGui::MenuItem("Camera")) {
    auto& component = entity.AddComponent<CameraComponent>();
    component.m_Camera.m_AspectRatio = Engine::GetRenderer()->GetAspectRatio();
  }
}

template <>
void RenderAddComponentImGui<BehaviorsComponent>(Entity entity) {
  if (ImGui::MenuItem("C# Script")) {
    if (!entity.HasComponent<BehaviorsComponent>()) {
      entity.AddComponent<BehaviorsComponent>();
    }
    BehaviorsComponent& component = entity.GetComponent<BehaviorsComponent>();
    //component.AddBehavior<CSharpBehavior>(entity, "");
  }
}

template <>
void CallRenderAddComponentImGui<BehaviorsComponent>(Entity entity) {
  RenderAddComponentImGui<BehaviorsComponent>(entity);
}
}  // namespace Wiesel