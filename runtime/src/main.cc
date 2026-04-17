//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "game/w_game_application.h"
#include "layer/w_layerscene.h"
#include "util/w_command.h"
#include "util/w_platform.h"
#include "w_engine.h"

#include <clocale>

#ifdef WIN32
#include <windows.h>
#endif

using namespace wiesel;

class RuntimeApplication : public GameApplication {
 public:
  RuntimeApplication() : GameApplication({"Wiesel", {1280, 720}}, {}) {}

  ~RuntimeApplication() override = default;

  void Init() override {
    PushLayer(std::make_shared<SceneLayer>());

    const EngineProperties& props = Engine::properties();

    std::filesystem::path game_info_path = props.game_info_path;

    if (game_info_path.empty()) {
      LOG_ERROR(
          "No gameinfo.wgame found. Use --game-info <path> or place it "
          "next to the executable.");
      return;
    }

    if (!LoadGame(game_info_path, props.app_assets_path)) {
      LOG_ERROR("Failed to load game from: {}", game_info_path.string());
    }
  }
};

Application* wiesel::CreateApp() {
  return new RuntimeApplication();
}

static int RunEngine(int argc, char** argv) {
  std::setlocale(LC_ALL, "C");
#ifdef WIN32
  // Check if --enable-stdio is in args before parsing
  bool wants_stdio = false;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--enable-stdio") {
      wants_stdio = true;
      break;
    }
  }
  if (wants_stdio) {
    AllocateConsole();
    EnableAnsiColors();
  } else {
    // Redirect stdout/stderr to NUL immediately so LOG_* calls don't crash
    FILE* f = nullptr;
    freopen_s(&f, "NUL", "w", stdout);
    freopen_s(&f, "NUL", "w", stderr);
  }
#endif
  DeveloperConsole::Init();

  EngineProperties properties = EngineProperties::Parse(argc, argv);

  Engine::InitEngine(properties);
  Engine::InitApplication();
  Engine::app().Run();
  Engine::CleanupApplication();
  Engine::CleanupAssets();
  Engine::CleanupWindow();
  Engine::CleanupEngine();
  Engine::CleanupRenderer();
  DeveloperConsole::Cleanup();
  return 0;
}

#ifdef WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return RunEngine(__argc, __argv);
}
#else
int main(int argc, char** argv) {
  return RunEngine(argc, argv);
}
#endif