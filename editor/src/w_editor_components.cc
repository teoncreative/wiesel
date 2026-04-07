
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

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <typeindex>
#include "../../wiesel/include/asset/w_sprite_loader.h"
#include "animation/w_animation.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_serializer.h"
#include "audio/w_audio.h"
#include "behavior/w_behavior.h"
#include "behavior/w_native_behavior.h"
#include "mono_wrappers.h"
#include "physics/w_collider.h"
#include "physics/w_mesh_collider_asset.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh.h"
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

namespace Wiesel {

// Command stack pointer set by the editor each frame for undo/redo tracking.
static Editor::CommandStack* s_command_stack = nullptr;

void SetInspectorCommandStack(Editor::CommandStack* stack) {
  s_command_stack = stack;
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
    float max_preview = 256.0f;
    float aspect =
        (tex->width_ > 0 && tex->height_ > 0)
            ? static_cast<float>(tex->width_) / static_cast<float>(tex->height_)
            : 1.0f;
    ImVec2 preview_size = (aspect >= 1.0f)
                              ? ImVec2(max_preview, max_preview / aspect)
                              : ImVec2(max_preview * aspect, max_preview);
    ImGui::Image(reinterpret_cast<ImTextureID>(desc), preview_size);
    ImGui::EndTooltip();
  }
}

// Shared drag-drop handler: accepts AssetHandle or BrowserFile payloads,
// auto-imports if needed, returns a valid handle or null.
static AssetHandle AcceptAssetDragDrop(AssetType required_type) {
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

  ImGui::Text("%s", label);
  ImGui::SameLine();
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
      static Editor::UndoTracker<glm::vec3> pos_tracker;
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
      static Editor::UndoTracker<glm::vec3> rot_tracker;
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
      static Editor::UndoTracker<glm::vec3> scale_tracker;
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

void RenderComponentImGui(ModelComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Model", &visible)) {
    auto& model = entity.GetComponent<ModelComponent>();
    auto& assets = Engine::asset_manager();

    // Asset selector: show current asset name + dropdown to pick from registered model assets
    const AssetMetadata* currentMeta =
        model.model_handle.IsValid() ? assets.GetMetadata(model.model_handle)
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
        if (!meta) {
          continue;
        }
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
        case AssetLoadState::Unloaded:
          stateStr = "Unloaded";
          break;
        case AssetLoadState::Loading:
          stateStr = "Loading...";
          break;
        case AssetLoadState::Loaded:
          stateStr = "Loaded";
          break;
        case AssetLoadState::Failed:
          stateStr = "Failed";
          break;
      }
      ImGui::TextDisabled("Status: %s", stateStr);
    }

    ImGui::Checkbox("Receive Shadows", &model.receive_shadows);
    if (s_command_stack) {
      static Editor::UndoTracker<bool> recv_shadow_tracker;
      recv_shadow_tracker.Track(
          *s_command_stack, "Toggle Receive Shadows", model.receive_shadows,
          [entity](const bool& v) mutable {
            entity.GetComponent<ModelComponent>().receive_shadows = v;
          });
    }
    ImGui::Checkbox("Render", &model.enable_rendering);
    if (s_command_stack) {
      static Editor::UndoTracker<bool> render_tracker;
      render_tracker.Track(
          *s_command_stack, "Toggle Render", model.enable_rendering,
          [entity](const bool& v) mutable {
            entity.GetComponent<ModelComponent>().enable_rendering = v;
          });
    }

    // Per-mesh material slots
    if (model.model_handle.IsValid()) {
      const std::shared_ptr<Model>& model_data =
          assets.GetOrLoad<Model>(model.model_handle);
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
            AssetHandle mat_handle =
                model.material_slot_handles[i].IsValid()
                    ? model.material_slot_handles[i]
                    : model_data->meshes[i]->material_handle;
            inst->base_material_handle = mat_handle;
            model.material_instances[i] = inst;
          }
        }

        if (ImGui::TreeNode("Materials")) {
          for (size_t i = 0; i < model.material_instances.size(); i++) {
            auto& inst = model.material_instances[i];
            if (!inst) {
              continue;
            }

            ImGui::PushID(static_cast<int>(i));

            auto base = inst->GetBaseMaterial();
            std::string slot_label = "Slot " + std::to_string(i);
            if (base) {
              if (!base->name.empty()) {
                slot_label = base->name;
              }
              if (base->base_texture.HasTexture()) {
                slot_label += " (textured)";
              }
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
              if (ImGui::ColorEdit4(PrefixLabel("Color Tint").c_str(),
                                    &tint.x)) {
                inst->SetColorTint(tint);
              }
              if (s_command_stack) {
                static Editor::UndoTracker<glm::vec4> tracker;
                auto inst_weak = std::weak_ptr<MaterialInstance>(inst);
                tracker.Track(*s_command_stack, "Change Color Tint", tint,
                              [inst_weak](const glm::vec4& v) {
                                if (auto p = inst_weak.lock()) {
                                  p->SetColorTint(v);
                                }
                              });
              }

              float roughness = inst->GetRoughness();
              if (ImGui::SliderFloat(PrefixLabel("Roughness").c_str(),
                                     &roughness, 0.0f, 1.0f)) {
                inst->SetRoughness(roughness);
              }
              if (s_command_stack) {
                static Editor::UndoTracker<float> tracker;
                auto inst_weak = std::weak_ptr<MaterialInstance>(inst);
                tracker.Track(*s_command_stack, "Change Roughness", roughness,
                              [inst_weak](const float& v) {
                                if (auto p = inst_weak.lock()) {
                                  p->SetRoughness(v);
                                }
                              });
              }

              float metallic = inst->GetMetallic();
              if (ImGui::SliderFloat(PrefixLabel("Metallic").c_str(), &metallic,
                                     0.0f, 1.0f)) {
                inst->SetMetallic(metallic);
              }
              if (s_command_stack) {
                static Editor::UndoTracker<float> tracker;
                auto inst_weak = std::weak_ptr<MaterialInstance>(inst);
                tracker.Track(*s_command_stack, "Change Metallic", metallic,
                              [inst_weak](const float& v) {
                                if (auto p = inst_weak.lock()) {
                                  p->SetMetallic(v);
                                }
                              });
              }

              float specular = inst->GetSpecular();
              if (ImGui::SliderFloat(PrefixLabel("Specular").c_str(), &specular,
                                     0.0f, 1.0f)) {
                inst->SetSpecular(specular);
              }
              if (s_command_stack) {
                static Editor::UndoTracker<float> tracker;
                auto inst_weak = std::weak_ptr<MaterialInstance>(inst);
                tracker.Track(*s_command_stack, "Change Specular", specular,
                              [inst_weak](const float& v) {
                                if (auto p = inst_weak.lock()) {
                                  p->SetSpecular(v);
                                }
                              });
              }

              // Show base material texture previews
              if (base) {
                if (ImGui::TreeNode("Textures")) {
                  std::shared_ptr<Texture> tex;
                  base->base_texture.Resolve(tex);
                  RenderTexturePreview("Diffuse", tex.get());
                  base->normal_map.Resolve(tex);
                  RenderTexturePreview("Normal", tex.get());
                  base->roughness_map.Resolve(tex);
                  RenderTexturePreview("Roughness", tex.get());
                  base->metallic_map.Resolve(tex);
                  RenderTexturePreview("Metallic", tex.get());
                  base->specular_map.Resolve(tex);
                  RenderTexturePreview("Specular", tex.get());
                  ImGui::TreePop();
                }
              }

              if (inst->HasOverride("color_tint") ||
                  inst->HasOverride("roughness") ||
                  inst->HasOverride("metallic") ||
                  inst->HasOverride("specular")) {
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
    Engine::renderer()->GetDeletionQueue().Push(
        [ubos = std::move(deferred_ubos), geo = std::move(deferred_geo),
         shadow = std::move(deferred_shadow),
         bone_ubo = std::move(deferred_bone_ubo),
         bone_desc = std::move(deferred_bone_desc),
         uniform = std::move(deferred_uniform)]() {
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
    if (s_command_stack) {
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<glm::vec3> tracker;
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
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Point Light", &visible)) {
    ImGui::DragFloat(PrefixLabel("Ambient").c_str(),
                     &component.light_data.base.ambient, 0.01f);
    if (s_command_stack) {
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
        tracker.Track(
            *s_command_stack, "Change Attenuation Linear",
            component.light_data.linear, [entity](const float& v) mutable {
              entity.GetComponent<LightPointComponent>().light_data.linear = v;
            });
      }

      ImGui::DragFloat(PrefixLabel("Exp").c_str(), &component.light_data.exp,
                       0.1f);
      if (s_command_stack) {
        static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<glm::vec3> tracker;
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
  static bool visible = true;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<glm::vec4> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<bool> tracker;
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
  for (FieldData& value : data.fields() | std::views::values) {
    if (value.field_type() == FieldType::Float) {
      float_t val = value.Get<float_t>(instance->handle());
      if (ImGui::DragFloat(PrefixLabel(value.formatted_name().c_str()).c_str(),
                           &val, 0.1f)) {
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
      if (ImGui::InputText(PrefixLabel(value.formatted_name().c_str()).c_str(),
                           &str)) {
        MonoString* newVal =
            mono_string_new(Engine::script_manager().app_domain(), str.c_str());
        value.Set(instance->handle(), newVal);
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
            entity_label =
                scene->GetComponent<TagComponent>(current_entity_id).name;
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
            MonoObject* new_entity =
                mono_object_new(Engine::script_manager().app_domain(),
                                Engine::script_manager().entity_class());
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
      ImGui::InputText(label.c_str(), &prefab_label,
                       ImGuiInputTextFlags_ReadOnly);

      // Drag-drop target: accept prefab assets
      // TODO replace with assest handle and use our common code AcceptAssetDragDrop
      if (ImGui::BeginDragDropTarget()) {
        std::string dropped_path;

        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("AssetHandle")) {
          AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
          const AssetMetadata* meta =
              Engine::asset_manager().GetMetadata(dropped);
          if (meta && meta->type == AssetType::Prefab) {
            dropped_path = meta->virtual_source_path;
          }
        } else if (const ImGuiPayload* payload =
                       ImGui::AcceptDragDropPayload("BrowserFile")) {
          std::string file_path(static_cast<const char*>(payload->Data));
          if (file_path.ends_with(".wprefab")) {
            // Convert physical path to VFS path via app:// mount
            auto physical_app = Engine::vfs()->GetPhysicalPath("app://");
            if (physical_app.has_value()) {
              std::filesystem::path rel =
                  std::filesystem::relative(file_path, *physical_app);
              dropped_path = "app://" + rel.generic_string();
            }
          }
        }

        if (!dropped_path.empty()) {
          MonoObject* new_prefab = prefab_obj;
          if (!new_prefab) {
            new_prefab =
                mono_object_new(Engine::script_manager().app_domain(),
                                Engine::script_manager().prefab_class());
            mono_runtime_object_init(new_prefab);
          }
          MonoClassField* path_field = mono_class_get_field_from_name(
              Engine::script_manager().prefab_class(), "path");
          if (path_field) {
            MonoString* path_val = mono_string_new(
                Engine::script_manager().app_domain(), dropped_path.c_str());
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
              if (meta) {
                display = meta->name;
              }
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
            obj = mono_object_new(Engine::script_manager().app_domain(),
                                  Engine::script_manager().audio_clip_class());
            mono_runtime_object_init(obj);
          }
          if (obj) {
            MonoClassField* handle_field = mono_class_get_field_from_name(
                mono_object_get_class(obj), "handle");
            if (handle_field) {
              MonoString* h_val =
                  mono_string_new(Engine::script_manager().app_domain(),
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
                                  IBehavior& behavior, Entity entity) {
  static bool visible = true;
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
  static bool visible = true;
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
  static bool visible = true;
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
  static bool visible = true;
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
  static bool visible = true;
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

    ImGui::Checkbox(PrefixLabel("One Way").c_str(), &component.is_one_way);

    // Bake from model button (when no baked asset or to create one)
    if (entity.HasComponent<ModelComponent>()) {
      auto& model_comp = entity.GetComponent<ModelComponent>();
      if (model_comp.model_handle.IsValid()) {
        if (ImGui::Button("Bake from Model")) {
          auto data = BakeMeshColliderFromModel(model_comp.model_handle);
          if (data) {
            const auto* model_meta =
                Engine::asset_manager().GetMetadata(model_comp.model_handle);
            std::string name =
                model_meta ? model_meta->name + "_collider" : "mesh_collider";
            std::string dir;
            if (model_meta) {
              std::string src = model_meta->virtual_source_path;
              size_t sl = src.rfind('/');
              dir = (sl != std::string::npos) ? src.substr(0, sl) : "app://";
            } else {
              dir = "app://";
            }
            std::string vfs_path = dir + "/" + name + ".wmeshcol";
            AssetHandle handle =
                AssetSerializerRegistry::Create<MeshColliderAssetData>(
                    name, AssetType::MeshCollider, vfs_path, data);
            if (handle.IsValid()) {
              AssetSerializerRegistry::Save(handle);
              component.collider_handle = handle;
            }
          }
        }
      }
    }

    if (ImGui::Button("Regenerate Collider")) {
      // Re-bake asset if one is assigned
      if (component.collider_handle.IsValid()) {
        auto baked = Engine::asset_manager().Get<MeshColliderAssetData>(
            component.collider_handle);
        if (baked && baked->source_model.IsValid()) {
          auto new_data = BakeMeshColliderFromModel(baked->source_model);
          if (new_data) {
            Engine::asset_manager().Store(component.collider_handle, new_data);
            AssetSerializerRegistry::Save(component.collider_handle);
          }
        }
      }
      auto& physics = entity.GetScene()->GetPhysicsWorld();
      physics.DestroyBody(entity.handle());
      physics.CreateBody(entity.handle());
    }
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
  static bool visible = true;
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
  static bool visible = true;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
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
        static Editor::UndoTracker<float> tracker;
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
      static Editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Position X",
          component.lock_position_x, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_position_x = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Y##fp", &component.lock_position_y);
    if (s_command_stack) {
      static Editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Position Y",
          component.lock_position_y, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_position_y = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Z##fp", &component.lock_position_z);
    if (s_command_stack) {
      static Editor::UndoTracker<bool> tracker;
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
      static Editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Rotation X",
          component.lock_rotation_x, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_rotation_x = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Y##fr", &component.lock_rotation_y);
    if (s_command_stack) {
      static Editor::UndoTracker<bool> tracker;
      tracker.Track(
          *s_command_stack, "Toggle Freeze Rotation Y",
          component.lock_rotation_y, [entity](const bool& v) mutable {
            entity.GetComponent<RigidBodyComponent>().lock_rotation_y = v;
          });
    }
    ImGui::SameLine();
    ImGui::Checkbox("Z##fr", &component.lock_rotation_z);
    if (s_command_stack) {
      static Editor::UndoTracker<bool> tracker;
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
  static bool visible = true;
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
  static bool visible = true;
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
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Canvas Scaler", &visible)) {
    // Check if the parent canvas is WorldSpace
    bool is_world_space = false;
    if (entity.HasComponent<CanvasComponent>()) {
      is_world_space = entity.GetComponent<CanvasComponent>().render_mode ==
                       CanvasRenderMode::WorldSpace;
    }

    if (is_world_space) {
      // WorldSpace mode: show pixels-per-unit instead of scale mode
      ImGui::InputFloat2(
          PrefixLabel("Reference Resolution").c_str(),
          reinterpret_cast<float*>(&component.reference_resolution), "%.0f",
          ImGuiInputTextFlags_EnterReturnsTrue);
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
        ImGui::InputFloat2(
            PrefixLabel("Reference Resolution").c_str(),
            reinterpret_cast<float*>(&component.reference_resolution), "%.0f",
            ImGuiInputTextFlags_EnterReturnsTrue);
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
    TextureDropField("Texture", component.texture_handle);

    ImGui::ColorEdit4(PrefixLabel("Tint").c_str(),
                      reinterpret_cast<float*>(&component.tint));
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
    if (AssetDropField("Font", AssetType::Font, component.font_handle)) {
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

    ImGui::SeparatorText("Shadow");
    if (ImGui::Checkbox(PrefixLabel("Shadow").c_str(), &component.shadow)) {
      component.gpu_dirty_ = true;
    }
    if (component.shadow) {
      ImGui::DragFloat2(PrefixLabel("Offset").c_str(),
                        reinterpret_cast<float*>(&component.shadow_offset),
                        0.5f);
      ImGui::ColorEdit4(PrefixLabel("Shadow Color").c_str(),
                        reinterpret_cast<float*>(&component.shadow_color));
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
    EnsureRectangleTransform(entity, true);
  }
}

void RenderAddComponentImGui_RectangleTransformComponent(Entity entity) {
  if (ImGui::MenuItem("Rectangle Transform")) {
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

void RenderComponentImGui(TextInputComponent& component, Entity entity) {
  static bool visible = true;
  if (ImGui::ClosableTreeNode("Text Input", &visible)) {
    char buf[256];
    strncpy(buf, component.placeholder.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(PrefixLabel("Placeholder").c_str(), buf,
                         sizeof(buf))) {
      component.placeholder = buf;
    }
    ImGui::InputInt(PrefixLabel("Max Length").c_str(), &component.max_length);
    ImGui::ColorEdit4(PrefixLabel("Cursor Color").c_str(),
                      reinterpret_cast<float*>(&component.cursor_color));
    ImGui::ColorEdit4(PrefixLabel("Placeholder Color").c_str(),
                      reinterpret_cast<float*>(&component.placeholder_color));
    ImGui::TreePop();
  }
  if (!visible) {
    entity.RemoveComponent<TextInputComponent>();
    visible = true;
  }
}

void RenderAddComponentImGui_TextInputComponent(Entity entity) {
  if (ImGui::MenuItem("Text Input")) {
    entity.AddComponent<TextInputComponent>();
    EnsureRectangleTransform(entity);
    if (!entity.HasComponent<TextComponent>()) {
      entity.AddComponent<TextComponent>();
    }
    if (!entity.HasComponent<InteractableComponent>()) {
      entity.AddComponent<InteractableComponent>();
    }
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
        model_ptr =
            Engine::asset_manager().GetOrLoad<Model>(model_comp.model_handle);
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
                                    ? "(None)"
                                    : component.current_state_name;
      ImGui::Text("State: %s", state_label.c_str());

      if (component.is_blending) {
        ImGui::ProgressBar(component.blend_weight, ImVec2(-1, 0),
                           "Blending...");
      }

      ImGui::DragFloat(PrefixLabel("State Time").c_str(), &component.state_time,
                       0.1f, 0.0f, 10000.0f);

      // Default state
      if (ImGui::BeginCombo(PrefixLabel("Default State").c_str(),
                            component.controller.default_state.c_str())) {
        for (const auto& state : component.controller.states) {
          bool selected = (component.controller.default_state == state.name);
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
          new_state.name =
              "State" + std::to_string(component.controller.states.size());
          component.controller.states.push_back(new_state);
        }
        ImGui::TreePop();
      }

      // Transitions
      if (ImGui::TreeNode("Transitions")) {
        for (size_t i = 0; i < component.controller.transitions.size(); i++) {
          auto& trans = component.controller.transitions[i];
          ImGui::PushID(static_cast<int>(i));
          std::string label =
              (trans.from_state.empty() ? "Any" : trans.from_state) + " -> " +
              trans.to_state;
          if (ImGui::TreeNode(label.c_str())) {
            // From state combo
            if (ImGui::BeginCombo("From", trans.from_state.empty()
                                              ? "(Any)"
                                              : trans.from_state.c_str())) {
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
            ImGui::DragFloat("Blend Duration", &trans.blend_duration, 0.01f,
                             0.0f, 5.0f);

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
                trans.conditions.erase(trans.conditions.begin() +
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
                                  ? "(None)"
                                  : component.current_clip_name;
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

struct ComponentDesc {
  std::string display_name;
  std::string group;
  std::function<void(Entity)> RenderSelf;
  std::function<void(Entity)> RenderAdd;
  std::function<void(Entity)> RenderModal;
  std::function<bool(Entity)> HasComponent;
};

std::map<std::type_index, ComponentDesc> kRegistry;

template <typename T>
void RegisterComponentType(const std::string& display_name,
                           const std::string& group,
                           void (*renderSelf)(T&, Entity),
                           void (*renderAdd)(Entity),
                           void (*renderModal)(Entity)) {
  auto ti = std::type_index(typeid(T));
  kRegistry[ti] =
      ComponentDesc{display_name,
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
                    [](Entity entity) { return entity.HasComponent<T>(); }};
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
    static Editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Volume", component.volume,
                  [entity](const float& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().volume = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Pitch").c_str(), &component.pitch, 0.1f,
                     3.0f);
  if (s_command_stack) {
    static Editor::UndoTracker<float> tracker;
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
    static Editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Spatial Blend",
                  component.spatial_blend, [entity](const float& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().spatial_blend =
                        v;
                  });
  }

  ImGui::Checkbox(PrefixLabel("Loop").c_str(), &component.loop);
  if (s_command_stack) {
    static Editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Loop", component.loop,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().loop = v;
                  });
  }

  ImGui::Checkbox(PrefixLabel("Play On Start").c_str(),
                  &component.play_on_start);
  if (s_command_stack) {
    static Editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Play On Start",
                  component.play_on_start, [entity](const bool& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().play_on_start =
                        v;
                  });
  }

  ImGui::Checkbox(PrefixLabel("Mute").c_str(), &component.mute);
  if (s_command_stack) {
    static Editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Mute", component.mute,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<AudioSourceComponent>().mute = v;
                  });
  }

  if (component.spatial_blend > 0.0f) {
    ImGui::DragFloat(PrefixLabel("Min Distance").c_str(),
                     &component.min_distance, 0.1f, 0.0f, 1000.0f);
    if (s_command_stack) {
      static Editor::UndoTracker<float> tracker;
      tracker.Track(*s_command_stack, "Change Min Distance",
                    component.min_distance, [entity](const float& v) mutable {
                      entity.GetComponent<AudioSourceComponent>().min_distance =
                          v;
                    });
    }

    ImGui::DragFloat(PrefixLabel("Max Distance").c_str(),
                     &component.max_distance, 1.0f, 0.0f, 10000.0f);
    if (s_command_stack) {
      static Editor::UndoTracker<float> tracker;
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
  static bool visible = true;
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
            std::make_unique<Editor::PropertyCommand<AssetHandle>>(
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
    static Editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Flip X", component.flip_x_,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().flip_x_ = v;
                  });
  }

  ImGui::SameLine();
  ImGui::Checkbox("Flip Y", &component.flip_y_);
  if (s_command_stack) {
    static Editor::UndoTracker<bool> tracker;
    tracker.Track(*s_command_stack, "Toggle Flip Y", component.flip_y_,
                  [entity](const bool& v) mutable {
                    entity.GetComponent<SpriteRendererComponent>().flip_y_ = v;
                  });
  }

  ImGui::ColorEdit4(PrefixLabel("Tint").c_str(), &component.tint_.r);
  if (s_command_stack) {
    static Editor::UndoTracker<glm::vec4> tracker;
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
    static Editor::UndoTracker<int> tracker;
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
    static Editor::UndoTracker<glm::vec2> tracker;
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

void RenderComponentImGui(SpriteAnimatorComponent& component, Entity entity) {
  static bool visible = true;
  if (!ImGui::ClosableTreeNode("Sprite Animator", &visible)) {
    if (!visible) {
      entity.RemoveComponent<SpriteAnimatorComponent>();
      visible = true;
    }
    return;
  }

  // Controller asset picker
  AssetDropField("Controller", AssetType::SpriteController,
                 component.controller_handle_);

  // Playback
  ImGui::Checkbox(PrefixLabel("Playing").c_str(), &component.playing_);
  ImGui::Text("State: %s", component.current_state_name_.empty()
                               ? "(None)"
                               : component.current_state_name_.c_str());
  ImGui::Text("Frame: %d", component.current_frame_index_);

  // Parameters
  if (ImGui::TreeNode("Parameters")) {
    auto& params = component.state_machine_.parameters;
    std::string param_to_remove;
    for (auto& [name, param] : params) {
      ImGui::PushID(name.c_str());
      ImGui::Text("%s", name.c_str());
      ImGui::SameLine(120);
      switch (param.type) {
        case AnimParamType::Bool:
          ImGui::Checkbox("##v", &param.b);
          break;
        case AnimParamType::Int:
          ImGui::SetNextItemWidth(80);
          ImGui::InputInt("##v", &param.i);
          break;
        case AnimParamType::Float:
          ImGui::SetNextItemWidth(80);
          ImGui::InputFloat("##v", &param.f, 0.1f);
          break;
        case AnimParamType::Trigger:
          if (ImGui::SmallButton("Fire")) {
            param.b = true;
          }
          break;
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("X")) {
        param_to_remove = name;
      }
      ImGui::PopID();
    }
    if (!param_to_remove.empty()) {
      params.erase(param_to_remove);
    }

    // Add parameter
    static char param_name[64] = "";
    static int param_type = 0;
    ImGui::InputText("##pn", param_name, sizeof(param_name));
    ImGui::SameLine();
    const char* ptypes[] = {"Bool", "Int", "Float", "Trigger"};
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("##pt", &param_type, ptypes, 4);
    ImGui::SameLine();
    if (ImGui::Button("+ Param") && param_name[0] != '\0') {
      switch (param_type) {
        case 0:
          params[param_name] = AnimParam::MakeBool(false);
          break;
        case 1:
          params[param_name] = AnimParam::MakeInt(0);
          break;
        case 2:
          params[param_name] = AnimParam::MakeFloat(0.0f);
          break;
        case 3:
          params[param_name] = AnimParam::MakeTrigger();
          break;
      }
      param_name[0] = '\0';
    }
    ImGui::TreePop();
  }

  ImGui::TreePop();
}

void RenderAddComponentImGui_SpriteAnimatorComponent(Entity entity) {
  if (ImGui::MenuItem("Sprite Animator") &&
      !entity.HasComponent<SpriteAnimatorComponent>()) {
    entity.AddComponent<SpriteAnimatorComponent>();
  }
}

void RenderComponentImGui(ButtonComponent& component, Entity entity) {
  static bool visible = true;
  if (!ImGui::ClosableTreeNode("Button", &visible)) {
    if (!visible) {
      entity.RemoveComponent<ButtonComponent>();
      visible = true;
    }
    return;
  }

  ImGui::SeparatorText("Colors");
  ImGui::ColorEdit4(PrefixLabel("Normal").c_str(), &component.normal_color.r);
  ImGui::ColorEdit4(PrefixLabel("Hovered").c_str(), &component.hovered_color.r);
  ImGui::ColorEdit4(PrefixLabel("Pressed").c_str(), &component.pressed_color.r);
  ImGui::ColorEdit4(PrefixLabel("Selected").c_str(),
                    &component.selected_color.r);
  ImGui::ColorEdit4(PrefixLabel("Disabled").c_str(),
                    &component.disabled_color.r);

  ImGui::SeparatorText("Textures (optional)");
  TextureDropField("Normal", component.normal_texture);
  TextureDropField("Hovered", component.hovered_texture);
  TextureDropField("Pressed", component.pressed_texture);
  TextureDropField("Selected", component.selected_texture);
  TextureDropField("Disabled", component.disabled_texture);

  ImGui::SeparatorText("Child Offsets");
  ImGui::DragFloat2(PrefixLabel("Hovered Offset").c_str(),
                    reinterpret_cast<float*>(&component.hovered_offset), 0.5f);
  ImGui::DragFloat2(PrefixLabel("Pressed Offset").c_str(),
                    reinterpret_cast<float*>(&component.pressed_offset), 0.5f);
  ImGui::DragFloat2(PrefixLabel("Selected Offset").c_str(),
                    reinterpret_cast<float*>(&component.selected_offset), 0.5f);

  const char* state_names[] = {"Normal", "Hovered", "Pressed", "Selected",
                               "Disabled"};
  ImGui::TextDisabled("State: %s",
                      state_names[static_cast<int>(component.state_)]);

  ImGui::TreePop();
}

void RenderAddComponentImGui_ButtonComponent(Entity entity) {
  if (ImGui::MenuItem("Button") && !entity.HasComponent<ButtonComponent>()) {
    entity.AddComponent<ButtonComponent>();
    if (!entity.HasComponent<InteractableComponent>()) {
      entity.AddComponent<InteractableComponent>();
    }
    if (!entity.HasComponent<NavigableComponent>()) {
      entity.AddComponent<NavigableComponent>();
    }
    if (!entity.HasComponent<RectangleTransformComponent>()) {
      entity.AddComponent<RectangleTransformComponent>();
    }
  }
}

void RenderComponentImGui(InteractableComponent& component, Entity entity) {
  static bool visible = true;
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
  static bool visible = true;
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
  static bool visible = true;
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
    static Editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Radius", component.radius,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().radius = v;
                  });
  }

  ImGui::DragFloat(PrefixLabel("Delay (ms)").c_str(), &component.delay_ms, 5.0f,
                   10.0f, 2000.0f);
  if (s_command_stack) {
    static Editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Delay", component.delay_ms,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().delay_ms = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Decay").c_str(), &component.decay, 0.0f,
                     1.0f);
  if (s_command_stack) {
    static Editor::UndoTracker<float> tracker;
    tracker.Track(*s_command_stack, "Change Reverb Decay", component.decay,
                  [entity](const float& v) mutable {
                    entity.GetComponent<ReverbZoneComponent>().decay = v;
                  });
  }

  ImGui::SliderFloat(PrefixLabel("Wet").c_str(), &component.wet, 0.0f, 1.0f);
  if (s_command_stack) {
    static Editor::UndoTracker<float> tracker;
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

void InitializeEditorComponents() {
  RegisterComponentType<IdComponent>("", "", nullptr, nullptr, nullptr);
  RegisterComponentType<TagComponent>("", "", nullptr, nullptr, nullptr);
  RegisterComponentType<TransformComponent>(
      "Transform", "", RenderComponentImGui, nullptr, nullptr);
  RegisterComponentType<ModelComponent>(
      "Model", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_ModelComponent, nullptr);
  RegisterComponentType<AnimatorComponent>(
      "Animator", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_AnimatorComponent, nullptr);
  RegisterComponentType<CameraComponent>(
      "Camera", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_CameraComponent, nullptr);
  RegisterComponentType<LightDirectComponent>(
      "Directional Light", "Lighting", RenderComponentImGui,
      RenderAddComponentImGui_LightDirectComponent, nullptr);
  RegisterComponentType<LightPointComponent>(
      "Point Light", "Lighting", RenderComponentImGui,
      RenderAddComponentImGui_LightPointComponent, nullptr);
  RegisterComponentType<BehaviorsComponent>(
      "C# Script", "Scripting", RenderComponentImGui,
      RenderAddComponentImGui_BehaviorsComponent,
      RenderModalComponentImGui_BehaviorsComponent);
  RegisterComponentType<BoxColliderComponent>(
      "Box Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_BoxColliderComponent, nullptr);
  RegisterComponentType<SphereColliderComponent>(
      "Sphere Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_SphereColliderComponent, nullptr);
  RegisterComponentType<CapsuleColliderComponent>(
      "Capsule Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_CapsuleColliderComponent, nullptr);
  RegisterComponentType<MeshColliderComponent>(
      "Mesh Collider", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_MeshColliderComponent, nullptr);
  RegisterComponentType<RigidBodyComponent>(
      "Rigid Body", "Physics", RenderComponentImGui,
      RenderAddComponentImGui_RigidBodyComponent, nullptr);
  RegisterComponentType<UIDocumentComponent>(
      "UI Document", "UI", RenderComponentImGui,
      RenderAddComponentImGui_UIDocumentComponent, nullptr);
  RegisterComponentType<RectangleTransformComponent>(
      "Rectangle Transform", "Canvas", RenderComponentImGui, nullptr, nullptr);
  RegisterComponentType<CanvasComponent>(
      "Canvas", "Canvas", RenderComponentImGui,
      RenderAddComponentImGui_CanvasComponent, nullptr);
  RegisterComponentType<CanvasScalerComponent>(
      "Canvas Scaler", "Canvas", RenderComponentImGui,
      RenderAddComponentImGui_CanvasScalerComponent, nullptr);
  RegisterComponentType<CanvasRectComponent>(
      "Canvas Rect", "Canvas", RenderComponentImGui,
      RenderAddComponentImGui_CanvasRectComponent, nullptr);
  RegisterComponentType<CanvasImageComponent>(
      "Canvas Image", "Canvas", RenderComponentImGui,
      RenderAddComponentImGui_CanvasImageComponent, nullptr);
  RegisterComponentType<TextComponent>("Text", "Canvas", RenderComponentImGui,
                                       RenderAddComponentImGui_TextComponent,
                                       nullptr);
  RegisterComponentType<TextInputComponent>(
      "Text Input", "UI", RenderComponentImGui,
      RenderAddComponentImGui_TextInputComponent, nullptr);
  RegisterComponentType<ButtonComponent>(
      "Button", "UI", RenderComponentImGui,
      RenderAddComponentImGui_ButtonComponent, nullptr);
  RegisterComponentType<InteractableComponent>(
      "Interactable", "UI", RenderComponentImGui,
      RenderAddComponentImGui_InteractableComponent, nullptr);
  RegisterComponentType<NavigableComponent>(
      "Navigable", "UI", RenderComponentImGui,
      RenderAddComponentImGui_NavigableComponent, nullptr);
  RegisterComponentType<AudioSourceComponent>(
      "Audio Source", "Audio", RenderComponentImGui,
      RenderAddComponentImGui_AudioSourceComponent, nullptr);
  RegisterComponentType<SpriteRendererComponent>(
      "Sprite Renderer", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_SpriteRendererComponent, nullptr);
  RegisterComponentType<SpriteAnimatorComponent>(
      "Sprite Animator", "Rendering", RenderComponentImGui,
      RenderAddComponentImGui_SpriteAnimatorComponent, nullptr);
  RegisterComponentType<ReverbZoneComponent>(
      "Reverb Zone", "Audio", RenderComponentImGui,
      RenderAddComponentImGui_ReverbZoneComponent, nullptr);
}

void RenderExistingComponents(Entity entity) {
  for (const auto& [ti, desc] : kRegistry | std::views::reverse) {
    if (desc.HasComponent(entity)) {
      desc.RenderSelf(entity);
    }
  }
}

void RenderModals(Entity entity) {
  for (const auto& item : kRegistry | std::views::reverse) {
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
  ImGui::InputTextWithHint("##component_search", "Search...", search_buf,
                           sizeof(search_buf));

  std::string filter(search_buf);
  // Lowercase for case-insensitive matching
  std::string filter_lower = filter;
  for (auto& c : filter_lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  bool has_filter = !filter_lower.empty();

  // Collect groups in consistent order
  static const std::vector<std::string> group_order = {
      "Rendering", "Lighting", "Audio", "Physics", "UI", "Canvas", "Scripting"};

  if (has_filter) {
    // Flat filtered list (no groups)
    for (const auto& [ti, desc] : kRegistry) {
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
      if (!has_items) {
        continue;
      }

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