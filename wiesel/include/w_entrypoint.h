
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

#include "w_engine.h"

#ifdef WIN32
#include <windows.h>

void EnableAnsiColors() {
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);
}
#endif

int main(int argc, char** argv) {
  using namespace Wiesel;
#ifdef WIN32
  EnableAnsiColors();
#endif

  LOG_INFO("Initializing engine...");
  EngineProperties properties = EngineProperties::Parse(argc, argv);
  Engine::InitEngine(properties);
  Engine::InitApplication();
  LOG_INFO("Running...");
  Application& application = Engine::app();
  application.Run();
  Engine::CleanupApplication();

  Engine::CleanupEngine();
  LOG_INFO("Done!");
}
