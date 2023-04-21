
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_material.hpp"

namespace Wiesel {
	Material::Material() {
	}

	Material::~Material() {
		BaseTexture = nullptr;
		NormalMap = nullptr;
		SpecularMap = nullptr;
		HeightMap = nullptr;
		AlbedoMap = nullptr;
		RoughnessMap = nullptr;
		MetallicMap = nullptr;
	}

	void Material::Set(Reference<Material> material, Reference<Texture> texture, TextureType type) {
		switch (type) {
			case TextureTypeNone:
				break;
			case TextureTypeDiffuse:
				material->BaseTexture = texture;
				break;
			case TextureTypeSpecular:
				material->SpecularMap = texture;
				break;
			case TextureTypeAmbient:
				break;
			case TextureTypeEmissive:
				break;
			case TextureTypeHeight:
				material->HeightMap = texture;
				break;
			case TextureTypeNormals:
				material->NormalMap = texture;
				break;
			case TextureTypeShininess:
				break;
			case TextureTypeOpacty:
				break;
			case TextureTypeDisplacement:
				break;
			case TextureTypeLightmap:
				break;
			case TextureTypeReflection:
				break;
			case TextureTypeBaseColor:
				material->AlbedoMap = texture;
				break;
			case TextureTypeNormalCamera:
				material->NormalMap = texture;
				break;
			case TextureTypeEmissionColor:
				break;
			case TextureTypeMetalness:
				material->MetallicMap = texture;
				break;
			case TextureTypeDiffuseRoughness:
				material->RoughnessMap = texture;
				break;
			case TextureTypeAmbientOcclusion:
				break;
			case TextureTypeSheen:
				break;
			case TextureTypeClearcoat:
				break;
			case TextureTypeTransmission:
				break;
		}
	}

}