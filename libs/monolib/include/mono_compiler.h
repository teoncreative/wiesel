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