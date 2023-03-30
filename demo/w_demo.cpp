//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_demo.h"
#include "w_application.h"
#include "w_model.h"

namespace WieselDemo {

	DemoLayer::DemoLayer(DemoApplication& app) : m_App(app) {
		m_InputX = 0.0f;
		m_InputY = 0.0f;
		m_MouseSpeed = 0.001f;
		m_LookXLimit = PI / 2.0f - (PI / 16.0f);
	}

	DemoLayer::~DemoLayer() {

	}

	void DemoLayer::OnAttach() {
		Wiesel::LogDebug("Layer: OnAttach");

		/*const std::vector<Wiesel::Vertex> vertices = {
				{{-0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
				{{0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
				{{0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
				{{-0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

				{{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
				{{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
				{{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
				{{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
		};

		const std::vector<Wiesel::Index> indices = {
				0, 1, 2, 2, 3, 0,
				4, 5, 6, 6, 7, 4
		};

		Wiesel::Reference<Wiesel::Mesh> mesh = Wiesel::CreateReference<Wiesel::Mesh>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), vertices, indices);
		mesh->SetTexture("assets/textures/texture.jpg");
		mesh->Allocate();
		Wiesel::Renderer::GetRenderer()->AddMesh(mesh);

		const std::vector<Wiesel::Vertex> vertices1 = {
				{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
				{{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
				{{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
				{{-0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}
		};

		Wiesel::Reference<Wiesel::Mesh> mesh1 = Wiesel::CreateReference<Wiesel::Mesh>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(), vertices1, indices);
		mesh1->SetTexture("assets/textures/texture.jpg");
		mesh1->Allocate();
		mesh1->Move(0.0f, 1.0f, 0.0f);
		Wiesel::Renderer::GetRenderer()->AddMesh(mesh1);*/

		Wiesel::Reference<Wiesel::Mesh> mesh = Wiesel::CreateReference<Wiesel::Mesh>(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat());
		mesh->LoadFromObj("assets/models/viking_room.obj", "assets/textures/viking_room.png");
		Wiesel::Renderer::GetRenderer()->AddMesh(mesh);

		Wiesel::Reference<Wiesel::Camera> camera = Wiesel::CreateReference<Wiesel::Camera>(glm::vec3(2.0f, 1.5f, 2.0f), glm::angleAxis(0.0f, glm::vec3(0.0f, 0.0f, 0.0f)), Wiesel::Renderer::GetRenderer()->GetAspectRatio(), 60);
		//camera->Rotate(PI / 4.0f, 0.0f, 1.0f, 0.0f);
		//camera->Rotate(-PI / 6.0f, 1.0f, 0.0f, 0.0f);
		Wiesel::Renderer::GetRenderer()->AddCamera(camera);
		Wiesel::Renderer::GetRenderer()->SetActiveCamera(camera->GetObjectId());
	}

	void DemoLayer::OnDetach() {
		Wiesel::LogDebug("Layer: OnDetach");
	}

	void DemoLayer::OnUpdate(float_t deltaTime) {
	//	Wiesel::LogInfo("Layer: OnUpdate " + std::to_string(deltaTime));
		const Wiesel::Reference<Wiesel::Renderer>& renderer = Wiesel::Renderer::GetRenderer();
		const Wiesel::Reference<Wiesel::Camera>& camera = renderer->GetActiveCamera();
		if (m_KeyManager.IsPressed(Wiesel::W)) {
			camera->Move(camera->GetForward() * deltaTime * 2.0f);
		} else if (m_KeyManager.IsPressed(Wiesel::S)) {
			camera->Move(camera->GetBackward() * deltaTime * 2.0f);
		}

		if (m_KeyManager.IsPressed(Wiesel::A)) {
			camera->Move(camera->GetLeft() * deltaTime * 2.0f);
		} else if (m_KeyManager.IsPressed(Wiesel::D)) {
			camera->Move(camera->GetRight() * deltaTime * 2.0f);
		}

		camera->SetRotation(std::clamp(m_InputY, -m_LookXLimit, m_LookXLimit), m_InputX, 0.0f);
	}

	void DemoLayer::OnEvent(Wiesel::Event& event) {
		Wiesel::EventDispatcher dispatcher(event);

		dispatcher.Dispatch<Wiesel::KeyPressedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyPress));
		dispatcher.Dispatch<Wiesel::KeyReleasedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyReleased));
		dispatcher.Dispatch<Wiesel::MouseMovedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnMouseMoved));
	}

	bool DemoLayer::OnKeyPress(Wiesel::KeyPressedEvent& event) {
		if (event.GetKeyCode() == ' ') {
			m_App.Close();
			return true;
		}
		m_KeyManager.Set(event.GetKeyCode(), true);
		return true;
	}

	bool DemoLayer::OnKeyReleased(Wiesel::KeyReleasedEvent& event) {
		if (event.GetKeyCode() == ' ') {
			m_App.Close();
			return true;
		}

		m_KeyManager.Set(event.GetKeyCode(), false);
		return true;
	}

	bool DemoLayer::OnMouseMoved(Wiesel::MouseMovedEvent& event) {
		const Wiesel::WindowSize& windowSize = Wiesel::Renderer::GetRenderer()->GetWindowSize();

		m_InputX += m_MouseSpeed * ((windowSize.Width / 2.0f) - event.GetX());
		m_InputY += m_MouseSpeed * ((windowSize.Height / 2.0f) - event.GetY());
		return true;
	}

	void DemoApplication::Init() {
		Wiesel::LogDebug("Init");
		PushLayer(Wiesel::CreateReference<DemoLayer>(*this));
		Wiesel::Renderer::GetRenderer()->GetAppWindow()->SetCursorMode(Wiesel::CursorModeRelative);
	}

	DemoApplication::DemoApplication() {
		Wiesel::LogDebug("DemoApp constructor");
	}

	DemoApplication::~DemoApplication() {
		Wiesel::LogDebug("DemoApp destructor");
	}

}

// Called from entrypoint
Wiesel::Application* Wiesel::CreateApp() {
	return new WieselDemo::DemoApplication();
}