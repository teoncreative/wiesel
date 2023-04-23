
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "input/w_input.hpp"
#include "w_engine.hpp"

namespace Wiesel {

	std::map<std::string, std::vector<KeyCode>> InputManager::m_KeyboardMapping;
	std::map<KeyCode, KeyData> InputManager::m_Keys;
	std::map<MouseCode, KeyData> InputManager::m_MouseButtons;
	std::map<std::string, float> InputManager::m_Axis;
	int InputManager::m_MouseX = 0;
	int InputManager::m_MouseY = 0;
	float InputManager::m_MouseAxisSensX = 2.0f;
	float InputManager::m_MouseAxisSensY = 2.0f;
	InputMode InputManager::m_InputMode = InputModeKeyboardAndMouse;

	void InputManager::Init() {
		m_KeyboardMapping["Up"] = {KeyArrowUp, KeyW};
		m_KeyboardMapping["Down"] = {KeyArrowDown, KeyS};
		m_KeyboardMapping["Left"] = {KeyArrowLeft, KeyA};
		m_KeyboardMapping["Right"] = {KeyArrowRight, KeyD};
		m_KeyboardMapping["Jump"] = {KeySpace};
		m_KeyboardMapping["Enter"] = {KeyEnter};
		m_KeyboardMapping["Left Shift"] = {KeyLeftShift};
		m_KeyboardMapping["Right Shift"] = {KeyRightShift};
		m_KeyboardMapping["Shift"] = {KeyLeftShift, KeyRightShift};
		m_KeyboardMapping["Left Control"] = {KeyLeftControl};
		m_KeyboardMapping["Right Control"] = {KeyRightControl};
		m_KeyboardMapping["Control"] = {KeyLeftControl, KeyRightControl};
		m_KeyboardMapping["Tab"] = {KeyTab};
		m_KeyboardMapping["Return"] = {KeyBackspace};
	}

	bool InputManager::GetKey(const std::string& key) {
		for (const auto& code : m_KeyboardMapping[key]) {
			if (m_Keys[code].Pressed) {
				return true;
			}
		}
		return false;
	}

	bool InputManager::IsPressed(KeyCode code) {
		return m_Keys[code].Pressed;
	}

	float InputManager::GetAxis(const std::string& axisName) {
		return m_Axis[axisName];
	}

}