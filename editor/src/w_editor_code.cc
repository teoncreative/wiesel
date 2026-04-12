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

#include <fstream>

#include <imgui.h>

#include "mono_compiler.h"
#include "util/w_command.h"
#include "util/w_thread_pool.h"
#include "w_csharp_lang.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_rml_lang.h"

namespace Wiesel::Editor {

void EditorLayer::StartLsp() {
  if (lsp_initialized_ || !active_project_) {
    return;
  }

  std::filesystem::path project_dir = active_project_->GetProjectDirectory();
  std::filesystem::path assets_dir = active_project_->GetAssetsDirectory();

  // Generate a .csproj for the LSP server to discover the project
  DotNetProject lsp_project("App");
  lsp_project.SetOutputPath((project_dir / "App.dll").string());

  // Collect all .cs files from the project assets directory
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(assets_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cs") {
      lsp_project.AddSource(entry.path().string());
    }
  }

  // Reference Core.dll for engine types (MonoBehavior, Entity, components, etc.)
  std::filesystem::path core_dll = std::filesystem::absolute("Core.dll");
  if (std::filesystem::exists(core_dll)) {
    lsp_project.AddReference(core_dll.string());
  } else {
    DCON_LOG_WARN("LSP: Core.dll not found, engine types won't be available");
  }

  lsp_project.Save();
  LOG_INFO("LSP: generated .csproj at {}",
           lsp_project.GetCsprojPath().string());

  // Generate a .sln in the project root so csharp-ls can discover the project.
  // Restore NuGet packages for full semantic analysis
  std::string restore_cmd =
      "dotnet restore \"" + lsp_project.GetCsprojPath().string() + "\"";
  Engine::thread_pool().Submit(
      [restore_cmd]() { std::system(restore_cmd.c_str()); });

  std::string lsp_command =
      editor_config_->Get<std::string>("lsp.csharp.command", "");
  if (lsp_command.empty()) {
    notifications_.PushWarning(
        "No C# LSP server configured. Set one in Edit > Editor Settings.");
    return;
  }

  if (!lsp_client_.Start(lsp_command, project_dir)) {
    notifications_.PushError("Failed to start LSP: " + lsp_command);
    return;
  }
  LOG_INFO("LSP: started with command: {}", lsp_command);
  lsp_client_.Initialize(project_dir);
  lsp_initialized_ = true;
  lsp_autocomplete_ = std::make_unique<LspAutocompleteProvider>(lsp_client_);
  text_editor_.SetAutocompleteProvider(lsp_autocomplete_.get());
  LOG_INFO("LSP: csharp-ls started");
}

void EditorLayer::StopLsp() {
  if (!lsp_initialized_) {
    return;
  }
  lsp_client_.Stop();
  lsp_initialized_ = false;
}

void EditorLayer::OpenCodeEditor(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    DCON_LOG_ERROR("Failed to open file: {}", path.string());
    return;
  }
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  // Close previous file in LSP
  if (!code_editor_uri_.empty() && lsp_initialized_) {
    lsp_client_.DidClose(code_editor_uri_);
  }

  code_editor_path_ = path;
  code_editor_uri_ = LspClient::PathToUri(path);
  text_editor_.SetText(content);
  text_editor_.SetFilePath(path.string());

  // Pick language definition based on file extension
  auto ext = path.extension().string();
  if (ext == ".rml") {
    static auto rml_lang = CreateRmlLanguageDefinition();
    text_editor_.SetLanguageDefinition(rml_lang);
  } else if (ext == ".rcss") {
    static auto rcss_lang = CreateRcssLanguageDefinition();
    text_editor_.SetLanguageDefinition(rcss_lang);
  } else {
    static auto csharp_lang = CreateCSharpLanguageDefinition();
    text_editor_.SetLanguageDefinition(csharp_lang);
  }

  text_editor_.SetShowWhitespaces(false);
  code_editor_unsaved_ = false;
  code_editor_open_ = true;

  // Start LSP only for C# files
  if (ext == ".cs") {
    StartLsp();
    semantic_tokens_received_ = false;
    if (lsp_initialized_) {
      lsp_client_.DidOpen(code_editor_uri_, content);
    }
  }
}

void EditorLayer::SaveCodeEditorFile() {
  if (code_editor_path_.empty()) {
    return;
  }
  std::ofstream file(code_editor_path_);
  if (!file.is_open()) {
    DCON_LOG_ERROR("Failed to save file: {}", code_editor_path_.string());
    return;
  }
  file << text_editor_.GetText();
  code_editor_unsaved_ = false;

  // Re-request semantic tokens after save
  if (lsp_initialized_) {
    lsp_client_.RequestSemanticTokens(code_editor_uri_);
  }
}

void EditorLayer::RenderCodeEditor() {
  if (!code_editor_open_) {
    return;
  }

  std::string title = code_editor_path_.filename().string();
  if (code_editor_unsaved_) {
    title += " *";
  }
  title += "###CodeEditor";

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(title.c_str(), &code_editor_open_,
                    ImGuiWindowFlags_MenuBar)) {
    ImGui::End();
    return;
  }

  // Menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save", "Ctrl+S", false, code_editor_unsaved_)) {
        SaveCodeEditorFile();
      }
      ImGui::EndMenu();
    }

    // Right-aligned buttons
    float avail = ImGui::GetContentRegionAvail().x;
    float button_width = ImGui::CalcTextSize("Open in VS Code").x + 20;
    ImGui::SameLine(avail - button_width + ImGui::GetCursorPosX());
    if (ImGui::SmallButton("Open in VS Code")) {
      std::string cmd = "code \"" + code_editor_path_.string() + "\"";
      std::system(cmd.c_str());
    }

    ImGui::EndMenuBar();
  }

  // Status bar
  auto cpos = text_editor_.GetCursorPosition();
  ImGui::Text("Ln %d, Col %d | %s", cpos.mLine + 1, cpos.mColumn + 1,
              code_editor_path_.filename().string().c_str());

  // Request semantic tokens if we haven't received them yet (throttled)
  if (lsp_initialized_ && !semantic_tokens_received_) {
    static float retry_timer = 0.0f;
    retry_timer += ImGui::GetIO().DeltaTime;
    if (retry_timer >= 1.0f) {
      retry_timer = 0.0f;
      lsp_client_.RequestSemanticTokens(code_editor_uri_);
    }
  }

  // Apply LSP semantic tokens as identifier highlights
  if (lsp_initialized_ && lsp_client_.HasSemanticTokens()) {
    semantic_tokens_received_ = true;
    std::vector<LspSemanticToken> tokens = lsp_client_.TakeSemanticTokens();
    const std::vector<std::string>& legend = lsp_client_.GetTokenTypeLegend();
    std::vector<std::string> lines = text_editor_.GetTextLines();

    int added_count = 0;
    for (const LspSemanticToken& token : tokens) {
      if (token.line >= static_cast<int>(lines.size())) {
        continue;
      }
      const std::string& line = lines[token.line];
      if (token.column + token.length > static_cast<int>(line.size())) {
        continue;
      }
      std::string token_text = line.substr(token.column, token.length);

      std::string type_name;
      if (token.token_type < static_cast<int>(legend.size())) {
        type_name = legend[token.token_type];
      }

      if (type_name == "class" || type_name == "struct" ||
          type_name == "interface" || type_name == "enum" ||
          type_name == "type" || type_name == "namespace") {
        text_editor_.AddIdentifier(token_text);
        added_count++;
      }
    }
    if (added_count > 0) {
      text_editor_.InvalidateColorize();
    }
  }

  // Apply LSP diagnostics as error markers
  if (lsp_initialized_ && lsp_client_.HasDiagnostics(code_editor_uri_)) {
    std::vector<LspDiagnostic> diags =
        lsp_client_.TakeDiagnostics(code_editor_uri_);
    TextEditor::ErrorMarkers markers;
    for (const LspDiagnostic& d : diags) {
      markers[d.line + 1] = d.message;  // TextEditor uses 1-based lines
    }
    text_editor_.SetErrorMarkers(markers);

    // Request semantic tokens now that the server has analyzed the file
    if (!lsp_client_.HasSemanticTokens()) {
      lsp_client_.RequestSemanticTokens(code_editor_uri_);
    }
  }

  // Editor (monospace font)
  if (code_editor_font_) {
    ImGui::PushFont(code_editor_font_);
  }

  bool was_modified = text_editor_.IsTextChanged();

  text_editor_.Render("##editor");

  if (text_editor_.IsTextChanged() && !was_modified) {
    code_editor_unsaved_ = true;
  }

  // Notify LSP of content changes so hover/signature/diagnostics stay current
  if (text_editor_.IsTextChanged() && lsp_initialized_) {
    lsp_client_.DidChange(code_editor_uri_, text_editor_.GetText());
  }

  // Hover tooltip - request on cursor idle, show when result arrives
  if (lsp_initialized_) {
    // Hover: track mouse position over the editor, request after 0.5s idle
    ImVec2 mouse_pos = ImGui::GetMousePos();
    auto mouse_coord = text_editor_.GetCoordinatesAt(mouse_pos);
    if (mouse_coord.mLine != hover_line_ || mouse_coord.mColumn != hover_col_) {
      hover_line_ = mouse_coord.mLine;
      hover_col_ = mouse_coord.mColumn;
      hover_timer_ = 0.0f;
      hover_requested_ = false;
      hover_text_.clear();
    }

    bool editor_hovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    if (editor_hovered) {
      hover_timer_ += ImGui::GetIO().DeltaTime;
      if (!hover_requested_ && hover_timer_ > 0.5f) {
        lsp_client_.RequestHover(code_editor_uri_, hover_line_, hover_col_);
        hover_requested_ = true;
      }
    }

    if (lsp_client_.HasHover()) {
      auto result = lsp_client_.TakeHover();
      hover_text_ = result.contents;
      auto is_ws = [](char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
      };
      while (!hover_text_.empty() && is_ws(hover_text_.front())) {
        hover_text_.erase(0, 1);
      }
      while (!hover_text_.empty() && is_ws(hover_text_.back())) {
        hover_text_.pop_back();
      }
    }

    if (!hover_text_.empty() && hover_requested_ && editor_hovered) {
      ImGui::BeginTooltip();
      text_editor_.RenderMarkdown(hover_text_, code_editor_font_);
      ImGui::EndTooltip();
    }

    // Signature help - request when typing inside parens
    if (lsp_client_.HasSignatureHelp()) {
      signature_help_ = lsp_client_.TakeSignatureHelp();
      signature_active_ = !signature_help_.signatures.empty();
    }

    // Signature help: fire pending request (deferred one frame so didChange
    // is processed by the server first)
    if (signature_pending_) {
      lsp_client_.RequestSignatureHelp(code_editor_uri_, signature_line_,
                                       signature_col_,
                                       std::string(1, signature_trigger_));
      signature_pending_ = false;
    }

    // Check if we should request signature help
    if (text_editor_.IsTextChanged()) {
      auto cursor = text_editor_.GetCursorPosition();
      std::vector<std::string> lines = text_editor_.GetTextLines();
      if (cursor.mLine < static_cast<int>(lines.size())) {
        const std::string& line = lines[cursor.mLine];
        int idx = cursor.mColumn - 1;
        if (idx >= 0 && idx < static_cast<int>(line.size())) {
          char ch = line[idx];
          const std::string& triggers = lsp_client_.GetSignatureTriggerChars();
          if (triggers.find(ch) != std::string::npos) {
            // Defer to next frame so didChange arrives first
            signature_pending_ = true;
            signature_trigger_ = ch;
            signature_line_ = cursor.mLine;
            signature_col_ = cursor.mColumn;
          } else if (ch == ')' || ch == '>' || ch == '}' || ch == ']') {
            signature_active_ = false;
          }
        }
      }
    }

    // Render signature help popup
    if (signature_active_ && !signature_help_.signatures.empty()) {
      int idx =
          std::clamp(signature_help_.active_signature, 0,
                     static_cast<int>(signature_help_.signatures.size()) - 1);
      const auto& sig = signature_help_.signatures[idx];

      // Position above the cursor line, not at mouse position
      auto cursor = text_editor_.GetCursorPosition();
      ImVec2 origin = text_editor_.GetContentOrigin();
      float line_height = ImGui::GetTextLineHeightWithSpacing();
      ImVec2 sig_pos(
          origin.x, origin.y + cursor.mLine * line_height - line_height - 4.0f);
      ImGui::SetNextWindowPos(sig_pos, ImGuiCond_Always, ImVec2(0, 1));
      ImGui::BeginTooltip();
      if (code_editor_font_) {
        ImGui::PushFont(code_editor_font_);
      }
      // Render signature with active parameter highlighted
      if (!sig.parameters.empty() &&
          signature_help_.active_parameter <
              static_cast<int>(sig.parameters.size())) {
        const auto& active_param =
            sig.parameters[signature_help_.active_parameter];
        size_t param_pos = sig.label.find(active_param.label);
        if (param_pos != std::string::npos) {
          std::string before = sig.label.substr(0, param_pos);
          if (!before.empty()) {
            text_editor_.RenderCodeLine(before.c_str(),
                                        before.c_str() + before.size());
            ImGui::SameLine(0, 0);
          }
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s",
                             active_param.label.c_str());
          ImGui::SameLine(0, 0);
          std::string after =
              sig.label.substr(param_pos + active_param.label.size());
          if (!after.empty()) {
            text_editor_.RenderCodeLine(after.c_str(),
                                        after.c_str() + after.size());
          }
        } else {
          text_editor_.RenderCodeLine(sig.label.c_str(),
                                      sig.label.c_str() + sig.label.size());
        }
        if (!active_param.documentation.empty()) {
          ImGui::TextDisabled("%s", active_param.documentation.c_str());
        }
      } else {
        text_editor_.RenderCodeLine(sig.label.c_str(),
                                    sig.label.c_str() + sig.label.size());
      }
      if (code_editor_font_) {
        ImGui::PopFont();
      }
      if (!sig.documentation.empty()) {
        ImGui::Separator();
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextDisabled("%s", sig.documentation.c_str());
        ImGui::PopTextWrapPos();
      }
      ImGui::EndTooltip();
    }
  }

  if (code_editor_font_) {
    ImGui::PopFont();
  }

  code_editor_focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

  // Ctrl+S to save within the editor window
  if (code_editor_focused_ && ImGui::GetIO().KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    SaveCodeEditorFile();
  }

  ImGui::End();
}

void EditorLayer::RenderLspDebugPanel() {
  if (!panel_lsp_debug_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(CODICON_INFO " LSP Debug", &panel_lsp_debug_)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Status: %s", lsp_initialized_
                                ? (lsp_client_.IsRunning() ? "Running" : "Died")
                                : "Not started");

  ImGui::Separator();

  std::vector<LspClient::LogEntry> log = lsp_client_.GetLog();
  if (ImGui::BeginChild("##lsp_log", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
    for (const LspClient::LogEntry& entry : log) {
      ImVec4 color = entry.outgoing ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f)
                                    : ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextWrapped("%s%s", entry.outgoing ? ">> " : "<< ",
                         entry.summary.c_str());
      ImGui::PopStyleColor();
      ImGui::Separator();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace Wiesel::Editor
