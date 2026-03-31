//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.h"

namespace Wiesel::Editor {

// RGBA pixel data for a single icon, scaled to a specific size.
// Caller is responsible for freeing pixels.
struct IconBitmap {
  uint8_t* pixels = nullptr;  // RGBA, row-major
  int width = 0;
  int height = 0;
};

// Interface for icon glyph sources. Implementations provide RGBA bitmaps
// for icon codepoints at any requested size.
class IIconSource {
 public:
  virtual ~IIconSource() = default;

  // Returns true if this source can provide a glyph for the given codepoint.
  virtual bool HasGlyph(uint32_t codepoint) const = 0;

  // Rasterize the icon at the given pixel size. Returns an IconBitmap with
  // newly allocated pixels (caller frees with free()). Returns empty on failure.
  virtual IconBitmap RasterizeGlyph(uint32_t codepoint, int size) const = 0;
};

// Icon source that loads PNG files from VFS and scales them on demand.
class PngIconSource : public IIconSource {
 public:
  // Register a PNG icon for a codepoint. Loads the PNG from VFS immediately.
  void AddIcon(uint32_t codepoint, const std::string& vfs_path);

  bool HasGlyph(uint32_t codepoint) const override;
  IconBitmap RasterizeGlyph(uint32_t codepoint, int size) const override;

 private:
  struct SourceImage {
    std::vector<uint8_t> pixels;  // RGBA
    int width = 0;
    int height = 0;
  };

  std::unordered_map<uint32_t, SourceImage> icons_;
};

}  // namespace Wiesel::Editor
