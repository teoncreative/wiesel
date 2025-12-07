
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_uuid.hpp"

#include <random>
#include <unordered_map>

#include "w_pch.hpp"

namespace Wiesel {

static std::mt19937_64::result_type seed_rng_() {
  // gather multiple 32-bit entropy chunks to seed a 64-bit engine well
  std::random_device rd;
  std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
  return std::mt19937_64(seq)();  // extract a seeded state
}

static void WriteU64BE(std::uint8_t* out, std::uint64_t v) noexcept {
  for (int i = 7; i >= 0; --i) {
    out[7 - i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
  }
}

static std::uint64_t ReadU64BE(const std::uint8_t* in) noexcept {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v = v << 8 | in[i];
  }
  return v;
}

UUID::UUID() noexcept : hi_(0), lo_(0) {}

UUID::UUID(const bytes& b) noexcept {
  FromBytes(b);
}

UUID UUID::GenerateV4() {
  // use std::random_device to seed, then 64-bit engine
  static thread_local std::mt19937_64 rng{seed_rng_()};
  std::uniform_int_distribution<std::uint64_t> dist;

  UUID u;
  std::uint64_t hi = dist(rng);
  std::uint64_t lo = dist(rng);

  // set version (4) -> high 4 bits of byte 6 (zero-based)
  // set variant (10xx) -> high 2 bits of byte 8
  bytes b{};
  WriteU64BE(b.data(), hi);
  WriteU64BE(b.data() + 8, lo);

  b[6] = static_cast<std::uint8_t>((b[6] & 0x0F) | 0x40);  // version 4
  b[8] = static_cast<std::uint8_t>((b[8] & 0x3F) | 0x80);  // variant RFC 4122

  u.FromBytes(b);
  return u;
}

UUID UUID::FromString(std::string_view s, bool* ok) noexcept {
  // accepted forms: 8-4-4-4-12 with lowercase/uppercase hex
  UUID out;
  if (s.size() != 36 || s[8] != '-' || s[13] != '-' || s[18] != '-' ||
      s[23] != '-') {
    if (ok)
      *ok = false;
    return out;
  }
  bytes b{};
  auto hex = [&](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
      return 10 + (c - 'A');
    return -1;
  };
  int bi = 0;
  for (size_t i = 0; i < s.size();) {
    if (s[i] == '-') {
      ++i;
      continue;
    }
    if (i + 1 >= s.size()) {
      if (ok)
        *ok = false;
      return out;
    }
    int h = hex(s[i]), l = hex(s[i + 1]);
    if (h < 0 || l < 0) {
      if (ok)
        *ok = false;
      return out;
    }
    b[bi++] = static_cast<std::uint8_t>((h << 4) | l);
    i += 2;
  }
  if (bi != 16) {
    if (ok)
      *ok = false;
    return out;
  }
  out.FromBytes(b);
  if (ok)
    *ok = true;
  return out;
}

std::string UUID::ToString() const {
  static constexpr char kHex[] = "0123456789abcdef";
  bytes b = ToBytes();
  std::string s;
  s.resize(36);
  int j = 0;
  for (int i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      s[j++] = '-';
    s[j++] = kHex[b[i] >> 4];
    s[j++] = kHex[b[i] & 0x0F];
  }
  return s;
}

UUID::bytes UUID::ToBytes() const noexcept {
  bytes b{};
  WriteU64BE(b.data(), hi_);
  WriteU64BE(b.data() + 8, lo_);
  return b;
}

void UUID::FromBytes(const bytes& b) noexcept {
  hi_ = ReadU64BE(b.data());
  lo_ = ReadU64BE(b.data() + 8);
}

}  // namespace Wiesel