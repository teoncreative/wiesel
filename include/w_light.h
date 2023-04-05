
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
/*
	// todo make these components
	class Light : public Object {
	public:
		Light(LightType type, float intensity, Colorf color);
		~Light();

		WIESEL_GETTER_FN LightType GetLightType();
		WIESEL_GETTER_FN Colorf& GetLightColor();
		WIESEL_GETTER_FN float GetLightIntensity();

		void SetLightIntensity(float intensity);
		void SetLightColor(const Colorf& color);

	private:
		LightType m_LightType;
		float m_LightIntensity;
		Colorf m_LightColor;

	};

	class PointLight : public Light {
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