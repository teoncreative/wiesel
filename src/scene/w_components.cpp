//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_components.h"

namespace Wiesel {
	glm::vec3 TransformComponent::GetForward() {
		return -LocalView[2];
	}

	glm::vec3 TransformComponent::GetBackward() {
		return LocalView[2];
	}

	glm::vec3 TransformComponent::GetLeft() {
		return -LocalView[0];
	}

	glm::vec3 TransformComponent::GetRight() {
		return LocalView[0];
	}

	glm::vec3 TransformComponent::GetUp() {
		return LocalView[1];
	}

	glm::vec3 TransformComponent::GetDown() {
		return -LocalView[1];
	}

	void TransformComponent::UpdateRenderData() {
		if (!IsChanged) {
			return;
		}
		glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
		LocalView = glm::translate(glm::mat4(1.0f), Position)
			   * rotation
			   * glm::scale(glm::mat4(1.0f), Scale);

		NormalMatrix = glm::inverseTranspose(glm::mat3(LocalView));
		IsChanged = false;
	}
}