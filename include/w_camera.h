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
#include "w_object.h"
#include "events/w_appevents.h"

namespace Wiesel {
	class Camera : public Object {
	public:
		Camera(const glm::vec3& position, const glm::quat& orientation, float aspectRatio, float fieldOfView = 45, float nearPlane = 0.1f, float farPlane = 1000.0f);
		~Camera();

		WIESEL_GETTER_FN const glm::mat4& GetProjection();
		WIESEL_GETTER_FN float GetFieldOfView() const;

		// todo set functions for field of view and near/far planes

		void OnEvent(Event& event) override;

		bool OnRecreateSwapChains(AppRecreateSwapChainsEvent& event);
	private:
		void UpdateProjection();

		float m_FieldOfView;
		float m_NearPlane;
		float m_FarPlane;
		float m_AspectRatio;
		glm::mat4 m_Projection;
	};

}
#endif //WIESEL_W_CAMERA_H
