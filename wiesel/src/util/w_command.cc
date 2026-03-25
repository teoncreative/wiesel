
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_command.h"
#include "util/w_logger.h"
#include "w_application.h"
#include "w_engine.h"

namespace Wiesel {

DeveloperConsole::DeveloperConsole() {
  Register("help", "List all available commands",
           [this](const std::vector<std::string>&) {
             LogInfo("Available commands:");
             for (const auto& [name, entry] : commands_) {
               LogInfo("  " + name + " - " + entry.description);
             }
           });

  Register("clear", "Clear console output",
           [this](const std::vector<std::string>&) { Clear(); });

  Register(
      "host_timescale", "Set game time scale (e.g. host_timescale 0.5)",
      [this](const std::vector<std::string>& args) {
        Application& app = Engine::app();
        if (args.empty()) {
          LogInfo("host_timescale = " + std::to_string(app.GetTimeScale()));
          return;
        }
        try {
          float scale = std::stof(args[0]);
          app.SetTimeScale(scale);
          LogInfo("host_timescale set to " + std::to_string(scale));
        } catch (...) {
          LogError("Usage: host_timescale <value>");
        }
      });

  Register("toggleconsole", "Toggle developer console visibility",
           [this](const std::vector<std::string>&) { Toggle(); });

  Register("max_fps", "Set max FPS limit (0 = unlimited)",
           [this](const std::vector<std::string>& args) {
             Application& app = Engine::app();
             if (args.empty()) {
               LogInfo("max_fps = " + std::to_string(app.GetMaxFPS()));
               return;
             }
             try {
               float fps = std::stof(args[0]);
               app.SetMaxFPS(fps);
               LogInfo("max_fps set to " + std::to_string(fps));
             } catch (...) {
               LogError("Usage: max_fps <value>");
             }
           });
}

void DeveloperConsole::Register(const std::string& name,
                                const std::string& description,
                                CommandCallback callback) {
  std::lock_guard lock(mutex_);
  commands_[name] = {name, description, std::move(callback)};
}

void DeveloperConsole::Unregister(const std::string& name) {
  std::lock_guard lock(mutex_);
  commands_.erase(name);
}

void DeveloperConsole::Execute(const std::string& command_line) {
  auto tokens = Tokenize(command_line);
  if (tokens.empty()) {
    return;
  }

  LogInfo("> " + command_line);

  const std::string& cmd_name = tokens[0];
  std::vector<std::string> args(tokens.begin() + 1, tokens.end());

  std::lock_guard lock(mutex_);
  auto it = commands_.find(cmd_name);
  if (it == commands_.end()) {
    LogError("Unknown command: " + cmd_name);
    return;
  }

  try {
    it->second.callback(args);
  } catch (const std::exception& e) {
    LogError("Command '" + cmd_name + "' failed: " + e.what());
  }
}

void DeveloperConsole::Log(ConsoleLogLevel level, const std::string& message) {
  log_.push_back({level, message});
  switch (level) {
    case ConsoleLogLevel::Info:
      LOG_INFO("[DCON] {}", message);
      break;
    case ConsoleLogLevel::Warning:
      LOG_WARN("[DCON] {}", message);
      break;
    case ConsoleLogLevel::Error:
      LOG_ERROR("[DCON] {}", message);
      break;
  }
}

void DeveloperConsole::Clear() {
  log_.clear();
}

std::vector<std::string> DeveloperConsole::Tokenize(
    const std::string& command_line) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_quotes = false;

  for (size_t i = 0; i < command_line.size(); i++) {
    char c = command_line[i];
    if (c == '"') {
      in_quotes = !in_quotes;
    } else if (c == ' ' && !in_quotes) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

}  // namespace Wiesel