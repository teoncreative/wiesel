
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
class UUID {
 public:
  using bytes = std::array<std::uint8_t, 16>;

  UUID() noexcept;
  explicit UUID(const bytes& b) noexcept;

  static UUID GenerateV4();

  static UUID FromString(std::string_view s, bool* ok = nullptr) noexcept;

  std::string ToString() const;

  bytes ToBytes() const noexcept;

  bool IsNil() const noexcept { return hi_ == 0 && lo_ == 0; }

  friend bool operator==(const UUID& a, const UUID& b) noexcept {
    return a.hi_ == b.hi_ && a.lo_ == b.lo_;
  }

  friend bool operator!=(const UUID& a, const UUID& b) noexcept {
    return !(a == b);
  }

  friend bool operator<(const UUID& a, const UUID& b) noexcept {
    return a.hi_ < b.hi_ || (a.hi_ == b.hi_ && a.lo_ < b.lo_);
  }


  std::uint64_t hi() const noexcept { return hi_; }

  std::uint64_t lo() const noexcept { return lo_; }

 private:
  std::uint64_t hi_;
  std::uint64_t lo_;

  void FromBytes(const bytes& b) noexcept;

};

}  // namespace Wiesel

// hashing support
namespace std {
template<> struct hash<Wiesel::UUID> {
  size_t operator()(const Wiesel::UUID& u) const noexcept {
    // 64-bit mix (xorshift64*)
    std::uint64_t x = u.hi() ^ (u.lo() * 0x9E3779B97F4A7C15ull);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
  }
};
}  // namespace std