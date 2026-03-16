
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "layer/w_layerconsole.hpp"
#include "layer/w_layerimgui.hpp"
#include "layer/w_layerscene.hpp"
#include "native_demo.hpp"
#include "native_light_bob.hpp"
#include "scene/w_componentutil.hpp"
#include "w_editor.hpp"
#include "scene/w_scene_manager.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"

using namespace Wiesel;
using namespace Wiesel::Editor;

namespace NativeDemo {

DemoApplication::DemoApplication(bool enable_editor)
    : GameApplication({"Wiesel Demo", {1600, 900}, true}, {}),
      enable_editor_(enable_editor) {
}

void DemoApplication::Init() {
  LOG_DEBUG("Init");

  // Register native C++ behaviors
  RegisterNativeBehavior<NativeLightBob>("NativeLightBob");

  std::shared_ptr<Scene> scene = std::make_shared<Scene>();
  if (enable_editor_) {
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<ConsoleLayer>());
    PushLayer(std::make_shared<EditorLayer>(*this, scene));
  } else {
    // Dev/release mode: load project from CLI arg, then run
    const auto& project_path = Engine::properties().project_path;
    if (!project_path.empty()) {
      LoadProjectAndScene(project_path, scene);
    }
    SceneManager::Get().SetActiveScene(scene);
    PushLayer(std::make_shared<SceneLayer>(scene));
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<ConsoleLayer>());
  }
}

}  // namespace LeapLand

// Called from entrypoint
Application* Wiesel::CreateApp() {
  bool enable_editor = Engine::properties().editor_enabled;
  return new NativeDemo::DemoApplication(enable_editor);
}