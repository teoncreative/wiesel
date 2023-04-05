//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#ifndef WIESEL_W_CAMERA_H
#define WIESEL_W_CAMERA_H

#include "w_pch.h"
#include "events/w_appevents.h"
#include "util/w_uuid.h"

namespace Wiesel {
	class Camera {
	public:
		Camera(const glm::vec3& position, const glm::vec3& rotation, float aspectRatio, float fieldOfView = 45, float nearPlane = 0.1f, float farPlane = 1000.0f);
		~Camera();

		WIESEL_GETTER_FN float GetFieldOfView() const;
		WIESEL_GETTER_FN const glm::vec3& GetPosition();
		WIESEL_GETTER_FN const glm::vec3& GetRotation();
		WIESEL_GETTER_FN const glm::mat4& GetView();
		WIESEL_GETTER_FN const glm::mat4& GetProjection();

		void Move(float x, float y, float z);
		void Move(const glm::vec3& move);

		void SetRotation(float pitch, float yaw, float roll);
		void SetRotation(glm::vec3 rotation);
		void SetLocalView(glm::mat4 localView);
		// todo set functions for field of view and near/far planes

		// Move these to our own Vector3 implementation
		glm::vec3 GetForward();
		glm::vec3 GetBackward();
		glm::vec3 GetLeft();
		glm::vec3 GetRight();
		glm::vec3 GetUp();
		glm::vec3 GetDown();

		void OnEvent(Event& event);
		bool OnRecreateSwapChains(AppRecreateSwapChainsEvent& event);

		void SetId(uint32_t id);
		WIESEL_GETTER_FN uint32_t GetId() const;
	private:
		void UpdateProjection();
		void UpdateView();

		uint32_t m_Id;
		float m_FieldOfView;
		float m_NearPlane;
		float m_FarPlane;
		float m_AspectRatio;
		glm::vec3 m_Position;
		glm::vec3 m_Rotation;
		glm::mat4 m_View;
		glm::mat4 m_Projection;
	};

}
#endif //WIESEL_W_CAMERA_H
