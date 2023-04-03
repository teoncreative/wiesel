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
#include "events/w_events.h"

namespace Wiesel {
	class Object {
	public:
		Object();
		Object(const glm::vec3& position, const glm::quat& orientation);
		Object(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& scale);
		virtual ~Object();

		WIESEL_GETTER_FN const glm::vec3& GetPosition();
		WIESEL_GETTER_FN const glm::quat& GetOrientation();
		WIESEL_GETTER_FN const glm::mat4& GetLocalView();

		virtual void Move(float x, float y, float z);
		virtual void Move(const glm::vec3& move);

		virtual void Rotate(float radians, float ax, float ay, float az);
		virtual void SetRotation(float pitch, float yaw, float roll);
		virtual void SetOrientation(glm::quat orientation);
		virtual void SetLocalView(glm::mat4 localView);

		virtual void SetScale(float x, float y, float z);

		// Move these to our own Vector3 implementation
		glm::vec3 GetForward();
		glm::vec3 GetBackward();
		glm::vec3 GetLeft();
		glm::vec3 GetRight();
		glm::vec3 GetUp();
		glm::vec3 GetDown();

		WIESEL_GETTER_FN uint64_t GetObjectId();

		void SetObjectId(uint64_t id);

		virtual void OnEvent(Event& event);
	protected:
		static uint64_t s_ObjectCounter;
		virtual void UpdateView();

		uint64_t m_ObjectId;
		glm::vec3 m_Position;
		glm::quat m_Orientation;
		glm::vec3 m_Scale;
		glm::mat4 m_LocalView;
		glm::mat3 m_NormalMatrix;
	};
}
