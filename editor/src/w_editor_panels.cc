//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor.h"

#include <imgui.h>

#include "asset/w_asset_manager.h"
#include "asset/w_asset_registry.h"
#include "rendering/w_render_feature.h"
#include "rendering/w_rendergraph.h"
#include "scene/w_scene_manager.h"
#include "ui/w_font.h"
#include "util/w_thread_pool.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

namespace Wiesel::Editor {

// Defined in w_editor.cc
std::shared_ptr<Scene> scene();

void EditorLayer::RenderRenderStatsPanel() {
  {
    bool& stats_open = panel_stats_;
    if (stats_open) {
      if (ImGui::Begin(CODICON_DASHBOARD " Render Stats", &stats_open)) {
        std::shared_ptr<Renderer> renderer = Engine::renderer();
        const auto& stats = renderer->GetStats();

        ImGui::SeparatorText("Performance");
        ImGui::Text("FPS: %.1f", app_.GetFPS());
        ImGui::Text("Frame Time: %.2f ms", stats.frame_time_ms);
        ImGui::Text("Delta Time: %.4f s", app_.GetDeltaTime());

        ImGui::SeparatorText("Draw Stats");
        ImGui::Text("Draw Calls: %u", stats.draw_calls);
        ImGui::Text("Models: %u", stats.models);
        ImGui::Text("Meshes: %u", stats.meshes);
        ImGui::Text("Vertices: %u", stats.vertices);
        ImGui::Text("Triangles: %u", stats.triangles);

        ImGui::SeparatorText("Renderer");
        ImGui::Text("MSAA: %s",
                    renderer->options().msaa_mode == SamplingMode::DISABLED
                        ? "Off"
                    : renderer->options().msaa_mode == SamplingMode::X2 ? "2x"
                    : renderer->options().msaa_mode == SamplingMode::X4 ? "4x"
                                                                        : "8x");
        ImGui::Text("VSync: %s", renderer->options().vsync ? "On" : "Off");
        ImGui::Text(
            "AA Mode: %s",
            renderer->options().aa_mode == AntiAliasingMode::None   ? "None"
            : renderer->options().aa_mode == AntiAliasingMode::FXAA ? "FXAA"
                                                                    : "TAA");
        ImGui::Text("Swap Chain Images: %u", stats.swap_chain_images);
        ImGui::Text("Frames in Flight: %u", stats.frames_in_flight);

        ImGui::SeparatorText("GPU Memory");
        {
          float used_mb =
              static_cast<float>(stats.gpu_memory_used) / (1024.0f * 1024.0f);
          float budget_mb =
              static_cast<float>(stats.gpu_memory_budget) / (1024.0f * 1024.0f);
          float ratio = budget_mb > 0 ? used_mb / budget_mb : 0.0f;
          ImGui::Text("VRAM: %.1f / %.1f MB (%.0f%%)", used_mb, budget_mb,
                      ratio * 100.0f);
          ImGui::ProgressBar(ratio, ImVec2(-1, 0));
        }
        ImGui::Text("VMA Allocations: %u (%u/s)", stats.gpu_allocation_count,
                    stats.gpu_allocations_per_second);
        {
          float tex_mb =
              static_cast<float>(stats.texture_memory) / (1024.0f * 1024.0f);
          ImGui::Text("Textures: %u (%.1f MB)", stats.texture_count, tex_mb);
        }
        ImGui::Text("Deletion Queue: %u", stats.deletion_queue_pending);
        ImGui::Text("Thread Pool Queue: %zu",
                    Engine::thread_pool().GetQueueSize());

        ImGui::SeparatorText("Assets");
        auto asset_stats = Engine::asset_manager().GetStats();
        ImGui::Text("Total: %zu", asset_stats.total);
        ImGui::Text("Loaded: %zu", asset_stats.loaded);
        if (asset_stats.loading > 0) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Loading: %zu",
                             asset_stats.loading);
        } else {
          ImGui::Text("Loading: %zu", asset_stats.loading);
        }
        ImGui::Text("Unloaded: %zu", asset_stats.unloaded);
        if (asset_stats.failed > 0) {
          ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Failed: %zu",
                             asset_stats.failed);
        }

        {
          ImGui::SeparatorText("Shadow Cascades");
          auto cam = renderer->GetCameraData();
          if (cam) {
            ImGui::Text("Shadows: %s", cam->does_shadow_pass ? "ON" : "OFF");
            for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; i++) {
              ImGui::Text("Cascade %d: split Z = %.2f", i,
                          cam->shadow_map_cascades[i].SplitDepth);
            }
          }
        }

        if (scene()) {
          ImGui::SeparatorText("Render Pipelines");

          // Collect unique pipelines and their cameras
          struct PipelineInfo {
            RenderPipeline* pipeline;
            std::vector<std::string> cameras;
            bool is_default;
          };

          std::map<RenderPipeline*, PipelineInfo> pipeline_map;

          auto default_pipeline = Engine::scene_manager().GetDefaultPipeline();
          if (default_pipeline) {
            pipeline_map[default_pipeline.get()] = {
                default_pipeline.get(), {}, true};
          }

          for (auto& s : Engine::scene_manager().GetLoadedScenes()) {
            for (entt::entity entity :
                 s->GetAllEntitiesWith<CameraComponent, TagComponent>()) {
              auto& cam = s->GetComponent<CameraComponent>(entity);
              auto& tag = s->GetComponent<TagComponent>(entity);
              RenderPipeline* pl = cam.render_pipeline
                                       ? cam.render_pipeline.get()
                                       : default_pipeline.get();
              if (pl) {
                auto& info = pipeline_map[pl];
                info.pipeline = pl;
                info.cameras.push_back(tag.name);
                if (pl == default_pipeline.get()) {
                  info.is_default = true;
                }
              }
            }
          }

          for (const auto& [ptr, info] : pipeline_map) {
            std::string label =
                info.is_default ? "Default Pipeline" : "Custom Pipeline";
            if (ImGui::TreeNodeEx(label.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
              // Cameras using this pipeline
              ImGui::TextDisabled("Cameras:");
              if (info.cameras.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(none)");
              }
              for (const auto& cam_name : info.cameras) {
                ImGui::SameLine();
                ImGui::Text("%s", cam_name.c_str());
              }

              // Feature list with timings
              ImGui::TextDisabled("Features:");
              const auto& features = info.pipeline->GetFeatures();

              // Collect pass timings: prefer external render graph (editor camera)
              // in edit mode since ECS camera graphs may have stale data.
              std::vector<PassTimingResult> timings;
              if (editor_state_ == EditorState::Edit) {
                auto ext_graph =
                    Engine::scene_manager().GetExternalRenderGraph();
                if (ext_graph) {
                  timings = ext_graph->GetPassTimings();
                }
              } else {
                for (auto& s : Engine::scene_manager().GetLoadedScenes()) {
                  bool found = false;
                  for (entt::entity entity :
                       s->GetAllEntitiesWith<CameraComponent>()) {
                    auto& cam = s->GetComponent<CameraComponent>(entity);
                    RenderPipeline* cam_pl = cam.render_pipeline
                                                 ? cam.render_pipeline.get()
                                                 : default_pipeline.get();
                    if (cam_pl != ptr) {
                      continue;
                    }
                    auto graph = cam.render_graph;
                    if (graph) {
                      timings = graph->GetPassTimings();
                    }
                    found = true;
                    break;
                  }
                  if (found) {
                    break;
                  }
                }
                // Fallback to external render graph if no ECS camera graph found
                if (timings.empty()) {
                  auto ext_graph =
                      Engine::scene_manager().GetExternalRenderGraph();
                  if (ext_graph) {
                    timings = ext_graph->GetPassTimings();
                  }
                }
              }

              for (const auto& feature : features) {
                const auto& fname = feature->GetName();
                float cpu_total = 0.0f;
                int pass_count = 0;
#ifdef WIESEL_GPU_PROFILING
                float gpu_total = 0.0f;
#endif
                for (const auto& t : timings) {
                  if (t.name.size() >= fname.size() &&
                      t.name.compare(0, fname.size(), fname) == 0) {
                    cpu_total += t.cpu_time_ms;
#ifdef WIESEL_GPU_PROFILING
                    gpu_total += t.gpu_time_ms;
#endif
                    pass_count++;
                  }
                }
                if (pass_count > 0) {
#ifdef WIESEL_GPU_PROFILING
                  ImGui::BulletText("%-20s  CPU %.3f ms  GPU %.3f ms",
                                    fname.c_str(), cpu_total, gpu_total);
#else
                  ImGui::BulletText("%-20s  CPU %.3f ms", fname.c_str(),
                                    cpu_total);
#endif
                } else {
                  ImGui::BulletText("%s", fname.c_str());
                }
              }

              if (!timings.empty()) {
                float total_cpu = 0.0f;
#ifdef WIESEL_GPU_PROFILING
                float total_gpu = 0.0f;
#endif
                for (const auto& t : timings) {
                  total_cpu += t.cpu_time_ms;
#ifdef WIESEL_GPU_PROFILING
                  total_gpu += t.gpu_time_ms;
#endif
                }
#ifdef WIESEL_GPU_PROFILING
                ImGui::Text("  Total: CPU %.3f ms  GPU %.3f ms", total_cpu,
                            total_gpu);
#else
                ImGui::Text("  Total: CPU %.3f ms", total_cpu);
#endif
              }
              ImGui::TreePop();
            }
          }
        }
      }
      ImGui::End();
    }
  }
}

void EditorLayer::RenderDeveloperConsolePanel() {
  {
    bool& console_open = panel_console_;
    if (console_open) {
      static std::vector<std::string> history;
      static int history_pos = -1;  // -1 = new line, 0..N = browsing history
      static char input_buf[512] = "";

      auto HistoryCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
          if (history.empty()) {
            return 0;
          }
          if (data->EventKey == ImGuiKey_UpArrow) {
            if (history_pos == -1) {
              history_pos = static_cast<int>(history.size()) - 1;
            } else if (history_pos > 0) {
              history_pos--;
            }
          } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (history_pos != -1) {
              history_pos++;
              if (history_pos >= static_cast<int>(history.size())) {
                history_pos = -1;
              }
            }
          }
          const char* text =
              (history_pos >= 0) ? history[history_pos].c_str() : "";
          data->DeleteChars(0, data->BufTextLen);
          data->InsertChars(0, text);
        }
        return 0;
      };

      if (ImGui::Begin(CODICON_TERMINAL " Developer Console", &console_open)) {
        auto& cmd = DeveloperConsole::Get();
        const auto& log = cmd.GetLog();

        // Toolbar
        if (ImGui::Button("Clear")) {
          cmd.Clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy Log")) {
          std::string full_log;
          for (const auto& line : log) {
            full_log += line.text;
            full_log += '\n';
          }
          ImGui::SetClipboardText(full_log.c_str());
        }
        ImGui::SameLine();
        static bool auto_scroll = true;
        ImGui::Checkbox("Auto-scroll", &auto_scroll);

        ImGui::Separator();

        // Log output - selectable/copyable text
        float footer_height = ImGui::GetStyle().ItemSpacing.y +
                              ImGui::GetFrameHeightWithSpacing();
        std::string full_log;
        for (const auto& line : log) {
          full_log += line.text;
          full_log += '\n';
        }
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::InputTextMultiline(
            "##ConsoleLog", full_log.data(), full_log.size() + 1,
            ImVec2(-1, -footer_height), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        // Input line with history
        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                          ImGuiInputTextFlags_CallbackHistory;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##ConsoleInput", input_buf, sizeof(input_buf),
                             input_flags, HistoryCallback)) {
          if (input_buf[0] != '\0') {
            // Don't duplicate consecutive identical commands
            if (history.empty() || history.back() != input_buf) {
              history.push_back(input_buf);
            }
            cmd.Execute(input_buf);
            input_buf[0] = '\0';
          }
          history_pos = -1;
          ImGui::SetKeyboardFocusHere(-1);
        }
      }
      ImGui::End();
    }
  }
}

void EditorLayer::RenderAssetBrowserPanel() {
  asset_browser_panel_.current_scene_path = current_scene_path_;
  asset_browser_panel_.Render(panel_asset_browser_);
  current_scene_path_ = asset_browser_panel_.current_scene_path;
}


void EditorLayer::RenderUndoHistoryPanel() {
  if (!panel_undo_history_) {
    return;
  }
  if (ImGui::Begin(CODICON_HISTORY " Undo History", &panel_undo_history_)) {
    const auto& history = command_stack_.GetHistory();
    int current = command_stack_.GetCurrentIndex();

    if (ImGui::Button("Clear History")) {
      command_stack_.Clear();
    }
    ImGui::Separator();

    if (history.empty()) {
      ImGui::TextDisabled("No actions recorded");
    } else {
      // Newest at top, oldest at bottom
      for (int i = static_cast<int>(history.size()) - 1; i >= 0; --i) {
        bool is_current = (i == current);
        bool is_undone = (i > current);

        if (is_undone) {
          ImGui::PushStyleColor(
              ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }

        std::string label =
            std::to_string(i + 1) + ". " + history[i]->GetDescription();
        if (ImGui::Selectable(label.c_str(), is_current)) {
          if (i < current) {
            while (command_stack_.GetCurrentIndex() > i) {
              command_stack_.Undo();
            }
          } else if (i > current) {
            while (command_stack_.GetCurrentIndex() < i) {
              command_stack_.Redo();
            }
          }
          scene_dirty_ = true;
        }

        if (is_undone) {
          ImGui::PopStyleColor();
        }
      }

      // Initial state at the bottom
      bool is_initial = (current < 0);
      if (ImGui::Selectable("-- Initial State --", is_initial)) {
        while (command_stack_.CanUndo()) {
          command_stack_.Undo();
        }
        scene_dirty_ = true;
      }
    }
  }
  ImGui::End();
}

}  // namespace Wiesel::Editor
