//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_camera.h"
#include "w_renderer.h"

namespace Wiesel {
	Camera::Camera(const glm::vec3& position, const glm::quat& orientation, float aspectRatio, float fieldOfView, float nearPlane, float farPlane) : Object(position, orientation), m_AspectRatio(aspectRatio), m_FieldOfView(fieldOfView), m_NearPlane(nearPlane), m_FarPlane(farPlane) {
		UpdateProjection();
	}

	Camera::~Camera() {
	}

	const glm::mat4& Camera::GetProjection() {
		return m_Projection;
	}

	float Camera::GetFieldOfView() const {
		return m_FieldOfView;
	}

	void Camera::UpdateProjection() {
		m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearPlane, m_FarPlane);
		m_Projection[1][1] *= -1; // glm is originally designed for OpenGL, which Y coords where flipped
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
}




