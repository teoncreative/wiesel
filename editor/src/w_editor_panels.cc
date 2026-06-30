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
#include "imgui_internal.h"
#include "networking/w_network.h"
#include "networking/w_replication_types.h"
#include "rendering/w_render_feature.h"
#include "rendering/w_rendergraph.h"
#include "scene/w_scene_manager.h"
#include "ui/w_font.h"
#include "util/w_command.h"
#include "util/w_command_parser.h"
#include <urkern/thread_pool.h>
#include "ui/w_ui_draw.h"
#include "ui/w_ui_field.h"
#include "ui/w_ui_layout.h"
#include "ui/w_ui_row.h"
#include "ui/w_ui_style.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

namespace wiesel::editor {

namespace style = ui::style;
namespace draw = ui::draw;

using ui::field::PrefixLabel;

// Defined in w_editor.cc
Scene* scene();

void EditorLayer::RenderRenderStatsPanel() {
  {
    bool& stats_open = panel_stats_;
    if (stats_open) {
      if (ImGui::Begin(ICON_LC_GAUGE " Render Stats", &stats_open)) {
        std::shared_ptr<Renderer> renderer = Engine::renderer();
        const auto& stats = renderer->GetStats();

        ImGui::SeparatorText("Performance");
        ImGui::Text("FPS: %.1f", app_.GetFPS());
        ImGui::Text("Frame Time: %.2f ms", stats.frame_time_ms);
        ImGui::Text("Delta Time: %.4f s", app_.GetDeltaTime());

        ImGui::SeparatorText("Draw Stats");
        ImGui::Text("Draw Calls: %u", stats.draw_calls);
        ImGui::Text("  Instanced: %u", stats.instanced_draw_calls);
        ImGui::Text("  Single: %u", stats.single_draw_calls);
        ImGui::Text("Instances: %u", stats.total_instances);
        ImGui::Text("Saved by Batching: %u", stats.saved_by_batching);
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
      // Selected suggestion index, driven by Up/Down while typing.
      static int suggestion_sel = 0;
      // Snapshot of the current suggestion list (label only) so the Tab
      // completion callback can read it without recomputing.
      static std::vector<std::string> suggestion_labels;
      // Range (start-inclusive, end-exclusive) of the last token in the
      // buffer. Used to replace it with the chosen suggestion on Tab.
      static int token_start_byte = 0;
      static int token_end_byte = 0;
      // Esc dismisses the suggestions until the user types a new
      // character; tracked by snapshotting the buffer contents at the
      // moment Esc was pressed.
      static std::string suggestions_dismissed_for;

      auto ConsoleInputCallback =
          [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
          if (!suggestion_labels.empty()) {
            const std::string& pick =
                suggestion_labels[suggestion_sel %
                                  static_cast<int>(suggestion_labels.size())];
            data->DeleteChars(token_start_byte,
                              token_end_byte - token_start_byte);
            data->InsertChars(token_start_byte, pick.c_str());
            // Trailing space so the user can immediately type the next
            // argument.
            data->InsertChars(data->CursorPos, " ");
          }
          return 0;
        }
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
          // While there's an autocomplete popup, Up/Down navigates it
          // instead of cycling the command history.
          if (!suggestion_labels.empty()) {
            const int n = static_cast<int>(suggestion_labels.size());
            if (data->EventKey == ImGuiKey_UpArrow) {
              suggestion_sel = (suggestion_sel - 1 + n) % n;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
              suggestion_sel = (suggestion_sel + 1) % n;
            }
            return 0;
          }
          if (history.empty()) return 0;
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

      if (ImGui::Begin(ICON_LC_TERMINAL " Developer Console", &console_open)) {
        auto& cmd = DeveloperConsole::Get();
        // Snapshot under lock - background threads push log lines via
        // DCON_LOG_*, which would otherwise reallocate mid-iteration.
        const std::vector<ConsoleLine> log = cmd.SnapshotLog();

        static bool auto_scroll = true;
        static bool word_wrap = false;
        {
          const ImVec4 muted =
              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
          ImGui::PushStyleColor(ImGuiCol_Text, muted);
          if (ImGui::Button(ICON_LC_TRASH_2 "  Clear")) {
            cmd.Clear();
          }
          ImGui::SameLine();
          if (ImGui::Button(ICON_LC_COPY "  Copy Log")) {
            std::string full_log;
            for (const auto& line : log) {
              full_log += line.text;
              full_log += '\n';
              if (!line.stack_trace.empty()) {
                full_log += line.stack_trace;
                full_log += '\n';
              }
            }
            ImGui::SetClipboardText(full_log.c_str());
          }
          ImGui::PopStyleColor();

          // Icon toggles - bright when on, muted when off.
          auto icon_toggle = [&](const char* icon, const char* id, bool& flag,
                                 const char* on_label, const char* off_label) {
            const ImVec4 col = flag
                                   ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                                   : muted;
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::PushID(id);
            if (ImGui::Button(icon)) {
              flag = !flag;
            }
            ImGui::PopID();
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", flag ? on_label : off_label);
            }
          };
          ImGui::SameLine();
          icon_toggle(ICON_LC_ARROW_DOWN_TO_LINE, "autoscroll", auto_scroll,
                      "Auto-scroll: on", "Auto-scroll: off");
          ImGui::SameLine();
          icon_toggle(ICON_LC_TEXT_WRAP, "wrap", word_wrap,
                      "Word wrap: on", "Word wrap: off");
        }

        ui::layout::Separator();

        const ImU32 col_user = ImGui::GetColorU32(ImGuiCol_CheckMark);
        const ImU32 col_info = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 col_warn = style::kLogWarnColor;
        const ImU32 col_err = style::kLogErrorColor;

        // Reserve vertical space for the input bar below.
        const float input_h = ImGui::GetFrameHeight();
        // Clamp: tiny/collapsed panels would otherwise hit EndChild asserts.
        const float log_h =
            std::max(1.0f,
                     ImGui::GetContentRegionAvail().y - input_h -
                         ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##ConsoleLog", ImVec2(0.0f, log_h),
                          ImGuiChildFlags_None,
                          word_wrap ? ImGuiWindowFlags_None
                                    : ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PopStyleColor();
        if (code_editor_font_) {
          ImGui::PushFont(code_editor_font_);
        }
        if (word_wrap) {
          ImGui::PushTextWrapPos(0.0f);
        }
        // Tighter vertical rhythm between log rows (~half of default).
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(ImGui::GetStyle().ItemSpacing.x,
                                   ImGui::GetStyle().ItemSpacing.y * 0.5f));
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
        // Soft-wrap at `max_w` pixels while preserving empty lines.
        auto wrap_text = [](const std::string& text, float max_w) {
          if (max_w <= 0.0f) return text;
          ImFont* f = ImGui::GetFont();
          std::string out;
          out.reserve(text.size() + 16);
          const char* base = text.c_str();
          size_t line_start = 0;
          while (line_start <= text.size()) {
            size_t nl = text.find('\n', line_start);
            const bool last = (nl == std::string::npos);
            if (last) nl = text.size();
            const char* p = base + line_start;
            const char* line_end = base + nl;
            if (p == line_end) {
              out += '\n';
            } else {
              while (p < line_end) {
                const char* w =
                    f->CalcWordWrapPositionA(1.0f, p, line_end, max_w);
                if (w == p) w = (p + 1 <= line_end) ? p + 1 : line_end;
                out.append(p, w - p);
                out += '\n';
                p = w;
                if (p < line_end && *p == ' ') p++;
              }
            }
            if (last) break;
            line_start = nl + 1;
          }
          if (!out.empty() && out.back() == '\n') out.pop_back();
          return out;
        };

        // Render a selectable, colored block of text inline - no inner
        // scroll, no frame, no padding. Backed by InputTextMultiline so
        // character-level selection + copy work.
        auto render_selectable_colored = [&](const std::string& text,
                                             ImU32 color,
                                             const char* id) {
          std::string buf = word_wrap ? wrap_text(
                                            text,
                                            ImGui::GetContentRegionAvail().x)
                                      : text;
          const int line_count =
              1 + static_cast<int>(std::count(buf.begin(), buf.end(), '\n'));
          const float h = ImGui::GetTextLineHeight() *
                          static_cast<float>(line_count);
          ImGui::PushStyleColor(ImGuiCol_Text, color);
          ImGui::PushStyleColor(ImGuiCol_FrameBg,
                                ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
          ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
          ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
          ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                              ImVec2(0.0f, 0.0f));
          ImGui::InputTextMultiline(id, buf.data(), buf.size() + 1,
                                    ImVec2(-1, h),
                                    ImGuiInputTextFlags_ReadOnly);
          ImGui::PopStyleVar(3);
          ImGui::PopStyleColor(2);
        };

        for (size_t i = 0; i < log.size(); i++) {
          const auto& line = log[i];
          ImU32 col = col_info;
          const char* prefix = "";
          switch (line.level) {
            case ConsoleLogLevel::UserInput:
              col = col_user;
              prefix = "> ";
              break;
            case ConsoleLogLevel::Info:    col = col_info; break;
            case ConsoleLogLevel::Warning: col = col_warn; break;
            case ConsoleLogLevel::Error:   col = col_err; break;
          }
          std::string full = std::string(prefix) + line.text;
          const bool has_trace = !line.stack_trace.empty();

          if (has_trace) {
            // Inline drawer: arrow + message + "+N" badge row; clicking
            // toggles the trace drawer below.
            const int trace_lines =
                1 + static_cast<int>(std::count(line.stack_trace.begin(),
                                                line.stack_trace.end(), '\n'));
            ImGui::PushID(static_cast<int>(i));
            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID state_id = ImGui::GetID("open");
            bool is_open = storage->GetBool(state_id, true);

            ImGuiContext& g = *GImGui;
            const float row_h = ImGui::GetFontSize() + g.Style.FramePadding.y;
            const ImVec2 row_pos = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            if (ImGui::InvisibleButton("##row", ImVec2(row_w, row_h))) {
              is_open = !is_open;
              storage->SetBool(state_id, is_open);
            }
            const bool row_hovered = ImGui::IsItemHovered();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 row_end(row_pos.x + row_w, row_pos.y + row_h);

            ui::row::DrawRowHighlight(dl, row_pos, row_end,
                                      /*selected=*/false, row_hovered);

            const float cy = row_pos.y + row_h * 0.5f;
            const float left_pad = g.Style.FramePadding.x;

            const float arrow_sz = g.FontSize;
            const ImVec2 arrow_pos(row_pos.x,
                                   cy - arrow_sz * 0.5f);
            const ImU32 arrow_col = ImGui::GetColorU32(
                (row_hovered || is_open) ? ImGuiCol_Text
                                         : ImGuiCol_TextDisabled);
            ImGui::RenderArrow(dl, arrow_pos, arrow_col,
                               is_open ? ImGuiDir_Down : ImGuiDir_Right,
                               0.85f);

            const float msg_x = row_pos.x + arrow_sz + left_pad * 0.5f;

            // "+N" badge on the right.
            std::string badge = "+" + std::to_string(trace_lines);
            const ImVec2 badge_text_sz =
                ImGui::CalcTextSize(badge.c_str());
            const float badge_w = badge_text_sz.x + style::kBadgePadX * 2.0f;
            const ImVec2 badge_max(row_end.x - left_pad,
                                   cy + g.FontSize * 0.5f);
            const ImVec2 badge_min(badge_max.x - badge_w,
                                   cy - g.FontSize * 0.5f);

            dl->PushClipRect(ImVec2(msg_x, row_pos.y),
                             ImVec2(badge_min.x - left_pad,
                                    row_pos.y + row_h),
                             true);
            dl->AddText(ImVec2(msg_x, cy - g.FontSize * 0.5f), col,
                        full.c_str());
            dl->PopClipRect();

            dl->AddRectFilled(badge_min, badge_max, style::kBadgeBg,
                              style::kBadgeRounding);
            dl->AddRect(badge_min, badge_max,
                        ImGui::GetColorU32(ImGuiCol_Border),
                        style::kBadgeRounding, 0, 1.0f);
            dl->AddText(ImVec2(badge_min.x + style::kBadgePadX,
                               cy - badge_text_sz.y * 0.5f),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled),
                        badge.c_str());

            if (is_open) {
              // Paint the drawer bg using the *actual* rendered bounds after
              // rendering the text, because InputTextMultiline's internal
              // padding throws off a pre-computed height.
              const float max_w = ImGui::GetContentRegionAvail().x;
              const std::string trace = word_wrap
                                             ? wrap_text(line.stack_trace,
                                                         max_w)
                                             : line.stack_trace;
              const float pad_y = g.Style.FramePadding.y;

              ImDrawList* dl2 = ImGui::GetWindowDrawList();
              ImDrawListSplitter splitter;
              splitter.Split(dl2, 2);
              splitter.SetCurrentChannel(dl2, 1);

              ImGui::PushStyleVar(
                  ImGuiStyleVar_ItemSpacing,
                  ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
              ImGui::Dummy(ImVec2(0.0f, pad_y));
              ImGui::Indent(left_pad);
              render_selectable_colored(trace,
                                         ImGui::GetColorU32(ImGuiCol_Text),
                                         "##trace");
              ImGui::Unindent(left_pad);
              ImGui::Dummy(ImVec2(0.0f, pad_y));
              ImGui::PopStyleVar();

              const float drawer_bottom =
                  ImGui::GetCursorScreenPos().y;
              splitter.SetCurrentChannel(dl2, 0);
              dl2->AddRectFilled(ImVec2(row_pos.x, row_end.y),
                                 ImVec2(row_end.x, drawer_bottom),
                                 style::kDrawerBg);
              splitter.Merge(dl2);
            }

            ImGui::PopID();
          } else {
            ImGui::PushID(static_cast<int>(i));
            render_selectable_colored(full, col, "##ln");
            ImGui::PopID();
          }
        }
        if (auto_scroll && ImGui::GetScrollY() >=
                               ImGui::GetScrollMaxY() - 1.0f) {
          ImGui::SetScrollHereY(1.0f);
        }
        ImGui::PopStyleVar();
        if (word_wrap) {
          ImGui::PopTextWrapPos();
        }
        if (code_editor_font_) {
          ImGui::PopFont();
        }
        ImGui::EndChild();

        // Suggestions are computed before InputText so the Tab/Up/Down
        // callback can read them via the static globals. Empty input
        // shows no popup.
        struct Suggestion {
          std::string label;  // what Tab would insert
          std::string desc;   // hint text rendered to the right
          bool is_arg_hint = false;  // true for schema hint rows
        };
        std::vector<Suggestion> suggestions;
        const bool input_nonempty = input_buf[0] != '\0';
        if (input_nonempty) {
          std::string current(input_buf);
          auto tokens = CommandParser::Tokenize(current);
          const bool trailing_space =
              !current.empty() && current.back() == ' ';
          const size_t cursor_tok_index =
              trailing_space ? tokens.size()
                             : (tokens.empty() ? 0 : tokens.size() - 1);

          // Byte range of the current (last) token, for Tab replacement.
          if (trailing_space || tokens.empty()) {
            token_start_byte = static_cast<int>(current.size());
            token_end_byte = token_start_byte;
          } else {
            token_end_byte = static_cast<int>(current.size());
            int i = token_end_byte - 1;
            while (i > 0 && current[i - 1] != ' ') {
              i--;
            }
            token_start_byte = i;
          }

          if (tokens.empty() || cursor_tok_index == 0) {
            const std::string prefix =
                tokens.empty() ? std::string() : tokens.front();
            for (const auto& [name, entry] : cmd.GetCommands()) {
              if (name.compare(0, prefix.size(), prefix) == 0) {
                suggestions.push_back({name, entry.description, false});
              }
            }
          } else {
            const std::string cmd_name = tokens[0];
            const CommandEntry* entry = cmd.Find(cmd_name);
            if (entry) {
              const int slot = CommandParser::SchemaSlotAtTokenIndex(
                  entry->params, 1, cursor_tok_index);
              std::string hint;
              for (size_t pi = 0; pi < entry->params.size(); pi++) {
                if (pi > 0) hint += "  ";
                const auto& p = entry->params[pi];
                const bool active = (static_cast<int>(pi) == slot);
                const char* open = p.optional ? "[" : "<";
                const char* close = p.optional ? "]" : ">";
                const char* type_name = "?";
                switch (p.type) {
                  case ParamType::Int:    type_name = "int"; break;
                  case ParamType::Float:  type_name = "float"; break;
                  case ParamType::Bool:   type_name = "bool"; break;
                  case ParamType::String: type_name = "string"; break;
                  case ParamType::Vec2:   type_name = "vec2"; break;
                  case ParamType::Vec3:   type_name = "vec3"; break;
                  case ParamType::Vec4:   type_name = "vec4"; break;
                }
                if (active) hint += "\x02";
                hint += open;
                hint += p.name;
                hint += ":";
                hint += type_name;
                hint += close;
                if (active) hint += "\x03";
              }
              suggestions.push_back({cmd_name, hint, true});
            }
          }
        }

        suggestion_labels.clear();
        suggestion_labels.reserve(suggestions.size());
        for (const auto& s : suggestions) {
          suggestion_labels.push_back(s.label);
        }
        if (suggestion_sel < 0 ||
            suggestion_sel >= static_cast<int>(suggestions.size())) {
          suggestion_sel = 0;
        }

        // Input line with history + completion. Tab completes the
        // current token; Up/Down navigates suggestions when any are
        // available, falling back to command history otherwise.
        ImGuiInputTextFlags input_flags =
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackHistory |
            ImGuiInputTextFlags_CallbackCompletion;
        ImGui::SetNextItemWidth(-1);
        const ImVec2 input_pos = ImGui::GetCursorScreenPos();
        const float input_w = ImGui::GetContentRegionAvail().x;
        bool submitted = ImGui::InputText(
            "##ConsoleInput", input_buf, sizeof(input_buf), input_flags,
            ConsoleInputCallback);
        const bool input_focused = ImGui::IsItemFocused();

        // Esc (while the input has focus) dismisses the suggestions
        // until the buffer changes again.
        if (input_focused &&
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
          suggestions_dismissed_for = input_buf;
        }
        if (suggestions_dismissed_for != input_buf) {
          suggestions_dismissed_for.clear();
        }
        const bool show_popup = input_focused && input_nonempty &&
                                !suggestions.empty() &&
                                suggestions_dismissed_for != input_buf;

        if (show_popup) {
          const ImGuiStyle& s = ImGui::GetStyle();
          const float row_pad_y = s.FramePadding.y;
          const float row_pad_x = s.FramePadding.x;
          const float row_h = ImGui::GetFontSize() + row_pad_y * 2.0f;
          const int kMaxRows = 8;
          const int rows =
              std::min(kMaxRows, static_cast<int>(suggestions.size()));
          const float panel_h = row_h * rows;
          const float gap = 4.0f;
          ImVec2 panel_min(input_pos.x, input_pos.y - panel_h - gap);
          ImVec2 panel_max(input_pos.x + input_w, input_pos.y - gap);
          const float R = s.PopupRounding;

          // Foreground drawlist so the popup paints over the log multiline.
          ImDrawList* dl = ImGui::GetForegroundDrawList();
          dl->AddRectFilled(panel_min, panel_max,
                            ImGui::GetColorU32(ImGuiCol_PopupBg), R);
          dl->AddRect(panel_min, panel_max,
                      ImGui::GetColorU32(ImGuiCol_Border), R, 0, 1.0f);

          const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
          const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
          const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
          const ImU32 sel_col = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
          for (int i = 0; i < rows; i++) {
            const auto& sg = suggestions[i];
            const float ry = panel_min.y + i * row_h;
            if (i == suggestion_sel) {
              // Round the corners of the selection on the edge rows so
              // the highlight is clipped by the popup's rounded corners
              // instead of overflowing them.
              ImDrawFlags flags = ImDrawFlags_RoundCornersNone;
              if (i == 0) flags |= ImDrawFlags_RoundCornersTop;
              if (i == rows - 1) flags |= ImDrawFlags_RoundCornersBottom;
              dl->AddRectFilled(
                  ImVec2(panel_min.x, ry),
                  ImVec2(panel_max.x, ry + row_h), sel_col,
                  flags == ImDrawFlags_RoundCornersNone ? 0.0f : R,
                  flags);
            }
            const float ty = ry + row_pad_y;
            dl->AddText(ImVec2(panel_min.x + row_pad_x, ty), text_col,
                        sg.label.c_str());
            if (!sg.desc.empty()) {
              float x = panel_min.x + row_pad_x +
                        ImGui::CalcTextSize(sg.label.c_str()).x +
                        row_pad_x * 2.0f;
              bool in_active = false;
              std::string buf;
              auto flush = [&]() {
                if (buf.empty()) return;
                dl->AddText(ImVec2(x, ty), in_active ? accent : muted,
                            buf.c_str());
                x += ImGui::CalcTextSize(buf.c_str()).x;
                buf.clear();
              };
              for (char c : sg.desc) {
                if (c == '\x02') {
                  flush();
                  in_active = true;
                } else if (c == '\x03') {
                  flush();
                  in_active = false;
                } else {
                  buf += c;
                }
              }
              flush();
            }
          }
        } else {
          // No popup -> no stale Tab target.
          suggestion_labels.clear();
        }

        if (submitted) {
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
  if (ImGui::Begin(ICON_LC_HISTORY " Undo History", &panel_undo_history_)) {
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

void EditorLayer::RenderFontDebugPanel() {
  if (!panel_font_debug_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Font Debug", &panel_font_debug_)) {
    ImGui::End();
    return;
  }

  ImGuiContext& g = *ImGui::GetCurrentContext();
  ImFont* font = g.Font;
  ImFontBaked* baked = g.FontBaked;

  // Metrics
  ImGui::SeparatorText("Metrics");
  ImGui::Text("FontSize (layout): %.1f", g.FontSize);
  if (baked) {
    ImGui::Text("Ascent:            %.1f", baked->Ascent);
    ImGui::Text("Descent:           %.1f", baked->Descent);
    ImGui::Text("Ascent - Descent:  %.1f", baked->Ascent - baked->Descent);
    ImGui::Text("Difference:        %.1f (Ascent-Descent - FontSize)",
                (baked->Ascent - baked->Descent) - g.FontSize);
  }
  ImGui::Text("TextLineHeight:    %.1f", ImGui::GetTextLineHeight());
  ImGui::Text("FrameHeight:       %.1f", ImGui::GetFrameHeight());
  ImGui::Text("FramePadding:      %.1f, %.1f", g.Style.FramePadding.x,
              g.Style.FramePadding.y);

  // Visual baseline test - draw text with rulers
  ImGui::SeparatorText("Baseline Visualization");
  {
    const char* test_str = "SsQqAaBbGgJjYy|";
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float line_h = g.FontSize;

    // Background
    dl->AddRectFilled(pos, ImVec2(pos.x + 400, pos.y + line_h + 20),
                      IM_COL32(40, 40, 40, 255));

    // Ascent line (red) - where ImGui thinks the top of text is
    float ascent_y = pos.y + 10;
    dl->AddLine(ImVec2(pos.x, ascent_y), ImVec2(pos.x + 400, ascent_y),
                IM_COL32(255, 80, 80, 200));
    dl->AddText(ImVec2(pos.x + 402, ascent_y - 6), IM_COL32(255, 80, 80, 200),
                "Ascent");

    // Baseline (green)
    float baseline_y = ascent_y + (baked ? baked->Ascent : g.FontSize);
    dl->AddLine(ImVec2(pos.x, baseline_y), ImVec2(pos.x + 400, baseline_y),
                IM_COL32(80, 255, 80, 200));
    dl->AddText(ImVec2(pos.x + 402, baseline_y - 6), IM_COL32(80, 255, 80, 200),
                "Baseline");

    // Descent line (blue) - where ImGui thinks the bottom of text is
    float descent_y = ascent_y + line_h;
    dl->AddLine(ImVec2(pos.x, descent_y), ImVec2(pos.x + 400, descent_y),
                IM_COL32(80, 80, 255, 200));
    dl->AddText(ImVec2(pos.x + 402, descent_y - 6), IM_COL32(80, 80, 255, 200),
                "FontSize");

    // Draw the test text
    dl->AddText(g.Font, g.FontSize, ImVec2(pos.x + 10, ascent_y),
                IM_COL32(255, 255, 255, 255), test_str);

    ImGui::Dummy(ImVec2(500, line_h + 30));
  }

  // Widget alignment tests
  ImGui::SeparatorText("Widget Alignment Tests");

  // Each row: draw a colored rect behind the frame height to show alignment
  auto draw_frame_bg = [&]() {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float frame_h = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + frame_h),
        IM_COL32(60, 60, 80, 100));
  };

  draw_frame_bg();
  ImGui::Button("Button Test Sg");
  ImGui::SameLine();
  ImGui::Text("SameLine Text");

  draw_frame_bg();
  static bool checkbox_val = true;
  ImGui::Checkbox("Checkbox Test Sg", &checkbox_val);

  draw_frame_bg();
  static float slider_val = 0.5f;
  ImGui::SliderFloat("Slider Test", &slider_val, 0.0f, 1.0f);

  draw_frame_bg();
  static char input_buf[64] = "Input Test Sg";
  ImGui::InputText("InputText", input_buf, sizeof(input_buf));

  draw_frame_bg();
  static int combo_val = 0;
  const char* combo_items[] = {"Option A", "Option B"};
  ImGui::Combo("Combo Test", &combo_val, combo_items, 2);

  // PrefixLabel test
  ImGui::SeparatorText("PrefixLabel Tests");
  draw_frame_bg();
  ImGui::Checkbox(PrefixLabel("PL Checkbox").c_str(), &checkbox_val);

  draw_frame_bg();
  ImGui::SliderFloat(PrefixLabel("PL Slider").c_str(), &slider_val, 0.0f, 1.0f);

  draw_frame_bg();
  static float drag3[3] = {1.0f, 2.0f, 3.0f};
  ImGui::DragFloat3(PrefixLabel("PL DragFloat3").c_str(), drag3, 0.1f);

  draw_frame_bg();
  ImGui::InputText(PrefixLabel("PL Input").c_str(), input_buf,
                   sizeof(input_buf));

  // Tree node test
  ImGui::SeparatorText("Tree Nodes");
  if (ImGui::TreeNode("TreeNode Test")) {
    ImGui::Text("Content inside tree");
    draw_frame_bg();
    ImGui::Checkbox(PrefixLabel("Nested Check").c_str(), &checkbox_val);
    ImGui::TreePop();
  }

  ImGui::End();
}

void EditorLayer::RenderNetworkPanel() {
  if (!panel_network_) {
    return;
  }

  if (ImGui::Begin(ICON_LC_GLOBE " Network", &panel_network_)) {
    auto& network = Engine::network();
    NetworkRole role = network.role();

    // Status
    const char* role_str = "Disconnected";
    ImVec4 status_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    if (role == NetworkRole::kServer) {
      role_str = "Server";
      status_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    } else if (role == NetworkRole::kClient) {
      role_str = "Client";
      status_color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
    } else if (role == NetworkRole::kListenServer) {
      role_str = "Listen Server";
      status_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
    }

    ImGui::TextColored(status_color, "Status: %s", role_str);
    ImGui::Separator();

    // Server controls
    ImGui::SeparatorText("Server");

    static char server_ip[64] = "0.0.0.0";
    static int server_port = 25000;

    bool is_hosting = network.is_server();

    if (is_hosting) {
      ImGui::BeginDisabled();
    }
    ImGui::InputText(PrefixLabel("Bind IP").c_str(), server_ip,
                     sizeof(server_ip));
    ImGui::InputInt(PrefixLabel("Port").c_str(), &server_port);
    if (is_hosting) {
      ImGui::EndDisabled();
    }

    if (!is_hosting) {
      if (ImGui::Button("Host Server")) {
        NetworkServerConfig config;
        config.bind_ip = server_ip;
        config.port = static_cast<uint16_t>(server_port);
        network.StartServer(config);
      }
    } else {
      if (ImGui::Button("Stop Server")) {
        network.StopServer();
      }
    }

    // Client controls
    ImGui::SeparatorText("Client");

    static char connect_ip[64] = "127.0.0.1";
    static int connect_port = 25000;

    bool is_connected = network.is_client();

    if (is_connected) {
      ImGui::BeginDisabled();
    }
    ImGui::InputText(PrefixLabel("Server IP").c_str(), connect_ip,
                     sizeof(connect_ip));
    ImGui::InputInt(PrefixLabel("Server Port").c_str(), &connect_port);
    if (is_connected) {
      ImGui::EndDisabled();
    }

    if (!is_connected) {
      if (ImGui::Button("Connect")) {
        NetworkClientConfig config;
        config.server_ip = connect_ip;
        config.port = static_cast<uint16_t>(connect_port);
        network.ConnectToServer(config);
      }
    } else {
      if (ImGui::Button("Disconnect")) {
        network.Disconnect();
      }
    }

    // Info
    if (role != NetworkRole::kNone) {
      ImGui::SeparatorText("Info");

      if (network.is_server()) {
        int client_count = 0;
        network.ForEachSession(
            [&client_count](uint64_t, std::shared_ptr<znet::PeerSession>) {
              client_count++;
            });
        ImGui::Text("Connected Clients: %d", client_count);
      }

      if (network.is_client() && network.is_connected()) {
        ImGui::Text("Connected to server");
      } else if (network.is_client()) {
        ImGui::Text("Connecting...");
      }

      // Count replicated entities
      auto active_scene = Engine::scene_manager().GetActiveScene();
      if (active_scene) {
        auto& registry = active_scene->GetRegistry();
        int net_entity_count = 0;
        auto view = registry.view<NetworkIdentityComponent>();
        for (auto entity : view) {
          net_entity_count++;
        }
        ImGui::Text("Replicated Entities: %d", net_entity_count);
      }
    }
  }
  ImGui::End();
}

}  // namespace wiesel::editor
