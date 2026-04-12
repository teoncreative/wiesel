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

namespace Wiesel {

enum class ConsoleLogLevel { Info, Warning, Error };

struct ConsoleLine {
  ConsoleLogLevel level;
  std::string text;
};

using CommandCallback =
    std::function<void(const std::vector<std::string>& args)>;

struct CommandEntry {
  std::string name;
  std::string description;
  CommandCallback callback;
};

class DeveloperConsole {
 public:
  DeveloperConsole();

  static void Init();
  static void Cleanup();
  static DeveloperConsole& Get();

  void Register(const std::string& name, const std::string& description,
                CommandCallback callback);
  void Unregister(const std::string& name);
  void Execute(const std::string& command_line);

  void Log(ConsoleLogLevel level, const std::string& message);

  void LogInfo(const std::string& message) {
    Log(ConsoleLogLevel::Info, message);
  }

  void LogWarning(const std::string& message) {
    Log(ConsoleLogLevel::Warning, message);
  }

  void LogError(const std::string& message) {
    Log(ConsoleLogLevel::Error, message);
  }

  void Clear();

  const std::vector<ConsoleLine>& GetLog() const { return log_; }

  const std::map<std::string, CommandEntry>& GetCommands() const {
    return commands_;
  }

 private:
  std::vector<std::string> Tokenize(const std::string& command_line);

  std::map<std::string, CommandEntry> commands_;
  std::vector<ConsoleLine> log_;
  std::mutex mutex_;

  bool visible_ = false;
};

#ifdef _MSC_VER

#define DCON_LOG_INFO(msg, ...) \
  ::Wiesel::DeveloperConsole::Get().LogInfo(std::format(msg, __VA_ARGS__))
#define DCON_LOG_WARN(msg, ...) \
  ::Wiesel::DeveloperConsole::Get().LogWarning(std::format(msg, __VA_ARGS__))
#define DCON_LOG_ERROR(msg, ...) \
  ::Wiesel::DeveloperConsole::Get().LogError(std::format(msg, __VA_ARGS__))

#else

#define DCON_LOG_INFO(msg, args...) \
  ::Wiesel::DeveloperConsole::Get().LogInfo(std::format(msg, ##args))
#define DCON_LOG_WARN(msg, args...) \
  ::Wiesel::DeveloperConsole::Get().LogWarning(std::format(msg, ##args))
#define DCON_LOG_ERROR(msg, args...) \
  ::Wiesel::DeveloperConsole::Get().LogError(std::format(msg, ##args))

#endif

}  // namespace Wiesel