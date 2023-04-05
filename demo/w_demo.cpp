//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_demo.h"
#include "util/w_keycodes.h"
#include "w_engine.h"

using namespace Wiesel;

namespace WieselDemo {

	DemoLayer::DemoLayer(DemoApplication& app) : m_App(app), Layer("Demo Layer") {
		m_Scene = app.GetScene();
		m_Renderer = Engine::GetRenderer();

		m_InputX = 0.0f;
		m_InputY = 0.0f;
		m_MouseSpeed = 0.001f;
		m_CameraMoveSpeed = 8.0f;
		m_LookXLimit = PI / 2.0f - (PI / 16.0f);
	}

	DemoLayer::~DemoLayer() = default;

	static entt::entity sponzaEntityHandle;

	void DemoLayer::OnAttach() {
		LOG_DEBUG("Layer: OnAttach");

		{
			const std::vector<Vertex> vertices = {
					{{-0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
					{{0.5f, 0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
					{{0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
					{{-0.5f, 0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
			};

			const std::vector<Index> indices = {
					0, 1, 2, 2, 3, 0
			};


			{
				Reference<Mesh> mesh = CreateReference<Mesh>(vertices, indices);
				mesh->Texture = m_Renderer->CreateTexture("assets/textures/texture.jpg", {});
				mesh->Allocate();

				Entity entity = m_Scene->CreateEntity("Custom Mesh");
				auto& model = entity.AddComponent<ModelComponent>();
				auto& transform = entity.GetComponent<TransformComponent>();
				//transform.Scale = {100, 100, 100};
				model.Data.Meshes.push_back(mesh);
			}
			{
				Reference<Mesh> mesh = CreateReference<Mesh>(vertices, indices);
				mesh->Texture = m_Renderer->CreateTexture("assets/textures/texture.jpg", {});
				mesh->Allocate();

				Entity entity = m_Scene->CreateEntity("Custom Mesh 2");
				auto& model = entity.AddComponent<ModelComponent>();
				auto& transform = entity.GetComponent<TransformComponent>();
				transform.Rotation = {PI, 0.0f, 0.0f};
				//transform.Scale = {100, 100, 100};
				model.Data.Meshes.push_back(mesh);
			}
		}

		// Loading a model to the scene
		{
			Entity entity = m_Scene->CreateEntity("Sponza");
			sponzaEntityHandle = (entt::entity) entity;
			auto& model = entity.AddComponent<ModelComponent>();
			auto& transform = entity.GetComponent<TransformComponent>();
			Engine::LoadModel(transform, model, "assets/models/city/gmae.obj");
		}

		// Custom camera
		Reference<Camera> camera = CreateReference<Camera>(glm::vec3(2.0f, 1.5f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), m_Renderer->GetAspectRatio(), 60);
		m_Renderer->AddCamera(camera);
		m_Renderer->SetActiveCamera(camera->GetId());
		m_Renderer->SetClearColor(0.02f, 0.02f, 0.04f);
	}

	void DemoLayer::OnDetach() {
		LOG_DEBUG("Layer: OnDetach");
	}

	void DemoLayer::OnUpdate(float_t deltaTime) {
	//	LogInfo("Layer: OnUpdate " + std::to_string(deltaTime));
		const Reference<Camera>& camera = m_Renderer->GetActiveCamera();
		if (m_KeyManager.IsPressed(KeyW)) {
			camera->Move(camera->GetForward() * deltaTime * m_CameraMoveSpeed);
		} else if (m_KeyManager.IsPressed(KeyS)) {
			camera->Move(camera->GetBackward() * deltaTime * m_CameraMoveSpeed);
		}

		if (m_KeyManager.IsPressed(KeyA)) {
			camera->Move(camera->GetLeft() * deltaTime * m_CameraMoveSpeed);
		} else if (m_KeyManager.IsPressed(KeyD)) {
			camera->Move(camera->GetRight() * deltaTime * m_CameraMoveSpeed);
		}

		m_InputY = std::clamp(m_InputY, -m_LookXLimit, m_LookXLimit);
		camera->SetRotation(m_InputY, m_InputX, 0.0f);


		Entity entity = {sponzaEntityHandle, &*m_Scene};
		auto& transform = entity.GetComponent<TransformComponent>();
		// add proper functions for rotation
		transform.Rotation.x += PI / 400.0f;
		transform.IsChanged = true;
	}

	void DemoLayer::OnEvent(Event& event) {
		EventDispatcher dispatcher(event);

		dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyPress));
		dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnKeyReleased));
		dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_EVENT_FUNCTION(OnMouseMoved));
	}

	bool DemoLayer::OnKeyPress(KeyPressedEvent& event) {
		if (event.GetKeyCode() == KeySpace) {
			m_App.Close();
			return true;
		} else if (event.GetKeyCode() == KeyEscape) {
			if (m_App.GetWindow()->GetCursorMode() == CursorModeRelative) {
				m_App.GetWindow()->SetCursorMode(CursorModeNormal);
			} else {
				m_App.GetWindow()->SetCursorMode(CursorModeRelative);
			}
			return true;
		}
		m_KeyManager.Set(event.GetKeyCode(), true);
		return true;
	}

	bool DemoLayer::OnKeyReleased(KeyReleasedEvent& event) {
		if (event.GetKeyCode() == ' ') {
			m_App.Close();
			return true;
		}

		m_KeyManager.Set(event.GetKeyCode(), false);
		return true;
	}

	bool DemoLayer::OnMouseMoved(MouseMovedEvent& event) {
		if (m_App.GetWindow()->GetCursorMode() != CursorModeRelative) {
			return true;
		}

		m_InputX += m_MouseSpeed * ((m_App.GetWindowSize().Width / 2.0f) - event.GetX());
		m_InputY += m_MouseSpeed * ((m_App.GetWindowSize().Height / 2.0f) - event.GetY());
		return true;
	}

	void DemoApplication::Init() {
		LOG_DEBUG("Init");
		PushLayer(CreateReference<DemoLayer>(*this));
	}

	DemoApplication::DemoApplication() {
		LOG_DEBUG("DemoApp constructor");
	}

	DemoApplication::~DemoApplication() {
		LOG_DEBUG("DemoApp destructor");
	}
}

// Called from entrypoint
Application* Wiesel::CreateApp() {
	return new WieselDemo::DemoApplication();
}
