
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

int main(int argc, char** argv) {
  using namespace Wiesel;

  LOG_INFO("Initializing engine...");
  Engine::InitEngine();
  Application& app = *CreateApp(argc, argv);
  LOG_INFO("Initializing app...");
  app.Init();
  LOG_INFO("Running...");
  app.Run();
  LOG_INFO("Cleaning up...");
  delete &app;
  LOG_INFO("Done!");
}
