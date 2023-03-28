//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_demo.h"
#include "w_application.h"

namespace WieselDemo {

	DemoLayer::DemoLayer() {

	}

	DemoLayer::~DemoLayer() {

	}

	void DemoLayer::OnAttach() {
		Wiesel::LogInfo("Layer: OnAttach");
	}

	void DemoLayer::OnDetach() {
		Wiesel::LogInfo("Layer: OnDetach");
	}

	void DemoLayer::OnUpdate(double_t deltaTime) {
		Wiesel::LogInfo("Layer: OnUpdate " + std::to_string(deltaTime));
	}

	void DemoLayer::OnEvent(Wiesel::Event& event) {
		Wiesel::LogInfo("Layer: OnEvent" + std::string(event.GetEventName()));
	}

	void DemoApplication::Init() {
		Wiesel::LogInfo("Init");
		PushLayer(Wiesel::CreateShared<DemoLayer>());
	}

	DemoApplication::DemoApplication() {
		Wiesel::LogInfo("Constructor");
	}

	DemoApplication::~DemoApplication() {
		Wiesel::LogInfo("Destructor");
	}

}

Wiesel::Application* Wiesel::CreateApp() {
	return new WieselDemo::DemoApplication();
}