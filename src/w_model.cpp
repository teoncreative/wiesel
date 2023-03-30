//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_model.h"

namespace Wiesel {

	Model::Model(const glm::vec3& position, const glm::quat& orientation) : Object(position, orientation) {

	}

	Model::~Model() {

	}
}