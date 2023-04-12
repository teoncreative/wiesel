//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_camera.h"

namespace Wiesel {
	Camera::Camera(const glm::vec3& position, const glm::vec3& rotation, float aspectRatio, float fieldOfView, float nearPlane, float farPlane) : m_Position(position), m_Rotation(rotation), m_AspectRatio(aspectRatio), m_FieldOfView(fieldOfView), m_NearPlane(nearPlane), m_FarPlane(farPlane) {
		UpdateView();
		UpdateProjection();
	}

	Camera::~Camera() {
	}

	float Camera::GetFieldOfView() const {
		return m_FieldOfView;
	}

	const glm::vec3& Camera::GetPosition() {
		return m_Position;
	}

	const glm::vec3& Camera::GetRotation() {
		return m_Rotation;
	}

	const glm::mat4& Camera::GetView() {
		return m_View;
	}

	const glm::mat4& Camera::GetProjection() {
		return m_Projection;
	}

	void Camera::Move(float x, float y, float z) {
		m_Position += glm::vec3(x, y, z);
		UpdateView();
	}

	void Camera::Move(const glm::vec3& move) {
		m_Position += move;
		UpdateView();
	}

	void Camera::SetRotation(float pitch, float yaw, float roll) {
		m_Rotation.x = pitch;
		m_Rotation.y = yaw;
		m_Rotation.z = roll;
		UpdateView();
	}

	void Camera::SetRotation(glm::vec3 rotation) {
		m_Rotation = rotation;
		UpdateView();
	}

	glm::vec3 Camera::GetForward() {
		return -m_View[2];
	}

	glm::vec3 Camera::GetBackward() {
		return m_View[2];
	}

	glm::vec3 Camera::GetLeft() {
		return -m_View[0];
	}

	glm::vec3 Camera::GetRight() {
		return m_View[0];
	}

	glm::vec3 Camera::GetUp() {
		return m_View[1];
	}

	glm::vec3 Camera::GetDown() {
		return -m_View[1];
	}

	void Camera::OnEvent(Event& event) {
		EventDispatcher dispatcher(event);

		dispatcher.Dispatch<AppRecreateSwapChainsEvent>(WIESEL_BIND_EVENT_FUNCTION(OnRecreateSwapChains));
	}

	bool Camera::OnRecreateSwapChains(AppRecreateSwapChainsEvent& event) {
		m_AspectRatio = event.GetAspectRatio();
		UpdateProjection();
		return false;
	}

	void Camera::SetId(uint32_t id) {
		m_Id = id;
	}

	uint32_t Camera::GetId() const {
		return m_Id;
	}

	void Camera::UpdateView() {
		glm::mat4 rotation = glm::toMat4(glm::quat(m_Rotation));
		m_View = glm::translate(glm::mat4(1.0f), m_Position) * rotation;
	}

	void Camera::UpdateProjection() {
		m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearPlane, m_FarPlane);
		m_Projection[1][1] *= -1; // glm is originally designed for OpenGL, which Y coords where flipped
	}
}




