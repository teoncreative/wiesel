//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "generator.h"
#include "parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void PrintUsage(const char* program) {
  std::cerr << "Usage: " << program
            << " -o <output_dir> -r <include_root> <header_file>...\n"
            << "\n"
            << "Options:\n"
            << "  -o <dir>   Output directory for generated files\n"
            << "  -r <dir>   Include root (generated #include paths are "
               "relative to this)\n"
            << "  -h         Show this help\n";
}

int main(int argc, char* argv[]) {
  std::string output_dir;
  std::string include_root;
  std::vector<std::string> header_files;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) {
      output_dir = argv[++i];
    } else if (arg == "-r" && i + 1 < argc) {
      include_root = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    } else if (arg[0] != '-') {
      header_files.push_back(arg);
    } else {
      std::cerr << "error: unknown option: " << arg << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  if (output_dir.empty()) {
    std::cerr << "error: -o <output_dir> is required\n";
    PrintUsage(argv[0]);
    return 1;
  }

  if (header_files.empty()) {
    std::cerr << "error: no header files specified\n";
    PrintUsage(argv[0]);
    return 1;
  }

  fs::create_directories(output_dir);

  std::vector<std::string> all_generated_includes;
  std::vector<std::string> all_function_names;

  for (const auto& header : header_files) {
    std::cout << "codegen: parsing " << header << "\n";

    auto parse_result = wiesel::code_gen::ParseHeader(header);

    if (parse_result.classes.empty()) {
      std::cout << "  no reflected classes found, skipping\n";
      continue;
    }

    for (const auto& cls : parse_result.classes) {
      std::cout << "  found " << cls.qualified_name << " with "
                << cls.fields.size() << " reflected fields\n";
    }

    fs::path header_path(header);
    std::string stem = header_path.stem().string();
    std::string generated_filename = stem + ".generated.h";
    fs::path generated_path = fs::path(output_dir) / generated_filename;

    // Compute #include path relative to include root
    std::string include_path = header_path.filename().string();
    if (!include_root.empty()) {
      auto rel = fs::relative(header_path, fs::path(include_root));
      if (!rel.empty() && rel.string().find("..") == std::string::npos) {
        include_path = rel.string();
        std::replace(include_path.begin(), include_path.end(), '\\', '/');
      }
    }

    std::string generated_code =
        wiesel::code_gen::GenerateReflection(parse_result, include_path);

    std::ofstream out_file(generated_path);
    if (!out_file) {
      std::cerr << "error: cannot write " << generated_path << "\n";
      return 1;
    }
    out_file << generated_code;
    out_file.close();

    std::cout << "  wrote " << generated_path.string() << "\n";

    all_generated_includes.push_back(generated_filename);
    for (const auto& cls : parse_result.classes) {
      all_function_names.push_back("Reflect" + cls.short_name);
    }
  }

  if (!all_function_names.empty()) {
    std::string reflect_all = wiesel::code_gen::GenerateReflectAll(
        all_generated_includes, all_function_names);

    fs::path reflect_all_path =
        fs::path(output_dir) / "w_reflect_all.generated.h";
    std::ofstream out_file(reflect_all_path);
    if (!out_file) {
      std::cerr << "error: cannot write " << reflect_all_path << "\n";
      return 1;
    }
    out_file << reflect_all;
    out_file.close();

    std::cout << "codegen: wrote " << reflect_all_path.string() << "\n";
  }

  return 0;
}
