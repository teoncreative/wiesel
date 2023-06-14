
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

#include "w_pch.hpp"

namespace Wiesel {

  // todo use proper implementation
  class UUID {
  public:
    UUID();
    explicit UUID(uint64_t uuid);
    UUID(const UUID&) = default;

    explicit operator uint64_t() const { return m_UUID; }
    bool operator==(const UUID& other) const { return other.m_UUID == m_UUID; }

  private:
    uint64_t m_UUID;
  };

}
namespace std {
  template<>
  struct hash<Wiesel::UUID> {
    std::size_t operator()(const Wiesel::UUID& k) const {
      return (uint64_t) k;
    }
  };
}// namespace std