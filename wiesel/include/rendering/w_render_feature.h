
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <entt/entt.hpp>
#include "util/w_utils.h"
#include "w_descriptor.h"
#include "w_framebuffer.h"
#include "w_pch.h"
#include "w_rendergraph.h"
#include "w_texture.h"

namespace wiesel {

class Renderer;
class Scene;
class MultiScene;
struct CameraComponent;
class RenderPipeline;

// Dynamic resource storage. Replaces flat CameraComponent fields.
// Resources are stored by name and looked up at runtime.
class CameraResourcePool {
 public:
  void SetTexture(const std::string& name,
                  std::shared_ptr<AttachmentTexture> tex);
  std::shared_ptr<AttachmentTexture> GetTexture(const std::string& name) const;
  bool HasTexture(const std::string& name) const;

  void SetFramebuffer(const std::string& name, std::shared_ptr<Framebuffer> fb);
  std::shared_ptr<Framebuffer> GetFramebuffer(const std::string& name) const;
  bool HasFramebuffer(const std::string& name) const;

  void SetDescriptor(const std::string& name,
                     std::shared_ptr<DescriptorSet> ds);
  std::shared_ptr<DescriptorSet> GetDescriptor(const std::string& name) const;
  bool HasDescriptor(const std::string& name) const;

  void SetImageView(const std::string& name, std::shared_ptr<ImageView> view);
  std::shared_ptr<ImageView> GetImageView(const std::string& name) const;
  bool HasImageView(const std::string& name) const;

  void SetBuffer(const std::string& name, std::shared_ptr<UniformBuffer> buf);
  std::shared_ptr<UniformBuffer> GetBuffer(const std::string& name) const;
  bool HasBuffer(const std::string& name) const;

  void Clear();

 private:
  std::unordered_map<std::string, std::shared_ptr<AttachmentTexture>> textures_;
  std::unordered_map<std::string, std::shared_ptr<Framebuffer>> framebuffers_;
  std::unordered_map<std::string, std::shared_ptr<DescriptorSet>> descriptors_;
  std::unordered_map<std::string, std::shared_ptr<ImageView>> image_views_;
  std::unordered_map<std::string, std::shared_ptr<UniformBuffer>> buffers_;
};

// Context passed to features during resource setup and pass building.
struct RenderContext {
  Renderer& renderer;
  MultiScene& scenes;
  CameraComponent& camera;
  CameraResourcePool& resources;
  glm::vec2 viewport_size;
  bool use_msaa_resolve;
  bool is_external = false;
  bool show_grid = false;
  entt::entity camera_entity = entt::null;
};

// Named render graph resource registry.
// Features register their outputs by well-known names so downstream features
// can look them up as inputs.
class RenderResourceRegistry {
 public:
  void Register(const std::string& name, RGResource handle);
  RGResource Get(const std::string& name) const;
  bool Has(const std::string& name) const;

 private:
  std::unordered_map<std::string, RGResource> resources_;
};

// Abstract base class for a render feature.
// Each feature encapsulates a reusable rendering effect (shadow mapping,
// SSAO, bloom, etc.) and owns its own GPU pipelines and render passes.
class RenderFeature {
 public:
  virtual ~RenderFeature() = default;
  virtual const std::string& GetName() const = 0;

  // Create GPU resources (textures, framebuffers, descriptors) and store
  // them in ctx.resources. Called when camera resources are dirty.
  virtual void SetupResources(RenderContext& ctx) = 0;

  // Add passes to the render graph. Import resources from the pool,
  // wire reads/writes, and register outputs in the registry.
  virtual void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                         RenderContext& ctx) = 0;

  // Whether this feature should run this frame.
  virtual bool IsEnabled(const RenderContext& ctx) const { return true; }
};

// Ordered list of features = a complete render pipeline.
// Applications create a RenderPipeline, add features to it, and assign
// it to the scene or to individual cameras.
class RenderPipeline {
 public:
  explicit RenderPipeline(std::shared_ptr<Renderer> renderer);

  template <typename T, typename... Args>
  T& AddFeature(Args&&... args) {
    auto feature = std::make_shared<T>(std::forward<Args>(args)...);
    T& ref = *feature;
    features_.push_back(std::move(feature));
    return ref;
  }

  void RemoveFeature(const std::string& name);

  // Setup all feature resources for a camera.
  void SetupResources(RenderContext& ctx);

  // Build the render graph for a camera (calls each feature's AddPasses).
  void BuildRenderGraph(RenderGraph& graph, RenderContext& ctx);

  // Well-known resource names:
  //   "PipelineOutput"           - final color attachment for the pipeline
  //   "PipelineOutputDescriptor" - descriptor set for PipelineOutput
  //   "GlobalDescriptor"         - per-frame global UBO descriptor
  //   "ShadowGlobalDescriptor"   - shadow pass global UBO descriptor
  //   "geometry.albedo"          - G-buffer albedo
  //   "geometry.normal"          - G-buffer normals
  //   "geometry.depth_stencil"   - G-buffer depth/stencil
  //   "geometry.entity_id"       - G-buffer entity ID for picking
  //   "geometry.output"          - G-buffer output descriptor
  //   "lighting.output"          - lighting pass output descriptor
  //   "ssao.output"              - SSAO output descriptor
  //   "shadow.depth_stencil"     - shadow map depth
  //
  // Naming convention: "{feature}.{resource}" (e.g. "bloom.composite").
  // Custom features should use a unique prefix to avoid collisions.

  // Get the final output from the "PipelineOutput" convention.
  std::shared_ptr<DescriptorSet> GetFinalOutputDescriptor(
      CameraResourcePool& pool) const;
  std::shared_ptr<AttachmentTexture> GetFinalOutputImage(
      CameraResourcePool& pool) const;

  const std::vector<std::shared_ptr<RenderFeature>>& GetFeatures() const {
    return features_;
  }

  std::shared_ptr<Renderer> GetRenderer() const { return renderer_; }

 private:
  std::shared_ptr<Renderer> renderer_;
  std::vector<std::shared_ptr<RenderFeature>> features_;
};

}  // namespace wiesel
