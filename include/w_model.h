
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"
#include "w_mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

namespace Wiesel {
	class Model : public Object {
	public:
		Model();
		Model(const glm::vec3& position, const glm::quat& orientation);
		Model(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& scale);
		~Model();

		void LoadModel(const std::string& path);

		const std::vector<Reference<Mesh>>& GetMeshes();

		void UpdateView();

		void Allocate();
		void Deallocate();
	private:
		std::vector<Reference<Mesh>> m_Meshes;
		std::string m_ModelPath;
		std::string m_TexturesPath;
		std::map<std::string, Reference<Texture>> m_Textures;

		void LoadTexture(Reference<Mesh> mesh, aiMaterial *mat, aiTextureType type);
		Reference<Mesh> ProcessMesh(aiMesh *aiMesh, const aiScene& aiScene, aiMatrix4x4 aiMatrix);
		void ProcessNode(aiNode *node, const aiScene& scene, std::vector<Reference<Mesh>>& meshes);

		static glm::mat4 ConvertMatrix(const aiMatrix4x4 &aiMat);
	};

}