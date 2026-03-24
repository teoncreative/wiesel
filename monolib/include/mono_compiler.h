#pragma once

#include <string>
#include <vector>

struct CompileResult {
  bool success;
  int exit_code;
  std::string output;
  std::string command;
};

CompileResult CompileToDLL(const std::string& output_file,
                           const std::vector<std::string>& source_files,
                           const std::string& lib_dir = "",
                           const std::vector<std::string>& link_libs = {},
                           bool debug = false);