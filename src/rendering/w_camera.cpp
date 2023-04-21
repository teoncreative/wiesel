
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_camera.hpp"

namespace Wiesel {

	void Camera::UpdateProjection() {
		m_Projection = glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearPlane, m_FarPlane);
		m_Projection[1][1] *= -1; // glm is originally designed for OpenGL, which Y coords where flipped
	}

	void Camera::UpdateView(glm::vec3& position, glm::vec3& rotation) {
		glm::mat4 rotationMatrix = glm::toMat4(glm::quat(rotation));
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), position) * rotationMatrix;
	}

}




