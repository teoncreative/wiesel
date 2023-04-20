//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_cube.hpp"

using namespace Wiesel;

namespace WieselDemo {

	DemoBehavior::~DemoBehavior() {

	}

	void DemoBehavior::OnUpdate(float_t deltaTime) {
		auto& transform = GetComponent<TransformComponent>();
		// do something with the transform
		transform.Move(0.0f, -1.0f * deltaTime, 0.0f);
	}
}
