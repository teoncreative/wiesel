
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

#include "animation/w_animation.hpp"
#include "audio/w_audio.hpp"
#include "asset/w_asset_manager.hpp"
#include "ui/w_canvas.hpp"
#include "behavior/w_behavior.hpp"
#include "behavior/w_native_behavior.hpp"
#include "physics/w_collider.hpp"
#include "physics/w_rigidbody.hpp"
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
#include <backends/imgui_impl_vulkan.h>

#include <ranges>

#include "project/w_project_loader.hpp"

namespace Wiesel {

// Shared drag-drop handler: accepts AssetHandle or BrowserFile payloads,
// auto-imports if needed, returns a valid handle or null.
static AssetHandle AcceptAssetDragDrop(AssetType required_type) {
  AssetHandle result;
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetHandle")) {
    AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
    const AssetMetadata* meta = Engine::asset_manager().GetMetadata(dropped);
    if (meta && meta->type == required_type) {
      result = dropped;
    }
  } else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BrowserFile")) {
    std::string file_path(static_cast<const char*>(payload->Data));
    std::string ext = std::filesystem::path(file_path).extension().string();
    if (ProjectLoader::ExtToAssetType(ext) == required_type) {
      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        auto rel = std::filesystem::relative(file_path, *physical_app);
        std::string vfs_path = "/app/" + rel.generic_string();
        std::string name = std::filesystem::path(file_path).stem().string();

        // Check existing .meta
        std::filesystem::path meta_path = file_path + ".meta";
        std::string handle_str = ProjectLoader::ReadMetaFile(meta_path);
        if (!handle_str.empty()) {
          result = AssetHandle::FromString(handle_str);
        }
        // Auto-import if not registered
        if (!result.IsValid()) {
          result = Engine::asset_manager().Register(name, required_type, vfs_path);
          if (result.IsValid()) {
            ProjectLoader::WriteMetaFile(meta_path, result);
          }
        }
      }
    }
  }
  return result;
}

static void RenderTexturePreview(const char* label, Texture* tex) {
  if (!tex) {
    ImGui::TextDisabled("  %s: No", label);
    return;
  }
  VkDescriptorSet desc = tex->GetImGuiDescriptor();
  if (!desc) {
    ImGui::TextDisabled("  %s: (loading)", label);
    return;
  }
  ImGui::Text("  %s:", label);
  ImGui::SameLine();
  ImVec2 thumb_size(16, 16);
  ImGui::Image(reinterpret_cast<ImTextureID>(desc), thumb_size);
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImVec2 preview_size(256, 256);
    ImGui::Image(reinterpret_cast<ImTextureID>(desc), preview_size);
    ImGui::EndTooltip();
  }
}

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
      const AssetMetadata* meta = Engine::asset_manager().GetMetadata(dropped);
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
    auto& assets = Engine::asset_manager();

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

    // Per-mesh material slots
    if (model.model_handle.IsValid()) {
      const std::shared_ptr<Model>& model_data = assets.GetOrLoad<Model>(model.model_handle);
      if (model_data) {
        // Ensure material instances match mesh count
        if (model.material_instances.size() != model_data->meshes.size()) {
          model.material_instances.resize(model_data->meshes.size());
          model.material_slot_handles.resize(model_data->meshes.size());
          model.material_versions.resize(model_data->meshes.size(), 0);
        }
        for (size_t i = 0; i < model_data->meshes.size(); i++) {
          if (!model.material_instances[i]) {
            auto inst = std::make_shared<MaterialInstance>();
            AssetHandle mat_handle = model.material_slot_handles[i].IsValid()
                ? model.material_slot_handles[i]
                : model_data->meshes[i]->material_handle;
            inst->base_material_handle = mat_handle;
            model.material_instances[i] = inst;
          }
        }

        if (ImGui::TreeNode("Materials")) {
          for (size_t i = 0; i < model.material_instances.size(); i++) {
            auto& inst = model.material_instances[i];
            if (!inst) continue;

            ImGui::PushID(static_cast<int>(i));

            auto base = inst->GetBaseMaterial();
            std::string slot_label = "Slot " + std::to_string(i);
            if (base) {
              if (!base->name.empty()) {
                slot_label = base->name;
              }
              if (base->base_texture) slot_label += " (textured)";
            }

            if (ImGui::TreeNode(slot_label.c_str())) {
              // Show material asset info
              if (base && base->asset_handle.IsValid()) {
                const auto* meta = assets.GetMetadata(base->asset_handle);
                if (meta) {
                  ImGui::TextDisabled("Asset: %s", meta->name.c_str());
                }
              }

              glm::vec4 tint = inst->GetColorTint();
              if (ImGui::ColorEdit4(PrefixLabel("Color Tint").c_str(), &tint.x)) {
                inst->SetColorTint(tint);
              }

              float roughness = inst->GetRoughness();
              if (ImGui::SliderFloat(PrefixLabel("Roughness").c_str(), &roughness, 0.0f, 1.0f)) {
                inst->SetRoughness(roughness);
              }

              float metallic = inst->GetMetallic();
              if (ImGui::SliderFloat(PrefixLabel("Metallic").c_str(), &metallic, 0.0f, 1.0f)) {
                inst->SetMetallic(metallic);
              }

              float specular = inst->GetSpecular();
              if (ImGui::SliderFloat(PrefixLabel("Specular").c_str(), &specular, 0.0f, 1.0f)) {
                inst->SetSpecular(specular);
              }

              // Show base material texture previews
              if (base) {
                if (ImGui::TreeNode("Textures")) {
                  RenderTexturePreview("Diffuse", base->base_texture.get());
                  RenderTexturePreview("Normal", base->normal_map.get());
                  RenderTexturePreview("Roughness", base->roughness_map.get());
                  RenderTexturePreview("Metallic", base->metallic_map.get());
                  RenderTexturePreview("Specular", base->specular_map.get());
                  ImGui::TreePop();
                }
              }

              if (inst->HasOverride("color_tint") || inst->HasOverride("roughness") ||
                  inst->HasOverride("metallic") || inst->HasOverride("specular")) {
                if (ImGui::Button("Reset to Default")) {
                  inst->overrides.clear();
                }
              }

              ImGui::TreePop();
            }
            ImGui::PopID();
          }
          ImGui::TreePop();
        }
      }
    }

    ImGui::TreePop();
  }
  if (!visible) {
    // Defer GPU resource destruction - the GPU may still be reading these
    // from in-flight frames. Move them into the deletion queue first.
    auto& model = entity.GetComponent<ModelComponent>();
    auto deferred_ubos = std::move(model.mesh_uniform_buffers_);
    auto deferred_geo = std::move(model.geometry_descriptors);
    auto deferred_shadow = std::move(model.shadow_descriptors);
    auto deferred_bone_ubo = std::move(model.bone_ubo_);
    auto deferred_bone_desc = std::move(model.bone_descriptor_);
    auto deferred_uniform = std::move(model.uniform_buffer);
    Engine::renderer()->GetDeletionQueue().Push([
        ubos = std::move(deferred_ubos),
        geo = std::move(deferred_geo),
        shadow = std::move(deferred_shadow),
        bone_ubo = std::move(deferred_bone_ubo),
        bone_desc = std::move(deferred_bone_desc),
        uniform = std::move(deferred_uniform)
    ]() {
      // Resources released when lambda is destroyed
    });
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
    ImGui::Text("Viewport: %dx%d",
        static_cast<int>(component.viewport_size.x),
        static_cast<int>(component.viewport_size.y));

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
    if (value.field_type() == FieldType::Float) {
      float_t val = value.Get<float_t>(instance->handle());
      if (ImGui::DragFloat(
              PrefixLabel(value.formatted_name().c_str()).c_str(), &val,
              0.1f)) {
        value.Set(instance->handle(), &val);
      }
    } else if (value.field_type() == FieldType::Integer) {
      int32_t val = value.Get<int32_t>(instance->handle());
      if (ImGui::DragInt(PrefixLabel(value.formatted_name().c_str()).c_str(),
                         &val, 1)) {
        value.Set(instance->handle(), &val);
      }
    } else if (value.field_type() == FieldType::Boolean) {
      bool val = value.Get<bool>(instance->handle());
      if (ImGui::Checkbox(PrefixLabel(value.formatted_name().c_str()).c_str(),
                          &val)) {
        value.Set(instance->handle(), &val);
      }
    } else if (value.field_type() == FieldType::String) {
      MonoObject* val = value.Get<MonoObject*>(instance->handle());
      MonoObjectWrapper wrapper{val};
      std::string str = wrapper.AsString();
      if (ImGui::InputText(
              PrefixLabel(value.formatted_name().c_str()).c_str(), &str)) {
        MonoString* newVal =
            mono_string_new(Engine::script_manager().app_domain(), str.c_str());
        value.Set(instance->handle(), newVal);
      }
      // Accept asset drag-drop onto string fields (sets the VFS path)
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("AssetHandle")) {
          AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
          const AssetMetadata* meta = Engine::asset_manager().GetMetadata(dropped);
          if (meta && !meta->virtual_source_path.empty()) {
            MonoString* newVal = mono_string_new(
                Engine::script_manager().app_domain(),
                meta->virtual_source_path.c_str());
            value.Set(instance->handle(), newVal);
          }
        }
        ImGui::EndDragDropTarget();
      }
    } else if (value.field_type() == FieldType::Entity) {
      MonoObject* entity_obj = value.Get<MonoObject*>(instance->handle());
      std::string entity_label = "(None)";
      entt::entity current_entity_id = entt::null;

      if (entity_obj) {
        MonoClassField* id_field = mono_class_get_field_from_name(
            Engine::script_manager().entity_class(), "entityId");
        if (id_field) {
          uint64_t id_val = 0;
          mono_field_get_value(entity_obj, id_field, &id_val);
          current_entity_id = static_cast<entt::entity>(id_val);
          Scene* scene = instance->behavior()->scene();
          if (scene && scene->HasEntity(current_entity_id) &&
              scene->HasComponent<TagComponent>(current_entity_id)) {
            entity_label = scene->GetComponent<TagComponent>(current_entity_id).tag;
          } else {
            entity_label = "(Invalid)";
          }
        }
      }

      std::string label = PrefixLabel(value.formatted_name().c_str());
      ImGui::InputText(label.c_str(), &entity_label,
                       ImGuiInputTextFlags_ReadOnly);

      // Drag-drop target: accept entities from scene hierarchy
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
          entt::entity dropped_entity =
              *static_cast<const entt::entity*>(payload->Data);
          Scene* scene = instance->behavior()->scene();
          if (scene) {
            MonoObject* new_entity = mono_object_new(
                Engine::script_manager().app_domain(), Engine::script_manager().entity_class());
            MonoMethod* ctor = mono_class_get_method_from_name(
                Engine::script_manager().entity_class(), ".ctor", 2);
            uint64_t scene_ptr = reinterpret_cast<uint64_t>(scene);
            uint64_t entity_id = static_cast<uint64_t>(dropped_entity);
            void* args[2] = {&scene_ptr, &entity_id};
            mono_runtime_invoke(ctor, new_entity, args, nullptr);
            value.Set(instance->handle(), new_entity);
          }
        }
        ImGui::EndDragDropTarget();
      }

      // Clear button
      if (current_entity_id != entt::null) {
        ImGui::SameLine();
        std::string clear_id = "X##clear_" + value.field_name();
        if (ImGui::SmallButton(clear_id.c_str())) {
          MonoObject* null_val = nullptr;
          value.Set(instance->handle(), &null_val);
        }
      }
    } else if (value.field_type() == FieldType::Prefab) {
      MonoObject* prefab_obj = value.Get<MonoObject*>(instance->handle());
      std::string prefab_label = "(None)";

      if (prefab_obj) {
        MonoClassField* path_field = mono_class_get_field_from_name(
            Engine::script_manager().prefab_class(), "path");
        if (path_field) {
          MonoString* path_str = nullptr;
          mono_field_get_value(prefab_obj, path_field, &path_str);
          if (path_str) {
            const char* cstr = mono_string_to_utf8(path_str);
            if (cstr && cstr[0]) {
              // Show just the filename
              std::filesystem::path p(cstr);
              prefab_label = p.stem().string();
            }
            mono_free((void*)cstr);
          }
        }
      }

      std::string label = PrefixLabel(value.formatted_name().c_str());
      ImGui::InputText(label.c_str(), &prefab_label, ImGuiInputTextFlags_ReadOnly);

      // Drag-drop target: accept prefab assets (AssetHandle or file path)
      if (ImGui::BeginDragDropTarget()) {
        std::string dropped_path;

        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("AssetHandle")) {
          AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
          const AssetMetadata* meta = Engine::asset_manager().GetMetadata(dropped);
          if (meta && meta->type == AssetType::Prefab) {
            dropped_path = meta->virtual_source_path;
          }
        } else if (const ImGuiPayload* payload =
                       ImGui::AcceptDragDropPayload("BrowserFile")) {
          std::string file_path(static_cast<const char*>(payload->Data));
          if (file_path.ends_with(".wprefab")) {
            // Convert physical path to VFS path via /app mount
            auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
            if (physical_app.has_value()) {
              std::filesystem::path rel = std::filesystem::relative(
                  file_path, *physical_app);
              dropped_path = "/app/" + rel.generic_string();
            }
          }
        }

        if (!dropped_path.empty()) {
          MonoObject* new_prefab = prefab_obj;
          if (!new_prefab) {
            new_prefab = mono_object_new(
                Engine::script_manager().app_domain(),
                Engine::script_manager().prefab_class());
            mono_runtime_object_init(new_prefab);
          }
          MonoClassField* path_field = mono_class_get_field_from_name(
              Engine::script_manager().prefab_class(), "path");
          if (path_field) {
            MonoString* path_val = mono_string_new(
                Engine::script_manager().app_domain(),
                dropped_path.c_str());
            mono_field_set_value(new_prefab, path_field, path_val);
          }
          value.Set(instance->handle(), new_prefab);
        }
        ImGui::EndDragDropTarget();
      }

      // Clear button
      if (prefab_label != "(None)") {
        ImGui::SameLine();
        std::string clear_id = "X##clear_" + value.field_name();
        if (ImGui::SmallButton(clear_id.c_str())) {
          MonoObject* null_val = nullptr;
          value.Set(instance->handle(), &null_val);
        }
      }
    } else if (value.field_type() == FieldType::AudioClip) {
      // AudioClip object with a "handle" field (asset handle UUID string)
      MonoObject* clip_obj = value.Get<MonoObject*>(instance->handle());
      std::string current_handle_str;
      std::string display = "(None)";

      if (clip_obj) {
        MonoClassField* handle_field = mono_class_get_field_from_name(
            mono_object_get_class(clip_obj), "handle");
        if (handle_field) {
          MonoString* h_str = nullptr;
          mono_field_get_value(clip_obj, handle_field, &h_str);
          if (h_str) {
            const char* cstr = mono_string_to_utf8(h_str);
            if (cstr && cstr[0]) {
              current_handle_str = cstr;
              AssetHandle h = AssetHandle::FromString(current_handle_str);
              const auto* meta = Engine::asset_manager().GetMetadata(h);
              if (meta) display = meta->name;
            }
            mono_free((void*)cstr);
          }
        }
      }

      std::string label = PrefixLabel(value.formatted_name().c_str());
      ImGui::InputText(label.c_str(), &display, ImGuiInputTextFlags_ReadOnly);

      if (ImGui::BeginDragDropTarget()) {
        AssetHandle dropped_handle = AcceptAssetDragDrop(AssetType::Audio);
        if (dropped_handle.IsValid()) {
          // Create or update AudioClip object
          MonoObject* obj = clip_obj;
          if (!obj) {
            obj = mono_object_new(
                Engine::script_manager().app_domain(),
                Engine::script_manager().audio_clip_class());
            mono_runtime_object_init(obj);
          }
          if (obj) {
            MonoClassField* handle_field = mono_class_get_field_from_name(
                mono_object_get_class(obj), "handle");
            if (handle_field) {
              MonoString* h_val = mono_string_new(
                  Engine::script_manager().app_domain(),
                  dropped_handle.ToString().c_str());
              mono_field_set_value(obj, handle_field, h_val);
            }
            value.Set(instance->handle(), obj);
          }
        }
        ImGui::EndDragDropTarget();
      }

      if (!current_handle_str.empty()) {
        ImGui::SameLine();
        std::string clear_id = "X##clear_" + value.field_name();
        if (ImGui::SmallButton(clear_id.c_str())) {
          MonoObject* null_val = nullptr;
          value.Set(instance->handle(), &null_val);
        }
      }
    }
    // todo objects, long and unsigned numbers
  }
}

bool RenderBehaviorComponentImGui(BehaviorsComponent& component,
                                  IBehavior& behavior,
                                  Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode(behavior.GetEditorName().c_str(), &visible)) {
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
                  auto newBehavior = std::make_shared<LuaBehavior>(entity, file);
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

void RenderComponentImGui(BoxColliderComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Box Collider", &visible)) {
    ImGui::DragFloat3(PrefixLabel("Offset").c_str(),
                      reinterpret_cast<float*>(&component.offset), 0.05f);
    ImGui::DragFloat3(PrefixLabel("Half Extents").c_str(),
                      reinterpret_cast<float*>(&component.half_extents), 0.05f,
                      0.0f, 1000.0f);
    ImGui::Checkbox(PrefixLabel("Is Trigger").c_str(), &component.is_trigger);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<BoxColliderComponent>();
    visible = true;
  }
}

void RenderComponentImGui(SphereColliderComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Sphere Collider", &visible)) {
    ImGui::DragFloat3(PrefixLabel("Offset").c_str(),
                      reinterpret_cast<float*>(&component.offset), 0.05f);
    ImGui::DragFloat(PrefixLabel("Radius").c_str(), &component.radius, 0.05f,
                     0.0f, 1000.0f);
    ImGui::Checkbox(PrefixLabel("Is Trigger").c_str(), &component.is_trigger);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<SphereColliderComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_BoxColliderComponent(Entity entity) {
  if (ImGui::MenuItem("Box Collider")) {
    entity.AddComponent<BoxColliderComponent>();
  }
}

void RenderAddComponentImGui_SphereColliderComponent(Entity entity) {
  if (ImGui::MenuItem("Sphere Collider")) {
    entity.AddComponent<SphereColliderComponent>();
  }
}

void RenderComponentImGui(RigidBodyComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Rigid Body", &visible)) {
    const char* types[] = {"Static", "Kinematic", "Dynamic"};
    int type_idx = (int)component.type;
    if (ImGui::Combo(PrefixLabel("Type").c_str(), &type_idx, types, 3)) {
      component.type = (RigidBodyType)type_idx;
      component.is_dirty = true;
    }
    if (component.type == RigidBodyType::Dynamic) {
      if (ImGui::DragFloat(PrefixLabel("Mass").c_str(), &component.mass, 0.1f,
                           0.01f, 10000.0f)) {
        component.is_dirty = true;
      }
    }
    if (ImGui::DragFloat(PrefixLabel("Friction").c_str(), &component.friction,
                         0.01f, 0.0f, 1.0f)) {
      component.is_dirty = true;
    }
    if (ImGui::DragFloat(PrefixLabel("Restitution").c_str(),
                         &component.restitution, 0.01f, 0.0f, 1.0f)) {
      component.is_dirty = true;
    }
    if (ImGui::DragFloat(PrefixLabel("Linear Damping").c_str(),
                         &component.linear_damping, 0.01f, 0.0f, 1.0f)) {
      component.is_dirty = true;
    }
    if (ImGui::DragFloat(PrefixLabel("Angular Damping").c_str(),
                         &component.angular_damping, 0.01f, 0.0f, 1.0f)) {
      component.is_dirty = true;
    }
    ImGui::Checkbox(PrefixLabel("Lock Position X").c_str(),
                    &component.lock_position_x);
    ImGui::Checkbox(PrefixLabel("Lock Position Y").c_str(),
                    &component.lock_position_y);
    ImGui::Checkbox(PrefixLabel("Lock Position Z").c_str(),
                    &component.lock_position_z);
    ImGui::Checkbox(PrefixLabel("Lock Rotation X").c_str(),
                    &component.lock_rotation_x);
    ImGui::Checkbox(PrefixLabel("Lock Rotation Y").c_str(),
                    &component.lock_rotation_y);
    ImGui::Checkbox(PrefixLabel("Lock Rotation Z").c_str(),
                    &component.lock_rotation_z);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<RigidBodyComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_RigidBodyComponent(Entity entity) {
  if (ImGui::MenuItem("Rigid Body")) {
    entity.AddComponent<RigidBodyComponent>();
  }
}

// Canvas / UI components

void RenderComponentImGui(RectangleTransformComponent& component, Entity entity) {
  if (ImGui::ClosableTreeNode("Rectangle Transform", nullptr)) {
    bool changed = false;
    changed |= ImGui::DragFloat2(PrefixLabel("Position").c_str(),
                                  reinterpret_cast<float*>(&component.position), 0.5f);
    changed |= ImGui::DragFloat(PrefixLabel("Rotation").c_str(),
                                 &component.rotation, 0.5f);
    changed |= ImGui::DragFloat2(PrefixLabel("Size").c_str(),
                                  reinterpret_cast<float*>(&component.size), 0.5f);
    changed |= ImGui::DragFloat2(PrefixLabel("Scale").c_str(),
                                  reinterpret_cast<float*>(&component.scale), 0.01f);

    const char* anchors[] = {
        "Top Left", "Top Center", "Top Right",
        "Middle Left", "Middle Center", "Middle Right",
        "Bottom Left", "Bottom Center", "Bottom Right",
        "Stretch All"};
    int anchor_idx = static_cast<int>(component.anchor);
    if (ImGui::Combo(PrefixLabel("Anchor").c_str(), &anchor_idx, anchors, 10)) {
      component.anchor = static_cast<AnchorPreset>(anchor_idx);
      changed = true;
    }

    int pivot_idx = static_cast<int>(component.pivot);
    if (ImGui::Combo(PrefixLabel("Pivot").c_str(), &pivot_idx, anchors, 10)) {
      component.pivot = static_cast<AnchorPreset>(pivot_idx);
      changed = true;
    }

    const char* size_modes[] = {"Fixed", "Percent"};
    int smx = static_cast<int>(component.size_mode_x);
    if (ImGui::Combo(PrefixLabel("Size Mode X").c_str(), &smx, size_modes, 2)) {
      component.size_mode_x = static_cast<SizeMode>(smx);
      changed = true;
    }
    int smy = static_cast<int>(component.size_mode_y);
    if (ImGui::Combo(PrefixLabel("Size Mode Y").c_str(), &smy, size_modes, 2)) {
      component.size_mode_y = static_cast<SizeMode>(smy);
      changed = true;
    }

    changed |= ImGui::DragFloat4(PrefixLabel("Padding").c_str(),
                                  reinterpret_cast<float*>(&component.padding), 0.5f);

    ImGui::TextDisabled("Computed: (%.0f, %.0f) %.0fx%.0f",
                        component.computed_position.x, component.computed_position.y,
                        component.computed_size.x, component.computed_size.y);

    if (changed) {
      component.is_changed = true;
    }
    ImGui::TreePop();
  }
}

void RenderComponentImGui(CanvasComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Canvas", &visible)) {
    const char* directions[] = {"None", "Row", "Column"};
    int dir = static_cast<int>(component.direction);
    if (ImGui::Combo(PrefixLabel("Direction").c_str(), &dir, directions, 3)) {
      component.direction = static_cast<LayoutDirection>(dir);
    }

    const char* alignments[] = {"Start", "Center", "End"};
    int align = static_cast<int>(component.alignment);
    if (ImGui::Combo(PrefixLabel("Alignment").c_str(), &align, alignments, 3)) {
      component.alignment = static_cast<ChildAlignment>(align);
    }

    ImGui::DragFloat(PrefixLabel("Spacing").c_str(), &component.spacing, 0.5f);
    ImGui::InputInt(PrefixLabel("Sort Order").c_str(), &component.sort_order);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CanvasComponent>();
    visible = true;
  }
}

void RenderComponentImGui(CanvasRectComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Canvas Rect", &visible)) {
    if (ImGui::ColorEdit4(PrefixLabel("Color").c_str(),
                          reinterpret_cast<float*>(&component.color))) {
      component.gpu_dirty_ = true;
    }
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CanvasRectComponent>();
    visible = true;
  }
}

void RenderComponentImGui(CanvasImageComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Canvas Image", &visible)) {
    if (ImGui::ColorEdit4(PrefixLabel("Tint").c_str(),
                          reinterpret_cast<float*>(&component.tint))) {
      component.gpu_dirty_ = true;
    }
    ImGui::DragFloat4(PrefixLabel("UV Rect").c_str(),
                      reinterpret_cast<float*>(&component.uv_rect), 0.01f);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CanvasImageComponent>();
    visible = true;
  }
}

void RenderComponentImGui(TextComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Text", &visible)) {
    if (ImGui::InputText(PrefixLabel("Text").c_str(), &component.text)) {
      component.gpu_dirty_ = true;
    }
    if (ImGui::InputText(PrefixLabel("Font").c_str(), &component.font_path)) {
      component.gpu_dirty_ = true;
    }
    if (ImGui::DragFloat(PrefixLabel("Font Size").c_str(), &component.font_size,
                         0.5f, 1.0f, 200.0f)) {
      component.gpu_dirty_ = true;
    }
    if (ImGui::ColorEdit4(PrefixLabel("Color").c_str(),
                          reinterpret_cast<float*>(&component.color))) {
      component.gpu_dirty_ = true;
    }
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<TextComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_CanvasComponent(Entity entity) {
  if (ImGui::MenuItem("Canvas")) {
    entity.AddComponent<CanvasComponent>();
  }
}

void RenderAddComponentImGui_RectangleTransformComponent(Entity entity) {
  if (ImGui::MenuItem("Rectangle Transform")) {
    entity.AddComponent<RectangleTransformComponent>();
  }
}

static void EnsureRectangleTransform(Entity entity) {
  if (!entity.HasComponent<RectangleTransformComponent>()) {
    entity.AddComponent<RectangleTransformComponent>();
  }
}

void RenderAddComponentImGui_CanvasRectComponent(Entity entity) {
  if (ImGui::MenuItem("Canvas Rect")) {
    entity.AddComponent<CanvasRectComponent>();
    EnsureRectangleTransform(entity);
  }
}

void RenderAddComponentImGui_CanvasImageComponent(Entity entity) {
  if (ImGui::MenuItem("Canvas Image")) {
    entity.AddComponent<CanvasImageComponent>();
    EnsureRectangleTransform(entity);
  }
}

void RenderAddComponentImGui_TextComponent(Entity entity) {
  if (ImGui::MenuItem("Text")) {
    entity.AddComponent<TextComponent>();
    EnsureRectangleTransform(entity);
  }
}

void RenderComponentImGui(AnimatorComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Animator", &visible)) {
    // Clip selector - show available clips from the model
    std::shared_ptr<Model> model_ptr;
    if (entity.HasComponent<ModelComponent>()) {
      auto& model_comp = entity.GetComponent<ModelComponent>();
      if (model_comp.model_handle.IsValid()) {
        model_ptr = Engine::asset_manager().GetOrLoad<Model>(model_comp.model_handle);
      }
    }

    ImGui::Checkbox(PrefixLabel("Playing").c_str(), &component.playing);
    ImGui::DragFloat(PrefixLabel("Speed").c_str(), &component.playback_speed,
                     0.01f, -10.0f, 10.0f);

    bool use_controller = component.UseController();

    if (use_controller) {
      // --- Controller mode ---
      ImGui::Separator();
      ImGui::TextDisabled("Controller Mode");

      // Current state
      std::string state_label = component.current_state_name.empty()
          ? "(None)" : component.current_state_name;
      ImGui::Text("State: %s", state_label.c_str());

      if (component.is_blending) {
        ImGui::ProgressBar(component.blend_weight, ImVec2(-1, 0),
                           "Blending...");
      }

      ImGui::DragFloat(PrefixLabel("State Time").c_str(),
                       &component.state_time, 0.1f, 0.0f, 10000.0f);

      // Default state
      if (ImGui::BeginCombo(PrefixLabel("Default State").c_str(),
                            component.controller.default_state.c_str())) {
        for (const auto& state : component.controller.states) {
          bool selected =
              (component.controller.default_state == state.name);
          if (ImGui::Selectable(state.name.c_str(), selected)) {
            component.controller.default_state = state.name;
          }
        }
        ImGui::EndCombo();
      }

      // States
      if (ImGui::TreeNode("States")) {
        for (size_t i = 0; i < component.controller.states.size(); i++) {
          auto& state = component.controller.states[i];
          ImGui::PushID(static_cast<int>(i));
          if (ImGui::TreeNode(state.name.c_str())) {
            ImGui::InputText("Name", &state.name);
            // Clip combo
            if (model_ptr && !model_ptr->animation_clips.empty()) {
              if (ImGui::BeginCombo("Clip", state.clip_name.c_str())) {
                for (const auto& clip : model_ptr->animation_clips) {
                  bool sel = (state.clip_name == clip.name);
                  if (ImGui::Selectable(clip.name.c_str(), sel)) {
                    state.clip_name = clip.name;
                  }
                }
                ImGui::EndCombo();
              }
            } else {
              ImGui::InputText("Clip", &state.clip_name);
            }
            ImGui::DragFloat("Speed", &state.speed, 0.01f, -10.0f, 10.0f);
            ImGui::Checkbox("Looping", &state.looping);
            if (ImGui::Button("Remove")) {
              component.controller.states.erase(
                  component.controller.states.begin() +
                  static_cast<ptrdiff_t>(i));
              ImGui::TreePop();
              ImGui::PopID();
              break;
            }
            ImGui::TreePop();
          }
          ImGui::PopID();
        }
        if (ImGui::Button("+ Add State")) {
          AnimationState new_state;
          new_state.name = "State" +
              std::to_string(component.controller.states.size());
          component.controller.states.push_back(new_state);
        }
        ImGui::TreePop();
      }

      // Transitions
      if (ImGui::TreeNode("Transitions")) {
        for (size_t i = 0; i < component.controller.transitions.size(); i++) {
          auto& trans = component.controller.transitions[i];
          ImGui::PushID(static_cast<int>(i));
          std::string label = (trans.from_state.empty() ? "Any" : trans.from_state)
                              + " -> " + trans.to_state;
          if (ImGui::TreeNode(label.c_str())) {
            // From state combo
            if (ImGui::BeginCombo("From",
                    trans.from_state.empty() ? "(Any)" : trans.from_state.c_str())) {
              if (ImGui::Selectable("(Any)", trans.from_state.empty())) {
                trans.from_state = "";
              }
              for (const auto& state : component.controller.states) {
                bool sel = (trans.from_state == state.name);
                if (ImGui::Selectable(state.name.c_str(), sel)) {
                  trans.from_state = state.name;
                }
              }
              ImGui::EndCombo();
            }
            // To state combo
            if (ImGui::BeginCombo("To", trans.to_state.c_str())) {
              for (const auto& state : component.controller.states) {
                bool sel = (trans.to_state == state.name);
                if (ImGui::Selectable(state.name.c_str(), sel)) {
                  trans.to_state = state.name;
                }
              }
              ImGui::EndCombo();
            }
            ImGui::DragFloat("Blend Duration", &trans.blend_duration,
                             0.01f, 0.0f, 5.0f);

            // Conditions
            for (size_t j = 0; j < trans.conditions.size(); j++) {
              auto& cond = trans.conditions[j];
              ImGui::PushID(static_cast<int>(j));
              ImGui::InputText("Param", &cond.param_name);
              const char* op_names[] = {"==", "!=", ">", "<"};
              int op_idx = static_cast<int>(cond.op);
              if (ImGui::Combo("Op", &op_idx, op_names, 4)) {
                cond.op = static_cast<ConditionOp>(op_idx);
              }
              const char* type_names[] = {"Bool", "Int", "Float", "Trigger"};
              int type_idx = static_cast<int>(cond.param_type);
              if (ImGui::Combo("Type", &type_idx, type_names, 4)) {
                cond.param_type = static_cast<AnimParamType>(type_idx);
              }
              switch (cond.param_type) {
                case AnimParamType::Bool:
                case AnimParamType::Trigger:
                  ImGui::Checkbox("Value", &cond.value.b);
                  break;
                case AnimParamType::Int:
                  ImGui::InputInt("Value", &cond.value.i);
                  break;
                case AnimParamType::Float:
                  ImGui::DragFloat("Value", &cond.value.f, 0.01f);
                  break;
              }
              if (ImGui::Button("Remove Condition")) {
                trans.conditions.erase(
                    trans.conditions.begin() +
                    static_cast<ptrdiff_t>(j));
                ImGui::PopID();
                break;
              }
              ImGui::PopID();
            }
            if (ImGui::Button("+ Condition")) {
              trans.conditions.push_back({});
            }
            ImGui::Separator();
            if (ImGui::Button("Remove Transition")) {
              component.controller.transitions.erase(
                  component.controller.transitions.begin() +
                  static_cast<ptrdiff_t>(i));
              ImGui::TreePop();
              ImGui::PopID();
              break;
            }
            ImGui::TreePop();
          }
          ImGui::PopID();
        }
        if (ImGui::Button("+ Add Transition")) {
          component.controller.transitions.push_back({});
        }
        ImGui::TreePop();
      }

      // Parameters
      if (ImGui::TreeNode("Parameters")) {
        static std::string new_param_name;
        static int new_param_type = 0;

        std::string to_remove;
        for (auto& [name, param] : component.parameters) {
          ImGui::PushID(name.c_str());
          ImGui::Text("%s", name.c_str());
          ImGui::SameLine();
          switch (param.type) {
            case AnimParamType::Bool:
              ImGui::Checkbox("##val", &param.b);
              break;
            case AnimParamType::Int:
              ImGui::InputInt("##val", &param.i);
              break;
            case AnimParamType::Float:
              ImGui::DragFloat("##val", &param.f, 0.01f);
              break;
            case AnimParamType::Trigger:
              if (ImGui::Button(param.b ? "ON" : "Fire")) {
                param.b = true;
              }
              break;
          }
          ImGui::SameLine();
          if (ImGui::Button("X")) {
            to_remove = name;
          }
          ImGui::PopID();
        }
        if (!to_remove.empty()) {
          component.parameters.erase(to_remove);
        }

        ImGui::Separator();
        ImGui::InputText("##newname", &new_param_name);
        ImGui::SameLine();
        const char* ptypes[] = {"Bool", "Int", "Float", "Trigger"};
        ImGui::SetNextItemWidth(80);
        ImGui::Combo("##newtype", &new_param_type, ptypes, 4);
        ImGui::SameLine();
        if (ImGui::Button("+ Add") && !new_param_name.empty() &&
            !component.parameters.contains(new_param_name)) {
          switch (static_cast<AnimParamType>(new_param_type)) {
            case AnimParamType::Bool:
              component.parameters[new_param_name] = AnimParam::MakeBool(false);
              break;
            case AnimParamType::Int:
              component.parameters[new_param_name] = AnimParam::MakeInt(0);
              break;
            case AnimParamType::Float:
              component.parameters[new_param_name] = AnimParam::MakeFloat(0.0f);
              break;
            case AnimParamType::Trigger:
              component.parameters[new_param_name] = AnimParam::MakeTrigger();
              break;
          }
          new_param_name.clear();
        }
        ImGui::TreePop();
      }

    } else {
      // --- Legacy single-clip mode ---
      if (model_ptr && !model_ptr->animation_clips.empty()) {
        const auto& clips = model_ptr->animation_clips;
        std::string current = component.current_clip_name.empty()
            ? "(None)" : component.current_clip_name;
        if (ImGui::BeginCombo(PrefixLabel("Clip").c_str(), current.c_str())) {
          for (const auto& clip : clips) {
            bool selected = (component.current_clip_name == clip.name);
            if (ImGui::Selectable(clip.name.c_str(), selected)) {
              component.current_clip_name = clip.name;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
      } else {
        ImGui::InputText(PrefixLabel("Clip").c_str(),
                         &component.current_clip_name);
        if (!model_ptr) {
          ImGui::TextDisabled("No model loaded");
        } else {
          ImGui::TextDisabled("No animation clips found");
        }
      }

      ImGui::Checkbox(PrefixLabel("Looping").c_str(), &component.looping);
      ImGui::DragFloat(PrefixLabel("Time").c_str(), &component.playback_time,
                       0.1f, 0.0f, 10000.0f);
    }

    ImGui::TextDisabled("Bones: %zu  Nodes: %zu",
                        component.bone_matrices.size(),
                        component.node_transforms.size());
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<AnimatorComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_AnimatorComponent(Entity entity) {
  if (ImGui::MenuItem("Animator")) {
    entity.AddComponent<AnimatorComponent>();
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
    Engine::renderer()->SetupCameraComponent(component);
  }
}
static entt::entity addMonoScriptEntityId = entt::null;
static bool shouldOpenMonoScriptPopup = false;

void RenderAddComponentImGui_BehaviorsComponent(Entity entity) {
  if (ImGui::MenuItem("Script")) {
    addMonoScriptEntityId = entity.handle();
    shouldOpenMonoScriptPopup = true;
  }
}

void RenderModalComponentImGui_BehaviorsComponent(Entity entity) {
  if (entity.handle() != addMonoScriptEntityId)
    return;

  if (shouldOpenMonoScriptPopup) {
    ImGui::OpenPopup("Add Script");
    shouldOpenMonoScriptPopup = false;
  }

  static int currentScriptIndex = 0;
  bool open = true;
  if (ImGui::BeginPopupModal("Add Script", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
    // Build combined list: native behaviors + C# scripts
    std::vector<std::string> all_scripts;
    std::vector<bool> is_native;

    const auto& native_names = Engine::behavior_registry().GetNames();
    for (const auto& name : native_names) {
      all_scripts.push_back(name + " (C++)");
      is_native.push_back(true);
    }
    const std::vector<std::string>& cs_names = Engine::script_manager().script_names();
    for (const auto& name : cs_names) {
      all_scripts.push_back(name + " (C#)");
      is_native.push_back(false);
    }

    if (!all_scripts.empty()) {
      if (currentScriptIndex >= static_cast<int>(all_scripts.size()))
        currentScriptIndex = 0;
      ImGui::Combo("Script", &currentScriptIndex,
                   [](void* data, int idx) -> const char* {
                     const auto& names = *static_cast<const std::vector<std::string>*>(data);
                     if (idx < 0 || idx >= static_cast<int>(names.size())) return nullptr;
                     return names[idx].c_str();
                   }, (void*)&all_scripts, static_cast<int>(all_scripts.size()));
    } else {
      ImGui::TextDisabled("No scripts found.");
    }

    if (ImGui::Button("Add") && !all_scripts.empty()) {
      if (!entity.HasComponent<BehaviorsComponent>())
        entity.AddComponent<BehaviorsComponent>();

      auto& bc = entity.GetComponent<BehaviorsComponent>();
      if (is_native[currentScriptIndex]) {
        const std::string& name = native_names[currentScriptIndex];
        NativeBehavior* native = Engine::behavior_registry().Create(name, entity);
        if (native) {
          bc.behaviors_.insert(std::pair(name, native));
        }
      } else {
        int cs_idx = currentScriptIndex - static_cast<int>(native_names.size());
        bc.AddBehavior<MonoBehavior>(entity, cs_names[cs_idx]);
      }
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
  std::string display_name;
  std::string group;
  std::function<void(Entity)> RenderSelf;
  std::function<void(Entity)> RenderAdd;
  std::function<void(Entity)> RenderModal;
  std::function<bool(Entity)> HasComponent;
};

std::unordered_map<std::type_index, ComponentDesc> kRegistry;

template<typename T>
void RegisterComponentType(
    const std::string& display_name,
    const std::string& group,
    void (*renderSelf)(T&, Entity),
    void (*renderAdd)(Entity),
    void (*renderModal)(Entity)
) {
  auto ti = std::type_index(typeid(T));
  kRegistry[ti] = ComponentDesc{
      display_name,
      group,
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

void RenderComponentImGui(AudioSourceComponent& component, Entity entity) {
  static bool visible = true;
  if (!ImGui::ClosableTreeNode("Audio Source", &visible)) {
    if (!visible) {
      entity.RemoveComponent<AudioSourceComponent>();
      visible = true;
    }
    return;
  }

  // Default clip (asset handle, drag-drop)
  std::string display = "(None)";
  if (component.clip.IsValid()) {
    const auto* meta = Engine::asset_manager().GetMetadata(component.clip);
    if (meta) display = meta->name;
  }
  ImGui::InputText(PrefixLabel("Clip").c_str(), &display, ImGuiInputTextFlags_ReadOnly);

  if (ImGui::BeginDragDropTarget()) {
    AssetHandle dropped = AcceptAssetDragDrop(AssetType::Audio);
    if (dropped.IsValid()) component.clip = dropped;
    ImGui::EndDragDropTarget();
  }

  if (component.clip.IsValid()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("X##clearclip")) {
      if (component.playing_handle_.IsValid()) {
        Engine::audio().Stop(component.playing_handle_);
        component.playing_handle_ = {};
      }
      component.clip = {};
    }
  }

  // Bus
  const char* bus_names[] = {"Master", "SFX", "Music"};
  int bus_idx = static_cast<int>(component.bus);
  if (ImGui::Combo(PrefixLabel("Bus").c_str(), &bus_idx, bus_names, 3)) {
    component.bus = static_cast<AudioBus>(bus_idx);
  }

  ImGui::SliderFloat(PrefixLabel("Volume").c_str(), &component.volume, 0.0f, 1.0f);
  ImGui::SliderFloat(PrefixLabel("Pitch").c_str(), &component.pitch, 0.1f, 3.0f);
  ImGui::SliderFloat(PrefixLabel("Spatial Blend").c_str(), &component.spatial_blend, 0.0f, 1.0f,
                      component.spatial_blend < 0.01f ? "2D" : (component.spatial_blend > 0.99f ? "3D" : "%.2f"));
  ImGui::Checkbox(PrefixLabel("Loop").c_str(), &component.loop);
  ImGui::Checkbox(PrefixLabel("Play On Start").c_str(), &component.play_on_start);
  ImGui::Checkbox(PrefixLabel("Mute").c_str(), &component.mute);

  if (component.spatial_blend > 0.0f) {
    ImGui::DragFloat(PrefixLabel("Min Distance").c_str(), &component.min_distance, 0.1f, 0.0f, 1000.0f);
    ImGui::DragFloat(PrefixLabel("Max Distance").c_str(), &component.max_distance, 1.0f, 0.0f, 10000.0f);
  }

  // Preview buttons
  if (component.clip.IsValid()) {
    if (component.playing_handle_.IsValid()) {
      if (ImGui::Button("Stop")) {
        Engine::audio().Stop(component.playing_handle_);
        component.playing_handle_ = {};
      }
    } else {
      if (ImGui::Button("Preview")) {
        SoundParams params;
        params.bus = component.bus;
        params.volume = component.volume;
        params.pitch = component.pitch;
        params.loop = component.loop;
        params.spatial_blend = 0.0f;  // preview always 2D
        component.playing_handle_ = Engine::audio().Play(component.clip, params);
      }
    }
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_AudioSourceComponent(Entity entity) {
  if (ImGui::MenuItem("Audio Source") && !entity.HasComponent<AudioSourceComponent>()) {
    entity.AddComponent<AudioSourceComponent>();
  }
}

void RenderComponentImGui(ReverbZoneComponent& component, Entity entity) {
  static bool visible = true;
  if (!ImGui::ClosableTreeNode("Reverb Zone", &visible)) {
    if (!visible) {
      entity.RemoveComponent<ReverbZoneComponent>();
      visible = true;
    }
    return;
  }

  ImGui::DragFloat(PrefixLabel("Radius").c_str(), &component.radius, 0.5f, 0.1f, 1000.0f);
  ImGui::DragFloat(PrefixLabel("Delay (ms)").c_str(), &component.delay_ms, 5.0f, 10.0f, 2000.0f);
  ImGui::SliderFloat(PrefixLabel("Decay").c_str(), &component.decay, 0.0f, 1.0f);
  ImGui::SliderFloat(PrefixLabel("Wet").c_str(), &component.wet, 0.0f, 1.0f);

  if (component.active_) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Active");
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_ReverbZoneComponent(Entity entity) {
  if (ImGui::MenuItem("Reverb Zone") && !entity.HasComponent<ReverbZoneComponent>()) {
    entity.AddComponent<ReverbZoneComponent>();
  }
}

void InitializeComponents() {
  RegisterComponentType<IdComponent>("", "", nullptr, nullptr, nullptr);
  RegisterComponentType<TagComponent>("", "", nullptr, nullptr, nullptr);
  RegisterComponentType<TransformComponent>("Transform", "", RenderComponentImGui, nullptr, nullptr);
  RegisterComponentType<ModelComponent>("Model", "Rendering", RenderComponentImGui, RenderAddComponentImGui_ModelComponent, nullptr);
  RegisterComponentType<AnimatorComponent>("Animator", "Rendering", RenderComponentImGui, RenderAddComponentImGui_AnimatorComponent, nullptr);
  RegisterComponentType<CameraComponent>("Camera", "Rendering", RenderComponentImGui, RenderAddComponentImGui_CameraComponent, nullptr);
  RegisterComponentType<LightDirectComponent>("Directional Light", "Lighting", RenderComponentImGui, RenderAddComponentImGui_LightDirectComponent, nullptr);
  RegisterComponentType<LightPointComponent>("Point Light", "Lighting", RenderComponentImGui, RenderAddComponentImGui_LightPointComponent, nullptr);
  RegisterComponentType<BehaviorsComponent>("C# Script", "Scripting", RenderComponentImGui, RenderAddComponentImGui_BehaviorsComponent, RenderModalComponentImGui_BehaviorsComponent);
  RegisterComponentType<BoxColliderComponent>("Box Collider", "Physics", RenderComponentImGui, RenderAddComponentImGui_BoxColliderComponent, nullptr);
  RegisterComponentType<SphereColliderComponent>("Sphere Collider", "Physics", RenderComponentImGui, RenderAddComponentImGui_SphereColliderComponent, nullptr);
  RegisterComponentType<RigidBodyComponent>("Rigid Body", "Physics", RenderComponentImGui, RenderAddComponentImGui_RigidBodyComponent,  nullptr);
  RegisterComponentType<RectangleTransformComponent>("Rectangle Transform", "Canvas", RenderComponentImGui, nullptr, nullptr);
  RegisterComponentType<CanvasComponent>("Canvas", "Canvas", RenderComponentImGui, RenderAddComponentImGui_CanvasComponent, nullptr);
  RegisterComponentType<CanvasRectComponent>("Canvas Rect", "Canvas", RenderComponentImGui, RenderAddComponentImGui_CanvasRectComponent, nullptr);
  RegisterComponentType<CanvasImageComponent>("Canvas Image", "Canvas", RenderComponentImGui, RenderAddComponentImGui_CanvasImageComponent, nullptr);
  RegisterComponentType<TextComponent>("Text", "Canvas", RenderComponentImGui, RenderAddComponentImGui_TextComponent, nullptr);
  RegisterComponentType<AudioSourceComponent>("Audio Source", "Audio", RenderComponentImGui, RenderAddComponentImGui_AudioSourceComponent, nullptr);
  RegisterComponentType<ReverbZoneComponent>("Reverb Zone", "Audio", RenderComponentImGui, RenderAddComponentImGui_ReverbZoneComponent, nullptr);
}

void RenderExistingComponents(Entity entity) {
  bool has_rect_transform = entity.HasComponent<RectangleTransformComponent>();
  auto transform_ti = std::type_index(typeid(TransformComponent));
  for (const auto& [ti, desc] : kRegistry) {
    // Hide TransformComponent when RectangleTransformComponent is present
    if (has_rect_transform && ti == transform_ti) continue;
    if (desc.HasComponent(entity)) {
      desc.RenderSelf(entity);
    }
  }
}

void RenderModals(Entity entity) {
  for (const auto& item : kRegistry) {
    item.second.RenderModal(entity);
  }
}

void RenderAddPopup(Entity entity) {
  static char search_buf[128] = "";

  ImGui::SetNextItemWidth(-1);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
    search_buf[0] = '\0';
  }
  ImGui::InputTextWithHint("##component_search", "Search...", search_buf, sizeof(search_buf));

  std::string filter(search_buf);
  // Lowercase for case-insensitive matching
  std::string filter_lower = filter;
  for (auto& c : filter_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  bool has_filter = !filter_lower.empty();

  // Collect groups in consistent order
  static const std::vector<std::string> group_order = {
      "Rendering", "Lighting", "Audio", "Physics", "Canvas", "Scripting"
  };

  if (has_filter) {
    // Flat filtered list (no groups)
    for (const auto& [ti, desc] : kRegistry) {
      if (!desc.RenderAdd || desc.display_name.empty()) continue;
      std::string name_lower = desc.display_name;
      for (auto& c : name_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      std::string group_lower = desc.group;
      for (auto& c : group_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (name_lower.find(filter_lower) != std::string::npos ||
          group_lower.find(filter_lower) != std::string::npos) {
        desc.RenderAdd(entity);
      }
    }
  } else {
    // Grouped view
    for (const auto& group : group_order) {
      bool has_items = false;
      for (const auto& [ti, desc] : kRegistry) {
        if (desc.group == group && desc.RenderAdd) {
          has_items = true;
          break;
        }
      }
      if (!has_items) continue;

      if (ImGui::BeginMenu(group.c_str())) {
        for (const auto& [ti, desc] : kRegistry) {
          if (desc.group == group && desc.RenderAdd) {
            desc.RenderAdd(entity);
          }
        }
        ImGui::EndMenu();
      }
    }
  }
}
}  // namespace Wiesel