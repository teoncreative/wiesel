//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_engine.h"
#include "window/w_glfwwindow.h"

namespace Wiesel {
	Reference<Renderer> Engine::s_Renderer;
	Reference<AppWindow> Engine::s_Window;

	void Engine::InitEngine() {

	}

	void Engine::InitWindow(WindowProperties props) {
		s_Window = CreateReference<GlfwAppWindow>(props);
	}

	void Engine::InitRenderer() {
		if (s_Window == nullptr) {
			LOG_ERROR("Window should be initialized before renderer!");
			abort();
		}
		s_Renderer = CreateReference<Renderer>(s_Window);
	}

	void Engine::CleanupRenderer() {
		s_Renderer->Cleanup();
		s_Renderer = nullptr;
	}

	void Engine::CleanupWindow() {
		s_Window = nullptr;
	}

	void Engine::CleanupEngine() {

	}

	Reference<Renderer> Engine::GetRenderer() {
		if (s_Renderer == nullptr) {
			throw std::runtime_error("Renderer is not initialized!");
		}
		return s_Renderer;
	}

	Reference<AppWindow> Engine::GetWindow() {
		return s_Window;
	}

	void Engine::LoadModel(TransformComponent& transform, ModelComponent& modelComponent, const std::string& path) {
		auto& model = modelComponent.Data;
		auto fsPath = std::filesystem::canonical(path);
		model.ModelPath = fsPath.generic_string();
		LOG_INFO("Loading model: " + path);
		model.TexturesPath = fsPath.parent_path().generic_string();

		Assimp::Importer importer;
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
		importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);
		importer.ReadFile(model.ModelPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcessPreset_TargetRealtime_Fast);
		aiScene* scene = importer.GetOrphanedScene();

		if(!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
			LOG_ERROR("Failed to load model " + path + ": " + importer.GetErrorString());
			return;
		}

		ProcessNode(model, scene->mRootNode, *scene, model.Meshes);
		for (const auto& item : model.Meshes) {
			item->Allocate();
		}
		LOG_INFO("Loaded " + std::to_string(model.Meshes.size()) + " meshes!");
		LOG_INFO("Loaded " + std::to_string(model.Textures.size()) + " textures!");
	}

	bool Engine::LoadTexture(Model& model, Reference<Mesh> mesh, aiMaterial *mat, aiTextureType type) {
		aiString str;
		mat->GetTexture(type, 0, &str);
		std::string s = std::string(str.C_Str());
		if (s.empty()) {
			return false;
		}
		std::string textureFullPath = model.TexturesPath + "/" + s;
		if (model.Textures.contains(textureFullPath)) {
			mesh->Texture = model.Textures[textureFullPath];
		} else {
			Reference<Texture> texture = Engine::GetRenderer()->CreateTexture(textureFullPath, {});
			mesh->Texture = texture;
			model.Textures.insert(std::pair(textureFullPath, texture));
		}
		return true;
	}

	Reference<Mesh> Engine::ProcessMesh(Model& model, aiMesh *aiMesh, const aiScene& aiScene, aiMatrix4x4 aiMatrix) {
		std::vector<Vertex> vertices;
		std::vector<Index> indices;
		glm::mat4 mat = ConvertMatrix(aiMatrix);

		aiMaterial* material = aiScene.mMaterials[aiMesh->mMaterialIndex];

		Reference<Mesh> mesh = CreateReference<Mesh>();
		// todo handle materials properly within another class
		bool hasTexture = LoadTexture(model, mesh, material, aiTextureType_DIFFUSE);

		for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
			Vertex vertex{};
			glm::vec3 vector;
			// positions
			vector.x = aiMesh->mVertices[i].x;
			vector.y = aiMesh->mVertices[i].y;
			vector.z = aiMesh->mVertices[i].z;
			vertex.Pos = vector;

			// normals
			vector.x = aiMesh->mNormals[i].x;
			vector.y = aiMesh->mNormals[i].y;
			vector.z = aiMesh->mNormals[i].z;
			vertex.Normal = vector;

			// texture coordinates
			if(aiMesh->mTextureCoords[0]) {
				glm::vec2 vec;
				vec.x = aiMesh->mTextureCoords[0][i].x;
				vec.y = aiMesh->mTextureCoords[0][i].y;
				vertex.UV = vec;
			} else {
				vertex.UV = glm::vec2(0.0f, 0.0f);
			}
			vertex.HasTexture = hasTexture;

			vertex.Color = {1.0f, 1.0f, 1.0f};

			// tangent
			//vector.x = mesh->mTangents[i].x;
			//vector.y = mesh->mTangents[i].y;
			//vector.z = mesh->mTangents[i].z;
			//vertex.Tangent = vector;

			// bitangent
			//vector.x = mesh->mBitangents[i].x;
			//vector.y = mesh->mBitangents[i].y;
			//vector.z = mesh->mBitangents[i].z;
			//vertex.BiTangent = vector;

			mesh->Vertices.push_back(vertex);
		}

		// now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
		for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
			aiFace face = aiMesh->mFaces[i];

			// retrieve all indices of the face and store them in the indices vector
			for (int j = face.mNumIndices - 1; j >= 0; j--) {
				mesh->Indices.push_back(face.mIndices[j]);
			}
		}

		return mesh;
	}

	void Engine::ProcessNode(Model& model, aiNode *node, const aiScene& scene, std::vector<Reference<Mesh>>& meshes) {
		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			aiMesh* aiMesh = scene.mMeshes[node->mMeshes[i]];
			Reference<Mesh> mesh = ProcessMesh(model, aiMesh, scene, node->mTransformation);
			if (mesh == nullptr) continue;
			meshes.push_back(mesh);
		}
		for (uint32_t i = 0; i < node->mNumChildren; i++) {
			ProcessNode(model, node->mChildren[i], scene, meshes);
		}
	}

	glm::mat4 Engine::ConvertMatrix(const aiMatrix4x4 &aiMat) {
		return {
				aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
				aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
				aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
				aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
		};
	}
}