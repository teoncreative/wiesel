//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_lsp_client.h"

#include <sstream>
#include "util/w_logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Wiesel::Editor {

namespace fs = std::filesystem;

LspClient::LspClient() = default;

LspClient::~LspClient() {
  Stop();
}

bool LspClient::Start(const std::string& command, const fs::path& working_dir) {
  if (running_) {
    Stop();
  }

#ifdef _WIN32
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE stdin_read, stdin_write, stdout_read, stdout_write;
  if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0) ||
      !CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
    LOG_ERROR("LspClient: failed to create pipes");
    return false;
  }

  SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

  // Redirect stderr to NUL to prevent blocking
  HANDLE nul_handle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                                  OPEN_EXISTING, 0, nullptr);

  STARTUPINFOA si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = stdin_read;
  si.hStdOutput = stdout_write;
  si.hStdError = nul_handle;

  PROCESS_INFORMATION pi{};
  std::string cmd_line = command;
  std::string wd = working_dir.string();

  if (!CreateProcessA(nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
                      CREATE_NO_WINDOW, nullptr,
                      wd.empty() ? nullptr : wd.c_str(), &si, &pi)) {
    LOG_ERROR("LspClient: failed to start process: {}", command);
    CloseHandle(stdin_read);
    CloseHandle(stdin_write);
    CloseHandle(stdout_read);
    CloseHandle(stdout_write);
    return false;
  }

  CloseHandle(stdin_read);
  CloseHandle(stdout_write);
  CloseHandle(nul_handle);
  CloseHandle(pi.hThread);

  process_handle_ = pi.hProcess;
  stdin_write_ = stdin_write;
  stdout_read_ = stdout_read;
#else
  int stdin_pipe[2], stdout_pipe[2];
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    LOG_ERROR("LspClient: failed to create pipes");
    return false;
  }

  pid_ = fork();
  if (pid_ < 0) {
    LOG_ERROR("LspClient: fork failed");
    return false;
  }

  if (pid_ == 0) {
    // Child
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    if (!working_dir.empty()) {
      chdir(working_dir.string().c_str());
    }
    execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
    _exit(1);
  }

  // Parent
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);
  stdin_fd_ = stdin_pipe[1];
  stdout_fd_ = stdout_pipe[0];
#endif

  running_ = true;
  reader_thread_ = std::thread(&LspClient::ReaderThread, this);

  LOG_INFO("LspClient: started process: {}", command);
  return true;
}

void LspClient::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

#ifdef _WIN32
  if (stdin_write_) {
    CloseHandle(stdin_write_);
    stdin_write_ = nullptr;
  }
  if (stdout_read_) {
    CloseHandle(stdout_read_);
    stdout_read_ = nullptr;
  }
  if (process_handle_) {
    TerminateProcess(process_handle_, 0);
    WaitForSingleObject(process_handle_, 3000);
    CloseHandle(process_handle_);
    process_handle_ = nullptr;
  }
#else
  if (stdin_fd_ >= 0) {
    close(stdin_fd_);
    stdin_fd_ = -1;
  }
  if (stdout_fd_ >= 0) {
    close(stdout_fd_);
    stdout_fd_ = -1;
  }
  if (pid_ > 0) {
    kill(pid_, SIGTERM);
    waitpid(pid_, nullptr, 0);
    pid_ = -1;
  }
#endif

  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }

  LOG_INFO("LspClient: stopped");
}

bool LspClient::IsRunning() const {
  return running_;
}

// --- JSON-RPC transport ---

void LspClient::SendRequest(const std::string& method, const json& params) {
  int id = next_id_++;

  json msg = {
      {"jsonrpc", "2.0"},
      {"id", id},
      {"method", method},
      {"params", params},
  };

  // Human-readable request summary
  std::string summary = "[" + std::to_string(id) + "] -> " + method;
  if (params.contains("textDocument") &&
      params["textDocument"].contains("uri")) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    size_t last_slash = uri.rfind('/');
    summary +=
        " " +
        (last_slash != std::string::npos ? uri.substr(last_slash + 1) : uri);
  }
  if (params.contains("position")) {
    summary += " L" + std::to_string(params["position"]["line"].get<int>()) +
               ":C" +
               std::to_string(params["position"]["character"].get<int>());
  }
  AddLog(true, summary);

  {
    std::lock_guard<std::mutex> lock(results_mutex_);
    pending_requests_[id] = method;
  }

  std::string body = msg.dump();
  std::string header =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  std::string packet = header + body;

  std::lock_guard<std::mutex> lock(write_mutex_);
#ifdef _WIN32
  DWORD written;
  WriteFile(stdin_write_, packet.data(), static_cast<DWORD>(packet.size()),
            &written, nullptr);
#else
  write(stdin_fd_, packet.data(), packet.size());
#endif
}

void LspClient::SendNotification(const std::string& method,
                                 const json& params) {
  json msg = {
      {"jsonrpc", "2.0"},
      {"method", method},
      {"params", params},
  };

  // Human-readable notification summary
  std::string summary = "-> " + method;
  if (params.contains("textDocument") &&
      params["textDocument"].contains("uri")) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    size_t last_slash = uri.rfind('/');
    summary +=
        " " +
        (last_slash != std::string::npos ? uri.substr(last_slash + 1) : uri);
  }
  if (method == "textDocument/didChange") {
    summary += " (full sync)";
  }
  AddLog(true, summary);

  std::string body = msg.dump();
  std::string header =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  std::string packet = header + body;

  std::lock_guard<std::mutex> lock(write_mutex_);
#ifdef _WIN32
  DWORD written;
  WriteFile(stdin_write_, packet.data(), static_cast<DWORD>(packet.size()),
            &written, nullptr);
#else
  write(stdin_fd_, packet.data(), packet.size());
#endif
}

void LspClient::ReaderThread() {
  AddLog(false, "(reader thread started)");
  std::string buffer;
  char chunk[4096];

  while (running_) {
#ifdef _WIN32
    DWORD bytes_read = 0;
    BOOL ok =
        ReadFile(stdout_read_, chunk, sizeof(chunk), &bytes_read, nullptr);
    if (!ok || bytes_read == 0) {
      DWORD err = GetLastError();
      AddLog(false, "(read failed, err=" + std::to_string(err) +
                        ", bytes=" + std::to_string(bytes_read) + ")");
      break;
    }
#else
    ssize_t bytes_read = read(stdout_fd_, chunk, sizeof(chunk));
    if (bytes_read <= 0) {
      AddLog(false, "(read failed)");
      break;
    }
#endif

    //AddLog(false, "(read " + std::to_string(bytes_read) + " bytes)");
    buffer.append(chunk, bytes_read);

    // Parse LSP messages from buffer
    while (true) {
      // Find header end
      auto header_end = buffer.find("\r\n\r\n");
      if (header_end == std::string::npos) {
        break;
      }

      // Parse Content-Length
      int content_length = 0;
      std::string header = buffer.substr(0, header_end);
      auto cl_pos = header.find("Content-Length: ");
      if (cl_pos != std::string::npos) {
        content_length = std::stoi(header.substr(cl_pos + 16));
      }

      size_t body_start = header_end + 4;
      if (buffer.size() < body_start + content_length) {
        break;  // incomplete body
      }

      std::string body = buffer.substr(body_start, content_length);
      buffer.erase(0, body_start + content_length);

      try {
        json msg = json::parse(body);
        HandleMessage(msg);
      } catch (const json::exception& e) {
        LOG_WARN("LspClient: failed to parse message: {}", e.what());
      }
    }
  }

  AddLog(false, "(reader thread exiting)");
  running_ = false;
}

void LspClient::HandleMessage(const json& msg) {
  bool has_id = msg.contains("id");
  bool has_method = msg.contains("method");
  bool has_result = msg.contains("result");
  bool has_error = msg.contains("error");

  if (has_id && has_method) {
    // Server-to-client request: must send a response
    auto id = msg["id"];
    std::string method = msg["method"].get<std::string>();
    AddLog(false, "server request: " + method);

    // Respond with empty result (acknowledge)
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", nullptr},
    };
    std::string body = response.dump();
    std::string header =
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    std::string packet = header + body;

    std::lock_guard<std::mutex> lock(write_mutex_);
#ifdef _WIN32
    DWORD written;
    WriteFile(stdin_write_, packet.data(), static_cast<DWORD>(packet.size()),
              &written, nullptr);
#else
    write(stdin_fd_, packet.data(), packet.size());
#endif
    AddLog(false, "[" + id.dump() + "] <- " + method + " (null)");
  } else if (has_id && has_result) {
    int id = msg["id"].get<int>();
    std::string method;
    {
      std::lock_guard<std::mutex> lock(results_mutex_);
      auto it = pending_requests_.find(id);
      if (it != pending_requests_.end()) {
        method = it->second;
      }
    }
    const json& result = msg["result"];

    // Null result - server has no data for this request
    if (result.is_null()) {
      AddLog(false, "[" + std::to_string(id) + "] <- " + method + " (null)");
      std::lock_guard<std::mutex> lock(results_mutex_);
      pending_requests_.erase(id);
      // Still notify so polling doesn't hang
      if (method == "textDocument/hover") {
        hover_ = {};
        has_hover_ = true;
      } else if (method == "textDocument/signatureHelp") {
        signature_help_ = {};
        has_signature_help_ = true;
      }
      return;
    }

    // Human-readable response summary
    std::string summary = "[" + std::to_string(id) + "] <- " + method;
    if (method == "textDocument/completion") {
      int count = result.is_array() ? static_cast<int>(result.size()) : 0;
      if (result.is_object() && result.contains("items")) {
        count = static_cast<int>(result["items"].size());
      }
      summary += " (" + std::to_string(count) + " items)";
    } else if (method == "textDocument/hover") {
      if (result.is_null()) {
        summary += " (null)";
      } else if (result.contains("contents")) {
        auto& c = result["contents"];
        std::string preview = c.is_string() ? c.get<std::string>()
                              : c.is_object() && c.contains("value")
                                  ? c["value"].get<std::string>()
                                  : "...";
        if (preview.size() > 80) {
          preview = preview.substr(0, 80) + "...";
        }
        summary += " \"" + preview + "\"";
      }
    } else if (method == "textDocument/signatureHelp") {
      if (result.is_null()) {
        summary += " (null)";
      } else if (result.contains("signatures")) {
        int count = static_cast<int>(result["signatures"].size());
        summary += " (" + std::to_string(count) + " signatures)";
        if (count > 0) {
          summary +=
              " active=" + std::to_string(result.value("activeParameter", 0));
        }
      }
    } else if (method == "textDocument/semanticTokens/full") {
      if (result.contains("data")) {
        summary +=
            " (" + std::to_string(result["data"].size() / 5) + " tokens)";
      }
    } else if (method == "initialize") {
      summary += " (server ready)";
    } else {
      summary += " (ok)";
    }
    AddLog(false, summary);
    HandleResponse(id, result);
  } else if (has_id && has_error) {
    int id = msg["id"].get<int>();
    std::string err_msg = msg["error"].value("message", "unknown error");
    AddLog(false, "[" + std::to_string(id) + "] ERROR: " + err_msg);
  } else if (has_method) {
    std::string method = msg["method"].get<std::string>();
    std::string summary = "<- " + method;
    const json& params = msg.value("params", json::object());
    if (method == "textDocument/publishDiagnostics") {
      int count = params.contains("diagnostics")
                      ? static_cast<int>(params["diagnostics"].size())
                      : 0;
      std::string uri = params.value("uri", "");
      size_t last_slash = uri.rfind('/');
      summary +=
          " " +
          (last_slash != std::string::npos ? uri.substr(last_slash + 1) : uri) +
          " (" + std::to_string(count) + " diagnostics)";
    }
    AddLog(false, summary);
    HandleNotification(method, msg.value("params", json::object()));
  }
}

void LspClient::HandleResponse(int id, const json& result) {
  std::string method;
  {
    std::lock_guard<std::mutex> lock(results_mutex_);
    auto it = pending_requests_.find(id);
    if (it == pending_requests_.end()) {
      return;
    }
    method = it->second;
    pending_requests_.erase(it);
  }

  if (method == "textDocument/completion") {
    std::vector<LspCompletionItem> items;
    json completion_list = result;
    if (result.contains("items")) {
      completion_list = result["items"];
    }
    if (completion_list.is_array()) {
      for (const auto& item : completion_list) {
        LspCompletionItem ci;
        ci.label = item.value("label", "");
        ci.detail = item.value("detail", "");
        ci.insert_text = item.value("insertText", ci.label);
        ci.kind = item.value("kind", 0);
        items.push_back(ci);
      }
    }
    AddLog(false, "completion: " + std::to_string(items.size()) + " items");
    std::lock_guard<std::mutex> lock(results_mutex_);
    completions_ = std::move(items);
    has_completions_ = true;
  } else if (method == "textDocument/hover") {
    LspHoverResult hover;
    if (result.contains("contents")) {
      auto& contents = result["contents"];
      if (contents.is_string()) {
        hover.contents = contents.get<std::string>();
      } else if (contents.is_object() && contents.contains("value")) {
        hover.contents = contents["value"].get<std::string>();
      }
    }
    std::lock_guard<std::mutex> lock(results_mutex_);
    hover_ = std::move(hover);
    has_hover_ = true;
  } else if (method == "textDocument/signatureHelp") {
    LspSignatureHelp sig_help;
    if (result.contains("signatures") && result["signatures"].is_array()) {
      for (const auto& sig : result["signatures"]) {
        LspSignatureInfo info;
        info.label = sig.value("label", "");
        if (sig.contains("documentation")) {
          auto& doc = sig["documentation"];
          if (doc.is_string()) {
            info.documentation = doc.get<std::string>();
          } else if (doc.is_object() && doc.contains("value")) {
            info.documentation = doc["value"].get<std::string>();
          }
        }
        if (sig.contains("parameters") && sig["parameters"].is_array()) {
          for (const auto& param : sig["parameters"]) {
            LspSignatureParameter p;
            if (param.contains("label")) {
              auto& lbl = param["label"];
              if (lbl.is_string()) {
                p.label = lbl.get<std::string>();
              }
            }
            if (param.contains("documentation")) {
              auto& doc = param["documentation"];
              if (doc.is_string()) {
                p.documentation = doc.get<std::string>();
              } else if (doc.is_object() && doc.contains("value")) {
                p.documentation = doc["value"].get<std::string>();
              }
            }
            info.parameters.push_back(std::move(p));
          }
        }
        sig_help.signatures.push_back(std::move(info));
      }
    }
    sig_help.active_signature = result.value("activeSignature", 0);
    sig_help.active_parameter = result.value("activeParameter", 0);
    std::lock_guard<std::mutex> lock(results_mutex_);
    signature_help_ = std::move(sig_help);
    has_signature_help_ = true;
  } else if (method == "initialize") {
    // Capture semantic token type legend from server capabilities
    if (result.contains("capabilities") &&
        result["capabilities"].contains("semanticTokensProvider")) {
      const json& provider = result["capabilities"]["semanticTokensProvider"];
      if (provider.contains("legend") &&
          provider["legend"].contains("tokenTypes")) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        token_type_legend_.clear();
        for (const json& t : provider["legend"]["tokenTypes"]) {
          token_type_legend_.push_back(t.get<std::string>());
        }
        AddLog(false, "semantic token legend: " +
                          std::to_string(token_type_legend_.size()) + " types");
      } else {
        AddLog(false, "semantic tokens: no legend in provider");
      }
    } else {
      AddLog(false, "semantic tokens: NOT supported by server");
    }

    // Log which capabilities the server supports
    if (result.contains("capabilities")) {
      const json& caps = result["capabilities"];
      std::string supported;
      if (caps.contains("hoverProvider")) {
        supported += " hover";
      }
      if (caps.contains("signatureHelpProvider")) {
        supported += " signatureHelp";
        const json& sh = caps["signatureHelpProvider"];
        if (sh.contains("triggerCharacters")) {
          signature_trigger_chars_.clear();
          supported += "(triggers:";
          for (const auto& t : sh["triggerCharacters"]) {
            std::string c = t.get<std::string>();
            signature_trigger_chars_ += c;
            supported += c;
          }
          supported += ")";
        }
      }
      if (caps.contains("completionProvider")) {
        supported += " completion";
      }
      AddLog(false, "server capabilities:" + supported);
    }
  } else if (method == "textDocument/semanticTokens/full") {
    std::vector<LspSemanticToken> tokens;
    if (result.contains("data") && result["data"].is_array()) {
      const json& data = result["data"];
      // LSP encodes tokens as groups of 5 integers:
      // [deltaLine, deltaStartChar, length, tokenType, tokenModifiers]
      int line = 0;
      int col = 0;
      for (size_t i = 0; i + 4 < data.size(); i += 5) {
        int delta_line = data[i].get<int>();
        int delta_col = data[i + 1].get<int>();
        int length = data[i + 2].get<int>();
        int token_type = data[i + 3].get<int>();
        int modifiers = data[i + 4].get<int>();

        if (delta_line > 0) {
          line += delta_line;
          col = delta_col;
        } else {
          col += delta_col;
        }

        tokens.push_back({line, col, length, token_type, modifiers});
      }
    }
    AddLog(false,
           "semantic tokens: " + std::to_string(tokens.size()) + " tokens");
    std::lock_guard<std::mutex> lock(results_mutex_);
    semantic_tokens_ = std::move(tokens);
    has_semantic_tokens_ = true;
  }
}

void LspClient::HandleNotification(const std::string& method,
                                   const json& params) {
  if (method == "textDocument/publishDiagnostics") {
    std::string uri = params.value("uri", "");
    std::vector<LspDiagnostic> diags;
    if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
      for (const auto& d : params["diagnostics"]) {
        LspDiagnostic diag;
        if (d.contains("range") && d["range"].contains("start")) {
          diag.line = d["range"]["start"].value("line", 0);
          diag.character = d["range"]["start"].value("character", 0);
        }
        diag.severity = d.value("severity", 1);
        diag.message = d.value("message", "");
        diags.push_back(diag);
      }
    }
    std::lock_guard<std::mutex> lock(results_mutex_);
    diagnostics_[uri] = std::move(diags);
  }
}

// --- LSP protocol methods ---

void LspClient::Initialize(const fs::path& root_path) {
  json params = {
      {"processId", nullptr},
      {"rootUri", PathToUri(root_path)},
      {"capabilities",
       {{"textDocument",
         {{"completion", {{"completionItem", {{"snippetSupport", false}}}}},
          {"hover", {{"contentFormat", {"plaintext"}}}},
          {"signatureHelp",
           {{"signatureInformation",
             {{"documentationFormat", {"plaintext"}},
              {"parameterInformation", {{"labelOffsetSupport", false}}}}}}},
          {"publishDiagnostics", {{"relatedInformation", false}}},
          {"semanticTokens",
           {{"requests", {{"full", true}}},
            {"tokenTypes",
             {"namespace", "type",     "class",         "enum",
              "interface", "struct",   "typeParameter", "parameter",
              "variable",  "property", "enumMember",    "event",
              "function",  "method",   "macro",         "keyword",
              "modifier",  "comment",  "string",        "number",
              "regexp",    "operator", "decorator"}},
            {"tokenModifiers", json::array()},
            {"formats", {"relative"}}}}}}}},
  };
  SendRequest("initialize", params);
  SendNotification("initialized", json::object());
}

void LspClient::DidOpen(const std::string& uri, const std::string& text) {
  SendNotification("textDocument/didOpen", {{"textDocument",
                                             {{"uri", uri},
                                              {"languageId", "csharp"},
                                              {"version", 1},
                                              {"text", text}}}});
}

void LspClient::DidChange(const std::string& uri, const std::string& text) {
  static int version = 2;
  SendNotification("textDocument/didChange",
                   {{"textDocument", {{"uri", uri}, {"version", version++}}},
                    {"contentChanges", {{{"text", text}}}}});
}

void LspClient::DidClose(const std::string& uri) {
  SendNotification("textDocument/didClose", {{"textDocument", {{"uri", uri}}}});
}

void LspClient::RequestCompletion(const std::string& uri, int line,
                                  int character) {
  SendRequest("textDocument/completion",
              {{"textDocument", {{"uri", uri}}},
               {"position", {{"line", line}, {"character", character}}}});
}

void LspClient::RequestHover(const std::string& uri, int line, int character) {
  SendRequest("textDocument/hover",
              {{"textDocument", {{"uri", uri}}},
               {"position", {{"line", line}, {"character", character}}}});
}

void LspClient::RequestSignatureHelp(const std::string& uri, int line,
                                     int character,
                                     const std::string& trigger_char) {
  json params = {{"textDocument", {{"uri", uri}}},
                 {"position", {{"line", line}, {"character", character}}}};
  if (!trigger_char.empty()) {
    params["context"] = {{"triggerKind", 2},  // TriggerCharacter
                         {"triggerCharacter", trigger_char},
                         {"isRetrigger", false}};
  } else {
    params["context"] = {{"triggerKind", 1},  // Invoked
                         {"isRetrigger", false}};
  }
  SendRequest("textDocument/signatureHelp", params);
}

void LspClient::RequestSemanticTokens(const std::string& uri) {
  SendRequest("textDocument/semanticTokens/full",
              {{"textDocument", {{"uri", uri}}}});
}

// --- Result polling ---

bool LspClient::HasCompletions() const {
  std::lock_guard<std::mutex> lock(results_mutex_);
  return has_completions_;
}

std::vector<LspCompletionItem> LspClient::TakeCompletions() {
  std::lock_guard<std::mutex> lock(results_mutex_);
  has_completions_ = false;
  return std::move(completions_);
}

bool LspClient::HasDiagnostics(const std::string& uri) const {
  std::lock_guard<std::mutex> lock(results_mutex_);
  return diagnostics_.contains(uri);
}

std::vector<LspDiagnostic> LspClient::TakeDiagnostics(const std::string& uri) {
  std::lock_guard<std::mutex> lock(results_mutex_);
  auto it = diagnostics_.find(uri);
  if (it == diagnostics_.end()) {
    return {};
  }
  auto result = std::move(it->second);
  diagnostics_.erase(it);
  return result;
}

bool LspClient::HasHover() const {
  std::lock_guard<std::mutex> lock(results_mutex_);
  return has_hover_;
}

LspHoverResult LspClient::TakeHover() {
  std::lock_guard<std::mutex> lock(results_mutex_);
  has_hover_ = false;
  return std::move(hover_);
}

bool LspClient::HasSignatureHelp() const {
  std::lock_guard<std::mutex> lock(results_mutex_);
  return has_signature_help_;
}

LspSignatureHelp LspClient::TakeSignatureHelp() {
  std::lock_guard<std::mutex> lock(results_mutex_);
  has_signature_help_ = false;
  return std::move(signature_help_);
}

bool LspClient::HasSemanticTokens() const {
  std::lock_guard<std::mutex> lock(results_mutex_);
  return has_semantic_tokens_;
}

std::vector<LspSemanticToken> LspClient::TakeSemanticTokens() {
  std::lock_guard<std::mutex> lock(results_mutex_);
  has_semantic_tokens_ = false;
  return std::move(semantic_tokens_);
}

// --- URI helpers ---

std::string LspClient::PathToUri(const fs::path& path) {
  std::string abs = fs::absolute(path).generic_string();
#ifdef _WIN32
  // file:///C:/path/to/file
  return "file:///" + abs;
#else
  return "file://" + abs;
#endif
}

fs::path LspClient::UriToPath(const std::string& uri) {
  std::string path = uri;
  if (path.starts_with("file:///")) {
#ifdef _WIN32
    path = path.substr(8);  // file:///C:/... -> C:/...
#else
    path = path.substr(7);  // file:///path -> /path
#endif
  } else if (path.starts_with("file://")) {
    path = path.substr(7);
  }
  return fs::path(path);
}

// --- Debug log ---

void LspClient::AddLog(bool outgoing, const std::string& summary) {
  std::lock_guard<std::mutex> lock(log_mutex_);
  if (log_.size() >= kMaxLogEntries) {
    log_.erase(log_.begin());
  }
  log_.push_back({outgoing, summary});
}

std::vector<LspClient::LogEntry> LspClient::GetLog() const {
  std::lock_guard<std::mutex> lock(log_mutex_);
  return log_;
}

}  // namespace Wiesel::Editor