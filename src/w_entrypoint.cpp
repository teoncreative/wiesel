//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_application.h"

int main() {
	Wiesel::Application& app = *Wiesel::CreateApp();
	WIESEL_PROFILE_BEGIN_SECTION("Application Init");
	app.Init();
	WIESEL_PROFILE_END_SECTION(std::cout);
	WIESEL_PROFILE_BEGIN_SECTION("Application Run");
	app.Run();
	WIESEL_PROFILE_END_SECTION(std::cout);
	WIESEL_PROFILE_BEGIN_SECTION("Application Cleanup");
	delete &app;
	WIESEL_PROFILE_END_SECTION(std::cout);
	Wiesel::LogInfo("Exiting...");
}
