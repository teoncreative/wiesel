//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_model.h"
#include "w_renderer.h"

namespace Wiesel {


	Model::Model() : Object() {

	}

	Model::Model(const glm::vec3& position, const glm::quat& orientation) : Object(position, orientation) {

	}

	Model::Model(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& scale) : Object(position, orientation, scale) {

	}

	Model::~Model() {
	    Deallocate();
		m_Meshes.clear();
	}

	void Model::LoadModel(const std::string& path) {
		auto fsPath = std::filesystem::canonical(path);
		m_ModelPath = fsPath.generic_string();
		LogInfo("Loading model: " + m_ModelPath);
		m_TexturesPath = fsPath.parent_path().generic_string();

		Assimp::Importer importer;
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
		importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);
		importer.ReadFile(m_ModelPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcessPreset_TargetRealtime_Fast);
		aiScene* scene = importer.GetOrphanedScene();

		if(!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
			LogError("Failed to load model " + path + ": " + importer.GetErrorString());
			return;
		}

		ProcessNode(scene->mRootNode, *scene, m_Meshes);
		LogInfo("Loaded " + std::to_string(m_Meshes.size()) + " meshes!");

		// todo animations
		Allocate();
		LogInfo("Loaded " + std::to_string(m_Textures.size()) + " textures!");
	}

	const std::vector<Reference<Mesh>>& Model::GetMeshes() {
		return m_Meshes;
	}

	void Model::UpdateView() {
		Object::UpdateView();
		for (const auto& mesh : m_Meshes) {
			mesh->SetLocalView(m_LocalView);
			mesh->SetOrientation(m_Orientation);
		}
	}

	void Model::Allocate() {
		for (const auto& mesh : m_Meshes) {
			mesh->Allocate();
		}
	}

	void Model::Deallocate() {
		for (const auto& mesh : m_Meshes) {
			mesh->Deallocate();
		}
	}

	bool Model::LoadTexture(Reference<Mesh> mesh, aiMaterial *mat, aiTextureType type) {
		aiString str;
		mat->GetTexture(type, 0, &str);
		std::string s = std::string(str.C_Str());
		if (s.empty()) {
			return false;
		}
		std::string textureFullPath = m_TexturesPath + "/" + s;
		if (m_Textures.contains(textureFullPath)) {
			mesh->SetTexture(m_Textures[textureFullPath]);
		} else {
			Reference<Texture> texture = Renderer::GetRenderer()->CreateTexture(textureFullPath, {});
			m_Textures.insert(std::pair(textureFullPath, texture));
			mesh->SetTexture(texture);
		}
		return true;
	}

	Reference<Mesh> Model::ProcessMesh(aiMesh *aiMesh, const aiScene& aiScene, aiMatrix4x4 aiMatrix) {
		std::vector<Vertex> vertices;
		std::vector<Index> indices;
		glm::mat4 mat = ConvertMatrix(aiMatrix);

		aiMaterial* material = aiScene.mMaterials[aiMesh->mMaterialIndex];

		Reference<Mesh> mesh = CreateReference<Mesh>();
		// todo handle materials properly within another class
		bool hasTexture = LoadTexture(mesh, material, aiTextureType_DIFFUSE);

		for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
			Vertex vertex{};
			glm::vec3 vector;
			// positions
			vector.x = aiMesh->mVertices[i].x;
			vector.y = aiMesh->mVertices[i].y;
			vector.z = aiMesh->mVertices[i].z;
			vertex.Pos = vector;
			// normals
			//vector.x = mesh->mNormals[i].x;
			//vector.y = mesh->mNormals[i].y;
			//vector.z = mesh->mNormals[i].z;
			//vertex.Normal = vector;

			// texture coordinates
			if(aiMesh->mTextureCoords[0]) {
				glm::vec2 vec;
				vec.x = aiMesh->mTextureCoords[0][i].x;
				vec.y = aiMesh->mTextureCoords[0][i].y;
				vertex.TexCoord = vec;
			} else {
				vertex.TexCoord = glm::vec2(0.0f, 0.0f);
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

			mesh->AddVertex(vertex);
		}

		// now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
		for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
			aiFace face = aiMesh->mFaces[i];

			// retrieve all indices of the face and store them in the indices vector
			for (int j = face.mNumIndices - 1; j >= 0; j--) {
				mesh->AddIndex(face.mIndices[j]);
			}
		}

		return mesh;
	}

	void Model::ProcessNode(aiNode *node, const aiScene& scene, std::vector<Reference<Mesh>>& meshes) {
		for (uint32_t i = 0; i < node->mNumMeshes; i++) {
			aiMesh* aiMesh = scene.mMeshes[node->mMeshes[i]];
			Reference<Mesh> mesh = ProcessMesh(aiMesh, scene, node->mTransformation);
			if (mesh == nullptr) continue;
			meshes.push_back(mesh);
		}
		for (uint32_t i = 0; i < node->mNumChildren; i++) {
			ProcessNode(node->mChildren[i], scene, meshes);
		}
	}

	glm::mat4 Model::ConvertMatrix(const aiMatrix4x4 &aiMat) {
		return {
				aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
				aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
				aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
				aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
		};
	}
}