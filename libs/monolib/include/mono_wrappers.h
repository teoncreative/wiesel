//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <mono/jit/jit.h>
#include <string>
#include <vector>

#include "mono_compiler.h"

class MonoStringWrapper {
 public:
  MonoStringWrapper(const char* string, int length)
      : string_(string), length_(length) {}

  ~MonoStringWrapper() {}

  void Release() { mono_free((void*)string_); }

  operator const char*() { return string_; }

  int length() const { return length_; }

 private:
  const char* string_;
  int length_;
};

class ScopedMonoStringWrapper {
 public:
  ScopedMonoStringWrapper(MonoStringWrapper string) : string_(string) {}

  ~ScopedMonoStringWrapper() { string_.Release(); }

  operator const char*() { return string_; }

  int length() const { return string_.length(); }

 private:
  MonoStringWrapper string_;
};

class MonoObjectWrapper {
 public:
  MonoObjectWrapper(MonoObject* object) : object_(object) {}

  ~MonoObjectWrapper() {}

  operator bool() { return object_ != nullptr; }

  std::string AsString() {
    MonoString* result_string = mono_object_to_string(object_, nullptr);
    const char* str = mono_string_to_utf8(result_string);
    std::string result(str);
    return result;
  }

  MonoStringWrapper AsWrappedString() {
    MonoString* resultString = mono_object_to_string(object_, nullptr);
    return {mono_string_to_utf8(resultString),
            mono_string_length(resultString)};
  }

 private:
  MonoObject* object_;
};
