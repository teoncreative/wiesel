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
#include "w_editor_components.h"

#include <clocale>
#include "layer/w_layerimgui.h"
#include "util/w_command.h"
#include "util/w_platform.h"
#include "w_engine.h"

using namespace wiesel;
using namespace wiesel::editor;

class EditorApplication : public Application {
 public:
  EditorApplication() : Application({"Wiesel Editor", {1600, 900}, true}, {}) {}

  ~EditorApplication() override = default;

  void Init() override {
    InitializeEditorComponents();
    InitializeScriptFieldRenderers();
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<EditorLayer>(*this));
  }
};

Application* wiesel::CreateApp() {
  return new EditorApplication();
}

int main(int argc, char** argv) {
  std::setlocale(LC_ALL, "C");
  EnableAnsiColors();
  DeveloperConsole::Init();
  EngineProperties properties = EngineProperties::Parse(argc, argv);
  properties.editor_enabled = true;
  Engine::InitEngine(properties);
  Engine::InitApplication();
  Application& application = Engine::app();
  application.Run();
  Engine::CleanupApplication();
  Engine::CleanupEngine();
  DeveloperConsole::Cleanup();
  return 0;
}