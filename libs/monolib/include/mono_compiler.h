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

#include <filesystem>
#include <string>
#include <vector>

struct CompileResult {
  bool success;
  int exit_code;
  std::string output;
  std::string command;
};

class DotNetProject {
 public:
  explicit DotNetProject(const std::string& assembly_name);

  void SetOutputPath(const std::string& path);
  void SetTargetFramework(const std::string& framework);
  void SetGenerateDocs(bool enable);
  void SetAllowUnsafe(bool enable);
  void SetLangVersion(const std::string& version);

  void AddSource(const std::string& path);
  void SetSources(const std::vector<std::string>& paths);
  void ClearSources();

  void AddReference(const std::string& dll_path);
  void SetReferences(const std::vector<std::string>& paths);
  void ClearReferences();

  // Write the .csproj file to disk. Returns the path to the generated file.
  std::filesystem::path Save();

  // Build the project. Calls Save() first if needed.
  CompileResult Build(bool debug = false);

  const std::string& GetAssemblyName() const { return assembly_name_; }

  const std::filesystem::path& GetCsprojPath() const { return csproj_path_; }

 private:
  std::string assembly_name_;
  std::string output_path_;
  std::string target_framework_ = "netstandard2.1";
  std::string lang_version_ = "latest";
  bool generate_docs_ = false;
  bool allow_unsafe_ = true;
  bool dirty_ = true;

  std::vector<std::string> sources_;
  std::vector<std::string> references_;

  std::filesystem::path csproj_path_;
};