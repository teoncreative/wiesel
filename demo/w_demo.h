
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"
#include "w_application.h"
#include "w_demo.h"
#include "events/w_keyevents.h"
#include "events/w_mouseevents.h"
#include "w_keymanager.h"

namespace WieselDemo {
	class DemoApplication : public Wiesel::Application {
	public:
		DemoApplication();
		~DemoApplication() override;

		void Init() override;
	};

	class DemoLayer : public Wiesel::Layer {
	public:
		explicit DemoLayer(DemoApplication& app);
		~DemoLayer() override;

		void OnAttach() override;
		void OnDetach() override;

		void OnUpdate(float_t deltaTime) override;
		void OnEvent(Wiesel::Event& event) override;

		bool OnKeyPress(Wiesel::KeyPressedEvent& event);
		bool OnKeyReleased(Wiesel::KeyReleasedEvent& event);
		bool OnMouseMoved(Wiesel::MouseMovedEvent& event);

	private:
		DemoApplication& m_App;
		Wiesel::KeyManager m_KeyManager;

		float_t m_InputX;
		float_t m_InputY;
		float_t m_MouseSpeed;
		float_t m_LookXLimit;
	};
}