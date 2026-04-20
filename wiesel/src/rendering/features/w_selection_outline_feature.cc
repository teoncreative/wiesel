//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_selection_outline_feature.h"
#include "asset/w_asset_manager.h"
#include "rendering/w_framebuffer.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_components.h"
#include "scene/w_scene_manager.h"
#include "w_engine.h"

namespace wiesel {

SelectionOutlineFeature::SelectionOutlineFeature(
    std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});

  // Mask pass
  mask_render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                                   "SelectionMask RenderPass");
  mask_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                   .format = VK_FORMAT_R8_UNORM,
                                   .msaa_mode = SamplingMode::DISABLED});
  mask_render_pass_->Bake();

  auto mask_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/selection_mask.vert"});
  auto mask_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/selection_mask.frag"});

  mask_push_constant_ = std::make_shared<SelectionMaskPushConstant>();

  {
    PipelineProperties props{};
    props.cull_mode = CullModeNone;
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    mask_pipeline_ = std::make_shared<Pipeline>(props);
  }
  mask_pipeline_->SetRenderPass(mask_render_pass_);
  mask_pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                                Vertex3D::GetAttributeDescriptions());
  mask_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Bone"));
  mask_pipeline_->AddPushConstant(mask_push_constant_,
                                  VK_SHADER_STAGE_VERTEX_BIT);
  mask_pipeline_->AddShader(mask_vert);
  mask_pipeline_->AddShader(mask_frag);
  mask_pipeline_->Bake();

  // Blur passes (shared R8 render pass)
  blur_render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                                   "SelectionBlur RenderPass");
  blur_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                   .format = VK_FORMAT_R8_UNORM,
                                   .msaa_mode = SamplingMode::DISABLED});
  blur_render_pass_->Bake();

  blur_push_constant_ = std::make_shared<SelectionBlurPushConstant>();

  auto single_tex_layout = std::make_shared<DescriptorSetLayout>();
  single_tex_layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  single_tex_layout->Bake();

  auto blur_h_frag =
      renderer_->CreateShader({ShaderTypeFragment,
                               ShaderLangGLSL,
                               "main",
                               ShaderSourceSource,
                               "engine://shaders/gaussian_blur.frag",
                               {"BLUR_SINGLE_CHANNEL", "USE_PUSH_RADIUS"}});
  auto blur_v_frag = renderer_->CreateShader(
      {ShaderTypeFragment,
       ShaderLangGLSL,
       "main",
       ShaderSourceSource,
       "engine://shaders/gaussian_blur.frag",
       {"BLUR_SINGLE_CHANNEL", "USE_PUSH_RADIUS", "BLUR_VERTICAL"}});

  auto create_blur_pipeline = [&](std::shared_ptr<Shader> frag) {
    PipelineProperties props{};
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    auto pipeline = std::make_shared<Pipeline>(props);
    pipeline->SetRenderPass(blur_render_pass_);
    pipeline->AddInputLayout(single_tex_layout);
    pipeline->AddPushConstant(blur_push_constant_,
                              VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline->AddShader(fullscreen_vert);
    pipeline->AddShader(frag);
    pipeline->Bake();
    return pipeline;
  };

  blur_h_pipeline_ = create_blur_pipeline(blur_h_frag);
  blur_v_pipeline_ = create_blur_pipeline(blur_v_frag);

  // Composite pass (3 inputs: scene, blurred mask, original mask)
  comp_render_pass_ = std::make_shared<RenderPass>(
      PassType::PostProcess, "SelectionOutlineComposite RenderPass");
  comp_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  comp_render_pass_->Bake();

  comp_desc_layout_ = std::make_shared<DescriptorSetLayout>();
  comp_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  comp_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  comp_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  comp_desc_layout_->Bake();

  comp_push_constant_ =
      std::make_shared<SelectionOutlineCompositePushConstant>();

  auto comp_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/selection_outline_composite.frag"});

  {
    PipelineProperties props{};
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    comp_pipeline_ = std::make_shared<Pipeline>(props);
  }
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(comp_desc_layout_);
  comp_pipeline_->AddPushConstant(comp_push_constant_,
                                  VK_SHADER_STAGE_FRAGMENT_BIT);
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(comp_frag);
  comp_pipeline_->Bake();
}

bool SelectionOutlineFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.is_external;
}

void SelectionOutlineFeature::SetupResources(RenderContext& /*ctx*/) {}

void SelectionOutlineFeature::AddPasses(RenderGraph& graph,
                                        RenderResourceRegistry& registry,
                                        RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SelectionOutlineFeature::AddPasses");

  CameraResourcePool* pool = &ctx.resources;

  struct MeshDraw {
    std::shared_ptr<Mesh> mesh;
    glm::mat4 transform;
    std::shared_ptr<DescriptorSet> bone_descriptor;
  };

  glm::vec4 outline_color = {0.0f, 0.0f, 0.0f, 0.0f};
  float outline_thickness = 0.0f;
  std::vector<MeshDraw> draws;

  const auto& loaded = Engine::scene_manager().GetLoadedScenes();
  for (size_t si = 0; si < loaded.size(); si++) {
    for (entt::entity e :
         loaded[si]->GetAllEntitiesWith<EditorSelectedComponent>()) {
      auto& sel = loaded[si]->GetComponent<EditorSelectedComponent>(e);
      outline_color = sel.color;
      outline_thickness = sel.thickness;

      if (!loaded[si]->HasComponent<TransformComponent>(e)) {
        break;
      }
      auto& tc = loaded[si]->GetComponent<TransformComponent>(e);

      auto try_add_mesh = [&](AssetHandle model_handle, int32_t mesh_index,
                              std::shared_ptr<DescriptorSet> bone_desc) {
        if (!model_handle.IsValid()) {
          return;
        }
        auto model = Engine::asset_manager().Get<Model>(model_handle);
        if (!model || mesh_index < 0 ||
            mesh_index >= static_cast<int32_t>(model->meshes.size())) {
          return;
        }
        auto& mesh = model->meshes[mesh_index];
        if (mesh->allocated_) {
          draws.push_back({mesh, tc.GetTransformMatrix(), bone_desc});
        }
      };

      if (loaded[si]->HasComponent<MeshRendererComponent>(e)) {
        auto& mr = loaded[si]->GetComponent<MeshRendererComponent>(e);
        if (mr.enable_rendering) {
          try_add_mesh(mr.model_handle, mr.mesh_index, nullptr);
        }
      }
      if (loaded[si]->HasComponent<SkinnedMeshRendererComponent>(e)) {
        auto& smr = loaded[si]->GetComponent<SkinnedMeshRendererComponent>(e);
        if (!smr.enable_rendering) {
          break;
        }
        std::shared_ptr<DescriptorSet> bone_desc;
        if (smr.skeleton_root != entt::null &&
            loaded[si]->HasComponent<SkeletalAnimRuntime>(smr.skeleton_root)) {
          auto& runtime =
              loaded[si]->GetComponent<SkeletalAnimRuntime>(smr.skeleton_root);
          bone_desc = runtime.bone_descriptor;
        }
        try_add_mesh(smr.model_handle, smr.mesh_index, bone_desc);
      }
      break;
    }
    if (!draws.empty()) {
      break;
    }
  }

  auto renderer = renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  RGTextureDesc r8_desc{.name = {},
                        .width = rw,
                        .height = rh,
                        .format = VK_FORMAT_R8_UNORM,
                        .samples = SamplingMode::DISABLED,
                        .type = AttachmentTextureType::Offscreen,
                        .layer_count = 1,
                        .sampled = true};
  r8_desc.name = "outline.mask";
  RGResource mask_out = graph.DeclareTransient(r8_desc);
  r8_desc.name = "outline.blur_h";
  RGResource blur_h_out = graph.DeclareTransient(r8_desc);
  r8_desc.name = "outline.blur_v";
  RGResource blur_v_out = graph.DeclareTransient(r8_desc);
  RGResource comp_out = graph.DeclareTransient(RGTextureDesc{
      .name = "outline.output",
      .width = rw,
      .height = rh,
      .format = renderer->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  RGResource pipeline_out = registry.Get("PipelineOutput");

  glm::mat4 vp = ctx.camera.projection * ctx.camera.view_matrix;
  auto mask_pipeline = mask_pipeline_;
  auto mask_push = mask_push_constant_;
  auto blur_h_pipeline = blur_h_pipeline_;
  auto blur_v_pipeline = blur_v_pipeline_;
  auto blur_push = blur_push_constant_;
  auto comp_pipeline = comp_pipeline_;

  blur_push->radius = outline_thickness;
  comp_push_constant_->outline_color = outline_color;

  // --- Mask pass ---
  uint32_t mask_pass = graph.AddPass(
      "SelectionMask", mask_render_pass_,
      [mask_pipeline, mask_push, draws, vp](VkCommandBuffer cmd) {
        mask_pipeline->Bind(PipelineBindPointGraphics, cmd);
        for (const MeshDraw& d : draws) {
          mask_push->mvp = vp * d.transform;
          mask_push->use_skinning = d.bone_descriptor ? 1 : 0;
          mask_pipeline->PushConstants(cmd);
          Engine::renderer()->DrawMeshSimple(cmd, d.mesh, d.bone_descriptor);
        }
      });
  graph.PassWritesColor(mask_pass, mask_out);
  graph.SetPassViewport(mask_pass, ctx.viewport_size);
  graph.SetPassClearColor(mask_pass, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      mask_pass, [this, mask_pass, mask_out, rw, rh](RenderGraph& g) {
        auto tex = g.GetTexture(mask_out);
        if (mask_key_ != tex.get()) {
          mask_framebuffer_ = mask_render_pass_->CreateFramebuffer(
              0, {tex.get()}, {rw, rh});
          mask_key_ = tex.get();
        }
        g.SetPassFramebuffer(mask_pass, mask_framebuffer_);
      });

  // --- Blur helper (single-sampler R8 blur) ---
  auto emit_blur = [&, this, renderer](
                       const std::shared_ptr<Pipeline>& pipeline,
                       BlurBindings& bindings, RGResource input_resource,
                       RGResource output_resource, const char* pass_name) {
    uint32_t pass = graph.AddPass(
        pass_name, blur_render_pass_,
        [renderer, pipeline, &bindings](VkCommandBuffer) {
          pipeline->Bind(PipelineBindPointGraphics);
          Engine::renderer()->DrawFullscreen(pipeline, {bindings.input_desc});
        });
    graph.PassReadsTexture(pass, input_resource);
    graph.PassWritesColor(pass, output_resource);
    graph.SetPassViewport(pass, ctx.viewport_size);
    graph.SetPassClearColor(pass, {0, 0, 0, 0});
    graph.SetPassResolveFn(
        pass,
        [this, renderer, &bindings, pass, input_resource, output_resource, rw,
         rh](RenderGraph& g) {
          auto output = g.GetTexture(output_resource);
          auto input = g.GetTexture(input_resource);
          auto linear = renderer->GetDefaultLinearSampler();

          if (bindings.output_key != output.get()) {
            bindings.framebuffer = blur_render_pass_->CreateFramebuffer(
                0, {output.get()}, {rw, rh});
            bindings.output_key = output.get();
          }
          g.SetPassFramebuffer(pass, bindings.framebuffer);

          if (bindings.input_key != input.get()) {
            auto desc = std::make_shared<DescriptorSet>();
            // Single-sampler layout is the first input layout on the pipeline.
            desc->SetLayout(renderer->GetDescriptorLayout("Present"));
            desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
            desc->Bake();
            bindings.input_desc = desc;
            bindings.input_key = input.get();
          }
        });
  };

  emit_blur(blur_h_pipeline, blur_h_bindings_, mask_out, blur_h_out,
            "SelectionBlurH");
  emit_blur(blur_v_pipeline, blur_v_bindings_, blur_h_out, blur_v_out,
            "SelectionBlurV");

  // --- Composite pass ---
  uint32_t comp_pass = graph.AddPass(
      "SelectionOutlineComposite", comp_render_pass_,
      [this, comp_pipeline](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(comp_pipeline, {comp_input_desc_});
      });
  if (pipeline_out.IsValid()) {
    graph.PassReadsTexture(comp_pass, pipeline_out);
  }
  graph.PassReadsTexture(comp_pass, blur_v_out);
  graph.PassReadsTexture(comp_pass, mask_out);
  graph.PassWritesColor(comp_pass, comp_out);
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 1});
  graph.SetPassResolveFn(
      comp_pass,
      [this, renderer, pool, comp_pass, comp_out, pipeline_out, blur_v_out,
       mask_out, rw, rh](RenderGraph& g) {
        auto output = g.GetTexture(comp_out);
        auto scene = pipeline_out.IsValid() ? g.GetTexture(pipeline_out)
                                            : std::shared_ptr<AttachmentTexture>{};
        auto blur = g.GetTexture(blur_v_out);
        auto mask = g.GetTexture(mask_out);
        auto linear = renderer->GetDefaultLinearSampler();
        auto nearest = renderer->GetDefaultNearestSampler();
        auto present_layout = renderer->GetDescriptorLayout("Present");

        if (comp_output_key_ != output.get()) {
          comp_framebuffer_ = comp_render_pass_->CreateFramebuffer(
              0, {output.get()}, {rw, rh});
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, output->image_views_[0], linear);
          desc->Bake();
          output_desc_ = desc;
          comp_output_key_ = output.get();
        }
        g.SetPassFramebuffer(comp_pass, comp_framebuffer_);

        AttachmentTexture* scene_key = scene ? scene.get() : nullptr;
        if (comp_scene_key_ != scene_key || comp_blur_key_ != blur.get() ||
            comp_mask_key_ != mask.get()) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(comp_desc_layout_);
          if (scene) {
            desc->AddCombinedImageSampler(0, scene->image_views_[0], linear);
          }
          desc->AddCombinedImageSampler(1, blur->image_views_[0], linear);
          desc->AddCombinedImageSampler(2, mask->image_views_[0], nearest);
          desc->Bake();
          comp_input_desc_ = desc;
          comp_scene_key_ = scene_key;
          comp_blur_key_ = blur.get();
          comp_mask_key_ = mask.get();
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", output_desc_);
      });

  registry.Register("PipelineOutput", comp_out);
}

}  // namespace wiesel
