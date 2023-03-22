//
// Created by Metehan Gezer on 22.03.2023.
//
#include "w_keymanager.h"

KeyData::KeyData() {
	this->pressed = false;
}

KeyData::KeyData(bool pressed) {
	this->pressed = pressed;
}


WieselKeyManager::WieselKeyManager() {

}

WieselKeyManager::~WieselKeyManager() {

}

void WieselKeyManager::set(int key, bool pressed) {
	keys[key].pressed = pressed;
}

bool WieselKeyManager::isPressed(int key) {
	return keys[key].pressed;
}
