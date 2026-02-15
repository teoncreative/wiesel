
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

#include "w_engine.hpp"

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
  Application& app = *CreateApp();
  LOG_INFO("Initializing app...");
  app.Init();
  LOG_INFO("Running...");
  app.Run();
  LOG_INFO("Cleaning up...");
  delete &app;
  LOG_INFO("Done!");
}
