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

namespace wiesel {

static std::unique_ptr<DeveloperConsole> console_;

void DeveloperConsole::Init() {
  console_ = std::make_unique<DeveloperConsole>();
}

void DeveloperConsole::Cleanup() {
  console_ = nullptr;
}

DeveloperConsole& DeveloperConsole::Get() {
  return *console_;
}

DeveloperConsole::DeveloperConsole() {
  Register("help", "List all available commands",
           [this](const CommandContext&) {
             LogInfo("Available commands:");
             for (const auto& [name, entry] : commands_) {
               LogInfo("  " + name + " - " + entry.description);
             }
           });

  Register("clear", "Clear console output",
           [this](const CommandContext&) { Clear(); });

  Register("host_timescale", "Set game time scale (e.g. host_timescale 0.5)",
           Params::Make(Params::Float("scale", 1.0f)),
           [this](const CommandContext& ctx) {
             Application& app = Engine::app();
             if (!ctx.Has("scale")) {
               LogInfo("host_timescale = " +
                       std::to_string(app.GetTimeScale()));
               return;
             }
             float scale = ctx.Float("scale");
             app.SetTimeScale(scale);
             LogInfo("host_timescale set to " + std::to_string(scale));
           });

  Register("max_fps", "Set max FPS limit (0 = unlimited)",
           Params::Make(Params::Float("fps", 0.0f)),
           [this](const CommandContext& ctx) {
             Application& app = Engine::app();
             if (!ctx.Has("fps")) {
               LogInfo("max_fps = " + std::to_string(app.GetMaxFPS()));
               return;
             }
             float fps = ctx.Float("fps");
             app.SetMaxFPS(fps);
             LogInfo("max_fps set to " + std::to_string(fps));
           });
}

void DeveloperConsole::Register(const std::string& name,
                                const std::string& description,
                                std::vector<Param> params,
                                CommandCallback callback) {
  std::lock_guard lock(mutex_);
  commands_[name] = CommandEntry{name, description, std::move(params),
                                 std::move(callback)};
}

void DeveloperConsole::Unregister(const std::string& name) {
  std::lock_guard lock(mutex_);
  commands_.erase(name);
}

const CommandEntry* DeveloperConsole::Find(const std::string& name) const {
  auto it = commands_.find(name);
  return (it == commands_.end()) ? nullptr : &it->second;
}

void DeveloperConsole::Execute(const std::string& command_line) {
  auto tokens = CommandParser::Tokenize(command_line);
  if (tokens.empty()) {
    return;
  }

  Log(ConsoleLogLevel::UserInput, command_line);

  const std::string cmd_name = tokens[0];
  CommandEntry entry_copy;
  bool found = false;
  {
    std::lock_guard lock(mutex_);
    auto it = commands_.find(cmd_name);
    if (it != commands_.end()) {
      entry_copy = it->second;
      found = true;
    }
  }
  // Log without holding the mutex - Log() takes it itself and we'd
  // otherwise deadlock on std::mutex (non-recursive).
  if (!found) {
    LogError("Unknown command: " + cmd_name);
    return;
  }

  auto parsed = CommandParser::ParseArgs(entry_copy.params, tokens, 1);
  if (!parsed.ok) {
    LogError("Command '" + cmd_name + "': " + parsed.error);
    return;
  }

  try {
    entry_copy.callback(parsed.context);
  } catch (const std::exception& e) {
    LogError("Command '" + cmd_name + "' failed: " + e.what());
  }
}

void DeveloperConsole::Log(ConsoleLogLevel level, const std::string& message,
                           std::string stack_trace) {
  {
    std::lock_guard lock(mutex_);
    log_.push_back({level, message, std::move(stack_trace)});
  }
  switch (level) {
    case ConsoleLogLevel::UserInput:
      LOG_INFO("> {}", message);
      break;
    case ConsoleLogLevel::Info:
      LOG_INFO("{}", message);
      break;
    case ConsoleLogLevel::Warning:
      LOG_WARN("{}", message);
      break;
    case ConsoleLogLevel::Error:
      LOG_ERROR("{}", message);
      break;
  }
}

void DeveloperConsole::Clear() {
  std::lock_guard lock(mutex_);
  log_.clear();
}

}  // namespace wiesel
