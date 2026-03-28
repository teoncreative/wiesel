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

#include <filesystem>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Wiesel::Editor {

using json = nlohmann::json;

struct LspCompletionItem {
  std::string label;
  std::string detail;
  std::string insert_text;
  int kind = 0;
};

struct LspDiagnostic {
  int line;
  int character;
  int severity;  // 1=error, 2=warning, 3=info, 4=hint
  std::string message;
};

struct LspHoverResult {
  std::string contents;
};

struct LspSemanticToken {
  int line;
  int column;
  int length;
  int token_type;  // index into server's token types legend
  int modifiers;
};

// Standard LSP semantic token types (indices into the legend)
enum class SemanticTokenType {
  Namespace = 0,
  Type,
  Class,
  Enum,
  Interface,
  Struct,
  TypeParameter,
  Parameter,
  Variable,
  Property,
  EnumMember,
  Event,
  Function,
  Method,
  Macro,
  Keyword,
  Modifier,
  Comment,
  String,
  Number,
  Regexp,
  Operator,
  Decorator,
  Count
};

class LspClient {
 public:
  LspClient();
  ~LspClient();

  // Lifecycle
  bool Start(const std::string& command,
             const std::filesystem::path& working_dir);
  void Stop();
  bool IsRunning() const;

  // LSP protocol
  void Initialize(const std::filesystem::path& root_path);
  void DidOpen(const std::string& uri, const std::string& text);
  void DidChange(const std::string& uri, const std::string& text);
  void DidClose(const std::string& uri);
  void RequestCompletion(const std::string& uri, int line, int character);
  void RequestHover(const std::string& uri, int line, int character);
  void RequestSemanticTokens(const std::string& uri);

  // Poll results (call from main thread)
  bool HasCompletions() const;
  std::vector<LspCompletionItem> TakeCompletions();

  bool HasDiagnostics(const std::string& uri) const;
  std::vector<LspDiagnostic> TakeDiagnostics(const std::string& uri);

  bool HasHover() const;
  LspHoverResult TakeHover();

  bool HasSemanticTokens() const;
  std::vector<LspSemanticToken> TakeSemanticTokens();

  const std::vector<std::string>& GetTokenTypeLegend() const {
    return token_type_legend_;
  }

  // Convert filesystem path to file:// URI
  static std::string PathToUri(const std::filesystem::path& path);
  static std::filesystem::path UriToPath(const std::string& uri);

 private:
  void SendRequest(const std::string& method, const json& params);
  void SendNotification(const std::string& method, const json& params);
  void ReaderThread();
  void HandleMessage(const json& msg);
  void HandleResponse(int id, const json& result);
  void HandleNotification(const std::string& method, const json& params);

  // Process I/O
#ifdef _WIN32
  void* process_handle_ = nullptr;
  void* stdin_write_ = nullptr;
  void* stdout_read_ = nullptr;
#else
  int pid_ = -1;
  int stdin_fd_ = -1;
  int stdout_fd_ = -1;
#endif

  std::thread reader_thread_;
  std::atomic<bool> running_{false};
  int next_id_ = 1;

  // Pending request tracking
  std::unordered_map<int, std::string> pending_requests_;  // id -> method

  // Results (written by reader thread, read by main thread)
  mutable std::mutex results_mutex_;
  std::vector<LspCompletionItem> completions_;
  std::unordered_map<std::string, std::vector<LspDiagnostic>> diagnostics_;
  LspHoverResult hover_;
  bool has_completions_ = false;
  bool has_hover_ = false;
  std::vector<LspSemanticToken> semantic_tokens_;
  bool has_semantic_tokens_ = false;
  std::vector<std::string> token_type_legend_;

  // Write mutex (main thread writes requests)
  std::mutex write_mutex_;

  // Debug log (ring buffer of recent messages)
 public:
  struct LogEntry {
    bool outgoing;  // true = sent, false = received
    std::string summary;
  };

  std::vector<LogEntry> GetLog() const;

 private:
  void AddLog(bool outgoing, const std::string& summary);
  mutable std::mutex log_mutex_;
  std::vector<LogEntry> log_;
  static constexpr size_t kMaxLogEntries = 200;
};

}  // namespace Wiesel::Editor