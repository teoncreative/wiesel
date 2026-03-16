
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_application.hpp"
#include "w_engine.hpp"
#include "behavior/w_native_behavior.hpp"
#include "project/w_project.hpp"

namespace Wiesel {

class GameApplication : public Application {
 public:
  GameApplication(const WindowProperties&& window_props,
                  const RendererProperties&& renderer_props)
      : Application(std::move(window_props), std::move(renderer_props)) {}

  ~GameApplication() override = default;

  template <typename T>
  void RegisterNativeBehavior(const std::string& name) {
    Engine::behavior_registry().Register<T>(name);
  }

  // Load a project and its start scene (convenience for non-editor mode).
  bool LoadProjectAndScene(const std::filesystem::path& project_path,
                           Ref<Scene> scene);

  Project* GetProject() const { return project_.get(); }

 protected:
  std::unique_ptr<Project> project_;
};

}  // namespace Wiesel