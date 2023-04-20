
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

#include "behavior/w_behavior.hpp"

using namespace Wiesel;

namespace WieselDemo {
	class DemoBehavior : public IBehavior {
	public:
		DemoBehavior(Entity entity) : IBehavior("DemoBehavior", entity) { }
		~DemoBehavior();

		void OnUpdate(float_t deltaTime);
	};
}
