
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

#include <glm/glm.hpp>

#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {
// todo
class Vector3 {
 public:
  Vector3();
  Vector3(Ref<glm::vec3> backingVector);
  ~Vector3();

  bool operator<(const Vector3& rhs) const {
    return m_BackingVector < rhs.m_BackingVector;
  }

  bool operator>(const Vector3& rhs) const {
    return m_BackingVector > rhs.m_BackingVector;
  }

  bool operator<=(const Vector3& rhs) const {
    return m_BackingVector <= rhs.m_BackingVector;
  }

  bool operator>=(const Vector3& rhs) const {
    return m_BackingVector >= rhs.m_BackingVector;
  }

  bool operator==(const Vector3& rhs) const {
    return m_BackingVector == rhs.m_BackingVector;
  }

  bool operator!=(const Vector3& rhs) const {
    return m_BackingVector != rhs.m_BackingVector;
  }

  Vector3 operator+(const Vector3& rhs) const {
    return {CreateReference<glm::vec3>((*m_BackingVector) +
                                       (*rhs.m_BackingVector))};
  }

  Vector3 operator-(const Vector3& rhs) const {
    return {CreateReference<glm::vec3>((*m_BackingVector) -
                                       (*rhs.m_BackingVector))};
  }

  Vector3 operator/(const Vector3& rhs) const {
    return {CreateReference<glm::vec3>((*m_BackingVector) /
                                       (*rhs.m_BackingVector))};
  }

  Vector3 operator*(const Vector3& rhs) const {
    return {CreateReference<glm::vec3>((*m_BackingVector) *
                                       (*rhs.m_BackingVector))};
  }

  operator const glm::vec3&() {  // NOLINT(google-explicit-constructor)
    return *m_BackingVector;
  }

  operator Ref<glm::vec3>() {  // NOLINT(google-explicit-constructor)
    return m_BackingVector;
  }

 private:
  Ref<glm::vec3> m_BackingVector;
};
}  // namespace Wiesel