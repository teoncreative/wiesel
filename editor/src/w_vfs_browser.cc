//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_vfs_browser.h"

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include "asset/w_asset_manager.h"
#include "asset/w_asset_utils.h"
#include "util/imgui/imgui_lucide.h"
#include <urkern/natural_sort.h>
#include "w_engine.h"

namespace wiesel::editor {

// ---- VfsBrowser - Navigation ----

void VfsBrowser::SetRoot(const std::string& root) {
  root_ = root;
  current_dir_.clear();
  cache_dirty_ = true;
}

void VfsBrowser::NavigateInto(const std::string& dir_name) {
  // At top level, navigating into a folder sets the VFS root
  if (root_.empty()) {
    static const std::map<std::string, std::string> root_map = {
        {"App", "app://"},
        {"Engine", "engine://"},
        {"Editor", "editor://"},
    };
    auto it = root_map.find(dir_name);
    if (it != root_map.end()) {
      root_ = it->second;
      current_dir_.clear();
      cache_dirty_ = true;
      return;
    }
  }
  current_dir_ += dir_name + "/";
  cache_dirty_ = true;
}

bool VfsBrowser::NavigateUp() {
  if (current_dir_.empty()) {
    // Go back to top level
    if (!root_.empty()) {
      root_.clear();
      cache_dirty_ = true;
      return true;
    }
    return false;
  }
  std::string trimmed = current_dir_.substr(0, current_dir_.size() - 1);
  auto slash = trimmed.rfind('/');
  if (slash == std::string::npos) {
    current_dir_.clear();
  } else {
    current_dir_ = trimmed.substr(0, slash + 1);
  }
  cache_dirty_ = true;
  return true;
}

void VfsBrowser::Invalidate() {
  cache_dirty_ = true;
}

std::string VfsBrowser::CurrentVfsDir() const {
  return root_ + current_dir_;
}

const std::vector<BrowserEntry>& VfsBrowser::Scan(AssetType filter) {
  std::string current = CurrentVfsDir();
  if (!cache_dirty_ && cached_dir_ == current && cached_filter_ == filter) {
    return cached_entries_;
  }

  cached_entries_.clear();
  cached_dir_ = current;
  cached_filter_ = filter;
  cache_dirty_ = false;

  // At top level, show virtual root folders
  if (root_.empty()) {
    cached_entries_.push_back({"App", "app://", true, AssetType::None});
    cached_entries_.push_back({"Engine", "engine://", true, AssetType::None});
    cached_entries_.push_back({"Editor", "editor://", true, AssetType::None});
    return cached_entries_;
  }

  auto vfs_entries = Engine::vfs()->ListDirectory(current);

  for (auto& ve : vfs_entries) {
    BrowserEntry be;
    be.name = ve.name;
    be.vfs_path = ve.vfs_path;
    be.is_dir = ve.is_dir;
    be.asset_type = AssetType::None;

    if (!be.is_dir) {
      std::string ext = VirtualFileSystem::Extension(be.name);
      be.asset_type = ExtToAssetType(ext);
      if (be.asset_type == AssetType::None && ext == ".cs") {
        be.asset_type = AssetType::Script;
      }
    }

    if (filter != AssetType::None && !be.is_dir && be.asset_type != filter) {
      continue;
    }

    cached_entries_.push_back(std::move(be));
  }

  std::ranges::sort(cached_entries_,
                    [](const BrowserEntry& a, const BrowserEntry& b) {
                      if (a.is_dir != b.is_dir) {
                        return a.is_dir > b.is_dir;
                      }
                      return urkern::NaturalLess(a.name, b.name);
                    });

  return cached_entries_;
}

std::vector<std::pair<std::string, std::string>> VfsBrowser::Breadcrumbs()
    const {
  std::vector<std::pair<std::string, std::string>> result;

  // Top-level "Assets" crumb (always present, navigates back to root)
  result.emplace_back("Assets", "");

  if (root_.empty()) {
    return result;
  }

  // VFS root crumb (e.g. "app" / "engine" / "editor")
  std::string root_display = root_;
  auto scheme_pos = root_display.find("://");
  if (scheme_pos != std::string::npos) {
    root_display = root_display.substr(0, scheme_pos);
  }
  // Use a sentinel so clicking this goes to root level of this VFS mount
  result.emplace_back(root_display, "/");

  if (current_dir_.empty()) {
    return result;
  }

  std::string accumulated;
  size_t pos = 0;
  while (pos < current_dir_.size()) {
    auto slash = current_dir_.find('/', pos);
    if (slash == std::string::npos) {
      break;
    }
    std::string segment = current_dir_.substr(pos, slash - pos);
    accumulated += segment + "/";
    result.emplace_back(segment, accumulated);
    pos = slash + 1;
  }

  return result;
}

// ---- VfsBrowser - Tile rendering ----

void VfsBrowser::BeginTileGrid() {
  float panel_width = ImGui::GetContentRegionAvail().x;
  // Outer tile width is tile_size + 2 * inner_pad (6 each side). No extra
  // spacing — tiles sit flush with their neighbors.
  float cell_size = tile_size + 12.0f;
  grid_columns_ = std::max(1, static_cast<int>(panel_width / cell_size));
  grid_col_ = 0;
  // Kill the per-item vertical gap so rows stack flush as well.
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
}

bool VfsBrowser::DrawTile(const char* label, ImVec4 icon_color,
                          const char* type_abbrev, bool is_selected,
                          bool is_folder, const ThumbnailEntry* thumbnail,
                          const AssetMetadata* asset_meta,
                          bool* double_clicked) {
  bool clicked = false;
  ImGui::PushID(label);

  const ImGuiStyle& style = ImGui::GetStyle();
  const float outer_rounding = style.WindowRounding;
  const float inner_rounding = style.FrameRounding;
  const float inner_pad = 6.0f;      // space between outer box edge and the preview rect
  const float label_pad = 4.0f;      // gap between preview rect and label
  const float label_h = ImGui::GetTextLineHeight();
  // Preview rect is half-height relative to tile_size, so tiles read as
  // landscape banners instead of squares.
  const float preview_h = tile_size * 0.5f;

  ImVec2 cursor = ImGui::GetCursorScreenPos();
  // Outer tile box: inner_pad on all sides + preview + label_pad gap + label.
  const ImVec2 box_min = cursor;
  const ImVec2 box_max = ImVec2(
      cursor.x + tile_size + inner_pad * 2.0f,
      cursor.y + inner_pad + preview_h + label_pad + label_h + inner_pad);
  // Inner preview box (icon / thumbnail rect).
  const ImVec2 icon_min = ImVec2(box_min.x + inner_pad, box_min.y + inner_pad);
  const ImVec2 icon_max = ImVec2(icon_min.x + tile_size, icon_min.y + preview_h);

  if (ImGui::InvisibleButton("##tile", ImVec2(box_max.x - box_min.x,
                                              box_max.y - box_min.y))) {
    clicked = true;
  }
  if (double_clicked && ImGui::IsItemHovered() &&
      ImGui::IsMouseDoubleClicked(0)) {
    *double_clicked = true;
  }
  bool hovered = ImGui::IsItemHovered();

  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Selection / hover highlight on the outer box.
  if (is_selected) {
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImVec4 accent_fill = accent;
    accent_fill.w = 0.18f;
    dl->AddRectFilled(box_min, box_max,
                      ImGui::ColorConvertFloat4ToU32(accent_fill),
                      outer_rounding);
    dl->AddRect(box_min, box_max,
                ImGui::ColorConvertFloat4ToU32(accent),
                outer_rounding, 0, 1.0f);
  } else if (hovered) {
    dl->AddRectFilled(box_min, box_max, IM_COL32(255, 255, 255, 18),
                      outer_rounding);
  }

  // Inner preview area. Background matches input frames (dark + 1px border)
  // so every tile reads as a consistent container regardless of its asset
  // type. Thumbnails draw inside that container.
  const ImU32 inner_bg = ImGui::ColorConvertFloat4ToU32(
      ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
  const ImU32 inner_border = ImGui::ColorConvertFloat4ToU32(
      ImGui::GetStyleColorVec4(ImGuiCol_Border));
  const float preview_w = icon_max.x - icon_min.x;
  const float preview_h_rect = icon_max.y - icon_min.y;

  dl->AddRectFilled(icon_min, icon_max, inner_bg, inner_rounding);

  if (thumbnail && thumbnail->texture_id) {
    // Fit the image inside the (possibly-rectangular) preview rect.
    const float max_side = std::min(preview_w, preview_h_rect);
    ImVec2 img_size = thumbnail->FitSize(max_side);
    float ox = (preview_w - img_size.x) * 0.5f;
    float oy = (preview_h_rect - img_size.y) * 0.5f;
    ImVec2 img_min(icon_min.x + ox, icon_min.y + oy);
    ImVec2 img_max(img_min.x + img_size.x, img_min.y + img_size.y);
    dl->AddImageRounded(reinterpret_cast<ImTextureID>(thumbnail->texture_id),
                        img_min, img_max, thumbnail->uv0, thumbnail->uv1,
                        IM_COL32_WHITE, inner_rounding);
  } else if (is_folder) {
    // Folder icon, centered, slightly bigger than body text.
    const char* glyph = ICON_LC_FOLDER;
    const float icon_size = ImGui::GetFontSize() * 1.5f;
    ImVec2 glyph_sz = ImGui::CalcTextSize(glyph);
    float scale = icon_size / glyph_sz.y;
    ImVec2 scaled_sz(glyph_sz.x * scale, glyph_sz.y * scale);
    ImVec2 pos(icon_min.x + (preview_w - scaled_sz.x) * 0.5f,
               icon_min.y + (preview_h_rect - scaled_sz.y) * 0.5f);
    dl->AddText(ImGui::GetFont(), icon_size, pos,
                ImGui::ColorConvertFloat4ToU32(
                    ImGui::GetStyleColorVec4(ImGuiCol_Text)),
                glyph);
  } else if (type_abbrev && type_abbrev[0]) {
    // Type abbreviation tinted with the asset-type color.
    ImVec4 tint = icon_color;
    ImVec2 text_sz = ImGui::CalcTextSize(type_abbrev);
    ImVec2 text_pos(icon_min.x + (preview_w - text_sz.x) * 0.5f,
                    icon_min.y + (preview_h_rect - text_sz.y) * 0.5f);
    dl->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(tint), type_abbrev);
  }

  // 1px border around the preview rect (matches input frames).
  if (style.FrameBorderSize > 0.0f)
    dl->AddRect(icon_min, icon_max, inner_border, inner_rounding, 0,
                style.FrameBorderSize);

  // Label below icon (centered, truncated)
  float max_text_w = tile_size;
  std::string display_label = label;
  ImVec2 label_sz = ImGui::CalcTextSize(display_label.c_str());
  if (label_sz.x > max_text_w) {
    while (display_label.size() > 3) {
      display_label.pop_back();
      std::string truncated = display_label + "..";
      if (ImGui::CalcTextSize(truncated.c_str()).x <= max_text_w) {
        display_label = truncated;
        break;
      }
    }
    label_sz = ImGui::CalcTextSize(display_label.c_str());
  }
  float label_x = icon_min.x + (tile_size - label_sz.x) * 0.5f;
  ImVec2 label_pos(label_x, icon_max.y + label_pad);
  dl->AddText(label_pos, IM_COL32(220, 220, 220, 255), display_label.c_str());

  // Tooltip
  if (hovered) {
    if (asset_meta) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(asset_meta->name.c_str());
      ImGui::Separator();
      ImGui::Text("Type: %s", AssetTypeToString(asset_meta->type));
      ImGui::Text("Handle: %s", asset_meta->handle.ToString().c_str());
      ImGui::Text("Path: %s", asset_meta->virtual_source_path.c_str());

      AssetLoadState load_state = asset_meta->load_state.load();
      const char* state_str = "Unknown";
      switch (load_state) {
        case AssetLoadState::Unloaded:
          state_str = "Unloaded";
          break;
        case AssetLoadState::Loading:
          state_str = "Loading";
          break;
        case AssetLoadState::Loaded:
          state_str = "Loaded";
          break;
        case AssetLoadState::Failed:
          state_str = "Failed";
          break;
      }
      ImGui::Text("State: %s", state_str);

      auto physical_path =
          Engine::vfs()->GetPhysicalPath(asset_meta->virtual_source_path);
      ImGui::Text("Source: %s",
                  physical_path.has_value() ? "Filesystem" : "Archive");

      ImGui::EndTooltip();
    } else {
      ImGui::SetTooltip("%s", label);
    }
  }

  ImGui::PopID();
  return clicked;
}

void VfsBrowser::NextColumn() {
  grid_col_++;
  if (grid_col_ < grid_columns_) {
    ImGui::SameLine(0.0f, 0.0f);
  } else {
    grid_col_ = 0;
  }
}

void VfsBrowser::EndTileGrid() {
  grid_col_ = 0;
  ImGui::PopStyleVar();
}

bool VfsBrowser::RenderBreadcrumbs() {
  bool changed = false;
  auto crumbs = Breadcrumbs();

  const ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
  for (size_t i = 0; i < crumbs.size(); i++) {
    if (i > 0) {
      ImGui::SameLine();
      ImGui::AlignTextToFramePadding();
      ImGui::PushStyleColor(ImGuiCol_Text, muted);
      ImGui::TextUnformatted(ICON_LC_CHEVRON_RIGHT);
      ImGui::PopStyleColor();
      ImGui::SameLine();
    }
    bool is_last = (i == crumbs.size() - 1);

    if (!is_last) {
      ImGui::PushStyleColor(ImGuiCol_Text, muted);
      const bool clicked = ImGui::Button(crumbs[i].first.c_str());
      ImGui::PopStyleColor();
      if (clicked) {
        if (i == 0) {
          // "Assets" crumb - go back to top level
          root_.clear();
          current_dir_.clear();
          cache_dirty_ = true;
        } else if (crumbs[i].second == "/") {
          // VFS root crumb - go to root of this mount
          current_dir_.clear();
          cache_dirty_ = true;
        } else {
          SetCurrentDir(crumbs[i].second);
        }
        changed = true;
      }
    } else {
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(crumbs[i].first.c_str());
    }
  }
  return changed;
}

// ---- Asset display helpers ----

ImVec4 VfsBrowser::GetAssetColor(AssetType type) {
  switch (type) {
    case AssetType::Texture:
      return {0.25f, 0.45f, 0.72f, 1.0f};
    case AssetType::Model:
      return {0.30f, 0.62f, 0.35f, 1.0f};
    case AssetType::Material:
      return {0.72f, 0.50f, 0.20f, 1.0f};
    case AssetType::Shader:
      return {0.55f, 0.30f, 0.68f, 1.0f};
    case AssetType::Sprite:
      return {0.20f, 0.60f, 0.65f, 1.0f};
    case AssetType::Skybox:
      return {0.25f, 0.55f, 0.55f, 1.0f};
    case AssetType::Font:
      return {0.65f, 0.60f, 0.25f, 1.0f};
    case AssetType::Script:
      return {0.55f, 0.70f, 0.30f, 1.0f};
    case AssetType::Scene:
      return {0.72f, 0.35f, 0.35f, 1.0f};
    case AssetType::Prefab:
      return {0.45f, 0.55f, 0.72f, 1.0f};
    case AssetType::Audio:
      return {0.72f, 0.45f, 0.60f, 1.0f};
    case AssetType::AnimClip:
      return {0.30f, 0.70f, 0.45f, 1.0f};
    case AssetType::AnimController:
      return {0.45f, 0.55f, 0.75f, 1.0f};
    case AssetType::UIDocument:
      return {0.85f, 0.40f, 0.20f, 1.0f};
    case AssetType::UIStylesheet:
      return {0.20f, 0.50f, 0.85f, 1.0f};
    case AssetType::CursorSet:
      return {0.65f, 0.45f, 0.70f, 1.0f};
    case AssetType::MeshCollider:
      return {0.50f, 0.72f, 0.50f, 1.0f};
    default:
      return {0.40f, 0.40f, 0.40f, 1.0f};
  }
}

const char* VfsBrowser::GetAssetAbbrev(AssetType type) {
  switch (type) {
    case AssetType::Texture:
      return "TEX";
    case AssetType::Model:
      return "MDL";
    case AssetType::Material:
      return "MAT";
    case AssetType::Shader:
      return "SHD";
    case AssetType::Sprite:
      return "SPR";
    case AssetType::Skybox:
      return "SKY";
    case AssetType::Font:
      return "FNT";
    case AssetType::Script:
      return "CS";
    case AssetType::Scene:
      return "SCN";
    case AssetType::Prefab:
      return "PFB";
    case AssetType::Audio:
      return "SND";
    case AssetType::AnimClip:
      return "CLP";
    case AssetType::AnimController:
      return "CTR";
    case AssetType::UIDocument:
      return "RML";
    case AssetType::UIStylesheet:
      return "CSS";
    case AssetType::CursorSet:
      return "CUR";
    case AssetType::MeshCollider:
      return "COL";
    default:
      return "?";
  }
}

// ---- VfsFilePicker ----

void VfsFilePicker::Open(const std::string& title, AssetType filter,
                         Callback callback) {
  title_ = title;
  filter_ = filter;
  save_mode_ = false;
  save_extension_.clear();
  callback_ = std::move(callback);
  browser_.SetRoot("app://");
  browser_.SetCurrentDir("");
  browser_.tile_size = 72.0f;
  selected_file_.clear();
  filename_buf_[0] = '\0';
  open_ = true;
  should_open_ = true;
}

void VfsFilePicker::OpenSave(const std::string& title,
                             const std::string& extension, Callback callback) {
  title_ = title;
  filter_ = AssetType::None;
  save_mode_ = true;
  save_extension_ = extension;
  callback_ = std::move(callback);
  browser_.SetRoot("app://");
  browser_.SetCurrentDir("");
  browser_.tile_size = 72.0f;
  selected_file_.clear();
  filename_buf_[0] = '\0';
  open_ = true;
  should_open_ = true;
}

void VfsFilePicker::Render() {
  if (!open_) {
    return;
  }

  if (should_open_) {
    ImGui::OpenPopup(title_.c_str());
    should_open_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(650, 500), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal(title_.c_str(), &open_)) {
    browser_.RenderBreadcrumbs();
    ImGui::Separator();

    const auto& entries = browser_.Scan(filter_);

    ImVec2 list_size(0, ImGui::GetContentRegionAvail().y - 35);
    if (ImGui::BeginChild("##picker_grid", list_size)) {
      browser_.BeginTileGrid();

      // ".." to go up
      if (!browser_.current_dir().empty()) {
        bool dbl = false;
        browser_.DrawTile("..", ImVec4(0.38f, 0.38f, 0.38f, 1.0f), "..", false,
                          true, nullptr, nullptr, &dbl);
        if (dbl) {
          browser_.NavigateUp();
        }
        browser_.NextColumn();
      }

      auto& mgr = Engine::asset_manager();
      ThumbnailCache* thumbs = ThumbnailCache::Get();

      for (auto& entry : entries) {
        bool is_sel = (selected_file_ == entry.vfs_path);
        bool dbl = false;

        // Look up thumbnail
        const ThumbnailEntry* thumb_ptr = nullptr;
        const AssetMetadata* meta_ptr = nullptr;
        ThumbnailEntry thumb_entry;
        AssetHandle handle = mgr.FindBySourcePath(entry.vfs_path);
        if (handle.IsValid()) {
          meta_ptr = mgr.GetMetadata(handle);
          if (thumbs && meta_ptr) {
            thumb_entry = thumbs->GetOrCreate(handle, *meta_ptr);
            if (thumb_entry.texture_id) {
              thumb_ptr = &thumb_entry;
            }
          }
        }

        ImVec4 color = entry.is_dir
                           ? ImVec4(0.38f, 0.38f, 0.38f, 1.0f)
                           : VfsBrowser::GetAssetColor(entry.asset_type);
        const char* abbrev =
            entry.is_dir ? "DIR" : VfsBrowser::GetAssetAbbrev(entry.asset_type);

        if (browser_.DrawTile(entry.name.c_str(), color, abbrev, is_sel,
                              entry.is_dir, thumb_ptr, meta_ptr, &dbl)) {
          selected_file_ = entry.vfs_path;
          if (save_mode_ && !entry.is_dir) {
            snprintf(filename_buf_, sizeof(filename_buf_), "%s",
                     entry.name.c_str());
          }
        }

        if (dbl) {
          if (entry.is_dir) {
            browser_.NavigateInto(entry.name);
            selected_file_.clear();
          } else {
            if (callback_) {
              callback_(entry.vfs_path);
            }
            open_ = false;
            ImGui::CloseCurrentPopup();
          }
        }

        browser_.NextColumn();
      }

      browser_.EndTileGrid();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Bottom bar
    if (save_mode_) {
      // Save mode: filename input + Save button
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 150);
      ImGui::InputTextWithHint("##filename", "filename...", filename_buf_,
                               sizeof(filename_buf_));
      ImGui::SameLine();
      bool can_save = filename_buf_[0] != '\0';
      if (!can_save) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button("Save")) {
        std::string name = filename_buf_;
        if (!save_extension_.empty() &&
            VirtualFileSystem::Extension(name) != save_extension_) {
          name += save_extension_;
        }
        std::string vfs_path = browser_.CurrentVfsDir() + name;
        if (callback_) {
          callback_(vfs_path);
        }
        open_ = false;
        ImGui::CloseCurrentPopup();
      }
      if (!can_save) {
        ImGui::EndDisabled();
      }
      ImGui::SameLine();
    } else {
      // Open mode: selected file + Select button
      if (!selected_file_.empty()) {
        ImGui::TextUnformatted(selected_file_.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Select")) {
          if (callback_) {
            callback_(selected_file_);
          }
          open_ = false;
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
      }
    }
    if (ImGui::Button("Cancel")) {
      open_ = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (!open_) {
    callback_ = nullptr;
  }
}

}  // namespace wiesel::editor
