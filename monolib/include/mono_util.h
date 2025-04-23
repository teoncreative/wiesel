#pragma once

#include <vector>
#include <mono/jit/jit.h>
#include <string>

bool CompileToDLL(const std::string& outputFile,
                  const std::vector<std::string>& sourceFiles,
                  const std::string& libDir = "",
                  const std::vector<std::string>& linkLibs = {},
                  bool debug = false);

class MonoStringWrapper {
 public:
  MonoStringWrapper(const char* string, int length) : string_(string), length_(length) {}
  ~MonoStringWrapper() {}

  void Release() {
    mono_free((void*) string_);
  }

  operator const char*() { return string_; }

  int length() const { return length_; }

 private:
  const char* string_;
  int length_;
};

class ScopedMonoStringWrapper {
 public:
  ScopedMonoStringWrapper(MonoStringWrapper string) : string_(string) {}

  ~ScopedMonoStringWrapper() {
    string_.Release();
  }

  operator const char*() { return string_; }

  int length() const { return string_.length(); }

 private:
  MonoStringWrapper string_;
};

class MonoObjectWrapper {
 public:
  MonoObjectWrapper(MonoObject* object) : object_(object) {}
  ~MonoObjectWrapper() { }

  operator bool() { return object_ != nullptr; }

  std::string AsString() {
    MonoString* resultString = mono_object_to_string(object_, nullptr);
    const char* str = mono_string_to_utf8(resultString);
    std::string result(str);
    return result;
  }

  MonoStringWrapper AsWrappedString() {
    MonoString* resultString = mono_object_to_string(object_, nullptr);
    return {mono_string_to_utf8(resultString), mono_string_length(resultString)};
  }

 private:
  MonoObject* object_;

};
