
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

#include <scene/w_components.hpp>
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

  Ref<AttachmentTexture> GeometryNormalImage;
  Ref<AttachmentTexture> GeometryNormalResolveImage;
  Ref<AttachmentTexture> GeometryDepthImage;
  Ref<AttachmentTexture> GeometryDepthResolveImage;
  Ref<AttachmentTexture> GeometryAlbedoImage;
  Ref<AttachmentTexture> GeometryAlbedoResolveImage;
  Ref<AttachmentTexture> GeometryViewPosImage;
  Ref<AttachmentTexture> GeometryViewPosResolveImage;
  Ref<AttachmentTexture> GeometryWorldPosImage;
  Ref<AttachmentTexture> GeometryWorldPosResolveImage;
  Ref<AttachmentTexture> GeometryMaterialImage;
  Ref<AttachmentTexture> GeometryMaterialResolveImage;
  Ref<AttachmentTexture> GeometryDepthStencil;

  Ref<AttachmentTexture> SSAOColorImage;
  Ref<AttachmentTexture> SSAOBlurColorImage;

  Ref<AttachmentTexture> LightingColorImage;
  Ref<AttachmentTexture> LightingColorResolveImage;

  Ref<AttachmentTexture> SpriteColorImage;

  Ref<AttachmentTexture> CompositeColorImage;
  Ref<AttachmentTexture> CompositeColorResolveImage;

  Ref<Framebuffer> GeometryFramebuffer;
  Ref<Framebuffer> SSAOGenFramebuffer;
  Ref<Framebuffer> SSAOBlurFramebuffer;
  Ref<Framebuffer> LightingFramebuffer;
  Ref<Framebuffer> SpriteFramebuffer;
  Ref<Framebuffer> CompositeFramebuffer;
  Ref<DescriptorSet> GlobalDescriptor;
  Ref<DescriptorSet> ShadowDescriptor;
  Ref<DescriptorSet> GeometryOutputDescriptor;
  Ref<DescriptorSet> SSAOOutputDescriptor;
  Ref<DescriptorSet> SSAOBlurOutputDescriptor;
  Ref<DescriptorSet> LightingOutputDescriptor;
  Ref<DescriptorSet> SpriteOutputDescriptor;
  Ref<DescriptorSet> CompositeOutputDescriptor;
  Ref<DescriptorSet> SSAOGenDescriptor;
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

  Ref<AttachmentTexture> GeometryNormalImage;
  Ref<AttachmentTexture> GeometryNormalResolveImage;
  Ref<AttachmentTexture> GeometryAlbedoImage;
  Ref<AttachmentTexture> GeometryAlbedoResolveImage;
  Ref<AttachmentTexture> GeometryViewPosImage;
  Ref<AttachmentTexture> GeometryViewPosResolveImage;
  Ref<AttachmentTexture> GeometryWorldPosImage;
  Ref<AttachmentTexture> GeometryWorldPosResolveImage;
  Ref<AttachmentTexture> GeometryDepthImage;
  Ref<AttachmentTexture> GeometryDepthResolveImage;
  Ref<AttachmentTexture> GeometryDepthStencil;
  Ref<AttachmentTexture> GeometryMaterialImage;
  Ref<AttachmentTexture> GeometryMaterialResolveImage;

  Ref<AttachmentTexture> SSAOColorImage;
  Ref<AttachmentTexture> SSAOBlurColorImage;

  Ref<AttachmentTexture> LightingColorImage;
  Ref<AttachmentTexture> LightingColorResolveImage;

  Ref<AttachmentTexture> SpriteColorImage;

  Ref<AttachmentTexture> CompositeColorImage;
  Ref<AttachmentTexture> CompositeColorResolveImage;

  Ref<Framebuffer> GeometryFramebuffer;
  Ref<Framebuffer> SSAOGenFramebuffer;
  Ref<Framebuffer> SSAOBlurFramebuffer;
  Ref<Framebuffer> LightingFramebuffer;
  Ref<Framebuffer> SpriteFramebuffer;
  Ref<Framebuffer> CompositeFramebuffer;
  Ref<DescriptorSet> GlobalDescriptor; // to draw geometry
  Ref<DescriptorSet> ShadowDescriptor; // to draw geometry to shadow pass
  Ref<DescriptorSet> GeometryOutputDescriptor; // to draw geometry pass output
  Ref<DescriptorSet> SSAOOutputDescriptor; // to draw ssao pass output
  Ref<DescriptorSet> SSAOBlurOutputDescriptor; // to draw ssao blur pass output
  Ref<DescriptorSet> LightingOutputDescriptor; // to draw lighting pass output
  Ref<DescriptorSet> SpriteOutputDescriptor; // to draw sprite pass output
  Ref<DescriptorSet> CompositeOutputDescriptor; // to draw composite pass output
  Ref<DescriptorSet>
      SSAOGenDescriptor; // used to render geometry pass output to ssao pass
  FrustumPlanes Planes;

  // Shadow stuff
  bool DoesShadowPass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> ShadowMapCascades;
  Ref<AttachmentTexture> ShadowDepthStencil;
  std::array<Ref<Framebuffer>, WIESEL_SHADOW_CASCADE_COUNT> ShadowFramebuffers;

  void TransferFrom(CameraComponent& camera, TransformComponent& transform) {
    // Perhaps we could do this differently?
    // At first, we had 3 variables to update, but now we have a lot more...

    Position = transform.Position;
    ViewMatrix = camera.ViewMatrix;
    Projection = camera.Projection;
    InvProjection = camera.InvProjection;
    ViewportSize = camera.ViewportSize;
    NearPlane = camera.NearPlane;
    FarPlane = camera.FarPlane;

    GeometryNormalImage = camera.GeometryNormalImage;
    GeometryNormalResolveImage = camera.GeometryNormalResolveImage;
    GeometryAlbedoImage = camera.GeometryAlbedoImage;
    GeometryAlbedoResolveImage = camera.GeometryAlbedoResolveImage;
    GeometryViewPosImage = camera.GeometryViewPosImage;
    GeometryViewPosResolveImage = camera.GeometryViewPosResolveImage;
    GeometryWorldPosImage = camera.GeometryWorldPosImage;
    GeometryWorldPosResolveImage = camera.GeometryWorldPosResolveImage;
    GeometryDepthImage = camera.GeometryDepthImage;
    GeometryDepthResolveImage = camera.GeometryDepthResolveImage;
    GeometryDepthStencil = camera.GeometryDepthStencil;
    GeometryMaterialImage = camera.GeometryMaterialImage;
    GeometryMaterialResolveImage = camera.GeometryMaterialResolveImage;

    SSAOColorImage = camera.SSAOColorImage;
    SSAOBlurColorImage = camera.SSAOBlurColorImage;

    LightingColorImage = camera.LightingColorImage;
    LightingColorResolveImage = camera.LightingColorResolveImage;

    SpriteColorImage = camera.SpriteColorImage;

    CompositeColorImage = camera.CompositeColorImage;
    CompositeColorResolveImage = camera.CompositeColorResolveImage;

    GeometryFramebuffer = camera.GeometryFramebuffer;
    SSAOGenFramebuffer = camera.SSAOGenFramebuffer;
    SSAOBlurFramebuffer = camera.SSAOBlurFramebuffer;
    LightingFramebuffer = camera.LightingFramebuffer;
    SpriteFramebuffer = camera.SpriteFramebuffer;
    CompositeFramebuffer = camera.CompositeFramebuffer;
    GlobalDescriptor = camera.GlobalDescriptor;
    ShadowDescriptor = camera.ShadowDescriptor;
    GeometryOutputDescriptor = camera.GeometryOutputDescriptor;
    SSAOOutputDescriptor = camera.SSAOOutputDescriptor;
    SSAOBlurOutputDescriptor = camera.SSAOBlurOutputDescriptor;
    LightingOutputDescriptor = camera.LightingOutputDescriptor;
    SpriteOutputDescriptor = camera.SpriteOutputDescriptor;
    CompositeOutputDescriptor = camera.CompositeOutputDescriptor;
    SSAOGenDescriptor = camera.SSAOGenDescriptor;
    Planes = camera.Planes;

    DoesShadowPass = camera.DoesShadowPass;
    ShadowMapCascades = camera.ShadowMapCascades;
    ShadowDepthStencil = camera.ShadowDepthStencil;
    ShadowFramebuffers = camera.ShadowFramebuffers;
  }

};

}  // namespace Wiesel
