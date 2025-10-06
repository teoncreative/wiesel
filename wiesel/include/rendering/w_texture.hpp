
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

#include "util/w_utils.hpp"
#include "w_pch.hpp"
#include "w_sampler.hpp"

namespace Wiesel {
class ImageView;

// taken from assimp
enum TextureType {
  /** Dummy value.
		   *
		   *  No texture, but the value to be used as 'texture semantic'
		   *  (#aiMaterialProperty::mSemantic) for all material properties
		   *  *not* related to textures.o
		   */
  TextureTypeNone = 0,

  /** LEGACY API MATERIALS
		 * Legacy refers to materials which
		 * Were originally implemented in the specifications around 2000.
		 * These must never be removed, as most engines support them.
		 */

  /** The texture is combined with the result of the diffuse
		 *  lighting equation.
		 *  OR
		 *  PBR Specular/Glossiness
		 */
  TextureTypeDiffuse = 1,

  /** The texture is combined with the result of the specular
		 *  lighting equation.
		 *  OR
		 *  PBR Specular/Glossiness
		 */
  TextureTypeSpecular = 2,

  /** The texture is combined with the result of the ambient
		 *  lighting equation.
		 */
  TextureTypeAmbient = 3,

  /** The texture is added to the result of the lighting
		 *  calculation. It isn't influenced by incoming light.
		 */
  TextureTypeEmissive = 4,

  /** The texture is a height map.
		 *
		 *  By convention, higher gray-scale values stand for
		 *  higher elevations from the base height.
		 */
  TextureTypeHeight = 5,

  /** The texture is a (tangent space) normal-map.
		 *
		 *  Again, there are several conventions for tangent-space
		 *  normal maps. Assimp does (intentionally) not
		 *  distinguish here.
		 */
  TextureTypeNormals = 6,

  /** The texture defines the glossiness of the material.
		 *
		 *  The glossiness is in fact the exponent of the specular
		 *  (phong) lighting equation. Usually there is a conversion
		 *  function defined to map the linear color values in the
		 *  texture to a suitable exponent. Have fun.
		*/
  TextureTypeShininess = 7,

  /** The texture defines per-pixel opacity.
		 *
		 *  Usually 'white' means opaque and 'black' means
		 *  'transparency'. Or quite the opposite. Have fun.
		*/
  TextureTypeOpacty = 8,

  /** Displacement texture
		 *
		 *  The exact purpose and format is application-dependent.
		 *  Higher color values stand for higher vertex displacements.
		*/
  TextureTypeDisplacement = 9,

  /** Lightmap texture (aka Ambient Occlusion)
		 *
		 *  Both 'Lightmaps' and dedicated 'ambient occlusion maps' are
		 *  covered by this material property. The texture contains a
		 *  scaling value for the final color value of a pixel. Its
		 *  intensity is not affected by incoming light.
		*/
  TextureTypeLightmap = 10,

  /** Reflection texture
		 *
		 * Contains the color of a perfect mirror reflection.
		 * Rarely used, almost never for real-time applications.
		*/
  TextureTypeReflection = 11,

  /** PBR Materials
		 * PBR definitions from maya and other modelling packages now use this standard.
		 * This was originally introduced around 2012.
		 * Support for this is in game engines like Godot, Unreal or Unity3D.
		 * Modelling packages which use this are very common now.
		 */

  TextureTypeBaseColor = 12,
  TextureTypeNormalCamera = 13,
  TextureTypeEmissionColor = 14,
  TextureTypeMetalness = 15,
  TextureTypeDiffuseRoughness = 16,
  TextureTypeAmbientOcclusion = 17,

  /** PBR Material Modifiers
		* Some modern renderers have further PBR modifiers that may be overlaid
		* on top of the 'base' PBR materials for additional realism.
		* These use multiple texture maps, so only the base type is directly defined
		*/

  /** Sheen
		* Generally used to simulate textiles that are covered in a layer of microfibers
		* eg velvet
		* https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_sheen
		*/
  TextureTypeSheen = 19,

  /** Clearcoat
		* Simulates a layer of 'polish' or 'lacquer' layered on top of a PBR substrate
		* https://autodesk.github.io/standard-surface/#closures/coating
		* https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_clearcoat
		*/
  TextureTypeClearcoat = 20,

  /** Transmission
		* Simulates transmission through the surface
		* May include further information such as wall thickness
		*/
  TextureTypeTransmission = 21
};


struct TextureProps {
  TextureType type = TextureTypeDiffuse;
  bool generate_mipmaps = true;
  VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;
  uint32_t width;
  uint32_t height;
};

class Texture {
 public:
  Texture(TextureType texture_type, const std::string& path);
  ~Texture();

  TextureType type_;
  VkImage image_;
  VkFormat format_;
  VkDeviceMemory device_memory_;
  Ref<ImageView> image_view_;
  VkSampler sampler_;
  uint32_t mip_levels_;

  uint32_t width_;
  uint32_t height_;
  int32_t channels_;
  VkDeviceSize size_;

  bool is_allocated_;
  std::string path_;
};

enum class AttachmentTextureType {
  Color,
  DepthStencil,
  SwapChain,
  Offscreen,
  Resolve
};

struct AttachmentTextureProps {
  uint32_t width = 0;
  uint32_t height = 0;
  AttachmentTextureType type = AttachmentTextureType::Color;
  uint32_t image_count = 1;
  VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;
  VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
  bool sampled = false;
  uint32_t layer_count = 1;
  bool transfer_dest = false;
};

class DescriptorSet;

struct AttachmentTextureInfo {
  AttachmentTextureType type;
  VkFormat format;
  VkSampleCountFlagBits msaa_samples;
  /*VkAttachmentLoadOp LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  VkAttachmentStoreOp StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkAttachmentLoadOp StencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  VkAttachmentStoreOp StencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;*/
};

class AttachmentTexture {
 public:
  AttachmentTexture() = default;
  ~AttachmentTexture();

  AttachmentTextureType type_;
  std::vector<VkImage> images_;
  std::vector<Ref<ImageView>> image_views_;
  std::vector<Ref<Sampler>> samplers_;
  std::vector<VkDeviceMemory> device_memories_;
  VkFormat format_;
  uint32_t width_;
  uint32_t height_;
  VkSampleCountFlagBits msaa_samples_;
  Ref<DescriptorSet> descriptors_; // Deprecated, I'll move this
  VkImageAspectFlags aspect_flags_;
  uint32_t mip_levels_;

  bool is_allocated_;

};

class ImageView {
 public:
  ImageView() = default;
  ~ImageView();

  VkImageView handle_;
  uint32_t layer_;
  uint32_t layer_count_;
};

}  // namespace Wiesel