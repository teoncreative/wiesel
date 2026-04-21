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
#include "util/imgui/w_imguiutil.h"
#include "w_editor_entity_factory.h"
#include "w_editor_icons.h"
#include "w_engine.h"

namespace wiesel::editor {

Scene* scene();

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

// Thin wrapper around the shared EntityFactoryRegistry-driven menu so the
// existing call site keeps the (Scene, dirty, parent, spawn_pos) signature.
static bool RenderAddEntityMenu(Scene& scene, bool& dirty,
                                CommandStack& commands,
                                entt::entity parent = entt::null,
                                const glm::vec3* spawn_pos = nullptr) {
  Entity parent_entity = parent == entt::null
                             ? kInvalidEntity
                             : Entity{parent, &scene};
  Entity created =
      RenderEntityFactoryMenu(scene, commands, parent_entity, spawn_pos);
  if (created) {
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    scene_panel_visible_ =
        ImGui::Begin(ICON_LC_EYE " Scene", &scene_view_open, sceneFlags);
    ImGui::PopStyleVar();
    if (scene_panel_visible_) {
      {
        const ImVec2 pad = ImGui::GetStyle().WindowPadding;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad.y);
        ImGui::Indent(pad.x);
        const float row_start_y = ImGui::GetCursorPosY();
        // BeginToolbarGroup adds 2px of inner padding above/below the row.
        const float row_height = ImGui::GetFrameHeight() + 4.0f;

        ImGui::BeginToolbarGroup("##GizmoToolbar");
        if (ImGui::ToolbarButton(ICON_LC_MOVE "##translate",
                                 current_op_ == ImGuizmo::TRANSLATE))
          current_op_ = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::ToolbarButton(ICON_LC_ROTATE_CW "##rotate",
                                 current_op_ == ImGuizmo::ROTATE))
          current_op_ = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::ToolbarButton(ICON_LC_SCALING "##scale",
                                 current_op_ == ImGuizmo::SCALE))
          current_op_ = ImGuizmo::SCALE;
        ImGui::SameLine();
        if (ImGui::ToolbarButton(current_mode_ == ImGuizmo::LOCAL
                                     ? ICON_LC_HOUSE "##mode"
                                     : ICON_LC_GLOBE "##mode"))
          current_mode_ = (current_mode_ == ImGuizmo::LOCAL) ? ImGuizmo::WORLD
                                                             : ImGuizmo::LOCAL;
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(current_mode_ == ImGuizmo::LOCAL ? "Local"
                                                             : "World");
        }
        ImGui::EndToolbarGroup();

        // Centered play/pause group. SameLine first to stay on this row.
        const float row_h = ImGui::GetFrameHeight();
        const float play_w = 2.0f * row_h;
        const float win_w = ImGui::GetWindowWidth();
        ImGui::SameLine();
        ImGui::SetCursorPosX((win_w - play_w) * 0.5f);
        DrawPlayStopButtons();

        const float settings_w = row_h;
        ImGui::SameLine();
        ImGui::SetCursorPosX(win_w - settings_w - pad.x);
        bool open_settings = false;
        ImGui::BeginToolbarGroup("##SceneSettingsGroup");
        if (ImGui::ToolbarButton(ICON_LC_ELLIPSIS "##SceneSettings")) {
          open_settings = true;
        }
        ImGui::EndToolbarGroup();
        if (open_settings) {
          ImGui::OpenPopup("SceneCameraSettings");
        }
        ImGui::Unindent(pad.x);
        // Jump to a known Y so the bottom gap is exact regardless of any
        // trailing ItemSpacing the last widget contributed.
        ImGui::SetCursorPosY(row_start_y + row_height + pad.y);
        ImGui::FullWidthSeparator();
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

        // Round only the image corners that touch the panel edge so the
        // image is clipped by the panel's radius instead of poking past it.
        {
          const ImVec2 start = ImGui::GetCursorScreenPos();
          const ImVec2 p_min = start;
          const ImVec2 p_max(p_min.x + image_size.x, p_min.y + image_size.y);
          const ImVec2 win_pos = ImGui::GetWindowPos();
          const ImVec2 win_size = ImGui::GetWindowSize();
          const float eps = 1.0f;
          const bool at_bottom = p_max.y >= win_pos.y + win_size.y - eps;
          const bool at_left = p_min.x <= win_pos.x + eps;
          const bool at_right = p_max.x >= win_pos.x + win_size.x - eps;
          ImDrawFlags corner_flags = ImDrawFlags_RoundCornersNone;
          if (at_bottom && at_left) {
            corner_flags |= ImDrawFlags_RoundCornersBottomLeft;
          }
          if (at_bottom && at_right) {
            corner_flags |= ImDrawFlags_RoundCornersBottomRight;
          }
          ImDrawList* dl = ImGui::GetWindowDrawList();
          if (corner_flags != ImDrawFlags_RoundCornersNone) {
            dl->AddImageRounded(desc, p_min, p_max, ImVec2(0, 0),
                                ImVec2(1, 1), IM_COL32_WHITE,
                                ImGui::GetStyle().WindowRounding,
                                corner_flags);
            ImGui::Dummy(image_size);
          } else {
            ImGui::Image(desc, image_size);
          }
        }

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

        Entity selected_entity = selected_entity_.Resolve();

        // ImGuizmo (uses editor camera matrices, disabled during right-click camera)
        if (selected_entity && !scene_right_active) {
          glm::mat4 view = editor_camera_.view_matrix;
          glm::mat4 proj = editor_camera_.projection;
          proj[1][1] *= -1;
          TransformComponent& transform = selected_entity.GetComponent<TransformComponent>();

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
            Entity parent = selected_entity.GetParent();
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
                selected_entity.GetScene(), selected_entity, old_pos, old_rot,
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

          if (selected_entity_.scene_handle.Resolve()->HasComponent<BoxColliderComponent>(
                  selected_entity_)) {
            auto& box =
                selected_entity_.scene_handle.Resolve()->GetComponent<BoxColliderComponent>(
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

          if (selected_entity_.scene_handle.Resolve()->HasComponent<SphereColliderComponent>(
                  selected_entity_)) {
            auto& sphere =
                selected_entity_.scene_handle.Resolve()->GetComponent<SphereColliderComponent>(
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

        }  // end static_cast<bool>(selected_entity_)

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

void EditorLayer::RenderGameViewportPanel() {
  Renderer* renderer = Engine::renderer().get();

  game_panel_hovered_ = false;
  bool& game_view_open = panel_game_view_;
  if (game_view_open) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    bool game_visible =
        ImGui::Begin(ICON_LC_CAMERA " Game", &game_view_open);
    ImGui::PopStyleVar();
    game_panel_visible_ = game_visible;
    game_panel_focused_ = ImGui::IsWindowFocused();
    game_panel_hovered_ =
        game_visible &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                               ImGuiHoveredFlags_ChildWindows);
    if (game_visible) {
      {
        const ImVec2 pad = ImGui::GetStyle().WindowPadding;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad.y);
        ImGui::Indent(pad.x);
        const float row_start_y = ImGui::GetCursorPosY();
        const float row_height = ImGui::GetFrameHeight() + 4.0f;

        // Centered play/pause group.
        const float row_h = ImGui::GetFrameHeight();
        const float play_w = 2.0f * row_h;
        const float win_w = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX((win_w - play_w) * 0.5f);
        DrawPlayStopButtons();

        const float combo_w = kResolutionComboWidth;
        ImGui::SameLine();
        ImGui::SetCursorPosX(win_w - combo_w - pad.x);
        ImGui::SetNextItemWidth(combo_w);
        const char* labels[kResolutionPresetCount];
        for (int i = 0; i < kResolutionPresetCount; i++) {
          labels[i] = kResolutionPresets[i].label;
        }
        int sel = resolution_preset_index_;
        if (ImGui::Combo("##GameResolution", &sel, labels,
                         kResolutionPresetCount)) {
          resolution_preset_index_ = sel;
          for (const auto& loaded_scene :
               Engine::scene_manager().GetLoadedScenes()) {
            loaded_scene->SetRenderResolution(kResolutionPresets[sel].size);
          }
        }
        ImGui::Unindent(pad.x);
        ImGui::SetCursorPosY(row_start_y + row_height + pad.y);
        ImGui::FullWidthSeparator();
      }

      {
        const glm::vec2 desired_res =
            kResolutionPresets[resolution_preset_index_].size;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        // Sync resolution + find first enabled camera in a single pass.
        bool has_camera = false;
        for (const auto& loaded_scene :
             Engine::scene_manager().GetLoadedScenes()) {
          if (loaded_scene->GetRenderResolution() != desired_res) {
            loaded_scene->SetRenderResolution(desired_res);
          }
          for (auto entity :
               loaded_scene->GetAllEntitiesWith<CameraComponent>()) {
            auto& cam = loaded_scene->GetComponent<CameraComponent>(entity);
            if (!cam.enabled) {
              continue;
            }
            has_camera = true;
            // Free Aspect: camera tracks panel size.
            if (desired_res.x <= 0) {
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
            }
            break;
          }
        }

        if (!has_camera) {
          const char* text = "No camera in scene";
          ImVec2 textSize = ImGui::CalcTextSize(text);
          ImGui::SetCursorPos(
              ImVec2(ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f,
                     ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f));
          ImGui::TextDisabled("%s", text);
        } else {
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

            // Center the image in the available area.
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const ImVec2 image_min(start.x + (avail.x - draw_size.x) * 0.5f,
                                   start.y + (avail.y - draw_size.y) * 0.5f);
            const ImVec2 image_max(image_min.x + draw_size.x,
                                   image_min.y + draw_size.y);

            // Round only the image corners that touch the panel edge so
            // the panel's border radius clips them; smaller images stay
            // fully rectangular.
            const ImVec2 win_pos = ImGui::GetWindowPos();
            const ImVec2 win_size = ImGui::GetWindowSize();
            const float eps = 1.0f;
            const bool at_bottom =
                image_max.y >= win_pos.y + win_size.y - eps;
            const bool at_left = image_min.x <= win_pos.x + eps;
            const bool at_right =
                image_max.x >= win_pos.x + win_size.x - eps;
            ImDrawFlags corner_flags = ImDrawFlags_RoundCornersNone;
            if (at_bottom && at_left) {
              corner_flags |= ImDrawFlags_RoundCornersBottomLeft;
            }
            if (at_bottom && at_right) {
              corner_flags |= ImDrawFlags_RoundCornersBottomRight;
            }
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (corner_flags != ImDrawFlags_RoundCornersNone) {
              dl->AddImageRounded(game_desc, image_min, image_max,
                                  ImVec2(0, 0), ImVec2(1, 1),
                                  IM_COL32_WHITE,
                                  ImGui::GetStyle().WindowRounding,
                                  corner_flags);
            } else {
              dl->AddImage(game_desc, image_min, image_max);
            }
            ImGui::Dummy(avail);

            // Set viewport origin and display size for UI hit testing
            // on all loaded scenes (UI may live in an additive scene)
            for (auto& s : Engine::scene_manager().GetLoadedScenes()) {
              s->SetViewportOrigin({image_min.x, image_min.y});
              s->SetViewportDisplaySize({draw_size.x, draw_size.y});
            }

            // FPS overlay (top-left of image)
            ImVec2 text_pos = ImVec2(image_min.x + 6, image_min.y + 6);
            std::string fpsStr =
                std::format("FPS: {}", static_cast<int>(app_.GetFPS()));
            ImGui::GetWindowDrawList()->AddText(
                text_pos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

            // Resolution overlay (top-right of image)
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
  ImGui::BeginToolbarGroup("##PlayToolbar");

  const bool is_playing = (editor_state_ == EditorState::Playing);
  const bool is_paused = (editor_state_ == EditorState::Paused);
  const bool is_edit = (editor_state_ == EditorState::Edit);
  const auto& compile_result = Engine::script_manager().last_compile_result();
  const bool compiling = Engine::script_manager().IsCompiling();
  const bool has_script_error =
      !compiling && !compile_result.success && !compile_result.output.empty();

  // Active-red: red icon + dim red bg, for Playing/Paused indication.
  const ImVec4 kAccentRed = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
  auto push_red = [&]() {
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(kAccentRed.x, kAccentRed.y, kAccentRed.z,
                                 0.22f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccentRed);
  };

  // Play toggles Edit->Playing / Playing->stop / Paused->resume. Disabled
  // while compiling or when a compile error would block running.
  if (is_edit && (compiling || has_script_error)) {
    ImGui::BeginDisabled();
  }
  if (is_playing) {
    push_red();
  }
  if (ImGui::ToolbarButton(ICON_LC_PLAY "##play", is_playing)) {
    if (is_edit) {
      AutoSave();
      TakeSnapshot();
      editor_state_ = EditorState::Playing;
      scene()->ResetFirstUpdate();
      ImGui::SetWindowFocus(ICON_LC_CAMERA " Game");
    } else if (is_playing) {
      deferred_action_ = DeferredAction::StopPlaying;
    } else if (is_paused) {
      editor_state_ = EditorState::Playing;
    }
    changed = true;
  }
  if (is_playing) {
    ImGui::PopStyleColor(2);
  }
  if (is_edit && (compiling || has_script_error)) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();

  // Pause is disabled in Edit; pressing while Paused resumes.
  if (is_edit) {
    ImGui::BeginDisabled();
  }
  if (is_paused) {
    push_red();
  }
  if (ImGui::ToolbarButton(ICON_LC_PAUSE "##pause", is_paused)) {
    if (is_playing) {
      editor_state_ = EditorState::Paused;
    } else if (is_paused) {
      editor_state_ = EditorState::Playing;
    }
    changed = true;
  }
  if (is_paused) {
    ImGui::PopStyleColor(2);
  }
  if (is_edit) {
    ImGui::EndDisabled();
  }

  ImGui::EndToolbarGroup();
  return changed;
}

bool EditorLayer::FindSpritesInScene(Scene* scene,
                                     const glm::mat4& vp, glm::vec2 pick_ndc,
                                     entt::entity& best, float& best_depth) {
  bool changed = false;
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
        changed = true;
      }
    }
  }
  return changed;
}

EntityRef EditorLayer::FindSpriteAtNDC(glm::vec2 pick_ndc) {
  glm::mat4 vp = editor_camera_.projection * editor_camera_.view_matrix;
  // Vulkan flips Y in projection, undo for NDC comparison
  vp[1][1] *= -1.0f;

  entt::entity best = entt::null;
  float best_depth = std::numeric_limits<float>::max();
  Scene* best_scene = nullptr;

  for (const auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
    Scene* scene = loaded_scene.get();
    if (FindSpritesInScene(scene, vp, pick_ndc, best, best_depth)) {
      best_scene = scene;
    }
  }
  if (best != entt::null && best_scene) {
    return {best, best_scene->GetHandle()};
  }
  return {};
}

}  // namespace wiesel::editor
