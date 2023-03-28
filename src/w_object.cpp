//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_object.h"

namespace Wiesel {
	Object::Object(const glm::vec3& position, const glm::quat& orientation) : m_Position(position), m_Orientation(orientation) {

	}

	Object::~Object() {

	}

	const glm::vec3& Object::GetPosition() {
		return m_Position;
	}

	const glm::quat& Object::GetOrientation() {
		return m_Orientation;
	}

	void Object::Rotate(float radians, float ax, float ay, float az) {
		m_Orientation *= glm::angleAxis(radians, glm::vec3(ax, ay, az));
		UpdateView();
	}

	void Object::Move(float x, float y, float z) {
		m_Position += glm::vec3(x, y, z);
		UpdateView();
	}

	void Object::Move(const glm::vec3& move) {
		m_Position += move;
		UpdateView();
	}

	void Object::UpdateView() {
		m_LocalView = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4((const glm::quat&) m_Orientation);
	}

	const glm::mat4& Object::GetLocalView() {
		return m_LocalView;
	}

}
