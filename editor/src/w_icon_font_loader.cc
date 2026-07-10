//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_icon_font_loader.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace wiesel::editor {

IIconSource* IconFontLoader::source_ = nullptr;

void IconFontLoader::SetSource(IIconSource* source) {
  source_ = source;
}

// ImFontLoader callbacks

static bool LoaderInit(ImFontAtlas*) {
  return true;
}

static void LoaderShutdown(ImFontAtlas*) {}

static bool FontSrcInit(ImFontAtlas*, ImFontConfig*) {
  return true;
}

static void FontSrcDestroy(ImFontAtlas*, ImFontConfig*) {}

static bool FontSrcContainsGlyph(ImFontAtlas*, ImFontConfig*,
                                 ImWchar codepoint) {
  if (!IconFontLoader::source_) {
    return false;
  }
  return IconFontLoader::source_->HasGlyph(codepoint);
}

static bool FontBakedInit(ImFontAtlas*, ImFontConfig*, ImFontBaked*, void*) {
  return true;
}

static void FontBakedDestroy(ImFontAtlas*, ImFontConfig*, ImFontBaked*, void*) {
}

static bool FontBakedLoadGlyph(ImFontAtlas* atlas, ImFontConfig* src,
                               ImFontBaked* baked, void*, ImWchar codepoint,
                               ImFontGlyph* out_glyph, float* out_advance_x) {
  if (!IconFontLoader::source_) {
    return false;
  }

  int size = static_cast<int>(baked->Size * 0.9f);
  if (size <= 0) {
    size = 16;
  }

  // Advance-only query
  if (out_advance_x != nullptr) {
    *out_advance_x = static_cast<float>(size) + 1.0f;
    return true;
  }

  IconBitmap bitmap = IconFontLoader::source_->RasterizeGlyph(codepoint, size);
  if (!bitmap.pixels) {
    return false;
  }

  // Pack into atlas
  ImFontAtlasRectId pack_id =
      ImFontAtlasPackAddRect(atlas, bitmap.width, bitmap.height);
  if (pack_id == ImFontAtlasRectId_Invalid) {
    free(bitmap.pixels);
    return false;
  }
  ImTextureRect* r = ImFontAtlasPackGetRect(atlas, pack_id);

  // Fill glyph metrics - center icon vertically on the ascent
  float ascent = IM_ROUND(baked->Ascent);
  float icon_size = static_cast<float>(size);
  float y_offset = (ascent - icon_size) * 0.5f + 2.0f;

  out_glyph->Codepoint = codepoint;
  out_glyph->AdvanceX = icon_size + 2.0f;
  out_glyph->X0 = 0;
  out_glyph->Y0 = y_offset;
  out_glyph->X1 = static_cast<float>(bitmap.width);
  out_glyph->Y1 = static_cast<float>(bitmap.height) + y_offset;
  out_glyph->Visible = true;
  out_glyph->Colored = true;
  out_glyph->PackId = pack_id;

  ImFontAtlasBakedSetFontGlyphBitmap(atlas, baked, src, out_glyph, r,
                                     bitmap.pixels, ImTextureFormat_RGBA32,
                                     bitmap.width * 4);
  free(bitmap.pixels);
  return true;
}

static ImFontLoader s_icon_loader = [] {
  ImFontLoader loader;
  loader.Name = "WieselIconLoader";
  loader.LoaderInit = nullptr;
  loader.LoaderShutdown = nullptr;
  loader.FontSrcInit = FontSrcInit;
  loader.FontSrcDestroy = FontSrcDestroy;
  loader.FontSrcContainsGlyph = FontSrcContainsGlyph;
  loader.FontBakedInit = FontBakedInit;
  loader.FontBakedDestroy = FontBakedDestroy;
  loader.FontBakedLoadGlyph = FontBakedLoadGlyph;
  loader.FontBakedSrcLoaderDataSize = 0;
  return loader;
}();

const ImFontLoader* IconFontLoader::GetLoader() {
  return &s_icon_loader;
}

}  // namespace wiesel::editor
