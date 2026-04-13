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

#include <functional>
#include <string>
#include <vector>
#include "asset/w_asset_handle.h"
#include "util/w_vfs.h"
#include "w_thumbnail_cache.h"

namespace Wiesel::Editor {

// A single entry in the VFS browser, combining VFS info with asset metadata.
struct BrowserEntry {
  std::string name;
  std::string vfs_path;
  bool is_dir = false;
  AssetType asset_type = AssetType::None;
};

// Shared VFS directory browser with tile-based rendering.
// Used by the asset browser panel, file picker modal, etc.
class VfsBrowser {
 public:
  // Navigation
  void SetRoot(const std::string& root);

  const std::string& root() const { return root_; }

  const std::string& current_dir() const { return current_dir_; }

  void SetCurrentDir(const std::string& dir) {
    current_dir_ = dir;
    cache_dirty_ = true;
  }

  void NavigateInto(const std::string& dir_name);
  bool NavigateUp();

  // Full VFS path of current directory (e.g. "app://models/")
  std::string CurrentVfsDir() const;

  // Scan current directory. Directories first, then files, natural sort.
  // Results are cached; call Invalidate() to force a re-scan.
  const std::vector<BrowserEntry>& Scan(AssetType filter = AssetType::None);

  // Mark the cached directory listing as stale so the next Scan() re-reads.
  void Invalidate();

  // Build breadcrumb segments: (display_name, relative_dir_path)
  std::vector<std::pair<std::string, std::string>> Breadcrumbs() const;

  // --- Tile rendering (shared visual layer) ---

  // Configuration
  float tile_size = 80.0f;

  // Call between BeginTileGrid / EndTileGrid to render tiles.
  void BeginTileGrid();

  // Draw a single tile. Returns true if clicked.
  // double_clicked is set if the item was double-clicked.
  bool DrawTile(const char* label, ImVec4 icon_color, const char* type_abbrev,
                bool is_selected, bool is_folder,
                const ThumbnailEntry* thumbnail = nullptr,
                const AssetMetadata* asset_meta = nullptr,
                bool* double_clicked = nullptr);

  void NextColumn();
  void EndTileGrid();

  int grid_columns() const { return grid_columns_; }

  void SetGridCol(int col) { grid_col_ = col; }

  // Render breadcrumb navigation bar. Returns true if navigation changed.
  bool RenderBreadcrumbs();

  // Asset display helpers
  static ImVec4 GetAssetColor(AssetType type);
  static const char* GetAssetAbbrev(AssetType type);

  // True when at the virtual top level (showing App/Engine/Editor folders)
  bool IsAtTopLevel() const { return root_.empty(); }

 private:
  std::string root_ = "app://";
  std::string current_dir_;

  // Cached directory listing (invalidated on navigation or explicit Invalidate())
  std::vector<BrowserEntry> cached_entries_;
  std::string cached_dir_;
  AssetType cached_filter_ = AssetType::None;
  bool cache_dirty_ = true;

  // Tile grid state (valid between BeginTileGrid / EndTileGrid)
  int grid_columns_ = 1;
  int grid_col_ = 0;
};

// ImGui file picker modal. Call Open() to show, Render() each frame.
// When the user picks a file, the callback fires with the VFS path.
class VfsFilePicker {
 public:
  using Callback = std::function<void(const std::string& vfs_path)>;

  // Open for picking an existing file.
  void Open(const std::string& title, AssetType filter, Callback callback);

  // Open for saving (allows typing a new filename).
  // extension: e.g. ".wscene" - appended automatically if missing.
  void OpenSave(const std::string& title, const std::string& extension,
                Callback callback);

  void Render();

  bool IsOpen() const { return open_; }

 private:
  bool open_ = false;
  bool should_open_ = false;
  bool save_mode_ = false;
  std::string title_;
  std::string save_extension_;
  AssetType filter_ = AssetType::None;
  Callback callback_;
  VfsBrowser browser_;
  std::string selected_file_;
  char filename_buf_[256] = {};
};

}  // namespace Wiesel::Editor
