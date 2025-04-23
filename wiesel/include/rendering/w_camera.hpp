
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "events/w_appevents.hpp"
#include "util/w_uuid.hpp"
#include "w_framebuffer.hpp"
#include "w_pch.hpp"
#include "w_texture.hpp"

namespace Wiesel {

struct FrustumPlanes {
  glm::vec4 Left, Right, Bottom, Top, Near, Far;
};
struct Cascade {
  float SplitDepth;
  glm::mat4 ViewProjMatrix;
};

struct CameraComponent {
  CameraComponent() = default;
  CameraComponent(const CameraComponent&) = default;
  ~CameraComponent() = default;

  float FieldOfView = 60;
  float NearPlane = 0.1f;
  float FarPlane = 400.0f;
  float AspectRatio = 1.0;

  glm::mat4 ViewMatrix;
  glm::mat4 Projection;
  glm::mat4 InvProjection;
  glm::vec2 ViewportSize;
  glm::mat4 InvViewMatrix;

  Ref<AttachmentTexture> GeometryColorImage;
  Ref<AttachmentTexture> GeometryDepthStencil;
  Ref<AttachmentTexture> GeometryColorResolveImage;
  Ref<AttachmentTexture> LightingColorImage;
  Ref<AttachmentTexture> LightingColorResolveImage;
  Ref<AttachmentTexture> CompositeColorImage;
  Ref<AttachmentTexture> CompositeColorResolveImage;
  Ref<Framebuffer> GeometryFramebuffer;
  Ref<Framebuffer> LightingFramebuffer;
  Ref<Framebuffer> CompositeFramebuffer;
  Ref<DescriptorData> Descriptors;
  Ref<DescriptorData> ShadowDescriptors;
  FrustumPlanes Planes;

  // Shadow stuff
  bool DoesShadowPass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> ShadowMapCascades;
  Ref<AttachmentTexture> ShadowDepthStencil;
  std::array<Ref<ImageView>, WIESEL_SHADOW_CASCADE_COUNT> ShadowDepthViews;
  Ref<ImageView> ShadowDepthViewArray;
  std::array<Ref<Framebuffer>, WIESEL_SHADOW_CASCADE_COUNT> ShadowFramebuffers;

  glm::vec3 PreviousLightDir;
  bool ForceLightReset = false;
  bool IsPosChanged = true;
  bool IsViewChanged = true;
  bool IsAnyChanged = true;
  bool IsEnabled = true;

  void UpdateProjection();
  void UpdateView(glm::vec3& position, glm::vec3& rotation);
  void UpdateAll();

  void ComputeCascades(const glm::vec3& lightDir);
  void ExtractFrustumPlanes();

};

struct CameraData {
  CameraData() = default;
  CameraData(glm::vec3 position, glm::mat4 viewMatrix, glm::mat4 projection)
      : Position(position), ViewMatrix(viewMatrix), Projection(projection){};
  CameraData(const CameraData&) = default;
  ~CameraData() = default;

  glm::vec3 Position;
  glm::mat4 ViewMatrix;
  glm::mat4 Projection;
  glm::mat4 InvProjection;
  glm::vec2 ViewportSize;
  float NearPlane = 0.1f;
  float FarPlane = 1000.0f;
  Ref<AttachmentTexture> GeometryColorImage;
  Ref<AttachmentTexture> GeometryDepthStencil;
  Ref<AttachmentTexture> GeometryColorResolveImage;
  Ref<AttachmentTexture> CompositeColorImage;
  Ref<AttachmentTexture> CompositeColorResolveImage;
  Ref<AttachmentTexture> LightingColorImage;
  Ref<AttachmentTexture> LightingColorResolveImage;
  Ref<Framebuffer> GeometryFramebuffer;
  Ref<Framebuffer> LightingFramebuffer;
  Ref<Framebuffer> CompositeFramebuffer;
  Ref<DescriptorData> Descriptors;
  Ref<DescriptorData> ShadowDescriptors;
  FrustumPlanes Planes;

  // Shadow stuff
  bool DoesShadowPass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> ShadowMapCascades;
  Ref<AttachmentTexture> ShadowDepthStencil;
  std::array<Ref<ImageView>, WIESEL_SHADOW_CASCADE_COUNT> ShadowDepthViews;
  Ref<ImageView> ShadowDepthViewArray;
  std::array<Ref<Framebuffer>, WIESEL_SHADOW_CASCADE_COUNT> ShadowFramebuffers;

};

}  // namespace Wiesel
