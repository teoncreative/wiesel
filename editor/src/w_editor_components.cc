
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_components.h"

#include "util/imgui/imgui_lucide.h"

#include <backends/imgui_impl_vulkan.h>
#include "script/w_script_field_registry.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <typeindex>
#include "animation/w_animation.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_registry.h"
#include "audio/w_audio.h"
#include "behavior/w_behavior.h"
#include "behavior/w_native_behavior.h"
#include "mono_wrappers.h"
#include "networking/w_replication_types.h"
#include "physics/w_collider.h"
#include "physics/w_mesh_collider_asset.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_billboard_renderer.h"
#include "rendering/w_billboard_text.h"
#include "rendering/w_sprite.h"
#include "scene/w_lights.h"
#include "script/mono/w_monobehavior.h"
#include "ui/w_canvas.h"
#include "ui/w_interactable.h"
#include "ui/w_navigable.h"
#include "ui/w_ui_document.h"
#include "util/imgui/w_imguiutil.h"
#include "util/w_dialogs.h"
#include "util/w_logger.h"
#include "w_application.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"
#include "w_undo_helpers.h"

#include <ranges>

#include "asset/w_asset_utils.h"

namespace wiesel {

// Command stack pointer set by the editor each frame for undo/redo tracking.
static editor::CommandStack* s_command_stack = nullptr;

void SetInspectorCommandStack(editor::CommandStack* stack) {
  s_command_stack = stack;
}

// Shared drag-drop handler: accepts AssetHandle or BrowserFile payloads,
// auto-imports if needed, returns a valid handle or null.
AssetHandle AcceptAssetDragDrop(AssetType required_type) {
  AssetHandle result;
  if (const ImGuiPayload* payload =
          ImGui::AcceptDragDropPayload("AssetHandle")) {
    AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
    const AssetMetadata* meta = Engine::asset_manager().GetMetadata(dropped);
    if (meta && meta->type == required_type) {
      result = dropped;
    }
  } else if (const ImGuiPayload* payload =
                 ImGui::AcceptDragDropPayload("BrowserFile")) {
    std::string file_path(static_cast<const char*>(payload->Data));
    std::string ext = std::filesystem::path(file_path).extension().string();
    if (ExtToAssetType(ext) == required_type) {
      auto physical_app = Engine::vfs()->GetPhysicalPath("app://");
      if (physical_app.has_value()) {
        auto rel = std::filesystem::relative(file_path, *physical_app);
        std::string vfs_path = "app://" + rel.generic_string();
        // Look up by VFS path - asset should already be imported by editor
        result = Engine::asset_manager().FindBySourcePath(vfs_path);
      }
    }
  }
  return result;
}

// Accepts an AssetHandle drag-drop payload, filtered by type.
// Returns true and writes the handle if accepted; false otherwise.
static bool AcceptAssetDragDrop(AssetType required_type,
                                AssetHandle& out_handle) {
  if (ImGui::BeginDragDropTarget()) {
    AssetHandle result = AcceptAssetDragDrop(required_type);
    ImGui::EndDragDropTarget();
    if (result.IsValid()) {
      out_handle = result;
      return true;
    }
  }
  return false;
}

// Renders an asset drag-drop field for a given type. Returns true if changed.
static bool AssetDropField(const char* label, AssetType type,
                           AssetHandle& handle) {
  std::string name = "(None)";
  const AssetMetadata* meta = nullptr;
  if (handle.IsValid()) {
    meta = Engine::asset_manager().GetMetadata(handle);
    if (meta) {
      name = meta->name;
    }
  }

  // Layout: label column (same width as PrefixLabel), then button fills rest
  float width = ImGui::CalcItemWidth();
  float label_end = ImGui::GetCursorPosX() + width * 0.5f +
                    ImGui::GetStyle().ItemInnerSpacing.x;

  // Show inline thumbnail next to the label
  ThumbnailCache* tc = ThumbnailCache::Get();
  if (tc && handle.IsValid() && meta) {
    ThumbnailEntry thumb = tc->GetOrCreate(handle, *meta);
    if (thumb.texture_id) {
      float h = ImGui::GetTextLineHeight();
      ImGui::Image(reinterpret_cast<ImTextureID>(thumb.texture_id),
                   ImVec2(h, h), thumb.uv0, thumb.uv1);
      ImGui::SameLine();
    }
  }

  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::SameLine();
  ImGui::SetCursorPosX(label_end);
  std::string btn_id = name + "##assetdrop_" + label;
  ImGui::Button(btn_id.c_str(), ImVec2(-1, 0));

  // Hover tooltip with larger preview
  if (tc && handle.IsValid() && meta && ImGui::IsItemHovered()) {
    ThumbnailEntry thumb = tc->GetOrCreate(handle, *meta);
    if (thumb.texture_id) {
      ImGui::BeginTooltip();
      ImGui::Image(reinterpret_cast<ImTextureID>(thumb.texture_id),
                   thumb.FitSize(128), thumb.uv0, thumb.uv1);
      ImGui::TextDisabled("%u x %u", thumb.VisibleWidth(),
                          thumb.VisibleHeight());
      ImGui::EndTooltip();
    }
  }

  AssetHandle dropped;
  if (AcceptAssetDragDrop(type, dropped)) {
    handle = dropped;
    return true;
  }

  // Right-click to clear
  if (handle.IsValid() && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    handle = {};
    return true;
  }

  return false;
}

static bool TextureDropField(const char* label, AssetHandle& handle) {
  return AssetDropField(label, AssetType::Texture, handle);
}

void RenderComponentImGui(TransformComponent& component, Entity entity) {
  if (ImGui::ClosableTreeNode("Transform", nullptr)) {
    bool changed = false;
    changed |= ImGui::DragFloat3(
        PrefixLabel("Position").c_str(),
        reinterpret_cast<float*>(&component.PositionMut()), 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<glm::vec3> pos_tracker;
      pos_tracker.Track(*s_command_stack, "Change Position",
                        component.GetPosition(),
                        [entity](const glm::vec3& v) mutable {
                          auto& tc = entity.GetComponent<TransformComponent>();
                          tc.SetPosition(v);
                        });
    }

    changed |= ImGui::DragFloat3(
        PrefixLabel("Rotation").c_str(),
        reinterpret_cast<float*>(&component.RotationMut()), 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<glm::vec3> rot_tracker;
      rot_tracker.Track(*s_command_stack, "Change Rotation",
                        component.GetRotation(),
                        [entity](const glm::vec3& v) mutable {
                          auto& tc = entity.GetComponent<TransformComponent>();
                          tc.SetRotation(v);
                        });
    }

    changed |= ImGui::DragFloat3(
        PrefixLabel("Scale").c_str(),
        reinterpret_cast<float*>(&component.ScaleMut()), 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<glm::vec3> scale_tracker;
      scale_tracker.Track(
          *s_command_stack, "Change Scale", component.GetScale(),
          [entity](const glm::vec3& v) mutable {
            auto& tc = entity.GetComponent<TransformComponent>();
            tc.SetScale(v);
          });
    }

    if (changed) {
      component.MarkChanged();
    }
    ImGui::TreePop();
  }
}

void RenderComponentImGui(LightDirectComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Directional Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.light_data.base.ambient, 0.01f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Light Ambient",
                    component.light_data.base.ambient,
                    [entity](const float& v) mutable {
                      entity.GetComponent<LightDirectComponent>()
                          .light_data.base.ambient = v;
                    });
    }

    ImGui::DragFloat(PrefixLabel("Diffuse").c_str(),
                     &component.light_data.base.diffuse, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Light Diffuse",
                    component.light_data.base.diffuse,
                    [entity](const float& v) mutable {
                      entity.GetComponent<LightDirectComponent>()
                          .light_data.base.diffuse = v;
                    });
    }

    ImGui::DragFloat(PrefixLabel("Specular").c_str(),
                     &component.light_data.base.specular, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Light Specular",
                    component.light_data.base.specular,
                    [entity](const float& v) mutable {
                      entity.GetComponent<LightDirectComponent>()
                          .light_data.base.specular = v;
                    });
    }

    ImGui::DragFloat(PrefixLabel("Density").c_str(),
                     &component.light_data.base.density, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Light Density",
                    component.light_data.base.density,
                    [entity](const float& v) mutable {
                      entity.GetComponent<LightDirectComponent>()
                          .light_data.base.density = v;
                    });
    }

    ImGui::ColorPicker3(
        PrefixLabel("Color").c_str(),
        reinterpret_cast<float*>(&component.light_data.base.color));
    if (s_command_stack) {
      static editor::UndoTracker<glm::vec3> tracker;
      tracker.Track(
          *s_command_stack, "Change Light Color",
          component.light_data.base.color,
          [entity](const glm::vec3& v) mutable {
            entity.GetComponent<LightDirectComponent>().light_data.base.color =
                v;
          });
    }

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<LightDirectComponent>();
    visible = true;
  }
}

void RenderComponentImGui(LightPointComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Point Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.light_data.base.ambient, 0.01f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(
          *s_command_stack, "Change Light Ambient",
          component.light_data.base.ambient, [entity](const float& v) mutable {
            entity.GetComponent<LightPointComponent>().light_data.base.ambient =
                v;
          });
    }

    ImGui::DragFloat(PrefixLabel("Diffuse").c_str(),
                     &component.light_data.base.diffuse, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(
          *s_command_stack, "Change Light Diffuse",
          component.light_data.base.diffuse, [entity](const float& v) mutable {
            entity.GetComponent<LightPointComponent>().light_data.base.diffuse =
                v;
          });
    }

    ImGui::DragFloat(PrefixLabel("Specular").c_str(),
                     &component.light_data.base.specular, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Light Specular",
                    component.light_data.base.specular,
                    [entity](const float& v) mutable {
                      entity.GetComponent<LightPointComponent>()
                          .light_data.base.specular = v;
                    });
    }

    ImGui::DragFloat(PrefixLabel("Density").c_str(),
                     &component.light_data.base.density, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(
          *s_command_stack, "Change Light Density",
          component.light_data.base.density, [entity](const float& v) mutable {
            entity.GetComponent<LightPointComponent>().light_data.base.density =
                v;
          });
    }

    if (ImGui::TreeNode("Attenuation")) {
      ImGui::DragFloat(PrefixLabel("Constant").c_str(),
                       &component.light_data.constant, 0.1f);
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Attenuation Constant",
            component.light_data.constant, [entity](const float& v) mutable {
              entity.GetComponent<LightPointComponent>().light_data.constant =
                  v;
            });
      }

      ImGui::DragFloat(PrefixLabel("Linear").c_str(),
                       &component.light_data.linear, 0.1f);
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Attenuation Linear",
            component.light_data.linear, [entity](const float& v) mutable {
              entity.GetComponent<LightPointComponent>().light_data.linear = v;
            });
      }

      ImGui::DragFloat(PrefixLabel("Exp").c_str(), &component.light_data.exp,
                       0.1f);
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Attenuation Exp",
            component.light_data.exp, [entity](const float& v) mutable {
              entity.GetComponent<LightPointComponent>().light_data.exp = v;
            });
      }

      ImGui::TreePop();
    }
    ImGui::ColorPicker3(
        "Color", reinterpret_cast<float*>(&component.light_data.base.color));
    if (s_command_stack) {
      static editor::UndoTracker<glm::vec3> tracker;
      tracker.Track(
          *s_command_stack, "Change Light Color",
          component.light_data.base.color,
          [entity](const glm::vec3& v) mutable {
            entity.GetComponent<LightPointComponent>().light_data.base.color =
                v;
          });
    }

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<LightPointComponent>();
    visible = true;
  }
}

void RenderComponentImGui(CameraComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Camera", &visible)) {
    bool changed = false;

    const char* proj_modes[] = {"Perspective", "Orthographic"};
    int proj_idx = static_cast<int>(component.projection_mode);
    if (ImGui::Combo(PrefixLabel("Projection").c_str(), &proj_idx, proj_modes,
                     2)) {
      component.projection_mode = static_cast<ProjectionMode>(proj_idx);
      changed = true;
    }

    if (component.projection_mode == ProjectionMode::Perspective) {
      changed |= ImGui::DragFloat(PrefixLabel("FOV").c_str(),
                                  &component.field_of_view, 1.0f, 1.0f, 179.0f);
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(*s_command_stack, "Change FOV", component.field_of_view,
                      [entity](const float& v) mutable {
                        auto& cam = entity.GetComponent<CameraComponent>();
                        cam.field_of_view = v;
                        cam.view_changed = true;
                        cam.resource_pipeline_version = 0;
                      });
      }
    } else {
      changed |= ImGui::DragFloat(PrefixLabel("Size").c_str(),
                                  &component.ortho_size, 0.1f, 0.01f, 1000.0f);
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(*s_command_stack, "Change Ortho Size",
                      component.ortho_size, [entity](const float& v) mutable {
                        auto& cam = entity.GetComponent<CameraComponent>();
                        cam.ortho_size = v;
                        cam.view_changed = true;
                        cam.resource_pipeline_version = 0;
                      });
      }

      ImGui::ColorEdit4(PrefixLabel("Background").c_str(),
                        &component.background_color.r);
      if (s_command_stack) {
        static editor::UndoTracker<glm::vec4> tracker;
        tracker.Track(
            *s_command_stack, "Change Background Color",
            component.background_color, [entity](const glm::vec4& v) mutable {
              entity.GetComponent<CameraComponent>().background_color = v;
            });
      }
    }

    changed |= ImGui::DragFloat(PrefixLabel("Near Plane").c_str(),
                                &component.near_plane, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Near Plane", component.near_plane,
                    [entity](const float& v) mutable {
                      auto& cam = entity.GetComponent<CameraComponent>();
                      cam.near_plane = v;
                      cam.view_changed = true;
                      cam.resource_pipeline_version = 0;
                    });
    }

    changed |= ImGui::DragFloat(PrefixLabel("Far Plane").c_str(),
                                &component.far_plane, 0.1f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Far Plane", component.far_plane,
                    [entity](const float& v) mutable {
                      auto& cam = entity.GetComponent<CameraComponent>();
                      cam.far_plane = v;
                      cam.view_changed = true;
                      cam.resource_pipeline_version = 0;
                    });
    }

    ImGui::Checkbox(PrefixLabel("Enabled").c_str(), &component.enabled);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(*s_command_stack, "Toggle Camera Enabled",
                    component.enabled, [entity](const bool& v) mutable {
                      entity.GetComponent<CameraComponent>().enabled = v;
                    });
    }

    ImGui::Text("Viewport: %dx%d", static_cast<int>(component.viewport_size.x),
                static_cast<int>(component.viewport_size.y));

    if (changed) {
      component.aspect_ratio =
          component.viewport_size.x / component.viewport_size.y;
      component.view_changed = true;
      component.resource_pipeline_version = 0;
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
  for (FieldData& fd : data.fields() | std::views::values) {
    std::string label = PrefixLabel(fd.formatted_name().c_str());

    if (fd.is_network_var()) {
      // NetworkVariable<T> - render the inner value via registry
      MonoObject* net_var = nullptr;
      mono_field_get_value(instance->handle(), fd.field(), &net_var);
      if (!net_var) {
        ImGui::Text("%s: (null)", fd.formatted_name().c_str());
        continue;
      }

      MonoClass* net_var_class = mono_object_get_class(net_var);
      MonoClassField* val_field =
          mono_class_get_field_from_name(net_var_class, "value");
      MonoProperty* val_prop =
          mono_class_get_property_from_name(net_var_class, "Value");
      if (!val_field || !val_prop) {
        continue;
      }

      auto* desc = ScriptFieldTypeRegistry::Find(fd.inner_type_name());
      if (desc && desc->Render) {
        desc->Render(net_var, val_field, val_prop, label);
      } else {
        ImGui::Text("%s: %s (no renderer)",
                    fd.formatted_name().c_str(),
                    fd.inner_type_name().c_str());
      }
    } else {
      auto* desc = ScriptFieldTypeRegistry::Find(fd.type_name());
      if (desc && desc->Render) {
        desc->Render(instance->handle(), fd.field(), nullptr, label);
      } else {
        ImGui::Text("%s: %s (unsupported)",
                    fd.formatted_name().c_str(),
                    fd.type_name().c_str());
      }
    }
  }
}

bool RenderBehaviorComponentImGui(BehaviorsComponent& component,
                                  IBehavior& behavior, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode(behavior.GetEditorName().c_str(), &visible)) {
    bool enabled = behavior.IsEnabled();
    if (ImGui::Checkbox(PrefixLabel("Enabled").c_str(), &enabled)) {
      behavior.SetEnabled(enabled);
    }
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
  bool visible = true;
  if (ImGui::ClosableTreeNode("Box Collider", &visible)) {
    ImGui::DragFloat3(PrefixLabel("Offset").c_str(),
                      reinterpret_cast<float*>(&component.offset), 0.05f);
    ImGui::DragFloat3(PrefixLabel("Half Extents").c_str(),
                      reinterpret_cast<float*>(&component.half_extents), 0.05f,
                      0.0f, 1000.0f);
    ImGui::Checkbox(PrefixLabel("Is Trigger").c_str(), &component.is_trigger);
    ImGui::Checkbox(PrefixLabel("One Way").c_str(), &component.is_one_way);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<BoxColliderComponent>();
    visible = true;
  }
}

void RenderComponentImGui(SphereColliderComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Sphere Collider", &visible)) {
    ImGui::DragFloat3(PrefixLabel("Offset").c_str(),
                      reinterpret_cast<float*>(&component.offset), 0.05f);
    ImGui::DragFloat(PrefixLabel("Radius").c_str(), &component.radius, 0.05f,
                     0.0f, 1000.0f);
    ImGui::Checkbox(PrefixLabel("Is Trigger").c_str(), &component.is_trigger);
    ImGui::Checkbox(PrefixLabel("One Way").c_str(), &component.is_one_way);
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

void RenderComponentImGui(CapsuleColliderComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Capsule Collider", &visible)) {
    ImGui::DragFloat3(PrefixLabel("Offset").c_str(), &component.offset.x,
                      0.01f);
    ImGui::DragFloat(PrefixLabel("Radius").c_str(), &component.radius, 0.01f,
                     0.01f, 100.0f);
    ImGui::DragFloat(PrefixLabel("Height").c_str(), &component.height, 0.01f,
                     0.01f, 100.0f);

    const char* axis_names[] = {"X", "Y", "Z"};
    int axis_idx = static_cast<int>(component.axis);
    if (ImGui::Combo(PrefixLabel("Axis").c_str(), &axis_idx, axis_names, 3)) {
      component.axis = static_cast<CapsuleAxis>(axis_idx);
    }

    ImGui::Checkbox(PrefixLabel("Is Trigger").c_str(), &component.is_trigger);
    ImGui::Checkbox(PrefixLabel("One Way").c_str(), &component.is_one_way);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CapsuleColliderComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_CapsuleColliderComponent(Entity entity) {
  if (ImGui::MenuItem("Capsule Collider") &&
      !entity.HasComponent<CapsuleColliderComponent>()) {
    entity.AddComponent<CapsuleColliderComponent>();
  }
}

void RenderComponentImGui(MeshColliderComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Mesh Collider", &visible)) {
    AssetDropField("Collider Asset", AssetType::MeshCollider,
                   component.collider_handle);

    // Show baked asset info
    if (component.collider_handle.IsValid()) {
      auto baked = Engine::asset_manager().Get<MeshColliderAssetData>(
          component.collider_handle);
      if (baked) {
        ImGui::TextDisabled("  %zu vertices, %zu triangles",
                            baked->vertices.size(), baked->indices.size() / 3);
      }
    }

    ImGui::DragFloat3(PrefixLabel("Offset").c_str(),
                      reinterpret_cast<float*>(&component.offset), 0.05f);
    ImGui::Checkbox(PrefixLabel("Is Trigger").c_str(), &component.is_trigger);
    ImGui::Checkbox(PrefixLabel("One Way").c_str(), &component.is_one_way);

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<MeshColliderComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_MeshColliderComponent(Entity entity) {
  if (ImGui::MenuItem("Mesh Collider") &&
      !entity.HasComponent<MeshColliderComponent>()) {
    entity.AddComponent<MeshColliderComponent>();
  }
}

void RenderComponentImGui(UIDocumentComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("UI Document", &visible)) {
    AssetDropField("Document", AssetType::UIDocument,
                   component.document_handle);
    ImGui::Checkbox(PrefixLabel("Visible").c_str(), &component.visible);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<UIDocumentComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_UIDocumentComponent(Entity entity) {
  if (ImGui::MenuItem("UI Document") &&
      !entity.HasComponent<UIDocumentComponent>()) {
    entity.AddComponent<UIDocumentComponent>();
    entity.AddComponent<RectangleTransformComponent>();
  }
}

void RenderComponentImGui(RigidBodyComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Rigid Body", &visible)) {
    const char* types[] = {"Static", "Kinematic", "Dynamic"};
    int type_idx = (int)component.type;
    if (ImGui::Combo(PrefixLabel("Type").c_str(), &type_idx, types, 3)) {
      component.type = (RigidBodyType)type_idx;
      component.needs_recreate = true;
    }
    if (component.type == RigidBodyType::Dynamic) {
      float mass = component.mass;
      if (ImGui::DragFloat(PrefixLabel("Mass").c_str(), &mass, 0.1f, 0.01f,
                           10000.0f)) {
        component.SetMassRuntime(mass);
      }
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Mass", component.mass,
            [entity](const float& v) mutable {
              entity.GetComponent<RigidBodyComponent>().SetMassRuntime(v);
            });
      }
    }
    {
      float friction = component.friction;
      if (ImGui::DragFloat(PrefixLabel("Friction").c_str(), &friction, 0.01f,
                           0.0f, 1.0f)) {
        component.SetFrictionRuntime(friction);
      }
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Friction", component.friction,
            [entity](const float& v) mutable {
              entity.GetComponent<RigidBodyComponent>().SetFrictionRuntime(v);
            });
      }
    }
    {
      float restitution = component.restitution;
      if (ImGui::DragFloat(PrefixLabel("Restitution").c_str(), &restitution,
                           0.01f, 0.0f, 1.0f)) {
        component.SetRestitutionRuntime(restitution);
      }
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Restitution", component.restitution,
            [entity](const float& v) mutable {
              entity.GetComponent<RigidBodyComponent>().SetRestitutionRuntime(
                  v);
            });
      }
    }
    {
      float linear_damping = component.linear_damping;
      if (ImGui::DragFloat(PrefixLabel("Linear Damping").c_str(),
                           &linear_damping, 0.01f, 0.0f, 1.0f)) {
        component.SetLinearDampingRuntime(linear_damping);
      }
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Linear Damping", component.linear_damping,
            [entity](const float& v) mutable {
              entity.GetComponent<RigidBodyComponent>().SetLinearDampingRuntime(
                  v);
            });
      }
    }
    {
      float angular_damping = component.angular_damping;
      if (ImGui::DragFloat(PrefixLabel("Angular Damping").c_str(),
                           &angular_damping, 0.01f, 0.0f, 1.0f)) {
        component.SetAngularDampingRuntime(angular_damping);
      }
      if (s_command_stack) {
        static editor::UndoTracker<float> tracker;
        tracker.Track(*s_command_stack, "Change Angular Damping",
                      component.angular_damping,
                      [entity](const float& v) mutable {
                        entity.GetComponent<RigidBodyComponent>()
                            .SetAngularDampingRuntime(v);
                      });
      }
    }
    ImGui::Text("Freeze Position");
    ImGui::SameLine();
    ImGui::Checkbox("X##fp", &component.lock_position_x);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Position X",
          component.lock_position_x, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_position_x = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Y##fp", &component.lock_position_y);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Position Y",
          component.lock_position_y, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_position_y = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Z##fp", &component.lock_position_z);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Position Z",
          component.lock_position_z, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_position_z = v;
          });
    }

    ImGui::Text("Freeze Rotation");
    ImGui::SameLine();
    ImGui::Checkbox("X##fr", &component.lock_rotation_x);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Rotation X",
          component.lock_rotation_x, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_rotation_x = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Y##fr", &component.lock_rotation_y);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Rotation Y",
          component.lock_rotation_y, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_rotation_y = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Z##fr", &component.lock_rotation_z);
    if (s_command_stack) {
      static editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Rotation Z",
          component.lock_rotation_z, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_rotation_z = v;
          });
    }

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

void RenderComponentImGui(RectangleTransformComponent& component,
                          Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Rectangle Transform", &visible)) {
    bool changed = false;
    changed |=
        ImGui::DragFloat2(PrefixLabel("Position").c_str(),
                          reinterpret_cast<float*>(&component.position), 0.5f);
    changed |= ImGui::DragFloat(PrefixLabel("Rotation").c_str(),
                                &component.rotation, 0.5f);
    changed |=
        ImGui::DragFloat2(PrefixLabel("Size").c_str(),
                          reinterpret_cast<float*>(&component.size), 0.5f);
    changed |=
        ImGui::DragFloat2(PrefixLabel("Scale").c_str(),
                          reinterpret_cast<float*>(&component.scale), 0.01f);

    const char* anchors[] = {"Top Left",    "Top Center",    "Top Right",
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

    changed |=
        ImGui::DragFloat4(PrefixLabel("Padding").c_str(),
                          reinterpret_cast<float*>(&component.padding), 0.5f);
    changed |=
        ImGui::DragFloat4(PrefixLabel("Margin").c_str(),
                          reinterpret_cast<float*>(&component.margin), 0.5f);

    ImGui::TextDisabled("Computed: (%.0f, %.0f) %.0fx%.0f",
                        component.computed_position.x,
                        component.computed_position.y,
                        component.computed_size.x, component.computed_size.y);

    /*if (changed) {
      component.is_changed = true;
    }*/
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<RectangleTransformComponent>();
    visible = true;
  }
}

static void EnsureRectangleTransform(Entity entity,
                                     bool default_zero_size = false) {
  if (!entity.HasComponent<RectangleTransformComponent>()) {
    auto& transform = entity.AddComponent<RectangleTransformComponent>();
    if (default_zero_size) {
      transform.size = {0, 0};  // auto sized
    }
  }
}

void RenderComponentImGui(CanvasComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Canvas", &visible)) {
    const char* render_modes[] = {"Screen Space - Overlay",
                                  "Screen Space - Camera", "World Space"};
    int rm = static_cast<int>(component.render_mode);
    if (ImGui::Combo(PrefixLabel("Render Mode").c_str(), &rm, render_modes,
                     3)) {
      component.render_mode = static_cast<CanvasRenderMode>(rm);
    }

    if (component.render_mode == CanvasRenderMode::ScreenSpaceCamera) {
      ImGui::DragFloat(PrefixLabel("Plane Distance").c_str(),
                       &component.plane_distance, 0.1f, 0.1f, 1000.0f);

      // Camera entity picker - show combo of all entities with CameraComponent
      {
        Scene* s = entity.GetScene();
        std::string current_name = "(None)";
        if (component.camera_entity != entt::null && s &&
            s->HasComponent<TagComponent>(component.camera_entity)) {
          current_name =
              s->GetComponent<TagComponent>(component.camera_entity).name;
        }
        if (ImGui::BeginCombo(PrefixLabel("Camera").c_str(),
                              current_name.c_str())) {
          if (ImGui::Selectable("(None)",
                                component.camera_entity == entt::null)) {
            component.camera_entity = entt::null;
          }
          if (s) {
            for (auto cam_entity : s->GetAllEntitiesWith<CameraComponent>()) {
              auto& tag = s->GetComponent<TagComponent>(cam_entity);
              bool selected = (cam_entity == component.camera_entity);
              if (ImGui::Selectable(tag.name.c_str(), selected)) {
                component.camera_entity = cam_entity;
              }
            }
          }
          ImGui::EndCombo();
        }
      }
    }
    // WorldSpace size is controlled by CanvasScaler.reference_pixels_per_unit

    ImGui::SeparatorText("Layout");
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
    ImGui::DragFloat(PrefixLabel("Start Spacing").c_str(),
                     &component.start_spacing, 0.5f);
    ImGui::DragFloat(PrefixLabel("End Spacing").c_str(), &component.end_spacing,
                     0.5f);
    ImGui::InputInt(PrefixLabel("Sort Order").c_str(), &component.sort_order);

    ImGui::SeparatorText("Input");
    ImGui::InputInt(PrefixLabel("Player Index").c_str(),
                    &component.player_index);
    component.player_index = std::clamp(component.player_index, 0, 3);
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CanvasComponent>();
    visible = true;
  }
}

void RenderComponentImGui(CanvasScalerComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Canvas Scaler", &visible)) {
    // Check if the parent canvas is WorldSpace
    bool is_world_space = false;
    if (entity.HasComponent<CanvasComponent>()) {
      is_world_space = entity.GetComponent<CanvasComponent>().render_mode ==
                       CanvasRenderMode::WorldSpace;
    }

    if (is_world_space) {
      // WorldSpace mode: show pixels-per-unit instead of scale mode
      ImGui::InputFloat2(PrefixLabel("Reference Resolution").c_str(),
                         glm::value_ptr(component.reference_resolution),
                         "%.0f", ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::DragFloat(PrefixLabel("Ref Pixels Per Unit").c_str(),
                       &component.reference_pixels_per_unit, 1.0f, 1.0f,
                       1000.0f);
    } else {
      const char* modes[] = {"Constant Pixel Size", "Scale With Screen Size"};
      int mode = static_cast<int>(component.scale_mode);
      if (ImGui::Combo(PrefixLabel("Scale Mode").c_str(), &mode, modes, 2)) {
        component.scale_mode = static_cast<ScaleMode>(mode);
      }
      if (component.scale_mode == ScaleMode::ScaleWithScreenSize) {
        ImGui::InputFloat2(PrefixLabel("Reference Resolution").c_str(),
                           glm::value_ptr(component.reference_resolution),
                           "%.0f");
        ImGui::SliderFloat(PrefixLabel("Match").c_str(),
                           &component.match_width_or_height, 0.0f, 1.0f,
                           "Width %.2f Height");
      }
    }
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<CanvasScalerComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_CanvasScalerComponent(Entity entity) {
  if (ImGui::MenuItem("Canvas Scaler")) {
    entity.AddComponent<CanvasScalerComponent>();
  }
}

void RenderAddComponentImGui_CanvasComponent(Entity entity) {
  if (ImGui::MenuItem("Canvas")) {
    entity.AddComponent<CanvasComponent>();
    EnsureRectangleTransform(entity, true);
  }
}

void RenderAddComponentImGui_RectangleTransformComponent(Entity entity) {
  if (ImGui::MenuItem("Rectangle Transform")) {
    entity.AddComponent<RectangleTransformComponent>();
  }
}

void RenderComponentImGui(AnimatorComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Animator", &visible)) {
    AssetDropField("Controller", AssetType::AnimController,
                   component.controller_handle);
    ImGui::Checkbox(PrefixLabel("Playing").c_str(), &component.playing);
    ImGui::DragFloat(PrefixLabel("Speed").c_str(), &component.playback_speed,
                     0.01f, -10.0f, 10.0f);

    // Current state
    std::string state_label = component.GetCurrentState();
    if (state_label.empty()) {
      state_label = "(None)";
    }
    ImGui::Text("State: %s", state_label.c_str());

    // Show blending progress if skeletal runtime is active
    if (entity.HasComponent<SkeletalAnimRuntime>()) {
      auto& skel = entity.GetComponent<SkeletalAnimRuntime>();
      if (skel.is_blending) {
        ImGui::ProgressBar(skel.blend_weight, ImVec2(-1, 0), "Blending...");
      }
      ImGui::TextDisabled("Bones: %zu  Nodes: %zu", skel.bone_matrices.size(),
                          skel.node_transforms.size());
    }

    // Show sprite frame info if sprite runtime is active
    if (entity.HasComponent<SpriteAnimRuntime>()) {
      auto& spr = entity.GetComponent<SpriteAnimRuntime>();
      ImGui::TextDisabled("Frame: %u", spr.current_frame_index);
    }

    // Parameters
    if (ImGui::TreeNode("Parameters")) {
      for (auto& [name, param] : component.state_machine.parameters) {
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
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
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

// --- MeshRendererComponent ---

void RenderComponentImGui(MeshRendererComponent& component, Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Mesh Renderer", &visible)) {
    ImGui::Checkbox(PrefixLabel("Rendering").c_str(),
                    &component.enable_rendering);
    ImGui::Checkbox(PrefixLabel("Receive Shadows").c_str(),
                    &component.receive_shadows);

    // Model selector
    AssetDropField("Model", AssetType::Model, component.model_handle);

    // Mesh index (for multi-mesh models)
    if (component.model_handle.IsValid()) {
      auto model = Engine::asset_manager().Get<Model>(component.model_handle);
      if (model && model->meshes.size() > 1) {
        int mesh_count = static_cast<int>(model->meshes.size());
        ImGui::SliderInt(PrefixLabel("Mesh Index").c_str(),
                         &component.mesh_index, 0, mesh_count - 1);
      }
    }

    // Material
    AssetDropField("Material", AssetType::Material, component.material_handle);

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<MeshRendererComponent>();
    visible = true;
  }
}

// --- SkinnedMeshRendererComponent ---

void RenderComponentImGui(SkinnedMeshRendererComponent& component,
                          Entity entity) {
  bool visible = true;
  if (ImGui::ClosableTreeNode("Skinned Mesh Renderer", &visible)) {
    ImGui::Checkbox(PrefixLabel("Rendering").c_str(),
                    &component.enable_rendering);
    ImGui::Checkbox(PrefixLabel("Receive Shadows").c_str(),
                    &component.receive_shadows);

    if (component.model_handle.IsValid()) {
      const auto* meta =
          Engine::asset_manager().GetMetadata(component.model_handle);
      ImGui::TextDisabled("Model: %s", meta ? meta->name.c_str() : "???");
    }
    ImGui::TextDisabled("Mesh Index: %d", component.mesh_index);

    if (component.skeleton_root != entt::null) {
      ImGui::TextDisabled("Skeleton Root: %u",
                          static_cast<uint32_t>(component.skeleton_root));
    } else {
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "No Skeleton Root");
    }

    AssetDropField("Material", AssetType::Material, component.material_handle);

    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<SkinnedMeshRendererComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_MeshRendererComponent(Entity entity) {
  if (ImGui::MenuItem("Mesh Renderer") &&
      !entity.HasComponent<MeshRendererComponent>()) {
    entity.AddComponent<MeshRendererComponent>();
  }
}

void RenderAddComponentImGui_SkinnedMeshRendererComponent(Entity entity) {
  if (ImGui::MenuItem("Skinned Mesh Renderer") &&
      !entity.HasComponent<SkinnedMeshRendererComponent>()) {
    entity.AddComponent<SkinnedMeshRendererComponent>();
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
    component.viewport_size = {1920, 1080};
    component.aspect_ratio = 1920.0f / 1080.0f;
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
  if (entity.handle() != addMonoScriptEntityId) {
    return;
  }

  if (shouldOpenMonoScriptPopup) {
    ImGui::OpenPopup("Add Script");
    shouldOpenMonoScriptPopup = false;
  }

  static int currentScriptIndex = 0;
  bool open = true;
  if (ImGui::BeginPopupModal("Add Script", &open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Build combined list: native behaviors + C# scripts
    std::vector<std::string> all_scripts;
    std::vector<bool> is_native;

    const auto& native_names = Engine::behavior_registry().GetNames();
    for (const auto& name : native_names) {
      all_scripts.push_back(name + " (C++)");
      is_native.push_back(true);
    }
    const std::vector<std::string>& cs_names =
        Engine::script_manager().script_names();
    for (const auto& name : cs_names) {
      all_scripts.push_back(name + " (C#)");
      is_native.push_back(false);
    }

    if (!all_scripts.empty()) {
      if (currentScriptIndex >= static_cast<int>(all_scripts.size())) {
        currentScriptIndex = 0;
      }
      ImGui::Combo(
          "Script", &currentScriptIndex,
          [](void* data, int idx) -> const char* {
            const auto& names =
                *static_cast<const std::vector<std::string>*>(data);
            if (idx < 0 || idx >= static_cast<int>(names.size())) {
              return nullptr;
            }
            return names[idx].c_str();
          },
          (void*)&all_scripts, static_cast<int>(all_scripts.size()));
    } else {
      ImGui::TextDisabled("No scripts found.");
    }

    if (ImGui::Button("Add") && !all_scripts.empty()) {
      if (!entity.HasComponent<BehaviorsComponent>()) {
        entity.AddComponent<BehaviorsComponent>();
      }

      auto& bc = entity.GetComponent<BehaviorsComponent>();
      if (is_native[currentScriptIndex]) {
        const std::string& name = native_names[currentScriptIndex];
        NativeBehavior* native =
            Engine::behavior_registry().Create(name, entity);
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

// ComponentUiRegistry backing storage. Kept here (not in the header) so the
// map/vector don't need to be visible to every TU that just wants to query.
namespace {
std::unordered_map<std::type_index, ComponentDesc>& RegistryMap() {
  static std::unordered_map<std::type_index, ComponentDesc> m;
  return m;
}
std::vector<std::type_index>& RegistryOrder() {
  static std::vector<std::type_index> v;
  return v;
}
}  // namespace

void ComponentUiRegistry::Install(std::type_index type, ComponentDesc desc) {
  auto& map = RegistryMap();
  auto& order = RegistryOrder();
  if (!map.contains(type)) {
    order.push_back(type);
  }
  map[type] = std::move(desc);
}

const ComponentDesc* ComponentUiRegistry::Get(std::type_index type) {
  auto& map = RegistryMap();
  auto it = map.find(type);
  return it == map.end() ? nullptr : &it->second;
}

const std::vector<std::type_index>& ComponentUiRegistry::All() {
  return RegistryOrder();
}

void RenderComponentImGui(AudioSourceComponent& component, Entity entity) {
  bool visible = true;
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
    if (meta) {
      display = meta->name;
    }
  }
  ImGui::InputText(PrefixLabel("Clip").c_str(), &display,
                   ImGuiInputTextFlags_ReadOnly);

  if (ImGui::BeginDragDropTarget()) {
    AssetHandle dropped = AcceptAssetDragDrop(AssetType::Audio);
    if (dropped.IsValid()) {
      component.clip = dropped;
    }
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

  ImGui::SliderFloat(PrefixLabel("Volume").c_str(), &component.volume, 0.0f,
                     1.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Volume", component.volume,
                  [entity](const float& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().volume = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Pitch").c_str(), &component.pitch, 0.1f,
                     3.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Pitch", component.pitch,
                  [entity](const float& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().pitch = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Spatial Blend").c_str(),
                     &component.spatial_blend, 0.0f, 1.0f,
                     component.spatial_blend < 0.01f
                         ? "2D"
                         : (component.spatial_blend > 0.99f ? "3D" : "%.2f"));
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Spatial Blend",
                  component.spatial_blend, [entity](const float& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().spatial_blend =
                        v;
                  });
  }

  ImGui::Checkbox(PrefixLabel("Loop").c_str(), &component.loop);
  if (s_command_stack) {
    static editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Loop", component.loop,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().loop = v;
                  });
  }

  ImGui::Checkbox(PrefixLabel("Play On Start").c_str(),
                  &component.play_on_start);
  if (s_command_stack) {
    static editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Play On Start",
                  component.play_on_start, [entity](const bool& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().play_on_start =
                        v;
                  });
  }

  ImGui::Checkbox(PrefixLabel("Mute").c_str(), &component.mute);
  if (s_command_stack) {
    static editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Mute", component.mute,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().mute = v;
                  });
  }

  if (component.spatial_blend > 0.0f) {
    ImGui::DragFloat(PrefixLabel("Min Distance").c_str(),
                     &component.min_distance, 0.1f, 0.0f, 1000.0f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Min Distance",
                    component.min_distance, [entity](const float& v) mutable {
                      entity.GetComponent<AudioSourceComponent>().min_distance =
                          v;
                    });
    }

    ImGui::DragFloat(PrefixLabel("Max Distance").c_str(),
                     &component.max_distance, 1.0f, 0.0f, 10000.0f);
    if (s_command_stack) {
      static editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Max Distance",
                    component.max_distance, [entity](const float& v) mutable {
                      entity.GetComponent<AudioSourceComponent>().max_distance =
                          v;
                    });
    }
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
        component.playing_handle_ =
            Engine::audio().Play(component.clip, params);
      }
    }
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_AudioSourceComponent(Entity entity) {
  if (ImGui::MenuItem("Audio Source") &&
      !entity.HasComponent<AudioSourceComponent>()) {
    entity.AddComponent<AudioSourceComponent>();
  }
}

void RenderComponentImGui(SpriteRendererComponent& component, Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Sprite Renderer", &visible)) {
    if (!visible) {
      entity.RemoveComponent<SpriteRendererComponent>();
      visible = true;
    }
    return;
  }

  // Sprite asset picker
  {
    AssetHandle prev_handle = component.sprite_handle_;
    if (AssetDropField("Sprite", AssetType::Sprite, component.sprite_handle_)) {
      if (s_command_stack) {
        AssetHandle new_handle = component.sprite_handle_;
        s_command_stack->Execute(
            std::make_unique<editor::PropertyCommand<AssetHandle>>(
                "Change Sprite",
                [entity](const AssetHandle& v) mutable {
                  entity.GetComponent<SpriteRendererComponent>()
                      .sprite_handle_ = v;
                },
                prev_handle, new_handle));
      }
    }
  }

  // Visual properties
  ImGui::Checkbox(PrefixLabel("Flip X").c_str(), &component.flip_x_);
  if (s_command_stack) {
    static editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Flip X", component.flip_x_,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().flip_x_ = v;
                  });
  }

  ImGui::SameLine();
  ImGui::Checkbox("Flip Y", &component.flip_y_);
  if (s_command_stack) {
    static editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Flip Y", component.flip_y_,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().flip_y_ = v;
                  });
  }

  ImGui::ColorEdit4(PrefixLabel("Tint").c_str(), &component.tint_.r);
  if (s_command_stack) {
    static editor::UndoTracker<glm::vec4> tracker;
    tracker.Track(*s_command_stack, "Change Sprite Tint", component.tint_,
                  [entity](const glm::vec4& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().tint_ = v;
                  });
  }

  int sort = component.sort_layer_;
  if (ImGui::InputInt(PrefixLabel("Sort Layer").c_str(), &sort)) {
    component.sort_layer_ = static_cast<uint8_t>(std::clamp(sort, 0, 255));
  }
  if (s_command_stack) {
    static editor::UndoTracker<int> tracker;
    tracker.Track(*s_command_stack, "Change Sort Layer",
                  static_cast<int>(component.sort_layer_),
                  [entity](const int& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().sort_layer_ =
                        static_cast<uint8_t>(std::clamp(v, 0, 255));
                  });
  }

  ImGui::DragFloat2(PrefixLabel("Pivot").c_str(), &component.pivot_.x, 0.01f,
                    0.0f, 1.0f);
  if (s_command_stack) {
    static editor::UndoTracker<glm::vec2> tracker;
    tracker.Track(*s_command_stack, "Change Pivot", component.pivot_,
                  [entity](const glm::vec2& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().pivot_ = v;
                  });
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_SpriteRendererComponent(Entity entity) {
  if (ImGui::MenuItem("Sprite Renderer") &&
      !entity.HasComponent<SpriteRendererComponent>()) {
    entity.AddComponent<SpriteRendererComponent>();
  }
}

void RenderComponentImGui(BillboardRendererComponent& component,
                          Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Billboard Renderer", &visible)) {
    if (!visible) {
      entity.RemoveComponent<BillboardRendererComponent>();
      visible = true;
    }
    return;
  }

  {
    AssetHandle prev_handle = component.texture_handle;
    if (AssetDropField("Texture", AssetType::Texture,
                       component.texture_handle)) {
      component.cached_texture = nullptr;
      component.cached_descriptor = nullptr;
      component.bound_handle = AssetHandle{};
      if (s_command_stack) {
        AssetHandle new_handle = component.texture_handle;
        s_command_stack->Execute(
            std::make_unique<editor::PropertyCommand<AssetHandle>>(
                "Change Billboard Texture",
                [entity](const AssetHandle& v) mutable {
                  auto& b =
                      entity.GetComponent<BillboardRendererComponent>();
                  b.texture_handle = v;
                  b.cached_texture = nullptr;
                  b.cached_descriptor = nullptr;
                  b.bound_handle = AssetHandle{};
                },
                prev_handle, new_handle));
      }
    }
  }

  ImGui::DragFloat2(PrefixLabel("Size").c_str(), &component.size.x, 1.0f,
                    1.0f, 4096.0f);
  if (s_command_stack) {
    static editor::UndoTracker<glm::vec2> tracker;
    tracker.Track(*s_command_stack, "Change Billboard Size", component.size,
                  [entity](const glm::vec2& v) mutable {
                    entity.GetComponent<BillboardRendererComponent>().size = v;
                  });
  }

  ImGui::DragFloat(PrefixLabel("Min Size").c_str(), &component.min_size, 0.01f,
                   0.0f, 100.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Billboard Min Size",
                  component.min_size, [entity](const float& v) mutable {
                    entity.GetComponent<BillboardRendererComponent>().min_size =
                        v;
                  });
  }

  ImGui::DragFloat(PrefixLabel("Max Size").c_str(), &component.max_size, 0.01f,
                   0.0f, 100.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Billboard Max Size",
                  component.max_size, [entity](const float& v) mutable {
                    entity.GetComponent<BillboardRendererComponent>().max_size =
                        v;
                  });
  }

  ImGui::DragFloat2(PrefixLabel("Pivot").c_str(), &component.pivot.x, 0.01f,
                    0.0f, 1.0f);
  if (s_command_stack) {
    static editor::UndoTracker<glm::vec2> tracker;
    tracker.Track(*s_command_stack, "Change Billboard Pivot", component.pivot,
                  [entity](const glm::vec2& v) mutable {
                    entity.GetComponent<BillboardRendererComponent>().pivot = v;
                  });
  }

  ImGui::ColorEdit4(PrefixLabel("Tint").c_str(), &component.tint.r);
  if (s_command_stack) {
    static editor::UndoTracker<glm::vec4> tracker;
    tracker.Track(*s_command_stack, "Change Billboard Tint", component.tint,
                  [entity](const glm::vec4& v) mutable {
                    entity.GetComponent<BillboardRendererComponent>().tint = v;
                  });
  }

  ImGui::InputInt(PrefixLabel("Sort Layer").c_str(), &component.sort_layer);
  if (s_command_stack) {
    static editor::UndoTracker<int32_t> tracker;
    tracker.Track(*s_command_stack, "Change Billboard Sort Layer",
                  component.sort_layer,
                  [entity](const int32_t& v) mutable {
                    entity.GetComponent<BillboardRendererComponent>()
                        .sort_layer = v;
                  });
  }

  const char* occlusion_names[] = {"Disabled", "Faded", "Always Visible"};
  int occlusion_idx = static_cast<int>(component.occlusion);
  if (ImGui::Combo(PrefixLabel("Occlusion").c_str(), &occlusion_idx,
                   occlusion_names, IM_ARRAYSIZE(occlusion_names))) {
    component.occlusion = static_cast<BillboardOcclusionMode>(occlusion_idx);
  }
  if (component.occlusion == BillboardOcclusionMode::Faded) {
    ImGui::DragFloat(PrefixLabel("Occluded Alpha").c_str(),
                     &component.occluded_alpha, 0.01f, 0.0f, 1.0f);
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_BillboardRendererComponent(Entity entity) {
  if (ImGui::MenuItem("Billboard Renderer") &&
      !entity.HasComponent<BillboardRendererComponent>()) {
    entity.AddComponent<BillboardRendererComponent>();
  }
}

void RenderComponentImGui(BillboardTextComponent& component, Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Billboard Text", &visible)) {
    if (!visible) {
      entity.RemoveComponent<BillboardTextComponent>();
      visible = true;
    }
    return;
  }

  AssetDropField("Font", AssetType::Font, component.font_handle);

  char buf[512];
  std::snprintf(buf, sizeof(buf), "%s", component.text.c_str());
  if (ImGui::InputText(PrefixLabel("Text").c_str(), buf, sizeof(buf))) {
    component.text = buf;
  }

  ImGui::DragFloat(PrefixLabel("Font Size").c_str(), &component.font_size, 0.5f,
                   1.0f, 512.0f);

  ImGui::ColorEdit4(PrefixLabel("Color").c_str(), &component.color.r);

  const char* alignment_names[] = {"Left", "Center", "Right"};
  int alignment_idx = static_cast<int>(component.alignment);
  if (ImGui::Combo(PrefixLabel("Alignment").c_str(), &alignment_idx,
                   alignment_names, IM_ARRAYSIZE(alignment_names))) {
    component.alignment = static_cast<TextAlignment>(alignment_idx);
  }

  ImGui::DragFloat(PrefixLabel("Min Size").c_str(), &component.min_size, 0.01f,
                   0.0f, 100.0f);
  ImGui::DragFloat(PrefixLabel("Max Size").c_str(), &component.max_size, 0.01f,
                   0.0f, 100.0f);
  ImGui::InputInt(PrefixLabel("Sort Layer").c_str(), &component.sort_layer);

  const char* occlusion_names[] = {"Disabled", "Faded", "Always Visible"};
  int occlusion_idx = static_cast<int>(component.occlusion);
  if (ImGui::Combo(PrefixLabel("Occlusion").c_str(), &occlusion_idx,
                   occlusion_names, IM_ARRAYSIZE(occlusion_names))) {
    component.occlusion = static_cast<BillboardOcclusionMode>(occlusion_idx);
  }
  if (component.occlusion == BillboardOcclusionMode::Faded) {
    ImGui::DragFloat(PrefixLabel("Occluded Alpha").c_str(),
                     &component.occluded_alpha, 0.01f, 0.0f, 1.0f);
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_BillboardTextComponent(Entity entity) {
  if (ImGui::MenuItem("Billboard Text") &&
      !entity.HasComponent<BillboardTextComponent>()) {
    entity.AddComponent<BillboardTextComponent>();
  }
}

void RenderComponentImGui(InteractableComponent& component, Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Interactable", &visible)) {
    if (!visible) {
      entity.RemoveComponent<InteractableComponent>();
      visible = true;
    }
    return;
  }

  ImGui::Checkbox(PrefixLabel("Enabled").c_str(), &component.enabled);
  ImGui::Checkbox(PrefixLabel("Blocks Raycast").c_str(),
                  &component.blocks_raycast);

  if (component.hovered_) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Hovered");
  }
  if (component.pressed_) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Pressed");
  }
  if (component.selected_) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "Selected");
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_InteractableComponent(Entity entity) {
  if (ImGui::MenuItem("Interactable") &&
      !entity.HasComponent<InteractableComponent>()) {
    entity.AddComponent<InteractableComponent>();
  }
}

void RenderComponentImGui(NavigableComponent& component, Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Navigable", &visible)) {
    if (!visible) {
      entity.RemoveComponent<NavigableComponent>();
      visible = true;
    }
    return;
  }

  auto nav_field = [&](const char* label, entt::entity& target) {
    Scene* scene = entity.GetScene();
    std::string name = "None (Auto)";
    if (target != entt::null && scene && scene->GetRegistry().valid(target)) {
      if (scene->HasComponent<TagComponent>(target)) {
        name = scene->GetComponent<TagComponent>(target).name;
      } else {
        name = "Entity";
      }
    }
    ImGui::Text("%s: %s", label, name.c_str());
    // Allow clearing with right-click
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      target = entt::null;
    }
  };

  nav_field("Up", component.nav_up);
  nav_field("Down", component.nav_down);
  nav_field("Left", component.nav_left);
  nav_field("Right", component.nav_right);

  ImGui::TextDisabled("Right-click to clear (reset to auto)");

  ImGui::TreePop();
}

void RenderAddComponentImGui_NavigableComponent(Entity entity) {
  if (ImGui::MenuItem("Navigable") &&
      !entity.HasComponent<NavigableComponent>()) {
    entity.AddComponent<NavigableComponent>();
    if (!entity.HasComponent<InteractableComponent>()) {
      entity.AddComponent<InteractableComponent>();
    }
  }
}

void RenderComponentImGui(ReverbZoneComponent& component, Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Reverb Zone", &visible)) {
    if (!visible) {
      entity.RemoveComponent<ReverbZoneComponent>();
      visible = true;
    }
    return;
  }

  ImGui::DragFloat(PrefixLabel("Radius").c_str(), &component.radius, 0.5f, 0.1f,
                   1000.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Radius", component.radius,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().radius = v;
                  });
  }

  ImGui::DragFloat(PrefixLabel("Delay (ms)").c_str(), &component.delay_ms, 5.0f,
                   10.0f, 2000.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Delay", component.delay_ms,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().delay_ms = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Decay").c_str(), &component.decay, 0.0f,
                     1.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Decay", component.decay,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().decay = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Wet").c_str(), &component.wet, 0.0f, 1.0f);
  if (s_command_stack) {
    static editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Wet", component.wet,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().wet = v;
                  });
  }

  if (component.active_) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Active");
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_ReverbZoneComponent(Entity entity) {
  if (ImGui::MenuItem("Reverb Zone") &&
      !entity.HasComponent<ReverbZoneComponent>()) {
    entity.AddComponent<ReverbZoneComponent>();
  }
}

void RenderComponentImGui(NetworkIdentityComponent& component, Entity entity) {
  bool visible = true;
  if (!ImGui::ClosableTreeNode("Network Identity", &visible)) {
    if (!visible) {
      entity.RemoveComponent<NetworkIdentityComponent>();
      visible = true;
    }
    return;
  }

  const char* authority_names[] = {"None", "Server", "Client"};
  int authority_idx = static_cast<int>(component.authority);
  ImGui::Text("Net ID: %u", component.net_id);
  ImGui::Text("Authority: %s",
              authority_names[std::min(authority_idx, 2)]);
  ImGui::Text("Owner Session: %lu", component.owner_session_id);
  ImGui::Text("Pending Spawn: %s", component.pending_spawn ? "Yes" : "No");

  ImGui::TreePop();
}

void RenderAddComponentImGui_NetworkIdentityComponent(Entity entity) {
  if (ImGui::MenuItem("Network Identity") &&
      !entity.HasComponent<NetworkIdentityComponent>()) {
    entity.AddComponent<NetworkIdentityComponent>();
  }
}

void InitializeEditorComponents() {
  ComponentUiRegistry::Register<IdComponent>("", "", nullptr, nullptr, nullptr);
  ComponentUiRegistry::Register<TagComponent>("", "", nullptr, nullptr, nullptr);
  ComponentUiRegistry::Register<TransformComponent>(
      "Transform", "", RenderComponentImGui, nullptr, nullptr,
      ICON_LC_BOX, 1);
  ComponentUiRegistry::Register<MeshRendererComponent>(
      "Mesh Renderer", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_MeshRendererComponent, nullptr,
      ICON_LC_BOX, 30);
  ComponentUiRegistry::Register<SkinnedMeshRendererComponent>(
      "Skinned Mesh Renderer", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_SkinnedMeshRendererComponent, nullptr,
      ICON_LC_BONE, 35);
  ComponentUiRegistry::Register<AnimatorComponent>(
      "Animator", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_AnimatorComponent, nullptr,
      ICON_LC_FILM, 20);
  ComponentUiRegistry::Register<CameraComponent>(
      "Camera", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_CameraComponent, nullptr,
      ICON_LC_CAMERA, 100);
  ComponentUiRegistry::Register<LightDirectComponent>(
      "Directional Light", "Lighting", RenderComponentImGui,
      RenderAddComponentImGui_LightDirectComponent, nullptr,
      ICON_LC_SUN, 80);
  ComponentUiRegistry::Register<LightPointComponent>(
      "Point Light", "Lighting", RenderComponentImGui,
      RenderAddComponentImGui_LightPointComponent, nullptr,
      ICON_LC_LIGHTBULB, 80);
  ComponentUiRegistry::Register<BehaviorsComponent>(
      "C# Script", "Scripting", RenderComponentImGui,
      RenderAddComponentImGui_BehaviorsComponent,
      RenderModalComponentImGui_BehaviorsComponent,
      ICON_LC_CODE, 25);
  ComponentUiRegistry::Register<BoxColliderComponent>(
      "Box Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_BoxColliderComponent, nullptr,
      ICON_LC_SQUARE_DASHED, 15);
  ComponentUiRegistry::Register<SphereColliderComponent>(
      "Sphere Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_SphereColliderComponent, nullptr,
      ICON_LC_CIRCLE, 15);
  ComponentUiRegistry::Register<CapsuleColliderComponent>(
      "Capsule Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_CapsuleColliderComponent, nullptr,
      ICON_LC_PILL, 15);
  ComponentUiRegistry::Register<MeshColliderComponent>(
      "Mesh Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_MeshColliderComponent, nullptr,
      ICON_LC_TRIANGLE, 15);
  ComponentUiRegistry::Register<RigidBodyComponent>(
      "Rigid Body", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_RigidBodyComponent, nullptr,
      ICON_LC_WEIGHT, 40);
  ComponentUiRegistry::Register<UIDocumentComponent>(
      "UI Document", "UI", RenderComponentImGui,
      RenderAddComponentImGui_UIDocumentComponent, nullptr,
      ICON_LC_FILE_TEXT, 45);
  ComponentUiRegistry::Register<RectangleTransformComponent>(
      "Rectangle Transform", "Canvas", RenderComponentImGui, nullptr, nullptr,
      ICON_LC_SQUARE, 5);
  ComponentUiRegistry::Register<CanvasComponent>(
      "Canvas", "Canvas", RenderComponentImGui,
      RenderAddComponentImGui_CanvasComponent, nullptr,
      ICON_LC_FRAME, 50);
  ComponentUiRegistry::Register<CanvasScalerComponent>(
      "Canvas Scaler", "Canvas", RenderComponentImGui,
      RenderAddComponentImGui_CanvasScalerComponent, nullptr,
      ICON_LC_RULER, 10);
  ComponentUiRegistry::Register<InteractableComponent>(
      "Interactable", "UI", RenderComponentImGui,
      RenderAddComponentImGui_InteractableComponent, nullptr,
      ICON_LC_HAND, 40);
  ComponentUiRegistry::Register<NavigableComponent>(
      "Navigable", "UI", RenderComponentImGui,
      RenderAddComponentImGui_NavigableComponent, nullptr,
      ICON_LC_NAVIGATION, 40);
  ComponentUiRegistry::Register<AudioSourceComponent>(
      "Audio Source", "Audio", RenderComponentImGui,
      RenderAddComponentImGui_AudioSourceComponent, nullptr,
      ICON_LC_VOLUME_2, 45);
  ComponentUiRegistry::Register<SpriteRendererComponent>(
      "Sprite Renderer", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_SpriteRendererComponent, nullptr,
      ICON_LC_IMAGE, 40);
  ComponentUiRegistry::Register<BillboardRendererComponent>(
      "Billboard Renderer", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_BillboardRendererComponent, nullptr,
      ICON_LC_IMAGE, 35);
  ComponentUiRegistry::Register<BillboardTextComponent>(
      "Billboard Text", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_BillboardTextComponent, nullptr,
      ICON_LC_TYPE, 35);
  ComponentUiRegistry::Register<ReverbZoneComponent>(
      "Reverb Zone", "Audio", RenderComponentImGui,
      RenderAddComponentImGui_ReverbZoneComponent, nullptr,
      ICON_LC_WAVES, 30);
  ComponentUiRegistry::Register<NetworkIdentityComponent>(
      "Network Identity", "Networking", RenderComponentImGui,
      RenderAddComponentImGui_NetworkIdentityComponent, nullptr,
      ICON_LC_NETWORK, 60);
}

void RenderExistingComponents(Entity entity) {
  for (const auto& ti : RegistryOrder()) {
    const auto& desc = RegistryMap().at(ti);
    if (!desc.HasComponent(entity)) {
      continue;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImDrawListSplitter splitter;
    splitter.Split(dl, 2);
    splitter.SetCurrentChannel(dl, 1);

    const float top_y = ImGui::GetCursorScreenPos().y;

    ImGui::ResetComponentHeaderState();
    ImGui::SetNextTreeNodeIcon(desc.icon.empty() ? nullptr
                                                 : desc.icon.c_str());
    desc.RenderSelf(entity);
    ImGui::SetNextTreeNodeIcon(nullptr);

    const ImGui::ComponentHeaderState state = ImGui::GetComponentHeaderState();
    // Strip the trailing ItemSpacing that ItemSize added, then add an
    // explicit drawer bottom pad so the box hugs the last inner widget.
    const float content_end_y =
        ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y;
    const float bottom_pad = (state.header_rendered && state.open)
                                 ? ImGui::GetStyle().WindowPadding.y
                                 : 0.0f;
    const float bottom_y = content_end_y + bottom_pad;

    splitter.SetCurrentChannel(dl, 0);
    const ImVec2 win_pos = ImGui::GetWindowPos();
    const ImVec2 win_size = ImGui::GetWindowSize();
    // Box spans the full window width, ignoring WindowPadding.
    const float left_x = win_pos.x;
    const float right_x = win_pos.x + win_size.x;
    const ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

    if (state.header_rendered && state.open) {
      // Fixed drawer bg matches the engine's input drawer color.
      const ImU32 drawer_col = IM_COL32(0x1a, 0x18, 0x17, 0xff);
      dl->AddRectFilled(ImVec2(left_x, state.header_bottom_y),
                        ImVec2(right_x, bottom_y), drawer_col);
    }

    if (state.header_rendered) {
      dl->AddLine(ImVec2(left_x, top_y), ImVec2(right_x, top_y),
                  border_col, 1.0f);
      dl->AddLine(ImVec2(left_x, top_y), ImVec2(left_x, bottom_y),
                  border_col, 1.0f);
      dl->AddLine(ImVec2(right_x, top_y), ImVec2(right_x, bottom_y),
                  border_col, 1.0f);
      dl->AddLine(ImVec2(left_x, bottom_y), ImVec2(right_x, bottom_y),
                  border_col, 1.0f);
    }

    splitter.Merge(dl);

    // Start the next component flush with this one's bottom border so
    // adjacent components share a single 1px divider instead of a gap.
    if (state.header_rendered) {
      ImVec2 cur = ImGui::GetCursorScreenPos();
      ImGui::SetCursorScreenPos(ImVec2(cur.x, bottom_y));
    }
  }
}

std::string_view GetEntityIcon(Entity entity) {
  const ComponentDesc* best = nullptr;
  for (const auto& [ti, desc] : RegistryMap()) {
    if (desc.icon.empty()) {
      continue;
    }
    if (!desc.HasComponent(entity)) {
      continue;
    }
    if (!best || desc.icon_priority >= best->icon_priority) {
      best = &desc;
    }
  }
  return best ? std::string_view(best->icon) : std::string_view();
}

void RenderModals(Entity entity) {
  for (const auto& ti : RegistryOrder()) {
    RegistryMap().at(ti).RenderModal(entity);
  }
}

void RenderAddPopup(Entity entity) {
  static char search_buf[128] = "";

  ImGui::SetNextItemWidth(-1);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
    search_buf[0] = '\0';
  }
  ImGui::InputTextWithHint("##component_search", "Search...", search_buf,
                           sizeof(search_buf));

  std::string filter(search_buf);
  // Lowercase for case-insensitive matching
  std::string filter_lower = filter;
  for (auto& c : filter_lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  bool has_filter = !filter_lower.empty();

  // Collect groups in consistent order. Paired with Lucide icons that are
  // prepended to the BeginMenu label.
  struct GroupEntry {
    const char* name;
    const char* icon;
  };
  static const std::vector<GroupEntry> group_order = {
      {"Rendering",  ICON_LC_IMAGE},
      {"Lighting",   ICON_LC_SUN},
      {"Audio",      ICON_LC_VOLUME_2},
      {"Physics",    ICON_LC_WEIGHT},
      {"UI",         ICON_LC_HAND},
      {"Canvas",     ICON_LC_FRAME},
      {"Scripting",  ICON_LC_CODE},
      {"Networking", ICON_LC_NETWORK},
  };

  auto stage_icon = [](const ComponentDesc& desc) {
    ImGui::SetNextMenuItemIcon(desc.icon.empty() ? nullptr
                                                 : desc.icon.c_str());
  };

  if (has_filter) {
    // Flat filtered list (no groups)
    for (const auto& ti_o : RegistryOrder()) {
      const auto& desc = RegistryMap().at(ti_o);
      if (!desc.RenderAdd || desc.display_name.empty()) {
        continue;
      }
      std::string name_lower = desc.display_name;
      for (auto& c : name_lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      std::string group_lower = desc.group;
      for (auto& c : group_lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if (name_lower.find(filter_lower) != std::string::npos ||
          group_lower.find(filter_lower) != std::string::npos) {
        stage_icon(desc);
        desc.RenderAdd(entity);
      }
    }
  } else {
    // Grouped view
    for (const auto& group : group_order) {
      bool has_items = false;
      for (const auto& ti_o : RegistryOrder()) {
        const auto& desc = RegistryMap().at(ti_o);
        if (desc.group == group.name && desc.RenderAdd) {
          has_items = true;
          break;
        }
      }
      if (!has_items) {
        continue;
      }

      std::string menu_label =
          std::string(group.icon) + "  " + group.name;
      if (ImGui::BeginMenu(menu_label.c_str())) {
        for (const auto& ti_o : RegistryOrder()) {
          const auto& desc = RegistryMap().at(ti_o);
          if (desc.group == group.name && desc.RenderAdd) {
            stage_icon(desc);
            desc.RenderAdd(entity);
          }
        }
        ImGui::EndMenu();
      }
    }
  }
}
void InitializeScriptFieldRenderers() {
  auto set_render = [](const char* type_name, ScriptFieldRenderFn fn) {
    auto* desc = ScriptFieldTypeRegistry::Find(type_name);
    if (desc) {
      desc->Render = std::move(fn);
    }
  };

  set_render("System.Int32",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        int32_t val = 0;
        mono_field_get_value(obj, field, &val);
        if (ImGui::DragInt(label.c_str(), &val, 1)) {
          if (prop) {
            void* args[1] = {&val};
            mono_property_set_value(prop, obj, args, nullptr);
          } else {
            mono_field_set_value(obj, field, &val);
          }
          return true;
        }
        return false;
      });

  set_render("System.Single",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        float val = 0.0f;
        mono_field_get_value(obj, field, &val);
        if (ImGui::DragFloat(label.c_str(), &val, 0.1f)) {
          if (prop) {
            void* args[1] = {&val};
            mono_property_set_value(prop, obj, args, nullptr);
          } else {
            mono_field_set_value(obj, field, &val);
          }
          return true;
        }
        return false;
      });

  set_render("System.Boolean",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        MonoBoolean val = 0;
        mono_field_get_value(obj, field, &val);
        bool b = val != 0;
        if (ImGui::Checkbox(label.c_str(), &b)) {
          MonoBoolean mb = b ? 1 : 0;
          if (prop) {
            void* args[1] = {&mb};
            mono_property_set_value(prop, obj, args, nullptr);
          } else {
            mono_field_set_value(obj, field, &mb);
          }
          return true;
        }
        return false;
      });

  set_render("System.String",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        MonoString* mono_str = nullptr;
        mono_field_get_value(obj, field, &mono_str);
        std::string str;
        if (mono_str) {
          char* cstr = mono_string_to_utf8(mono_str);
          str = cstr;
          mono_free(cstr);
        }
        if (ImGui::InputText(label.c_str(), &str)) {
          MonoString* new_val = mono_string_new(
              Engine::script_manager().app_domain(), str.c_str());
          if (prop) {
            void* args[1] = {new_val};
            mono_property_set_value(prop, obj, args, nullptr);
          } else {
            mono_field_set_value(obj, field, new_val);
          }
          return true;
        }
        return false;
      });

  // Entity reference
  set_render("WieselEngine.Entity",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        MonoObject* entity_obj = nullptr;
        mono_field_get_value(obj, field, &entity_obj);
        std::string entity_label = "(None)";
        entt::entity current_entity_id = entt::null;

        if (entity_obj) {
          MonoClassField* id_field = mono_class_get_field_from_name(
              Engine::script_manager().entity_class(), "entityId");
          if (id_field) {
            uint64_t id_val = 0;
            mono_field_get_value(entity_obj, id_field, &id_val);
            current_entity_id = static_cast<entt::entity>(id_val);
          }
        }

        ImGui::InputText(label.c_str(), &entity_label,
                         ImGuiInputTextFlags_ReadOnly);

        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
            auto* ref = static_cast<const EntityRef*>(payload->Data);
            Entity resolved = ref->Resolve();
            if (resolved) {
              MonoObject* new_entity =
                  mono_object_new(Engine::script_manager().app_domain(),
                                  Engine::script_manager().entity_class());
              MonoMethod* ctor = mono_class_get_method_from_name(
                  Engine::script_manager().entity_class(), ".ctor", 2);
              uint64_t scene_ptr =
                  reinterpret_cast<uint64_t>(resolved.GetScene());
              uint64_t entity_id = static_cast<uint64_t>(resolved.handle());
              void* args[2] = {&scene_ptr, &entity_id};
              mono_runtime_invoke(ctor, new_entity, args, nullptr);
              mono_field_set_value(obj, field, new_entity);
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (current_entity_id != entt::null) {
          ImGui::SameLine();
          std::string clear_id = "X##clear_" + label;
          if (ImGui::SmallButton(clear_id.c_str())) {
            MonoObject* null_val = nullptr;
            mono_field_set_value(obj, field, &null_val);
            return true;
          }
        }
        return false;
      });

  // Prefab
  set_render("WieselEngine.Prefab",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        MonoObject* prefab_obj = nullptr;
        mono_field_get_value(obj, field, &prefab_obj);
        std::string prefab_label = "(None)";

        if (prefab_obj) {
          MonoClassField* handle_field = mono_class_get_field_from_name(
              Engine::script_manager().prefab_class(), "handle");
          if (handle_field) {
            MonoString* handle_str = nullptr;
            mono_field_get_value(prefab_obj, handle_field, &handle_str);
            if (handle_str) {
              const char* cstr = mono_string_to_utf8(handle_str);
              if (cstr && cstr[0]) {
                AssetHandle h = AssetHandle::FromString(cstr);
                const AssetMetadata* meta =
                    Engine::asset_manager().GetMetadata(h);
                if (meta) {
                  prefab_label =
                      VirtualFileSystem::Stem(meta->virtual_source_path);
                } else {
                  prefab_label = cstr;
                }
              }
              mono_free((void*)cstr);
            }
          }
        }

        ImGui::InputText(label.c_str(), &prefab_label,
                         ImGuiInputTextFlags_ReadOnly);

        bool changed = false;
        if (ImGui::BeginDragDropTarget()) {
          AssetHandle dropped_handle;

          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("AssetHandle")) {
            AssetHandle dropped =
                *static_cast<const AssetHandle*>(payload->Data);
            const AssetMetadata* meta =
                Engine::asset_manager().GetMetadata(dropped);
            if (meta && meta->type == AssetType::Prefab) {
              dropped_handle = dropped;
            }
          } else if (const ImGuiPayload* payload =
                         ImGui::AcceptDragDropPayload("BrowserFile")) {
            std::string file_path(static_cast<const char*>(payload->Data));
            if (file_path.ends_with(".wprefab")) {
              auto physical_app = Engine::vfs()->GetPhysicalPath("app://");
              if (physical_app.has_value()) {
                std::filesystem::path rel =
                    std::filesystem::relative(file_path, *physical_app);
                std::string vfs_path = "app://" + rel.generic_string();
                dropped_handle =
                    Engine::asset_manager().FindBySourcePath(vfs_path);
              }
            }
          }

          if (dropped_handle.IsValid()) {
            MonoObject* new_prefab = prefab_obj;
            if (!new_prefab) {
              new_prefab =
                  mono_object_new(Engine::script_manager().app_domain(),
                                  Engine::script_manager().prefab_class());
              mono_runtime_object_init(new_prefab);
            }
            MonoClassField* handle_field = mono_class_get_field_from_name(
                Engine::script_manager().prefab_class(), "handle");
            if (handle_field) {
              std::string handle_str = dropped_handle.ToString();
              MonoString* mono_val = mono_string_new(
                  Engine::script_manager().app_domain(), handle_str.c_str());
              mono_field_set_value(new_prefab, handle_field, mono_val);
            }
            mono_field_set_value(obj, field, new_prefab);
            changed = true;
          }
          ImGui::EndDragDropTarget();
        }

        if (prefab_label != "(None)") {
          ImGui::SameLine();
          std::string clear_id = "X##clear_" + label;
          if (ImGui::SmallButton(clear_id.c_str())) {
            MonoObject* null_val = nullptr;
            mono_field_set_value(obj, field, &null_val);
            return true;
          }
        }
        return changed;
      });

  // AudioClip
  set_render("WieselEngine.AudioClip",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        MonoObject* clip_obj = nullptr;
        mono_field_get_value(obj, field, &clip_obj);
        std::string display = "(None)";
        std::string current_handle_str;

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
                if (meta) {
                  display = meta->name;
                }
              }
              mono_free((void*)cstr);
            }
          }
        }

        ImGui::InputText(label.c_str(), &display, ImGuiInputTextFlags_ReadOnly);

        bool changed = false;
        if (ImGui::BeginDragDropTarget()) {
          AssetHandle dropped_handle = AcceptAssetDragDrop(AssetType::Audio);
          if (dropped_handle.IsValid()) {
            MonoObject* new_clip = clip_obj;
            if (!new_clip) {
              new_clip = mono_object_new(
                  Engine::script_manager().app_domain(),
                  Engine::script_manager().audio_clip_class());
              mono_runtime_object_init(new_clip);
            }
            if (new_clip) {
              MonoClassField* handle_field = mono_class_get_field_from_name(
                  mono_object_get_class(new_clip), "handle");
              if (handle_field) {
                MonoString* h_val =
                    mono_string_new(Engine::script_manager().app_domain(),
                                    dropped_handle.ToString().c_str());
                mono_field_set_value(new_clip, handle_field, h_val);
              }
              mono_field_set_value(obj, field, new_clip);
              changed = true;
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (!current_handle_str.empty()) {
          ImGui::SameLine();
          std::string clear_id = "X##clear_" + label;
          if (ImGui::SmallButton(clear_id.c_str())) {
            MonoObject* null_val = nullptr;
            mono_field_set_value(obj, field, &null_val);
            return true;
          }
        }
        return changed;
      });

  set_render("WieselEngine.Font",
      [](MonoObject* obj, MonoClassField* field, MonoProperty* prop,
         const std::string& label) -> bool {
        MonoObject* font_obj = nullptr;
        mono_field_get_value(obj, field, &font_obj);
        std::string display = "(None)";
        std::string current_handle_str;

        if (font_obj) {
          MonoClassField* handle_field = mono_class_get_field_from_name(
              mono_object_get_class(font_obj), "handle");
          if (handle_field) {
            MonoString* h_str = nullptr;
            mono_field_get_value(font_obj, handle_field, &h_str);
            if (h_str) {
              const char* cstr = mono_string_to_utf8(h_str);
              if (cstr && cstr[0]) {
                current_handle_str = cstr;
                AssetHandle h = AssetHandle::FromString(current_handle_str);
                const auto* meta = Engine::asset_manager().GetMetadata(h);
                if (meta) {
                  display = meta->name;
                }
              }
              mono_free((void*)cstr);
            }
          }
        }

        ImGui::InputText(label.c_str(), &display, ImGuiInputTextFlags_ReadOnly);

        bool changed = false;
        if (ImGui::BeginDragDropTarget()) {
          AssetHandle dropped_handle = AcceptAssetDragDrop(AssetType::Font);
          if (dropped_handle.IsValid()) {
            MonoObject* new_font = font_obj;
            if (!new_font) {
              new_font = mono_object_new(
                  Engine::script_manager().app_domain(),
                  Engine::script_manager().font_class());
              mono_runtime_object_init(new_font);
            }
            if (new_font) {
              MonoClassField* handle_field = mono_class_get_field_from_name(
                  mono_object_get_class(new_font), "handle");
              if (handle_field) {
                MonoString* h_val =
                    mono_string_new(Engine::script_manager().app_domain(),
                                    dropped_handle.ToString().c_str());
                mono_field_set_value(new_font, handle_field, h_val);
              }
              mono_field_set_value(obj, field, new_font);
              changed = true;
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (!current_handle_str.empty()) {
          ImGui::SameLine();
          std::string clear_id = "X##clear_" + label;
          if (ImGui::SmallButton(clear_id.c_str())) {
            MonoObject* null_val = nullptr;
            mono_field_set_value(obj, field, &null_val);
            return true;
          }
        }
        return changed;
      });
}

}  // namespace wiesel