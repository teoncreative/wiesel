
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
#include "util/w_imguiutil.h"
#include "scene/w_entity.h"
#include "scene/w_components.h"

namespace Wiesel {
	template<class T>
	void RenderComponent(T& component, Entity entity);

}
