
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

#include "w_application.h"
#include "rendering/w_renderer.h"

namespace Wiesel {
	class Engine {
	public:
		static void InitEngine();
		static void InitWindow(WindowProperties props);
		static void InitRenderer();

		static void CleanupRenderer();
		static void CleanupWindow();
		static void CleanupEngine();
		WIESEL_GETTER_FN static Reference<Renderer> GetRenderer();
		WIESEL_GETTER_FN static Reference<AppWindow> GetWindow();

		static aiScene* LoadAssimpModel(ModelComponent& modelComponent, const std::string& path);
		static void LoadModel(TransformComponent& transform, ModelComponent& modelComponent, const std::string& path);
		static void LoadModel(aiScene* scene, TransformComponent& transform, ModelComponent& modelComponent, const std::string& path);

	private:
		static Reference<Renderer> s_Renderer;
		static Reference<AppWindow> s_Window;

		// model loading
		static glm::mat4 ConvertMatrix(const aiMatrix4x4 &aiMat);
		static bool LoadTexture(Model& model, Reference<Mesh> mesh, aiMaterial *mat, aiTextureType type);
		static Reference<Mesh> ProcessMesh(Model& model, aiMesh *aiMesh, const aiScene& aiScene, aiMatrix4x4 aiMatrix);
		static void ProcessNode(Model& model, aiNode *node, const aiScene& scene, std::vector<Reference<Mesh>>& meshes);
	};

	Application* CreateApp();
}