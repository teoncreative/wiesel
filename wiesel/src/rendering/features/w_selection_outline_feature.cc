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
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_components.h"
#include "scene/w_scene_manager.h"
#include "w_engine.h"

namespace Wiesel {

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

void SelectionOutlineFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("SelectionOutlineFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  auto linear = renderer.GetDefaultLinearSampler();
  auto nearest = renderer.GetDefaultNearestSampler();
  auto present_layout = renderer.GetDescriptorLayout("Present");

  auto make_r8 = [&](const std::string& name) {
    pool.SetTexture(name,
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Offscreen, 1,
                         VK_FORMAT_R8_UNORM, SamplingMode::DISABLED, true}));
  };

  make_r8("outline.mask");
  make_r8("outline.blur_h");
  make_r8("outline.blur_v");

  pool.SetFramebuffer(
      "outline.mask",
      mask_render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("outline.mask").get()}, {rw, rh}));
  pool.SetFramebuffer(
      "outline.blur_h",
      blur_render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("outline.blur_h").get()}, {rw, rh}));
  pool.SetFramebuffer(
      "outline.blur_v",
      blur_render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("outline.blur_v").get()}, {rw, rh}));

  // Blur H input: reads mask
  auto blur_h_input = std::make_shared<DescriptorSet>();
  blur_h_input->SetLayout(present_layout);
  blur_h_input->AddCombinedImageSampler(
      0, pool.GetTexture("outline.mask")->image_views_[0], linear);
  blur_h_input->Bake();
  pool.SetDescriptor("outline.blur_h_input", blur_h_input);

  // Blur V input: reads blur H output
  auto blur_v_input = std::make_shared<DescriptorSet>();
  blur_v_input->SetLayout(present_layout);
  blur_v_input->AddCombinedImageSampler(
      0, pool.GetTexture("outline.blur_h")->image_views_[0], linear);
  blur_v_input->Bake();
  pool.SetDescriptor("outline.blur_v_input", blur_v_input);

  // Composite output
  pool.SetTexture(
      "outline.output",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));
  pool.SetFramebuffer(
      "outline.output",
      comp_render_pass_->CreateFramebuffer(
          0, {pool.GetTexture("outline.output").get()}, {rw, rh}));

  // Composite input: scene + blurred mask + original mask
  auto comp_input = std::make_shared<DescriptorSet>();
  comp_input->SetLayout(comp_desc_layout_);
  comp_input->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0], linear);
  comp_input->AddCombinedImageSampler(
      1, pool.GetTexture("outline.blur_v")->image_views_[0], linear);
  comp_input->AddCombinedImageSampler(
      2, pool.GetTexture("outline.mask")->image_views_[0], nearest);
  comp_input->Bake();
  pool.SetDescriptor("outline.comp_input", comp_input);

  auto output_desc = std::make_shared<DescriptorSet>();
  output_desc->SetLayout(present_layout);
  output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("outline.output")->image_views_[0], linear);
  output_desc->Bake();
  pool.SetDescriptor("outline.output_desc", output_desc);

  pool.SetTexture("PipelineOutput", pool.GetTexture("outline.output"));
  pool.SetDescriptor("PipelineOutputDescriptor", output_desc);
}

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

  RGResource pipeline_out = registry.Get("PipelineOutput");
  RGResource comp_out =
      graph.ImportTexture("OutlineOutput", pool->GetTexture("outline.output"));

  glm::mat4 vp = ctx.camera.projection * ctx.camera.view_matrix;
  auto mask_pipeline = mask_pipeline_;
  auto mask_push = mask_push_constant_;
  auto blur_h_pipeline = blur_h_pipeline_;
  auto blur_v_pipeline = blur_v_pipeline_;
  auto blur_push = blur_push_constant_;
  auto comp_pipeline = comp_pipeline_;

  blur_push->radius = outline_thickness;
  comp_push_constant_->outline_color = outline_color;

  RGResource mask_out =
      graph.ImportTexture("OutlineMask", pool->GetTexture("outline.mask"));
  RGResource blur_h_out =
      graph.ImportTexture("OutlineBlurH", pool->GetTexture("outline.blur_h"));
  RGResource blur_v_out =
      graph.ImportTexture("OutlineBlurV", pool->GetTexture("outline.blur_v"));

  // Pass 1: Mask
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
  graph.SetPassFramebuffer(mask_pass, pool->GetFramebuffer("outline.mask"));
  graph.SetPassViewport(mask_pass, ctx.viewport_size);
  graph.SetPassClearColor(mask_pass, {0, 0, 0, 0});

  // Pass 2: Blur horizontal
  uint32_t blur_h_pass = graph.AddPass(
      "SelectionBlurH", blur_render_pass_,
      [pool, blur_h_pipeline](VkCommandBuffer) {
        blur_h_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            blur_h_pipeline, {pool->GetDescriptor("outline.blur_h_input")});
      });

  graph.PassReadsTexture(blur_h_pass, mask_out);
  graph.PassWritesColor(blur_h_pass, blur_h_out);
  graph.SetPassFramebuffer(blur_h_pass, pool->GetFramebuffer("outline.blur_h"));
  graph.SetPassViewport(blur_h_pass, ctx.viewport_size);
  graph.SetPassClearColor(blur_h_pass, {0, 0, 0, 0});

  // Pass 3: Blur vertical
  uint32_t blur_v_pass = graph.AddPass(
      "SelectionBlurV", blur_render_pass_,
      [pool, blur_v_pipeline](VkCommandBuffer) {
        blur_v_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            blur_v_pipeline, {pool->GetDescriptor("outline.blur_v_input")});
      });

  graph.PassReadsTexture(blur_v_pass, blur_h_out);
  graph.PassWritesColor(blur_v_pass, blur_v_out);
  graph.SetPassFramebuffer(blur_v_pass, pool->GetFramebuffer("outline.blur_v"));
  graph.SetPassViewport(blur_v_pass, ctx.viewport_size);
  graph.SetPassClearColor(blur_v_pass, {0, 0, 0, 0});

  // Pass 4: Composite
  uint32_t comp_pass = graph.AddPass(
      "SelectionOutlineComposite", comp_render_pass_,
      [pool, comp_pipeline](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        Engine::renderer()->DrawFullscreen(
            comp_pipeline, {pool->GetDescriptor("outline.comp_input")});
      });

  if (pipeline_out.IsValid()) {
    graph.PassReadsTexture(comp_pass, pipeline_out);
  }
  graph.PassReadsTexture(comp_pass, blur_v_out);
  graph.PassReadsTexture(comp_pass, mask_out);
  graph.PassWritesColor(comp_pass, comp_out);
  graph.SetPassFramebuffer(comp_pass, pool->GetFramebuffer("outline.output"));
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 1});

  registry.Register("PipelineOutput", comp_out);
}

}  // namespace Wiesel
