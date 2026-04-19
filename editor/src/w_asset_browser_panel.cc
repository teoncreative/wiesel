//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_asset_browser_panel.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include "asset/w_asset_manager.h"
#include "asset/w_asset_registry.h"
#include "asset/w_asset_utils.h"
#include "util/imgui/w_imguiutil.h"
#include "util/w_dialogs.h"
#include "util/w_logger.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_project_loader.h"

namespace wiesel::editor {

void AssetBrowserPanel::RevealAsset(const std::string& vfs_path) {
  // Split a vfs path like "app://models/chair/chair.gltf" into the root
  // ("app://"), the relative directory ("models/chair/") and the file
  // name ("chair.gltf"), then point the browser there.
  const size_t scheme_end = vfs_path.find("://");
  if (scheme_end == std::string::npos) {
    return;
  }
  std::string root = vfs_path.substr(0, scheme_end + 3);
  std::string rest = vfs_path.substr(scheme_end + 3);
  const size_t last_slash = rest.find_last_of('/');
  std::string dir =
      (last_slash == std::string::npos) ? "" : rest.substr(0, last_slash + 1);
  std::string file = (last_slash == std::string::npos)
                         ? rest
                         : rest.substr(last_slash + 1);

  browser_.SetRoot(root);
  browser_.SetCurrentDir(dir);
  browser_.Invalidate();
  selected_file_ = file;
}

void AssetBrowserPanel::SetCallbacks(AssetBrowserCallbacks callbacks) {
  callbacks_ = std::move(callbacks);
}

void AssetBrowserPanel::Render(bool& open) {
  if (!open) {
    return;
  }
  if (!ImGui::Begin(ICON_LC_FOLDER_OPEN " Asset Browser", &open)) {
    ImGui::End();
    return;
  }

  auto& mgr = Engine::asset_manager();
  bool read_only = IsReadOnly();
  const auto& entries = browser_.Scan();

  // Breadcrumb bar
  {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    browser_.RenderBreadcrumbs();
    ImGui::PopStyleColor();
  }

  if (!read_only) {
    const ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImGui::PushStyleColor(ImGuiCol_Text, muted);
    ImGui::SameLine();
    if (ImGui::Button(ICON_LC_IMPORT "  Import")) {
      ImGui::OpenPopup("ImportAssetPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_LC_FOLDER_PLUS "  Folder")) {
      ImGui::OpenPopup("NewFolderPopup");
    }
    ImGui::PopStyleColor();
  }

  // New folder popup
  if (!read_only && ImGui::BeginPopup("NewFolderPopup")) {
    ImGui::Text("Folder name:");
    ImGui::InputText("##foldername", new_folder_name_,
                     sizeof(new_folder_name_));
    if (ImGui::Button("Create") && new_folder_name_[0] != '\0') {
      std::string dir_vfs =
          browser_.CurrentVfsDir() + std::string(new_folder_name_);
      Engine::vfs()->CreateDirectory(dir_vfs);
      browser_.Invalidate();
      new_folder_name_[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      new_folder_name_[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Import file into the current asset browser directory
  auto ImportFileToCurrentDir = [this](const std::string& file,
                                       AssetType type) {
    namespace fs = std::filesystem;
    if (file.empty()) {
      return;
    }

    fs::path abs = fs::absolute(file);
    auto dest_physical =
        Engine::vfs()->ResolvePhysicalPath(browser_.CurrentVfsDir());
    if (!dest_physical) {
      DCON_LOG_ERROR("No app:// mount point - open a project first");
      return;
    }
    fs::path dest_dir = *dest_physical;
    auto physical_app = Engine::vfs()->GetPhysicalPath("app://");
    if (!physical_app.has_value()) {
      return;
    }
    fs::path app_assets = fs::absolute(*physical_app);

    std::error_code ec;
    fs::create_directories(dest_dir, ec);

    if (type == AssetType::Model) {
      fs::path source_dir = abs.parent_path();
      fs::path model_dest_dir = dest_dir / abs.stem();
      fs::create_directories(model_dest_dir, ec);
      for (const auto& entry : fs::directory_iterator(source_dir)) {
        if (!entry.is_regular_file()) {
          continue;
        }
        fs::path file_dest = model_dest_dir / entry.path().filename();
        fs::copy_file(entry.path(), file_dest, fs::copy_options::skip_existing,
                      ec);
        if (ec) {
          DCON_LOG_WARN("Failed to copy '{}': {}", entry.path().string(),
                        ec.message());
          ec.clear();
        }
      }
      for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
        if (entry.is_directory()) {
          continue;
        }
        auto rel_to_source = fs::relative(entry.path(), source_dir);
        fs::path file_dest = model_dest_dir / rel_to_source;
        fs::create_directories(file_dest.parent_path(), ec);
        fs::copy_file(entry.path(), file_dest, fs::copy_options::skip_existing,
                      ec);
        ec.clear();
      }
      auto vfs_rel = fs::relative(model_dest_dir / abs.filename(), app_assets);
      std::string vfs_path = "app://" + vfs_rel.generic_string();
      std::string name = abs.stem().string();
      Engine::asset_manager().Register(name, type, vfs_path);
      DCON_LOG_INFO("Imported model directory {} to {}", name, vfs_path);
    } else {
      fs::path dest = dest_dir / abs.filename();
      fs::copy_file(abs, dest, fs::copy_options::skip_existing, ec);
      if (ec) {
        DCON_LOG_ERROR("Failed to import '{}' to '{}': {}", file, dest.string(),
                       ec.message());
        return;
      }
      auto vfs_rel = fs::relative(dest, app_assets);
      std::string vfs_path = "app://" + vfs_rel.generic_string();
      std::string name = abs.stem().string();
      Engine::asset_manager().Register(name, type, vfs_path);
      DCON_LOG_INFO("Imported {} to {}", name, vfs_path);
    }
  };

  if (!read_only && ImGui::BeginPopup("ImportAssetPopup")) {
    if (ImGui::MenuItem("Model...")) {
      dialogs::OpenFileDialog(
          {{"Model file", "obj,gltf,glb,fbx"}},
          [ImportFileToCurrentDir](const std::string& file) {
            ImportFileToCurrentDir(file, AssetType::Model);
          });
    }
    if (ImGui::MenuItem("Texture...")) {
      dialogs::OpenFileDialog(
          {{"Image file", "png,jpg,jpeg,tga,bmp"}},
          [ImportFileToCurrentDir](const std::string& file) {
            ImportFileToCurrentDir(file, AssetType::Texture);
          });
    }
    ImGui::EndPopup();
  }

  // Search + tile size slider share one row, slider pinned to the right.
  {
    ImGui::SameLine();
    const float slider_w = 100.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(avail - slider_w - spacing, 50.0f));
    ImGui::InputTextWithHint("##AssetSearch",
                             ICON_LC_SEARCH "  Search assets...", search_,
                             sizeof(search_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(slider_w);
    ImGui::SliderFloat("##tilesize", &browser_.tile_size, 48.0f, 128.0f, "");
  }

  ImGui::FullWidthSeparator();
  // Slight gap between the separator and the grid below.
  ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

  // Filter entries by case-insensitive substring match on name.
  const bool has_search = search_[0] != '\0';
  std::string search_lower;
  if (has_search) {
    search_lower = search_;
    std::transform(search_lower.begin(), search_lower.end(),
                   search_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
  }
  std::vector<const BrowserEntry*> filtered;
  filtered.reserve(entries.size());
  for (const auto& e : entries) {
    if (!has_search) {
      filtered.push_back(&e);
      continue;
    }
    std::string lower = e.name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower.find(search_lower) != std::string::npos) {
      filtered.push_back(&e);
    }
  }

  // Content area
  if (ImGui::BeginChild("asset_content", ImVec2(0, 0), ImGuiChildFlags_None)) {
    browser_.BeginTileGrid();
    int columns = browser_.grid_columns();

    // ".." back folder counts as one item in the grid.
    // Hide it while filtering since navigation isn't the user's intent.
    bool has_back = !has_search &&
                    (!browser_.current_dir().empty() || !browser_.IsAtTopLevel());
    int total_items = static_cast<int>(filtered.size()) + (has_back ? 1 : 0);
    // Must stay in sync with the outer-box height in VfsBrowser::DrawTile():
    //   outer_h = tile_size*0.5 + 2*inner_pad(6) + label_pad(4) + text_line_h.
    float row_height = browser_.tile_size * 0.5f + 16.0f +
                       ImGui::GetTextLineHeight() +
                       ImGui::GetStyle().ItemSpacing.y;
    int total_rows = (total_items + columns - 1) / columns;

    // Determine visible row range from scroll position
    float scroll_y = ImGui::GetScrollY();
    float visible_h = ImGui::GetContentRegionAvail().y;
    int first_visible_row =
        std::max(0, static_cast<int>(scroll_y / row_height) - 1);
    int last_visible_row =
        std::min(total_rows - 1,
                 static_cast<int>((scroll_y + visible_h) / row_height) + 1);

    // Reserve space for rows above the visible region
    if (first_visible_row > 0) {
      ImGui::Dummy(ImVec2(0, first_visible_row * row_height));
    }

    // Determine which entry indices are visible (accounting for ".." tile)
    int first_visible_item = first_visible_row * columns;
    int last_visible_item =
        std::min(total_items, (last_visible_row + 1) * columns);

    // ".." back folder
    if (has_back && first_visible_item == 0) {
      bool back_dbl = false;
      browser_.DrawTile("..", ImVec4(0.35f, 0.35f, 0.4f, 1.0f), "..", false,
                        true, nullptr, nullptr, &back_dbl);
      if (back_dbl) {
        browser_.NavigateUp();
      }
      // Drop target on ".." to move files to parent directory
      if (!read_only && ImGui::BeginDragDropTarget()) {
        std::string src_vfs;
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("AssetHandle")) {
          AssetHandle h = *static_cast<const AssetHandle*>(payload->Data);
          const AssetMetadata* m = Engine::asset_manager().GetMetadata(h);
          if (m) {
            src_vfs = m->virtual_source_path;
          }
        } else if (const ImGuiPayload* payload =
                       ImGui::AcceptDragDropPayload("BrowserFile")) {
          src_vfs = std::string(static_cast<const char*>(payload->Data));
        }
        if (!src_vfs.empty()) {
          auto last_slash = src_vfs.rfind('/');
          if (last_slash != std::string::npos) {
            std::string filename = src_vfs.substr(last_slash + 1);
            std::string parent_dir = src_vfs.substr(0, last_slash);
            auto second_slash = parent_dir.rfind('/');
            if (second_slash != std::string::npos) {
              std::string dest_vfs =
                  parent_dir.substr(0, second_slash + 1) + filename;
              if (Engine::vfs()->RenameFile(src_vfs, dest_vfs)) {
                if (current_scene_path == src_vfs) {
                  current_scene_path = dest_vfs;
                  if (callbacks_.on_update_title) {
                    callbacks_.on_update_title();
                  }
                }
                browser_.Invalidate();
                if (callbacks_.on_scan_assets) {
                  callbacks_.on_scan_assets();
                }
              }
            }
          }
        }
        ImGui::EndDragDropTarget();
      }
      browser_.NextColumn();
    }

    // File and directory tiles - only iterate visible range
    int entry_offset = has_back ? 1 : 0;
    int first_entry = std::max(0, first_visible_item - entry_offset);
    int last_entry = std::min(static_cast<int>(filtered.size()),
                              last_visible_item - entry_offset);

    // If the ".." tile is visible but the first few entries start mid-row,
    // we need to align the grid column counter
    if (has_back && first_entry == 0 && first_visible_item == 0) {
      // ".." already rendered above, entries start after it
    } else if (first_entry > 0) {
      // Starting mid-grid, set column position for proper layout
      int grid_pos = (first_entry + entry_offset) % columns;
      browser_.SetGridCol(grid_pos);
      if (grid_pos > 0) {
        ImGui::SameLine(0, 8.0f);
      }
    }

    for (int i = first_entry; i < last_entry; i++) {
      const auto& fe = *filtered[i];
      bool is_sel = selected_file_ == fe.name;
      bool dbl_clicked = false;

      AssetHandle handle;
      const AssetMetadata* meta = nullptr;

      if (fe.is_dir) {
        if (browser_.DrawTile(fe.name.c_str(),
                              ImVec4(0.38f, 0.38f, 0.38f, 1.0f), "DIR", is_sel,
                              true, nullptr, nullptr, &dbl_clicked)) {
          selected_file_ = fe.name;
        }
        if (dbl_clicked) {
          browser_.NavigateInto(fe.name);
        }
      } else {
        // Look up asset in AssetManager for thumbnails
        handle = mgr.FindBySourcePath(fe.vfs_path);
        if (handle.IsValid()) {
          meta = mgr.GetMetadata(handle);
        }

        const ThumbnailEntry* thumbnail = nullptr;
        ThumbnailEntry thumb_entry;
        ThumbnailCache* thumbs = ThumbnailCache::Get();
        if (meta && thumbs &&
            (meta->type == AssetType::Texture ||
             meta->type == AssetType::Sprite)) {
          thumb_entry = thumbs->GetOrCreate(handle, *meta);
          if (thumb_entry.texture_id) {
            thumbnail = &thumb_entry;
          }
        }

        bool is_imported = handle.IsValid();
        ImVec4 tile_color = VfsBrowser::GetAssetColor(fe.asset_type);
        if (!is_imported && fe.asset_type != AssetType::None) {
          tile_color.x *= 0.4f;
          tile_color.y *= 0.4f;
          tile_color.z *= 0.4f;
        }
        if (browser_.DrawTile(fe.name.c_str(), tile_color,
                              VfsBrowser::GetAssetAbbrev(fe.asset_type), is_sel,
                              false, thumbnail, meta, &dbl_clicked)) {
          selected_file_ = fe.name;
          if (handle.IsValid() && callbacks_.on_select_asset) {
            callbacks_.on_select_asset(handle);
          }
        }

        if (dbl_clicked) {
          if (fe.asset_type == AssetType::Scene && callbacks_.on_open_scene) {
            callbacks_.on_open_scene(fe.vfs_path);
          } else if (fe.asset_type == AssetType::Prefab &&
                     callbacks_.on_open_prefab) {
            callbacks_.on_open_prefab(fe.vfs_path);
          } else if ((fe.asset_type == AssetType::Script ||
                      fe.asset_type == AssetType::UIDocument ||
                      fe.asset_type == AssetType::UIStylesheet) &&
                     callbacks_.on_open_code_editor) {
            auto phys = Engine::vfs()->GetPhysicalPath(fe.vfs_path);
            if (phys) {
              callbacks_.on_open_code_editor(*phys);
            }
          } else if (fe.asset_type == AssetType::AnimController &&
                     callbacks_.on_open_anim_controller) {
            callbacks_.on_open_anim_controller(handle);
          }
        }

        // Drag source for asset files (preferred) or raw browser files (fallback)
        if (handle.IsValid() &&
            ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          ImGui::SetDragDropPayload("AssetHandle", &handle,
                                    sizeof(AssetHandle));
          ImGui::Text("%s", fe.name.c_str());
          ImGui::EndDragDropSource();
        } else if (ImGui::BeginDragDropSource(
                       ImGuiDragDropFlags_SourceAllowNullID)) {
          ImGui::SetDragDropPayload("BrowserFile", fe.vfs_path.c_str(),
                                    fe.vfs_path.size() + 1);
          ImGui::Text("%s %s", fe.is_dir ? "[DIR]" : "", fe.name.c_str());
          ImGui::EndDragDropSource();
        }
      }

      // Drop target on directories to move files into them
      if (!read_only && fe.is_dir && ImGui::BeginDragDropTarget()) {
        std::string src_vfs;
        if (const ImGuiPayload* p =
                ImGui::AcceptDragDropPayload("AssetHandle")) {
          AssetHandle h = *static_cast<const AssetHandle*>(p->Data);
          const AssetMetadata* m = Engine::asset_manager().GetMetadata(h);
          if (m) {
            src_vfs = m->virtual_source_path;
          }
        } else if (const ImGuiPayload* p =
                       ImGui::AcceptDragDropPayload("BrowserFile")) {
          src_vfs = std::string(static_cast<const char*>(p->Data));
        }
        if (!src_vfs.empty()) {
          auto last_slash = src_vfs.rfind('/');
          std::string filename = (last_slash != std::string::npos)
                                     ? src_vfs.substr(last_slash + 1)
                                     : src_vfs;
          std::string dest_vfs = fe.vfs_path + "/" + filename;
          if (Engine::vfs()->RenameFile(src_vfs, dest_vfs)) {
            if (current_scene_path == src_vfs) {
              current_scene_path = dest_vfs;
              if (callbacks_.on_update_title) {
                callbacks_.on_update_title();
              }
            }
            browser_.Invalidate();
            if (callbacks_.on_scan_assets) {
              callbacks_.on_scan_assets();
            }
          }
        }
        ImGui::EndDragDropTarget();
      }

      // Right-click context menu (only for writable roots)
      if (!read_only && is_sel &&
          ImGui::BeginPopupContextItem(("##ctx_" + fe.name).c_str())) {
        if (!fe.is_dir && !handle.IsValid() &&
            fe.asset_type != AssetType::None) {
          if (ImGui::MenuItem("Import")) {
            std::string import_vfs = browser_.CurrentVfsDir() + fe.name;
            AssetHandle new_handle =
                GameLoader::ImportAsset(fe.name, fe.asset_type, import_vfs);
            if (new_handle.IsValid()) {
              if (fe.asset_type == AssetType::Prefab ||
                  fe.asset_type == AssetType::Scene) {
                mgr.SetLoadState(new_handle, AssetLoadState::Unloaded,
                                 AssetLoadState::Loaded);
              }
            }
          }
          ImGui::Separator();
        }
        if (!fe.is_dir && fe.asset_type == AssetType::Texture &&
            handle.IsValid()) {
          if (ImGui::MenuItem("Slice into Sprites")) {
            if (callbacks_.on_slice_texture) {
              callbacks_.on_slice_texture(handle);
            }
            ImGui::CloseCurrentPopup();
          }
          ImGui::Separator();
        }
        if (ImGui::MenuItem("Rename")) {
          renaming_file_ = fe.name;
          std::string stem = VirtualFileSystem::Stem(fe.name);
          snprintf(rename_buf_, sizeof(rename_buf_), "%s", stem.c_str());
        }
        if (!fe.is_dir && ImGui::MenuItem("Duplicate")) {
          std::string stem = VirtualFileSystem::Stem(fe.name);
          std::string ext = VirtualFileSystem::Extension(fe.name);
          std::string parent_vfs =
              fe.vfs_path.substr(0, fe.vfs_path.rfind('/') + 1);
          std::string copy_vfs = parent_vfs + stem + "_copy" + ext;
          int n = 1;
          while (Engine::vfs()->FileExists(copy_vfs)) {
            copy_vfs = parent_vfs + stem + "_copy" + std::to_string(n++) + ext;
          }
          if (Engine::vfs()->CopyFile(fe.vfs_path, copy_vfs)) {
            browser_.Invalidate();
            if (callbacks_.on_scan_assets) {
              callbacks_.on_scan_assets();
            }
          }
        }
        if (fe.is_dir && ImGui::MenuItem("Duplicate")) {
          std::string parent_vfs =
              fe.vfs_path.substr(0, fe.vfs_path.rfind('/') + 1);
          std::string copy_vfs = parent_vfs + fe.name + "_copy";
          int n = 1;
          while (Engine::vfs()->FileExists(copy_vfs)) {
            copy_vfs = parent_vfs + fe.name + "_copy" + std::to_string(n++);
          }
          if (Engine::vfs()->CopyDirectory(fe.vfs_path, copy_vfs)) {
            browser_.Invalidate();
            if (callbacks_.on_scan_assets) {
              callbacks_.on_scan_assets();
            }
          }
        }
        if (!fe.is_dir && handle.IsValid() &&
            AssetRegistry::HasProperties(fe.asset_type)) {
          // Properties panel auto-selects on click, no menu item needed.
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
          if (fe.is_dir) {
            Engine::vfs()->DeleteDirectory(fe.vfs_path);
          } else {
            Engine::vfs()->DeleteFile(fe.vfs_path);
          }
          selected_file_.clear();
          browser_.Invalidate();
          if (callbacks_.on_scan_assets) {
            callbacks_.on_scan_assets();
          }
        }
        ImGui::EndPopup();
      }

      browser_.NextColumn();
    }

    // Reserve space for rows below the visible region
    int rows_after = total_rows - last_visible_row - 1;
    if (rows_after > 0) {
      ImGui::Dummy(ImVec2(0, rows_after * row_height));
    }

    // Rename popup
    if (!renaming_file_.empty()) {
      ImGui::OpenPopup("RenamePopup");
    }
    if (ImGui::BeginPopup("RenamePopup")) {
      ImGui::Text("Rename:");
      ImGui::InputText("##rename", rename_buf_, sizeof(rename_buf_));
      if (ImGui::Button("OK") && rename_buf_[0] != '\0') {
        std::string old_vfs = browser_.CurrentVfsDir() + renaming_file_;
        std::string ext = VirtualFileSystem::Extension(renaming_file_);
        std::string new_name = std::string(rename_buf_) + ext;
        std::string new_vfs = browser_.CurrentVfsDir() + new_name;
        if (Engine::vfs()->RenameFile(old_vfs, new_vfs)) {
          if (current_scene_path == old_vfs) {
            current_scene_path = new_vfs;
            if (callbacks_.on_update_title) {
              callbacks_.on_update_title();
            }
          }
          browser_.Invalidate();
          if (callbacks_.on_scan_assets) {
            callbacks_.on_scan_assets();
          }
        }
        renaming_file_.clear();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        renaming_file_.clear();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // Right-click on empty space (create menu, app:// only)
    if (!read_only && ImGui::BeginPopupContextWindow(
                          "##browser_ctx", ImGuiPopupFlags_NoOpenOverItems)) {
      if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Scene")) {
          if (callbacks_.on_new_scene) {
            callbacks_.on_new_scene();
          }
        }
        if (ImGui::MenuItem("Folder")) {
          open_folder_popup_ = true;
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("C# Script")) {
          open_script_popup_ = true;
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Skybox")) {
          if (callbacks_.on_show_create_skybox) {
            callbacks_.on_show_create_skybox();
          }
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Sprite")) {
          if (callbacks_.on_show_create_sprite) {
            callbacks_.on_show_create_sprite();
          }
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Animation Controller")) {
          if (callbacks_.on_create_anim_controller) {
            callbacks_.on_create_anim_controller();
          }
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Cursor Set")) {
          if (callbacks_.on_show_create_cursorset) {
            callbacks_.on_show_create_cursorset();
          }
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Mesh Collider")) {
          if (callbacks_.on_show_create_meshcollider) {
            callbacks_.on_show_create_meshcollider();
          }
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
      }
      ImGui::EndPopup();
    }
    if (open_folder_popup_) {
      ImGui::OpenPopup("NewFolderPopup");
      open_folder_popup_ = false;
    }
  }
  browser_.EndTileGrid();
  ImGui::EndChild();

  // New C# script popup
  if (open_script_popup_) {
    ImGui::OpenPopup("NewScriptPopup");
    open_script_popup_ = false;
  }
  if (ImGui::BeginPopup("NewScriptPopup")) {
    ImGui::Text("Script name:");
    ImGui::InputText("##scriptname", new_script_name_,
                     sizeof(new_script_name_));
    if (ImGui::Button("Create") && new_script_name_[0] != '\0') {
      namespace fs = std::filesystem;
      auto physical_app = Engine::vfs()->GetPhysicalPath("app://");
      if (physical_app.has_value()) {
        fs::path base = fs::absolute(*physical_app);
        if (!browser_.current_dir().empty()) {
          base = base / browser_.current_dir();
        }
        fs::path script_path = base / (std::string(new_script_name_) + ".cs");
        if (!fs::exists(script_path)) {
          VfsFile tmpl =
              Engine::vfs()->Open("engine://templates/script.cs.template");
          std::string content;
          if (tmpl) {
            content = std::string(reinterpret_cast<const char*>(tmpl.Data()),
                                  tmpl.Size());
          } else {
            content =
                "using WieselEngine;\n\npublic class {{CLASS_NAME}} : "
                "MonoBehavior\n{\n}\n";
          }
          std::string class_name = new_script_name_;
          size_t pos = 0;
          while ((pos = content.find("{{CLASS_NAME}}", pos)) !=
                 std::string::npos) {
            content.replace(pos, 14, class_name);
            pos += class_name.length();
          }
          std::ofstream out(script_path);
          if (out.is_open()) {
            out << content;
          }
          browser_.Invalidate();
          if (callbacks_.on_scan_assets) {
            callbacks_.on_scan_assets();
          }
        }
      }
      new_script_name_[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      new_script_name_[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

}  // namespace wiesel::editor
