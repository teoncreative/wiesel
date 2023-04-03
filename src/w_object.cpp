//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_object.h"

namespace Wiesel {
	uint64_t Object::s_ObjectCounter = 0;

	Object::Object() : Object(glm::vec3(0.0f, 0.0f, 0.0f), glm::angleAxis(0.0f, glm::vec3(0.0f, 0.0f, 0.0f))) {
	}

	Object::Object(const glm::vec3& position, const glm::quat& orientation) : m_Position(position), m_Orientation(orientation) {
		m_Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		m_ObjectId = s_ObjectCounter++;
		UpdateView();
	}

	Object::Object(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& scale) : m_Position(position), m_Orientation(orientation), m_Scale(scale) {
		m_Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		m_ObjectId = s_ObjectCounter++;
		UpdateView();
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

	void Object::SetRotation(float pitch, float yaw, float roll) {
		m_Orientation = glm::quat(glm::vec3(pitch, yaw, roll));
		UpdateView();
	}

	void Object::SetOrientation(glm::quat orientation) {
		m_Orientation = orientation;
		UpdateView();
	}

	void Object::SetLocalView(glm::mat4 localView) {
		m_LocalView = localView;
	}

	void Object::SetScale(float x, float y, float z) {
		m_Scale = glm::vec3(x, y, z);
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
		m_LocalView = glm::scale(m_LocalView, m_Scale);
		m_NormalMatrix = glm::inverseTranspose(glm::mat3(m_LocalView));
	}

	const glm::mat4& Object::GetLocalView() {
		return m_LocalView;
	}

	glm::vec3 Object::GetForward() {
		return -m_LocalView[2];
	}

	glm::vec3 Object::GetBackward() {
		return m_LocalView[2];
	}

	glm::vec3 Object::GetLeft() {
		return -m_LocalView[0];
	}

	glm::vec3 Object::GetRight() {
		return m_LocalView[0];
	}

	glm::vec3 Object::GetUp() {
		return m_LocalView[1];
	}

	glm::vec3 Object::GetDown() {
		return -m_LocalView[1];
	}

	uint64_t Object::GetObjectId() {
		return m_ObjectId;
	}

	void Object::SetObjectId(uint64_t id) {
		m_ObjectId = id;
	}

	void Object::OnEvent(Event& event) {

	}

}
