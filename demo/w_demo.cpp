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

	DemoLayer::DemoLayer(DemoApplication& app) : m_App(app) {
	}

	DemoLayer::~DemoLayer() {

	}

	void DemoLayer::OnAttach() {
		Wiesel::LogDebug("Layer: OnAttach");

		Wiesel::Reference<Wiesel::Mesh> mesh = Wiesel::CreateReference<Wiesel::Mesh>(glm::vec3(1.0f), glm::quat());
		mesh->AddVertex({{-0.5f, 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
		mesh->AddVertex({{0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
		mesh->AddVertex({{0.5f, 0.0f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});
		mesh->AddVertex({{-0.5f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
		mesh->AddVertex({{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
		mesh->AddVertex({{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
		mesh->AddVertex({{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
		mesh->AddVertex({{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});

		mesh->AddIndex(0);
		mesh->AddIndex(1);
		mesh->AddIndex(2);
		mesh->AddIndex(2);
		mesh->AddIndex(3);
		mesh->AddIndex(0);
		mesh->AddIndex(4);
		mesh->AddIndex(5);
		mesh->AddIndex(6);
		mesh->AddIndex(6);
		mesh->AddIndex(7);
		mesh->AddIndex(4);
		mesh->Allocate();
		Wiesel::Renderer::GetRenderer()->AddMesh(mesh);

		Wiesel::Reference<Wiesel::Camera> camera = Wiesel::CreateReference<Wiesel::Camera>(glm::vec3(3.0f, 2.5f, 3.0f), glm::angleAxis(0.0f, glm::vec3(0.0f, 0.0f, 0.0f)), Wiesel::Renderer::GetRenderer()->GetAspectRatio());
		camera->Rotate(PI / 4.0f, 0.0f, 1.0f, 0.0f);
		camera->Rotate(-PI / 6.0f, 1.0f, 0.0f, 0.0f);
		Wiesel::Renderer::GetRenderer()->AddCamera(camera);
		Wiesel::Renderer::GetRenderer()->SetActiveCamera(camera->GetObjectId());
	}

	void DemoLayer::OnDetach() {
		Wiesel::LogDebug("Layer: OnDetach");
	}

	void DemoLayer::OnUpdate(double_t deltaTime) {
	//	Wiesel::LogInfo("Layer: OnUpdate " + std::to_string(deltaTime));
	}

	void DemoLayer::OnEvent(Wiesel::Event& event) {
	//	Wiesel::LogInfo("Layer: OnEvent" + std::string(event.GetEventName()));
	Wiesel::EventDispatcher dispatcher(event);
	dispatcher.Dispatch<Wiesel::KeyPressedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyPress));
	}

	bool DemoLayer::OnKeyPress(Wiesel::KeyPressedEvent& event) {
		if (event.GetKeyCode() == ' ') {
			m_App.Close();
		}
		return true;
	}

	void DemoApplication::Init() {
		Wiesel::LogDebug("Init");
		PushLayer(Wiesel::CreateReference<DemoLayer>(*this));
	}

	DemoApplication::DemoApplication() {
		Wiesel::LogDebug("DemoApp constructor");
	}

	DemoApplication::~DemoApplication() {
		Wiesel::LogDebug("DemoApp destructor");
	}

}

Wiesel::Application* Wiesel::CreateApp() {
	return new WieselDemo::DemoApplication();
}