//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "mono_compiler.h"

#include <fstream>

namespace fs = std::filesystem;

static std::pair<int, std::string> ExecuteAndGetOutput(const std::string& cmd,
                                                       const fs::path& tmp) {
  int rc = std::system(cmd.c_str());
  std::ifstream in(tmp, std::ios::binary);
  std::string out((std::istreambuf_iterator<char>(in)), {});
  std::error_code ec;
  fs::remove(tmp, ec);
  return {rc, out};
}

DotNetProject::DotNetProject(const std::string& assembly_name)
    : assembly_name_(assembly_name) {}

void DotNetProject::SetOutputPath(const std::string& path) {
  output_path_ = path;
  dirty_ = true;
}

void DotNetProject::SetTargetFramework(const std::string& framework) {
  target_framework_ = framework;
  dirty_ = true;
}

void DotNetProject::SetGenerateDocs(bool enable) {
  generate_docs_ = enable;
  dirty_ = true;
}

void DotNetProject::SetAllowUnsafe(bool enable) {
  allow_unsafe_ = enable;
  dirty_ = true;
}

void DotNetProject::SetLangVersion(const std::string& version) {
  lang_version_ = version;
  dirty_ = true;
}

void DotNetProject::AddSource(const std::string& path) {
  sources_.push_back(path);
  dirty_ = true;
}

void DotNetProject::SetSources(const std::vector<std::string>& paths) {
  sources_ = paths;
  dirty_ = true;
}

void DotNetProject::ClearSources() {
  sources_.clear();
  dirty_ = true;
}

void DotNetProject::AddReference(const std::string& dll_path) {
  references_.push_back(dll_path);
  dirty_ = true;
}

void DotNetProject::SetReferences(const std::vector<std::string>& paths) {
  references_ = paths;
  dirty_ = true;
}

void DotNetProject::ClearReferences() {
  references_.clear();
  dirty_ = true;
}

fs::path DotNetProject::Save() {
  fs::path output = fs::absolute(output_path_);
  fs::path output_dir = output.parent_path();

  if (!output_dir.empty() && !fs::exists(output_dir)) {
    fs::create_directories(output_dir);
  }

  csproj_path_ = output_dir / (assembly_name_ + ".csproj");

  std::ofstream f(csproj_path_);
  f << "<Project Sdk=\"Microsoft.NET.Sdk\">\n";
  f << "  <PropertyGroup>\n";
  f << "    <TargetFramework>" << target_framework_ << "</TargetFramework>\n";
  f << "    <AssemblyName>" << assembly_name_ << "</AssemblyName>\n";
  f << "    <OutputType>Library</OutputType>\n";
  f << "    <OutputPath>" << output_dir.generic_string() << "</OutputPath>\n";
  f << "    <AppendTargetFrameworkToOutputPath>false"
       "</AppendTargetFrameworkToOutputPath>\n";
  f << "    <AppendRuntimeIdentifierToOutputPath>false"
       "</AppendRuntimeIdentifierToOutputPath>\n";
  f << "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n";
  f << "    <LangVersion>" << lang_version_ << "</LangVersion>\n";
  if (allow_unsafe_) {
    f << "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n";
  }
  if (generate_docs_) {
    f << "    <GenerateDocumentationFile>true</GenerateDocumentationFile>\n";
  }
  f << "  </PropertyGroup>\n";

  // Source files
  f << "  <ItemGroup>\n";
  for (const auto& src : sources_) {
    f << "    <Compile Include=\"" << fs::absolute(src).generic_string()
      << "\" />\n";
  }
  f << "  </ItemGroup>\n";

  // Reference DLLs
  if (!references_.empty()) {
    f << "  <ItemGroup>\n";
    for (const auto& lib : references_) {
      fs::path lib_path = fs::absolute(lib);
      f << "    <Reference Include=\"" << lib_path.stem().generic_string()
        << "\">\n";
      f << "      <HintPath>" << lib_path.generic_string() << "</HintPath>\n";
      f << "      <Private>false</Private>\n";
      f << "    </Reference>\n";
    }
    f << "  </ItemGroup>\n";
  }

  f << "</Project>\n";
  f.close();

  dirty_ = false;
  return csproj_path_;
}

CompileResult DotNetProject::Build(bool debug) {
  if (dirty_ || csproj_path_.empty()) {
    Save();
  }

  std::string configuration = debug ? "Debug" : "Release";
  std::string command = "dotnet build \"" + csproj_path_.string() + "\" -c " +
                        configuration + " --nologo -v quiet";

  auto tmp = fs::temp_directory_path() / "wiesel_cmd_out.txt";
  std::string full_command = command + " > \"" + tmp.string() + "\" 2>&1";

#ifdef WIN32
  full_command = "\"" + full_command + "\"";
#endif

  auto [exit_code, output] = ExecuteAndGetOutput(full_command, tmp);
  return {exit_code == 0, exit_code, output, command};
}