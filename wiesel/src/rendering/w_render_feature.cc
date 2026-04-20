
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_render_feature.h"
#include "rendering/w_renderer.h"
#include "w_engine.h"

namespace wiesel {

void CameraResourcePool::SetTexture(const std::string& name,
                                    std::shared_ptr<AttachmentTexture> tex) {
  textures_[name] = std::move(tex);
}

std::shared_ptr<AttachmentTexture> CameraResourcePool::GetTexture(
    const std::string& name) const {
  auto it = textures_.find(name);
  if (it != textures_.end()) {
    return it->second;
  }
  LOG_ERROR(
      "CameraResourcePool: texture '{}' not found - a render feature "
      "may be disabled or not yet initialized",
      name);
  return nullptr;
}

bool CameraResourcePool::HasTexture(const std::string& name) const {
  return textures_.count(name) > 0;
}

void CameraResourcePool::SetDescriptor(const std::string& name,
                                       std::shared_ptr<DescriptorSet> ds) {
  auto it = descriptors_.find(name);
  if (it != descriptors_.end() && it->second) {
    std::shared_ptr<DescriptorSet> old = std::move(it->second);
    Engine::renderer()->GetDeletionQueue().Push([old]() {});
  }
  descriptors_[name] = std::move(ds);
}

std::shared_ptr<DescriptorSet> CameraResourcePool::GetDescriptor(
    const std::string& name) const {
  auto it = descriptors_.find(name);
  if (it != descriptors_.end()) {
    return it->second;
  }
  LOG_ERROR(
      "CameraResourcePool: descriptor '{}' not found - a render feature "
      "may be disabled or not yet initialized",
      name);
  return nullptr;
}

bool CameraResourcePool::HasDescriptor(const std::string& name) const {
  return descriptors_.count(name) > 0;
}

void CameraResourcePool::SetImageView(const std::string& name,
                                      std::shared_ptr<ImageView> view) {
  image_views_[name] = std::move(view);
}

std::shared_ptr<ImageView> CameraResourcePool::GetImageView(
    const std::string& name) const {
  auto it = image_views_.find(name);
  if (it != image_views_.end()) {
    return it->second;
  }
  return nullptr;
}

bool CameraResourcePool::HasImageView(const std::string& name) const {
  return image_views_.count(name) > 0;
}

void CameraResourcePool::SetBuffer(const std::string& name,
                                   std::shared_ptr<UniformBuffer> buf) {
  buffers_[name] = std::move(buf);
}

std::shared_ptr<UniformBuffer> CameraResourcePool::GetBuffer(
    const std::string& name) const {
  auto it = buffers_.find(name);
  if (it != buffers_.end()) {
    return it->second;
  }
  return nullptr;
}

bool CameraResourcePool::HasBuffer(const std::string& name) const {
  return buffers_.count(name) > 0;
}

void CameraResourcePool::Clear() {
  textures_.clear();
  for (auto& [key, ds] : descriptors_) {
    if (ds) {
      std::shared_ptr<DescriptorSet> old = std::move(ds);
      Engine::renderer()->GetDeletionQueue().Push([old]() {});
    }
  }
  descriptors_.clear();
  image_views_.clear();
  buffers_.clear();
}

void RenderResourceRegistry::Register(const std::string& name,
                                      RGResource handle) {
  resources_[name] = handle;
}

RGResource RenderResourceRegistry::Get(const std::string& name) const {
  auto it = resources_.find(name);
  if (it != resources_.end()) {
    return it->second;
  }
  return RGResource{};  // invalid handle
}

bool RenderResourceRegistry::Has(const std::string& name) const {
  return resources_.count(name) > 0;
}

RenderPipeline::RenderPipeline(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

void RenderPipeline::RemoveFeature(const std::string& name) {
  features_.erase(
      std::remove_if(features_.begin(), features_.end(),
                     [&name](const std::shared_ptr<RenderFeature>& f) {
                       return f->GetName() == name;
                     }),
      features_.end());
}

void RenderPipeline::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("RenderPipeline::SetupResources");
  for (auto& feature : features_) {
    if (feature->IsEnabled(ctx)) {
      feature->SetupResources(ctx);
    }
  }
}

void RenderPipeline::BuildRenderGraph(RenderGraph& graph, RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("RenderPipeline::BuildRenderGraph");
  RenderResourceRegistry registry;
  for (auto& feature : features_) {
    if (feature->IsEnabled(ctx)) {
      feature->AddPasses(graph, registry, ctx);
    }
  }
}

std::shared_ptr<DescriptorSet> RenderPipeline::GetFinalOutputDescriptor(
    CameraResourcePool& pool) const {
  return pool.GetDescriptor("PipelineOutputDescriptor");
}

std::shared_ptr<AttachmentTexture> RenderPipeline::GetFinalOutputImage(
    CameraResourcePool& pool) const {
  return pool.GetTexture("PipelineOutput");
}

}  // namespace wiesel
