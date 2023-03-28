//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"
#include "util/w_utils.h"
#include "util/w_logger.h"

namespace Wiesel {
	class Object {
	public:
		Object(const glm::vec3& position, const glm::quat& orientation);
		virtual ~Object();

		WIESEL_GETTER_FN const glm::vec3& GetPosition();
		WIESEL_GETTER_FN const glm::quat& GetOrientation();
		WIESEL_GETTER_FN const glm::mat4& GetLocalView();

		virtual void Move(float x, float y, float z);
		virtual void Move(const glm::vec3& move);

		virtual void Rotate(float radians, float ax, float ay, float az);
	protected:
		virtual void UpdateView();

		glm::vec3 m_Position;
		glm::quat m_Orientation;
		glm::mat4 m_LocalView;
	};
}
