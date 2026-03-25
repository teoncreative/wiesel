//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor.hpp"
#include "w_editor_components.hpp"

#include "layer/w_layerimgui.hpp"
#include "scene/w_scene.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"

using namespace Wiesel;
using namespace Wiesel::Editor;

class EditorApplication : public Application {
 public:
  EditorApplication() : Application({"Wiesel Editor", {1600, 900}, true}, {}) {}

  ~EditorApplication() override = default;

  void Init() override {
    InitializeEditorComponents();
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<EditorLayer>(*this));
  }
};

Application* Wiesel::CreateApp() {
  // Force editor mode when running standalone
  auto& props = const_cast<EngineProperties&>(Engine::properties());
  props.editor_enabled = true;

  return new EditorApplication();
}