//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_rmlui_renderer.h"

#include <stb_image.h>

#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "util/w_logger.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

RmlRenderInterface::RmlRenderInterface(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

RmlRenderInterface::~RmlRenderInterface() {
  if (fullscreen_quad_) {
    ReleaseGeometry(fullscreen_quad_);
    fullscreen_quad_ = 0;
  }
  loaded_textures_.clear();
  texture_descriptors_.clear();
}

static std::shared_ptr<Pipeline> CreateRmlPipeline(
    PipelineProperties props, std::shared_ptr<RenderPass> render_pass,
    std::shared_ptr<DescriptorSetLayout> layout,
    std::shared_ptr<RmlRenderInterface::PushConstantData> push_data,
    std::shared_ptr<Shader> vert, std::shared_ptr<Shader> frag) {
  auto pipeline = std::make_shared<Pipeline>(props);
  pipeline->SetRenderPass(render_pass);
  pipeline->AddInputLayout(layout);
  pipeline->AddPushConstant(push_data, VK_SHADER_STAGE_VERTEX_BIT);
  pipeline->SetVertexData(RmlVertex::GetBindingDescription(),
                          RmlVertex::GetAttributeDescriptions());
  pipeline->AddShader(vert);
  pipeline->AddShader(frag);
  pipeline->Bake();
  return pipeline;
}

void RmlRenderInterface::Init(std::shared_ptr<RenderPass> render_pass) {
  render_pass_ = std::move(render_pass);

  descriptor_layout_ = std::make_shared<DescriptorSetLayout>();
  descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  descriptor_layout_->Bake();

  push_constant_data_ = std::make_shared<PushConstantData>();

  auto vert = renderer_->CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                                       ShaderSourceSource,
                                       "engine://shaders/rmlui_shader.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/rmlui_shader.frag"});

  PipelineProperties base_props;
  base_props.sampling_mode = SamplingMode::DISABLED;
  base_props.cull_mode = CullModeNone;
  base_props.enable_wireframe = false;
  base_props.enable_alpha_blending = true;
  base_props.enable_depth_test = false;
  base_props.enable_depth_write = false;

  // 1. Normal pipeline (no stencil)
  pipeline_ = CreateRmlPipeline(base_props, render_pass_, descriptor_layout_,
                                push_constant_data_, vert, frag);

  // 2. Stencil test pipeline (EQUAL compare, color write ON)
  PipelineProperties stencil_test_props = base_props;
  stencil_test_props.enable_stencil_test = true;
  stencil_test_props.stencil_compare_op = VK_COMPARE_OP_EQUAL;
  stencil_test_props.stencil_pass_op = VK_STENCIL_OP_KEEP;
  pipeline_stencil_test_ =
      CreateRmlPipeline(stencil_test_props, render_pass_, descriptor_layout_,
                        push_constant_data_, vert, frag);

  // 3. Stencil set pipeline (ALWAYS compare, REPLACE op, no color write)
  PipelineProperties stencil_set_props = base_props;
  stencil_set_props.enable_stencil_test = true;
  stencil_set_props.stencil_compare_op = VK_COMPARE_OP_ALWAYS;
  stencil_set_props.stencil_pass_op = VK_STENCIL_OP_REPLACE;
  stencil_set_props.color_write_enabled = false;
  pipeline_stencil_set_ =
      CreateRmlPipeline(stencil_set_props, render_pass_, descriptor_layout_,
                        push_constant_data_, vert, frag);

  // 4. Stencil incr pipeline (ALWAYS compare, INCR op, no color write)
  PipelineProperties stencil_incr_props = base_props;
  stencil_incr_props.enable_stencil_test = true;
  stencil_incr_props.stencil_compare_op = VK_COMPARE_OP_ALWAYS;
  stencil_incr_props.stencil_pass_op = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  stencil_incr_props.color_write_enabled = false;
  pipeline_stencil_incr_ =
      CreateRmlPipeline(stencil_incr_props, render_pass_, descriptor_layout_,
                        push_constant_data_, vert, frag);

  // Blank texture for untextured geometry
  uint32_t white_pixel = 0xFFFFFFFF;
  TextureProps tex_props;
  tex_props.width = 1;
  tex_props.height = 1;
  tex_props.image_format = VK_FORMAT_R8G8B8A8_UNORM;
  tex_props.generate_mipmaps = false;
  blank_texture_ = renderer_->CreateTexture(&white_pixel, 4, tex_props, {});

  blank_descriptor_ = std::make_shared<DescriptorSet>();
  blank_descriptor_->SetLayout(descriptor_layout_);
  blank_descriptor_->AddCombinedImageSampler(0, blank_texture_->image_view_,
                                             blank_texture_->sampler_);
  blank_descriptor_->Bake();

  // Full-screen quad for mid-pass stencil clearing
  std::vector<Rml::Vertex> quad_verts(4);
  quad_verts[0].position = {-10000, -10000};
  quad_verts[1].position = {10000, -10000};
  quad_verts[2].position = {10000, 10000};
  quad_verts[3].position = {-10000, 10000};
  for (auto& v : quad_verts) {
    v.colour = Rml::ColourbPremultiplied(255, 255, 255, 255);
    v.tex_coord = {0, 0};
  }
  std::vector<int> quad_indices = {0, 1, 2, 0, 2, 3};
  fullscreen_quad_ = CompileGeometry(
      Rml::Span<const Rml::Vertex>(quad_verts.data(), quad_verts.size()),
      Rml::Span<const int>(quad_indices.data(), quad_indices.size()));
}

void RmlRenderInterface::BuildProjection() {
  Rml::Matrix4f rml_projection = Rml::Matrix4f::ProjectOrtho(
      0.0f, viewport_size_.x, viewport_size_.y, 0.0f, -10000.0f, 10000.0f);

  Rml::Matrix4f correction;
  correction.SetColumns(Rml::Vector4f(1.0f, 0.0f, 0.0f, 0.0f),
                        Rml::Vector4f(0.0f, -1.0f, 0.0f, 0.0f),
                        Rml::Vector4f(0.0f, 0.0f, 0.5f, 0.0f),
                        Rml::Vector4f(0.0f, 0.0f, 0.5f, 1.0f));

  projection_ = correction * rml_projection;
}

void RmlRenderInterface::RenderGeometryWithPipeline(
    Pipeline* pipeline, Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation, Rml::TextureHandle texture) {
  PROFILE_ZONE_SCOPED_N("RmlUi::RenderGeometry");
  auto* compiled = reinterpret_cast<RmlCompiledGeometry*>(geometry);
  if (!compiled || !active_cmd_) {
    return;
  }

  pipeline->Bind(PipelineBindPointGraphics, active_cmd_);

  // Match official RmlUi Vulkan backend: translation added to position in
  // shader, then multiplied by projection * transform.
  Rml::Matrix4f final_mat = projection_ * transform_rml_;
  memcpy(&push_constant_data_->transform, &final_mat, sizeof(glm::mat4));
  push_constant_data_->translate = {translation.x, translation.y};
  pipeline->PushConstants(active_cmd_);

  auto desc = GetOrCreateDescriptor(texture);
  VkDescriptorSet ds = desc->descriptor_set_;
  vkCmdBindDescriptorSets(active_cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->layout_, 0, 1, &ds, 0, nullptr);

  VkBuffer vb = compiled->vertex_buffer->buffer_handle_;
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(active_cmd_, 0, 1, &vb, &offset);
  vkCmdBindIndexBuffer(active_cmd_, compiled->index_buffer->buffer_handle_, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(active_cmd_, compiled->index_count, 1, 0, 0, 0);
}

Rml::CompiledGeometryHandle RmlRenderInterface::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
  PROFILE_ZONE_SCOPED_N("RmlUi::CompileGeometry");
  auto* compiled = new RmlCompiledGeometry();

  std::vector<RmlVertex> verts(vertices.size());
  for (size_t i = 0; i < vertices.size(); i++) {
    const Rml::Vertex& rv = vertices[i];
    verts[i].position = {rv.position.x, rv.position.y};
    verts[i].color = rv.colour.red | (rv.colour.green << 8) |
                     (rv.colour.blue << 16) | (rv.colour.alpha << 24);
    verts[i].tex_coord = {rv.tex_coord.x, rv.tex_coord.y};
  }
  compiled->vertex_buffer = renderer_->CreateVertexBuffer(verts);

  std::vector<Index> idx(indices.size());
  for (size_t i = 0; i < indices.size(); i++) {
    idx[i] = static_cast<Index>(indices[i]);
  }
  compiled->index_buffer = renderer_->CreateIndexBuffer(idx);
  compiled->index_count = static_cast<uint32_t>(indices.size());

  return reinterpret_cast<Rml::CompiledGeometryHandle>(compiled);
}

void RmlRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                        Rml::Vector2f translation,
                                        Rml::TextureHandle texture) {
  if (!active_cmd_) {
    return;
  }

  Pipeline* active_pipeline =
      clip_mask_enabled_ ? pipeline_stencil_test_.get() : pipeline_.get();

  if (clip_mask_enabled_) {
    vkCmdSetStencilReference(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                             static_cast<uint32_t>(stencil_ref_));
    vkCmdSetStencilCompareMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                               0xFF);
    vkCmdSetStencilWriteMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
  }

  RenderGeometryWithPipeline(active_pipeline, geometry, translation, texture);
}

void RmlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
  delete reinterpret_cast<RmlCompiledGeometry*>(geometry);
}

Rml::TextureHandle RmlRenderInterface::LoadTexture(
    Rml::Vector2i& texture_dimensions, const Rml::String& source) {
  auto file = Engine::vfs()->Open(source);
  if (!file) {
    LOG_WARN("[RmlUi] Failed to load texture: {}", source);
    return 0;
  }

  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* pixels =
      stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()), &w, &h,
                            &channels, STBI_rgb_alpha);
  if (!pixels) {
    return 0;
  }

  TextureProps props;
  props.width = w;
  props.height = h;
  props.image_format = VK_FORMAT_R8G8B8A8_UNORM;
  props.generate_mipmaps = true;

  auto texture = renderer_->CreateTexture(pixels, 4, props, {});
  stbi_image_free(pixels);

  if (!texture || !texture->is_allocated_) {
    return 0;
  }

  Rml::TextureHandle handle = next_texture_id_++;
  loaded_textures_[handle] = texture;
  texture_dimensions.x = w;
  texture_dimensions.y = h;
  return handle;
}

Rml::TextureHandle RmlRenderInterface::GenerateTexture(
    Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
  TextureProps props;
  props.width = source_dimensions.x;
  props.height = source_dimensions.y;
  props.image_format = VK_FORMAT_R8G8B8A8_UNORM;
  props.generate_mipmaps = false;

  auto texture = renderer_->CreateTexture(const_cast<Rml::byte*>(source.data()),
                                          4, props, {});

  if (!texture || !texture->is_allocated_) {
    return 0;
  }

  Rml::TextureHandle handle = next_texture_id_++;
  loaded_textures_[handle] = texture;
  return handle;
}

void RmlRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
  texture_descriptors_.erase(texture);
  loaded_textures_.erase(texture);
}

void RmlRenderInterface::EnableScissorRegion(bool enable) {
  scissor_enabled_ = enable;
  if (!enable && active_cmd_) {
    renderer_->SetScissor(0, 0, static_cast<int>(viewport_size_.x),
                          static_cast<int>(viewport_size_.y), active_cmd_);
  }
}

void RmlRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
  if (active_cmd_ && scissor_enabled_) {
    renderer_->SetScissor(region.Left(), region.Top(), region.Width(),
                          region.Height(), active_cmd_);
  }
}

void RmlRenderInterface::EnableClipMask(bool enable) {
  clip_mask_enabled_ = enable;
  if (!enable) {
    stencil_ref_ = 0;
  }
}

void RmlRenderInterface::RenderToClipMask(Rml::ClipMaskOperation operation,
                                          Rml::CompiledGeometryHandle geometry,
                                          Rml::Vector2f translation) {
  PROFILE_ZONE_SCOPED_N("RmlUi::RenderToClipMask");
  if (!active_cmd_) {
    return;
  }

  using Rml::ClipMaskOperation;

  switch (operation) {
    case ClipMaskOperation::Set: {
      // Clear stencil to 0 via fullscreen quad with REPLACE ref=0
      vkCmdSetStencilReference(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
      vkCmdSetStencilCompareMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                                 0xFF);
      vkCmdSetStencilWriteMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                               0xFF);
      RenderGeometryWithPipeline(pipeline_stencil_set_.get(), fullscreen_quad_,
                                 {0, 0}, 0);

      // Write 1 where the mask geometry is
      vkCmdSetStencilReference(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
      RenderGeometryWithPipeline(pipeline_stencil_set_.get(), geometry,
                                 translation, 0);
      stencil_ref_ = 1;
      break;
    }

    case ClipMaskOperation::SetInverse: {
      // Fill stencil with 1
      vkCmdSetStencilReference(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
      vkCmdSetStencilCompareMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                                 0xFF);
      vkCmdSetStencilWriteMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                               0xFF);
      RenderGeometryWithPipeline(pipeline_stencil_set_.get(), fullscreen_quad_,
                                 {0, 0}, 0);

      // Write 0 where the mask geometry is (punch hole)
      vkCmdSetStencilReference(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
      RenderGeometryWithPipeline(pipeline_stencil_set_.get(), geometry,
                                 translation, 0);
      stencil_ref_ = 1;
      break;
    }

    case ClipMaskOperation::Intersect: {
      // Increment stencil where the geometry is
      vkCmdSetStencilReference(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
      vkCmdSetStencilCompareMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                                 0xFF);
      vkCmdSetStencilWriteMask(active_cmd_, VK_STENCIL_FACE_FRONT_AND_BACK,
                               0xFF);
      RenderGeometryWithPipeline(pipeline_stencil_incr_.get(), geometry,
                                 translation, 0);
      stencil_ref_++;
      break;
    }
  }
}

void RmlRenderInterface::SetTransform(const Rml::Matrix4f* transform) {
  if (transform) {
    transform_rml_ = *transform;
  } else {
    transform_rml_ = Rml::Matrix4f::Identity();
  }
}

std::shared_ptr<DescriptorSet> RmlRenderInterface::GetOrCreateDescriptor(
    Rml::TextureHandle texture) {
  if (texture == 0) {
    return blank_descriptor_;
  }

  auto it = texture_descriptors_.find(texture);
  if (it != texture_descriptors_.end()) {
    return it->second;
  }

  auto tex_it = loaded_textures_.find(texture);
  if (tex_it == loaded_textures_.end()) {
    return blank_descriptor_;
  }

  auto& tex = tex_it->second;
  auto desc = std::make_shared<DescriptorSet>();
  desc->SetLayout(descriptor_layout_);
  desc->AddCombinedImageSampler(0, tex->image_view_, tex->sampler_);
  desc->Bake();

  texture_descriptors_[texture] = desc;
  return desc;
}

void RmlRenderInterface::RenderToTexture(VkCommandBuffer cmd,
                                         Rml::Context* context,
                                         glm::vec2 size) {
  PROFILE_ZONE_SCOPED_N("RmlUi::RenderToTexture");
  if (!context || !cmd) {
    return;
  }
  active_cmd_ = cmd;
  viewport_size_ = size;
  clip_mask_enabled_ = false;
  stencil_ref_ = 0;
  BuildProjection();
  context->Render();
  active_cmd_ = VK_NULL_HANDLE;
}

}  // namespace Wiesel
