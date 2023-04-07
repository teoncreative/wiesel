
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.h"
#include "util/w_color.h"

namespace Wiesel {
	enum LightType {
		LightTypeDirect,
		LightTypePoint
	};

	struct LightsPushConstant {

	};
	/*
	struct LightDirect {
	public:
		Light(LightType type, float intensity, Colorf color);
		~Light();

		LightType LightType;
		float LightIntensity;
		Colorf LightColor;

	};

	class LightPoint {
	public:
		PointLight(LightType type, float intensity, Colorf color, float constant, float linear, float quadratic);
		~PointLight();

		WIESEL_GETTER_FN float GetLightConstant();
		WIESEL_GETTER_FN float GetLightLinear();
		WIESEL_GETTER_FN float GetLightQuadratic();

		void SetLightConstant(float constant);
		void SetLightLinear(float linear);
		void SetLightQuadratic(float quadratic);

	private:
		float m_LightConstant;
		float m_LightLinear;
		float m_LightQuadratic;

	};
*/
}