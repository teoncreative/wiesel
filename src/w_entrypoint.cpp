//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_engine.hpp"
#include "rendering/w_texture.hpp"

using namespace Wiesel;

int main() {
	Wiesel::Engine::InitEngine();
	Wiesel::Application& app = *Wiesel::CreateApp();
	app.Init();
	app.Run();
	delete &app;
	LOG_INFO("Exiting...");
}
