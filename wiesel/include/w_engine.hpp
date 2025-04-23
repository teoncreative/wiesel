
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

#include "rendering/w_renderer.hpp"
#include "w_application.hpp"

namespace Wiesel {

class Engine {
 public:
  static void InitEngine();
  static void InitWindow(const WindowProperties&& props);
  static void InitRenderer(const RendererProperties&& props);

  static void CleanupRenderer();
  static void CleanupWindow();
  static void CleanupEngine();
  WIESEL_GETTER_FN static Ref<Renderer> GetRenderer();
  WIESEL_GETTER_FN static Ref<AppWindow> GetWindow();

  static aiScene* LoadAssimpModel(ModelComponent& modelComponent,
                                  const std::string& path,
                                  bool convertToLeftHanded = true);
  static void LoadModel(TransformComponent& transform,
                        ModelComponent& modelComponent,
                        const std::string& path,
                        bool convertToLeftHanded = true);

  static void LoadModel(aiScene* scene, TransformComponent& transform,
                        ModelComponent& modelComponent,
                        const std::string& path);

 private:
  static glm::mat4 ConvertMatrix(const aiMatrix4x4& aiMat);
  static bool LoadTexture(Model& model, Ref<Mesh> mesh, aiMaterial* mat,
                          aiTextureType type);
  static Ref<Mesh> ProcessMesh(Model& model, aiMesh* aiMesh,
                                     const aiScene& aiScene,
                                     aiMatrix4x4 aiMatrix);
  static void ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                          std::vector<Ref<Mesh>>& meshes);

 private:
  static Ref<Renderer> s_Renderer;
  static Ref<AppWindow> s_Window;
};

Application* CreateApp();
}  // namespace Wiesel