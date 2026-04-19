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

#include "util/w_command_parser.h"
#include "w_pch.h"

namespace wiesel {

enum class ConsoleLogLevel { UserInput, Info, Warning, Error };

struct ConsoleLine {
  ConsoleLogLevel level;
  std::string text;
  // Optional call-stack / context that the console panel renders below
  // the message in a selectable block when non-empty.
  std::string stack_trace;
};

using CommandCallback = std::function<void(const CommandContext&)>;

struct CommandEntry {
  std::string name;
  std::string description;
  std::vector<Param> params;
  CommandCallback callback;
};

class DeveloperConsole {
 public:
  DeveloperConsole();

  static void Init();
  static void Cleanup();
  static DeveloperConsole& Get();

  void Register(const std::string& name, const std::string& description,
                std::vector<Param> params, CommandCallback callback);
  void Register(const std::string& name, const std::string& description,
                CommandCallback callback) {
    Register(name, description, {}, std::move(callback));
  }
  void Unregister(const std::string& name);
  void Execute(const std::string& command_line);

  // Lookup for autocomplete / help UIs.
  const CommandEntry* Find(const std::string& name) const;

  void Log(ConsoleLogLevel level, const std::string& message,
           std::string stack_trace = "");

  void LogInfo(const std::string& message, std::string stack_trace = "") {
    Log(ConsoleLogLevel::Info, message, std::move(stack_trace));
  }
  void LogWarning(const std::string& message,
                  std::string stack_trace = "") {
    Log(ConsoleLogLevel::Warning, message, std::move(stack_trace));
  }
  void LogError(const std::string& message, std::string stack_trace = "") {
    Log(ConsoleLogLevel::Error, message, std::move(stack_trace));
  }

  void Clear();

  const std::vector<ConsoleLine>& GetLog() const { return log_; }
  // Thread-safe snapshot — callers that iterate across a frame should
  // use this so background threads pushing new lines can't invalidate
  // the iterator mid-read.
  std::vector<ConsoleLine> SnapshotLog() {
    std::lock_guard lock(mutex_);
    return log_;
  }
  const std::map<std::string, CommandEntry>& GetCommands() const {
    return commands_;
  }

 private:
  std::map<std::string, CommandEntry> commands_;
  std::vector<ConsoleLine> log_;
  std::mutex mutex_;
  bool visible_ = false;
};

#ifdef _MSC_VER

#define DCON_LOG_INFO(msg, ...) \
  ::wiesel::DeveloperConsole::Get().LogInfo(std::format(msg, __VA_ARGS__))
#define DCON_LOG_WARN(msg, ...) \
  ::wiesel::DeveloperConsole::Get().LogWarning(std::format(msg, __VA_ARGS__))
#define DCON_LOG_ERROR(msg, ...) \
  ::wiesel::DeveloperConsole::Get().LogError(std::format(msg, __VA_ARGS__))

#else

#define DCON_LOG_INFO(msg, args...) \
  ::wiesel::DeveloperConsole::Get().LogInfo(std::format(msg, ##args))
#define DCON_LOG_WARN(msg, args...) \
  ::wiesel::DeveloperConsole::Get().LogWarning(std::format(msg, ##args))
#define DCON_LOG_ERROR(msg, args...) \
  ::wiesel::DeveloperConsole::Get().LogError(std::format(msg, ##args))

#endif

}  // namespace wiesel
