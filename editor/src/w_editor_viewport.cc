//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor.h"

#include "asset/w_asset_manager.h"
#include "imgui_internal.h"
#include "physics/w_collider.h"
#include "rendering/w_renderer.h"
#include "rendering/w_sprite.h"
#include "scene/w_scene_manager.h"
#include "script/w_scriptmanager.h"
#include "w_editor_icons.h"
#include "w_engine.h"

namespace wiesel::editor {

// Free functions defined in w_editor.cc with external linkage.
std::shared_ptr<Scene> scene();

struct ResolutionPreset {
  const char* label;
  glm::vec2 size;  // {0,0} = Free Aspect
};

static const ResolutionPreset kResolutionPresets[] = {
    {"2560x1440", {2560, 1440}}, {"1920x1080", {1920, 1080}},
    {"1600x900", {1600, 900}},   {"1280x720", {1280, 720}},
    {"854x480", {854, 480}},     {"Free Aspect", {0, 0}},
};
static constexpr int kResolutionPresetCount =
    sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]);

static constexpr float kResolutionComboWidth = 130.0f;
static constexpr float kSettingsButtonWidth = 24.0f;

// Helper: renders the contents of an "Add" entity menu.
// If parent != entt::null, the entity is linked as a child.
// Returns true if an entity was created.
static bool RenderAddEntityMenu(Scene& scene, bool& dirty,
                                CommandStack& commands,
                                entt::entity parent = entt::null,
                                const glm::vec3* spawn_pos = nullptr) {
  Entity created{entt::null, nullptr};

  if (ImGui::MenuItem("Empty Entity")) {
    created = scene.CreateEntity();
  }

  if (ImGui::BeginMenu("3D Shape")) {
    const char* shapes[] = {"Cube", "Sphere", "Plane", "Cylinder", "Capsule"};
    for (const char* shape : shapes) {
      if (ImGui::MenuItem(shape)) {
        created = scene.CreateEntity(shape);
        auto& mc = created.AddComponent<MeshRendererComponent>();
        mc.model_handle = Engine::GetPrimitive(shape);
        mc.mesh_index = 0;
      }
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Light")) {
    if (ImGui::MenuItem("Directional Light")) {
      created = scene.CreateEntity("Directional Light");
      created.AddComponent<LightDirectComponent>();
    }
    if (ImGui::MenuItem("Point Light")) {
      created = scene.CreateEntity("Point Light");
      created.AddComponent<LightPointComponent>();
    }
    ImGui::EndMenu();
  }

  if (ImGui::MenuItem("Camera")) {
    created = scene.CreateEntity("Camera");
    created.AddComponent<CameraComponent>();
  }

  if (created.handle() != entt::null) {
    if (parent != entt::null) {
      scene.LinkEntities(parent, created);
    }
    if (spawn_pos) {
      auto& tc = created.GetComponent<TransformComponent>();
      tc.SetPosition(*spawn_pos);
    }
    auto shared_scene = Engine::scene_manager().FindSceneByPtr(&scene);
    if (shared_scene) {
      commands.Execute(std::make_unique<EntityCreateCommand>(shared_scene,
                                                             created.handle()));
    }
    dirty = true;
    return true;
  }

  return false;
}

void EditorLayer::RenderSceneViewportPanel() {
  Renderer* renderer = Engine::renderer().get();

  bool& scene_view_open = panel_scene_view_;
  if (scene_view_open) {
    ImGuiWindowFlags sceneFlags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    scene_panel_visible_ =
        ImGui::Begin(CODICON_PREVIEW " Scene", &scene_view_open, sceneFlags);
    if (scene_panel_visible_) {
      // --- Centered Play/Pause/Stop toolbar ---
      {
        float toolbar_width = 70.0f;
        float avail_w = ImGui::GetContentRegionAvail().x;
        float settings_w = 30.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (avail_w - toolbar_width) * 0.5f);

        DrawPlayStopButtons();

        // ... settings button on the right
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - settings_w);
        if (ImGui::Button(CODICON_ELLIPSIS "##SceneSettings")) {
          ImGui::OpenPopup("SceneCameraSettings");
        }
        if (ImGui::BeginPopup("SceneCameraSettings")) {
          ImGui::SeparatorText("Camera Mode");
          if (ImGui::RadioButton(
                  "Free", editor_camera_mode_ == EditorCameraMode::Free)) {
            editor_camera_mode_ = EditorCameraMode::Free;
            editor_camera_.projection_mode = ProjectionMode::Perspective;
            editor_camera_.view_changed = true;
            editor_camera_.resource_pipeline_version = 0;
            piloting_camera_ = entt::null;
          }
          if (ImGui::RadioButton(
                  "2D", editor_camera_mode_ == EditorCameraMode::Mode2D)) {
            editor_camera_mode_ = EditorCameraMode::Mode2D;
            editor_camera_.projection_mode = ProjectionMode::Orthographic;
            editor_camera_.ortho_size = editor_2d_zoom_;
            editor_pitch_ = 0.0f;
            editor_yaw_ = 0.0f;
            editor_camera_transform_.SetPosition(
                glm::vec3(0.0f, 180.0f, -5.0f));
            editor_camera_transform_.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
            editor_camera_transform_.MarkChanged();
            editor_camera_.view_changed = true;
            editor_camera_.resource_pipeline_version = 0;
            piloting_camera_ = entt::null;
          }

          ImGui::SeparatorText("Camera Settings");
          ImGui::DragFloat("Speed", &camera_speed_, 0.5f, 0.1f, 100.0f);
          ImGui::DragFloat("Sensitivity", &mouse_sensitivity_, 1.0f, 10.0f,
                           500.0f);
          if (editor_camera_mode_ == EditorCameraMode::Free) {
            ImGui::DragFloat("FOV", &editor_camera_.field_of_view, 1.0f, 1.0f,
                             179.0f);
          }

          ImGui::SeparatorText("Snap to Camera");
          for (auto entity : scene()
                                 ->GetAllEntitiesWith<CameraComponent,
                                                      TransformComponent>()) {
            auto& tag = scene()->GetComponent<TagComponent>(entity);
            bool is_piloting = (piloting_camera_ == entity);
            if (ImGui::Selectable(tag.name.c_str(), is_piloting)) {
              auto& cam = scene()->GetComponent<CameraComponent>(entity);
              auto& tc = scene()->GetComponent<TransformComponent>(entity);
              editor_camera_transform_.SetPosition(tc.GetPosition());
              editor_camera_transform_.SetRotation(tc.GetRotation());
              editor_yaw_ = tc.GetRotation().y;
              editor_pitch_ = tc.GetRotation().x;
              if (cam.projection_mode == ProjectionMode::Orthographic) {
                editor_camera_mode_ = EditorCameraMode::Mode2D;
                editor_camera_.projection_mode = ProjectionMode::Orthographic;
                editor_camera_.ortho_size = cam.ortho_size;
                editor_2d_zoom_ = cam.ortho_size;
              } else {
                editor_camera_mode_ = EditorCameraMode::Free;
                editor_camera_.projection_mode = ProjectionMode::Perspective;
                editor_camera_.field_of_view = cam.field_of_view;
              }
              editor_camera_.near_plane = cam.near_plane;
              editor_camera_.far_plane = cam.far_plane;
              editor_camera_.background_color = cam.background_color;
              editor_camera_.view_changed = true;
              editor_camera_.resource_pipeline_version = 0;
              piloting_camera_ = is_piloting ? entt::null : entity;
            }
          }
          if (piloting_camera_ != entt::null) {
            if (ImGui::Button("Stop Piloting")) {
              piloting_camera_ = entt::null;
            }
          }

          ImGui::SeparatorText("Overlays");
          ImGui::EndPopup();
        }
      }

      // Editor camera output from its own resource pool
      auto editor_desc = editor_camera_.resource_pool.GetDescriptor(
          "PipelineOutputDescriptor");
      auto editor_image =
          editor_camera_.resource_pool.GetTexture("PipelineOutput");

      // Handle viewport resize - editor camera always tracks panel size
      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (avail.x > 0 && avail.y > 0) {
        uint32_t new_width = static_cast<uint32_t>(avail.x);
        uint32_t new_height = static_cast<uint32_t>(avail.y);
        if (new_width > 0 && new_height > 0 &&
            (new_width != editor_camera_.viewport_size.x ||
             new_height != editor_camera_.viewport_size.y)) {
          editor_camera_.viewport_size = {new_width, new_height};
          editor_camera_.aspect_ratio =
              static_cast<float>(new_width) / static_cast<float>(new_height);
          editor_camera_.view_changed = true;
          editor_camera_.resource_pipeline_version = 0;
        }
      }
      if (editor_desc && editor_image) {
        ImTextureID desc =
            reinterpret_cast<ImTextureID>(editor_desc->descriptor_set_);

        float image_aspect =
            (float)editor_image->width_ / (float)editor_image->height_;
        float avail_aspect = avail.x / avail.y;

        ImVec2 image_size;
        if (avail_aspect > image_aspect) {
          image_size.y = avail.y;
          image_size.x = image_size.y * image_aspect;
        } else {
          image_size.x = avail.x;
          image_size.y = image_size.x / image_aspect;
        }

        ImGui::Image(desc, image_size);

        // Accept model asset drag-drop onto the viewport
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("AssetHandle")) {
            AssetHandle handle =
                *static_cast<const AssetHandle*>(payload->Data);
            InstantiateModelAsset(handle);
          }
          ImGui::EndDragDropTarget();
        }

        ImVec2 image_min = ImGui::GetItemRectMin();
        ImVec2 image_max = ImGui::GetItemRectMax();
        bool scene_hovered = ImGui::IsItemHovered();

        // Gizmo toolbar overlay (top-left of viewport image, vertical)
        {
          float overlay_x = image_min.x + 6;
          float overlay_y = image_min.y + 6;

          ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
          ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

          auto gizmo_btn = [&](const char* label, bool active, float y) {
            ImGui::SetCursorScreenPos(ImVec2(overlay_x, y));
            if (active) {
              ImGui::PushStyleColor(ImGuiCol_Button,
                                    ImVec4(0.86f, 0.26f, 0.26f, 0.8f));
            }
            bool clicked = ImGui::Button(label);
            if (active) {
              ImGui::PopStyleColor();
            }
            return clicked;
          };

          float btn_h = ImGui::GetFrameHeight() + 2.0f;
          float y = overlay_y;

          if (gizmo_btn(CODICON_MOVE "##translate",
                        current_op_ == ImGuizmo::TRANSLATE, y)) {
            current_op_ = ImGuizmo::TRANSLATE;
          }
          y += btn_h;
          if (gizmo_btn(CODICON_REFRESH "##rotate",
                        current_op_ == ImGuizmo::ROTATE, y)) {
            current_op_ = ImGuizmo::ROTATE;
          }
          y += btn_h;
          if (gizmo_btn(CODICON_ARROW_BOTH "##scale",
                        current_op_ == ImGuizmo::SCALE, y)) {
            current_op_ = ImGuizmo::SCALE;
          }
          y += btn_h + 4.0f;  // extra gap before mode toggle
          if (gizmo_btn(current_mode_ == ImGuizmo::LOCAL ? CODICON_HOME "##mode"
                                                         : CODICON_GLOBE
                            "##mode",
                        false, y)) {
            current_mode_ = (current_mode_ == ImGuizmo::LOCAL)
                                ? ImGuizmo::WORLD
                                : ImGuizmo::LOCAL;
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(current_mode_ == ImGuizmo::LOCAL ? "Local"
                                                               : "World");
          }

          ImGui::PopStyleColor(3);
          ImGui::PopStyleVar();
        }

        // Right-click mouse look
        static bool scene_right_active = false;
        bool scene_focused = ImGui::IsWindowFocused();
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
          if (scene_hovered && !scene_right_active) {
            scene_right_active = true;
            Engine::window()->SetCursorCaptureSource(
                CursorCaptureSource::Editor);
            Engine::window()->SetCursorMode(CursorModeRelative);
          }
        } else {
          if (scene_right_active) {
            scene_right_active = false;
            Engine::window()->SetCursorMode(CursorModeNormal);
            Engine::window()->SetCursorCaptureSource(CursorCaptureSource::None);
          }
        }

        // Mouse look is handled in OnMouseMoved via the event system

        // Camera movement (only when no other widget wants keyboard input)
        if ((scene_focused || scene_right_active) &&
            !ImGui::GetIO().WantTextInput) {
          ImGuiIO& io = ImGui::GetIO();
          float dt = io.DeltaTime;
          float speed = camera_speed_ * dt;
          if (io.KeyShift) {
            speed *= 3.0f;
          }

          if (editor_camera_mode_ == EditorCameraMode::Mode2D) {
            // 2D: WASD = pan on XY plane
            if (ImGui::IsKeyDown(ImGuiKey_W)) {
              editor_camera_transform_.Move(0, speed, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_S)) {
              editor_camera_transform_.Move(0, -speed, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_D)) {
              editor_camera_transform_.Move(speed, 0, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_A)) {
              editor_camera_transform_.Move(-speed, 0, 0);
            }
          } else {
            // Free: WASD + QE = 3D fly camera
            glm::quat q =
                glm::quat(glm::radians(editor_camera_transform_.GetRotation()));
            glm::mat4 R = glm::toMat4(q);
            glm::vec3 cam_right = glm::vec3(R[0]);
            glm::vec3 cam_forward = glm::vec3(R[2]);

            if (ImGui::IsKeyDown(ImGuiKey_W)) {
              editor_camera_transform_.Move(cam_forward * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_S)) {
              editor_camera_transform_.Move(-cam_forward * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_D)) {
              editor_camera_transform_.Move(cam_right * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_A)) {
              editor_camera_transform_.Move(-cam_right * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_E)) {
              editor_camera_transform_.Move(0, speed, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_Q)) {
              editor_camera_transform_.Move(0, -speed, 0);
            }
          }
        }

        // Scroll to zoom
        if (scene_hovered) {
          float scroll = ImGui::GetIO().MouseWheel;
          if (std::abs(scroll) > 0.01f) {
            if (editor_camera_mode_ == EditorCameraMode::Mode2D) {
              // 2D: scroll = zoom (adjust ortho size)
              editor_2d_zoom_ -= scroll * editor_2d_zoom_ * 0.1f;
              editor_2d_zoom_ = glm::clamp(editor_2d_zoom_, 0.1f, 1000.0f);
              editor_camera_.ortho_size = editor_2d_zoom_;
              editor_camera_.view_changed = true;
            } else {
              // Free: scroll = dolly forward/back
              glm::quat q = glm::quat(
                  glm::radians(editor_camera_transform_.GetRotation()));
              glm::mat4 R = glm::toMat4(q);
              glm::vec3 cam_forward = glm::vec3(R[2]);
              editor_camera_transform_.Move(cam_forward * scroll *
                                            camera_speed_ * 0.3f);
            }
          }
        }

        // Follow piloted camera (read-only - editor follows entity, doesn't modify it)
        if (piloting_camera_ != entt::null &&
            scene()->HasEntity(piloting_camera_)) {
          auto& tc =
              scene()->GetComponent<TransformComponent>(piloting_camera_);
          editor_camera_transform_.SetPosition(tc.GetPosition());
          editor_camera_transform_.SetRotation(tc.GetRotation());
          editor_yaw_ = tc.GetRotation().y;
          editor_pitch_ = tc.GetRotation().x;

          auto& cam = scene()->GetComponent<CameraComponent>(piloting_camera_);
          editor_camera_.background_color = cam.background_color;
          editor_camera_.near_plane = cam.near_plane;
          editor_camera_.far_plane = cam.far_plane;

          if (cam.projection_mode != editor_camera_.projection_mode) {
            editor_camera_.projection_mode = cam.projection_mode;
            if (cam.projection_mode == ProjectionMode::Orthographic) {
              editor_camera_mode_ = EditorCameraMode::Mode2D;
            } else {
              editor_camera_mode_ = EditorCameraMode::Free;
            }
            editor_camera_.resource_pipeline_version = 0;
          }

          if (cam.projection_mode == ProjectionMode::Orthographic) {
            editor_camera_.ortho_size = cam.ortho_size;
            editor_2d_zoom_ = cam.ortho_size;
          } else {
            editor_camera_.field_of_view = cam.field_of_view;
          }
          editor_camera_.view_changed = true;
        }

        // ImGuizmo (uses editor camera matrices, disabled during right-click camera)
        if (has_selected_entity_ && !scene_right_active) {
          glm::mat4 view = editor_camera_.view_matrix;
          glm::mat4 proj = editor_camera_.projection;
          proj[1][1] *= -1;
          TransformComponent& transform =
              selected_entity_scene_->GetComponent<TransformComponent>(
                  selected_entity_);

          // Capture old transform for undo
          glm::vec3 old_pos = transform.GetPosition();
          glm::vec3 old_rot = transform.GetRotation();
          glm::vec3 old_scale = transform.GetScale();

          glm::mat4 model = transform.GetTransformMatrix();
          ImGuizmo::SetOrthographic(editor_camera_mode_ ==
                                    EditorCameraMode::Mode2D);
          ImGuizmo::SetLeftHanded(true);
          ImGuizmo::AllowAxisFlip(false);
          ImGuizmo::SetDrawlist();
          ImGuizmo::SetRect(image_min.x, image_min.y, image_size.x,
                            image_size.y);
          if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                   current_op_, current_mode_,
                                   glm::value_ptr(model))) {
            // ImGuizmo returns a world-space matrix. If the entity has a parent,
            // convert back to local space before setting position/rotation/scale.
            Entity selected{selected_entity_, selected_entity_scene_.get()};
            Entity parent = selected.GetParent();
            if (parent && parent.HasComponent<TransformComponent>()) {
              glm::mat4 parent_world = parent.GetComponent<TransformComponent>()
                                           .GetTransformMatrix();
              model = glm::inverse(parent_world) * model;
            }

            glm::vec3 translation, rotation, scale;
            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(model), glm::value_ptr(translation),
                glm::value_ptr(rotation), glm::value_ptr(scale));

            transform.SetPosition(translation);
            transform.SetRotation(rotation);
            transform.SetScale(scale);

            command_stack_.Execute(std::make_unique<TransformCommand>(
                selected_entity_scene_, selected_entity_, old_pos, old_rot,
                old_scale, translation, rotation, scale));
            scene_dirty_ = true;
          }

          // Draw collider wireframes for selected entity
          glm::mat4 vp = proj * view;
          ImDrawList* drawList = ImGui::GetWindowDrawList();

          auto ProjectPoint = [&](glm::vec3 worldPos) -> ImVec2 {
            glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
            if (clip.w <= 0.001f) {
              return ImVec2(-9999, -9999);
            }
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            return ImVec2(image_min.x + (ndc.x * 0.5f + 0.5f) * image_size.x,
                          image_min.y + (-ndc.y * 0.5f + 0.5f) * image_size.y);
          };

          auto DrawLine3D = [&](glm::vec3 a, glm::vec3 b, ImU32 color) {
            ImVec2 sa = ProjectPoint(a);
            ImVec2 sb = ProjectPoint(b);
            drawList->AddLine(sa, sb, color, 1.5f);
          };

          if (selected_entity_scene_->HasComponent<BoxColliderComponent>(
                  selected_entity_)) {
            auto& box =
                selected_entity_scene_->GetComponent<BoxColliderComponent>(
                    selected_entity_);
            glm::vec3 center = transform.GetWorldPosition() + box.offset;
            glm::vec3 h = box.half_extents;
            glm::vec3 corners[8] = {
                center + glm::vec3(-h.x, -h.y, -h.z),
                center + glm::vec3(h.x, -h.y, -h.z),
                center + glm::vec3(h.x, h.y, -h.z),
                center + glm::vec3(-h.x, h.y, -h.z),
                center + glm::vec3(-h.x, -h.y, h.z),
                center + glm::vec3(h.x, -h.y, h.z),
                center + glm::vec3(h.x, h.y, h.z),
                center + glm::vec3(-h.x, h.y, h.z),
            };
            ImU32 col = IM_COL32(0, 255, 0, 200);
            DrawLine3D(corners[0], corners[1], col);
            DrawLine3D(corners[1], corners[2], col);
            DrawLine3D(corners[2], corners[3], col);
            DrawLine3D(corners[3], corners[0], col);
            DrawLine3D(corners[4], corners[5], col);
            DrawLine3D(corners[5], corners[6], col);
            DrawLine3D(corners[6], corners[7], col);
            DrawLine3D(corners[7], corners[4], col);
            DrawLine3D(corners[0], corners[4], col);
            DrawLine3D(corners[1], corners[5], col);
            DrawLine3D(corners[2], corners[6], col);
            DrawLine3D(corners[3], corners[7], col);
          }

          if (selected_entity_scene_->HasComponent<SphereColliderComponent>(
                  selected_entity_)) {
            auto& sphere =
                selected_entity_scene_->GetComponent<SphereColliderComponent>(
                    selected_entity_);
            glm::vec3 center = transform.GetWorldPosition() + sphere.offset;
            float r = sphere.radius;
            ImU32 col = IM_COL32(0, 255, 0, 200);
            constexpr int segments = 32;
            for (int ring = 0; ring < 3; ring++) {
              for (int i = 0; i < segments; i++) {
                float a0 = (float)i / segments * 2.0f * glm::pi<float>();
                float a1 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                glm::vec3 p0, p1;
                if (ring == 0) {
                  p0 = center + glm::vec3(cosf(a0), sinf(a0), 0) * r;
                  p1 = center + glm::vec3(cosf(a1), sinf(a1), 0) * r;
                } else if (ring == 1) {
                  p0 = center + glm::vec3(cosf(a0), 0, sinf(a0)) * r;
                  p1 = center + glm::vec3(cosf(a1), 0, sinf(a1)) * r;
                } else {
                  p0 = center + glm::vec3(0, cosf(a0), sinf(a0)) * r;
                  p1 = center + glm::vec3(0, cosf(a1), sinf(a1)) * r;
                }
                DrawLine3D(p0, p1, col);
              }
            }
          }

        }  // end has_selected_entity_

        // Entity picking: click on Scene panel to select (only when not right-clicking)
        if (!scene_right_active && ImGui::IsMouseClicked(0) && scene_hovered &&
            !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
          ImVec2 mouse = ImGui::GetIO().MousePos;
          float rel_x = mouse.x - image_min.x;
          float rel_y = mouse.y - image_min.y;
          if (rel_x >= 0 && rel_y >= 0 && rel_x < image_size.x &&
              rel_y < image_size.y) {
            uint32_t render_w = editor_image->width_;
            uint32_t render_h = editor_image->height_;
            uint32_t px =
                static_cast<uint32_t>(rel_x * render_w / image_size.x);
            uint32_t py =
                static_cast<uint32_t>(rel_y * render_h / image_size.y);
            auto entity_id_tex = editor_camera_.resource_pool.GetTexture(
                "geometry.entity_id_resolve");
            auto billboard_entity_id_tex =
                editor_camera_.resource_pool.GetTexture(
                    "billboard.entity_id_resolve");
            if (entity_id_tex) {
              // Check billboard first (overlays geometry), fall back to geometry
              renderer->RequestEntityPick(px, py, billboard_entity_id_tex,
                                          entity_id_tex);
            }
            // Store NDC for fallback sprite/canvas picking
            pending_pick_ndc_ = {
                (rel_x / image_size.x) * 2.0f - 1.0f,
                1.0f - (rel_y / image_size.y) * 2.0f  // flip Y
            };
          }
        }
        // Shift+R: open quick-add menu at camera position
        if (scene_hovered && ImGui::IsKeyPressed(ImGuiKey_R, false) &&
            ImGui::GetIO().KeyShift) {
          ImGui::OpenPopup("##QuickAdd");
        }
        if (ImGui::BeginPopup("##QuickAdd")) {
          glm::vec3 cam_pos = editor_camera_transform_.GetPosition();
          RenderAddEntityMenu(*scene(), scene_dirty_, command_stack_,
                              entt::null, &cam_pos);
          ImGui::EndPopup();
        }
      }
    }
    ImGui::End();
  }
}

void EditorLayer::RenderResolutionDropdown() {
  float combo_width = kResolutionComboWidth;
  float right_edge = ImGui::GetWindowContentRegionMax().x;
  ImGui::SameLine(right_edge - combo_width);
  ImGui::SetNextItemWidth(combo_width);
  if (ImGui::BeginCombo("##GameResolution",
                        kResolutionPresets[resolution_preset_index_].label)) {
    for (int i = 0; i < kResolutionPresetCount; i++) {
      bool selected = (i == resolution_preset_index_);
      if (ImGui::Selectable(kResolutionPresets[i].label, selected)) {
        resolution_preset_index_ = i;
        for (auto loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
          loaded_scene->SetRenderResolution(kResolutionPresets[i].size);
        }
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

void EditorLayer::RenderGameViewportPanel() {
  Renderer* renderer = Engine::renderer().get();

  bool& game_view_open = panel_game_view_;
  if (game_view_open) {
    bool game_visible =
        ImGui::Begin(CODICON_CAMERA_VIDEO " Game", &game_view_open);
    game_panel_visible_ = game_visible;
    game_panel_focused_ = ImGui::IsWindowFocused();
    if (game_visible) {
      float toolbar_width = 70.0f;
      float avail_w = ImGui::GetContentRegionAvail().x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           (avail_w - toolbar_width) * 0.5f);
      DrawPlayStopButtons();
      RenderResolutionDropdown();

      {
        // Check if any camera exists across all loaded scenes
        bool has_camera = false;
        for (auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
          for (auto entity :
               loaded_scene->GetAllEntitiesWith<CameraComponent>()) {
            auto& cam = loaded_scene->GetComponent<CameraComponent>(entity);
            if (cam.enabled) {
              has_camera = true;
              break;
            }
          }
          if (has_camera) {
            break;
          }
        }

        if (!has_camera) {
          ImVec2 avail = ImGui::GetContentRegionAvail();
          const char* text = "No camera in scene";
          ImVec2 textSize = ImGui::CalcTextSize(text);
          ImGui::SetCursorPos(
              ImVec2(ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f,
                     ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f));
          ImGui::TextDisabled("%s", text);
        } else {
          ImVec2 avail = ImGui::GetContentRegionAvail();
          if (scene()->GetRenderResolution().x <= 0) {
            // Free Aspect: game camera tracks panel size
            for (auto& loaded_Scene :
                 Engine::scene_manager().GetLoadedScenes()) {
              for (auto entity :
                   loaded_Scene->GetAllEntitiesWith<CameraComponent>()) {
                auto& cam = loaded_Scene->GetComponent<CameraComponent>(entity);
                if (!cam.enabled) {
                  continue;
                }
                uint32_t w = static_cast<uint32_t>(avail.x);
                uint32_t h = static_cast<uint32_t>(avail.y);
                if (w > 0 && h > 0 &&
                    (w != static_cast<uint32_t>(cam.viewport_size.x) ||
                     h != static_cast<uint32_t>(cam.viewport_size.y))) {
                  cam.viewport_size = {w, h};
                  cam.aspect_ratio =
                      static_cast<float>(w) / static_cast<float>(h);
                  cam.view_changed = true;
                  cam.resource_pipeline_version = 0;
                }
                break;
              }
            }
          }

          auto final_output_desc = renderer->GetFinalOutputDescriptor();
          auto final_output_image = renderer->GetFinalOutputImage();
          if (final_output_desc && final_output_image) {
            ImTextureID game_desc = reinterpret_cast<ImTextureID>(
                final_output_desc->descriptor_set_);

            float image_aspect =
                static_cast<float>(final_output_image->width_) /
                static_cast<float>(final_output_image->height_);
            float avail_aspect = avail.x / avail.y;

            ImVec2 draw_size;
            if (avail_aspect > image_aspect) {
              draw_size.y = avail.y;
              draw_size.x = draw_size.y * image_aspect;
            } else {
              draw_size.x = avail.x;
              draw_size.y = draw_size.x / image_aspect;
            }
            ImGui::Image(game_desc, draw_size);

            ImVec2 image_min = ImGui::GetItemRectMin();
            ImVec2 image_max = ImGui::GetItemRectMax();

            // Set viewport origin and display size for UI hit testing
            // on all loaded scenes (UI may live in an additive scene)
            for (auto& s : Engine::scene_manager().GetLoadedScenes()) {
              s->SetViewportOrigin({image_min.x, image_min.y});
              s->SetViewportDisplaySize({draw_size.x, draw_size.y});
            }

            // FPS overlay (top-left)
            ImVec2 text_pos = ImVec2(image_min.x + 6, image_min.y + 6);
            std::string fpsStr =
                std::format("FPS: {}", static_cast<int>(app_.GetFPS()));
            ImGui::GetWindowDrawList()->AddText(
                text_pos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

            // Resolution overlay (top-right)
            std::string res_str =
                std::format("{}x{}", final_output_image->width_,
                            final_output_image->height_);
            ImVec2 res_text_size = ImGui::CalcTextSize(res_str.c_str());
            ImVec2 res_pos =
                ImVec2(image_max.x - res_text_size.x - 6, image_min.y + 6);
            ImGui::GetWindowDrawList()->AddText(
                res_pos, IM_COL32(0, 255, 0, 255), res_str.c_str());
          }
        }
      }
    }
    ImGui::End();
  }
}

bool EditorLayer::DrawPlayStopButtons() {
  bool changed = false;
  if (editor_state_ == EditorState::Edit) {
    // Play button
    bool compiling = Engine::script_manager().IsCompiling();
    if (compiling) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(CODICON_PLAY "##play")) {
      AutoSave();
      TakeSnapshot();
      editor_state_ = EditorState::Playing;
      scene()->ResetFirstUpdate();
      ImGui::SetWindowFocus(CODICON_CAMERA_VIDEO " Game");
      changed = true;
    }
    if (compiling) {
      ImGui::EndDisabled();
    }
    // Pause disabled in edit mode
    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button(CODICON_PAUSE "##pause");
    ImGui::EndDisabled();
  } else if (editor_state_ == EditorState::Playing) {
    // Stop button
    if (ImGui::Button(CODICON_STOP "##stop")) {
      deferred_action_ = DeferredAction::StopPlaying;
      changed = true;
    }
    // Pause button
    ImGui::SameLine();
    if (ImGui::Button(CODICON_PAUSE "##pause")) {
      editor_state_ = EditorState::Paused;
      changed = true;
    }
  } else if (editor_state_ == EditorState::Paused) {
    // Stop button
    if (ImGui::Button(CODICON_STOP "##stop")) {
      deferred_action_ = DeferredAction::StopPlaying;
      changed = true;
    }
    // Resume (play) button
    ImGui::SameLine();
    if (ImGui::Button(CODICON_PLAY "##resume")) {
      editor_state_ = EditorState::Playing;
      changed = true;
    }
  }
  return changed;
}

void EditorLayer::FindSpritesInScene(const std::shared_ptr<Scene>& scene,
                                     const glm::mat4& vp, glm::vec2 pick_ndc,
                                     entt::entity& best, float& best_depth,
                                     std::shared_ptr<Scene>& best_scene) {
  for (auto entity : scene->GetAllEntitiesWith<SpriteRendererComponent,
                                               TransformComponent>()) {
    auto& tc = scene->GetComponent<TransformComponent>(entity);
    glm::vec3 world_pos = tc.GetWorldPosition();
    glm::vec4 clip = vp * glm::vec4(world_pos, 1.0f);
    if (clip.w <= 0.0f) {
      continue;
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    // Approximate sprite screen size from scale
    glm::vec3 world_scale = tc.GetWorldScale();
    float half_w = world_scale.x * 0.5f;
    float half_h = world_scale.y * 0.5f;
    glm::vec4 corner =
        vp * glm::vec4(world_pos + glm::vec3(half_w, half_h, 0), 1.0f);
    if (corner.w <= 0.0f) {
      continue;
    }
    glm::vec3 corner_ndc = glm::vec3(corner) / corner.w;
    float extent_x = std::abs(corner_ndc.x - ndc.x);
    float extent_y = std::abs(corner_ndc.y - ndc.y);

    if (std::abs(pick_ndc.x - ndc.x) <= extent_x &&
        std::abs(pick_ndc.y - ndc.y) <= extent_y) {
      if (ndc.z < best_depth) {
        best = entity;
        best_depth = ndc.z;
        best_scene = scene;
      }
    }
  }
}

entt::entity EditorLayer::FindSpriteAtNDC(glm::vec2 pick_ndc) {
  glm::mat4 vp = editor_camera_.projection * editor_camera_.view_matrix;
  // Vulkan flips Y in projection, undo for NDC comparison
  vp[1][1] *= -1.0f;

  entt::entity best = entt::null;
  float best_depth = std::numeric_limits<float>::max();
  std::shared_ptr<Scene> best_scene;

  for (auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
    FindSpritesInScene(loaded_scene, vp, pick_ndc, best, best_depth,
                       best_scene);
  }
  if (best != entt::null && best_scene) {
    selected_entity_scene_ = best_scene;
  }
  return best;
}

}  // namespace wiesel::editor
