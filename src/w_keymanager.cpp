//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_keymanager.h"

namespace Wiesel {
	KeyData::KeyData() {
		this->Pressed = false;
	}

	KeyData::KeyData(bool pressed) {
		this->Pressed = pressed;
	}

	KeyManager::KeyManager() {

	}

	KeyManager::~KeyManager() {

	}

	void KeyManager::Set(int key, bool pressed) {
		m_Keys[key].Pressed = pressed;
	}

	bool KeyManager::IsPressed(int key) {
		return m_Keys[key].Pressed;
	}
}
