#pragma once

#include <string>
#include <vector>

bool CompileToDLL(const std::string& output_file,
                  const std::vector<std::string>& source_files,
                  const std::string& lib_dir = "",
                  const std::vector<std::string>& link_libs = {},
                  bool debug = false);