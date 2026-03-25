//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 12.02.2026.
//

#include <filesystem>
#include <iostream>
#include "mono_compiler.h"
#include "wpak/wpak.h"

namespace fs = std::filesystem;

bool PackAssets(const fs::path& input_dir, const fs::path& output_pak) {
  auto files_result = Wpak::CollectFiles(input_dir);
  if (!files_result) {
    std::cerr << "Error: " << files_result.error.message << "\n";
    return false;
  }

  std::cout << "Packing " << files_result.value.size() << " files...\n";

  auto status = Wpak::WriteArchive(
      output_pak, files_result.value,
      [](size_t i, size_t total, const std::string& name) {
        std::cout << "  [" << (i + 1) << "/" << total << "] " << name << "\n";
      });

  if (!status) {
    std::cerr << "Error: " << status.error.message << "\n";
    return false;
  }

  std::cout << "Archive created: " << output_pak << "\n";
  std::cout << "Total size: " << fs::file_size(output_pak) << " bytes\n";
  return true;
}

bool CompileScripts(const fs::path& scripts_dir, const fs::path& output_dll) {
  std::vector<std::string> source_files;

  if (!fs::exists(scripts_dir)) {
    std::cerr << "Scripts directory does not exist: " << scripts_dir << "\n";
    return false;
  }

  for (const auto& entry : fs::recursive_directory_iterator(scripts_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cs") {
      source_files.push_back(entry.path().string());
    }
  }

  if (source_files.empty()) {
    std::cerr << "No .cs files found in " << scripts_dir << "\n";
    return false;
  }

  std::cout << "Compiling " << source_files.size() << " C# scripts to "
            << output_dll << "\n";
  auto result = CompileToDLL(output_dll.string(), source_files);
  if (!result.success) {
    std::cerr << "Compilation failed (exit code " << result.exit_code << "):\n"
              << result.output << "\n";
  }
  return result.success;
}

void PrintUsage() {
  std::cerr << "Usage:\n"
            << "  assetpacker pack <input_dir> <output.pak>\n"
            << "  assetpacker compile <scripts_dir> <output.dll>\n"
            << "  assetpacker bundle <engine_assets_dir> <editor_assets_dir>"
            << " <output_dir>\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  std::string command = argv[1];

  if (command == "pack") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    if (!PackAssets(argv[2], argv[3])) {
      return 1;
    }
  } else if (command == "compile") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    if (!CompileScripts(argv[2], argv[3])) {
      return 1;
    }
  } else if (command == "bundle") {
    if (argc != 5) {
      PrintUsage();
      return 1;
    }
    fs::path engine_assets = argv[2];
    fs::path editor_assets = argv[3];
    fs::path output_dir = argv[4];

    fs::create_directories(output_dir);

    if (!PackAssets(engine_assets, output_dir / "engine.pak")) {
      return 1;
    }
    if (!PackAssets(editor_assets, output_dir / "editor.pak")) {
      return 1;
    }

    fs::path scripts_dir = engine_assets / "scripts";
    if (fs::exists(scripts_dir)) {
      if (!CompileScripts(scripts_dir, output_dir / "Core.dll")) {
        return 1;
      }
    }
  } else {
    // Legacy mode: assetpacker <input_dir> <output.pak>
    if (argc == 3) {
      if (!PackAssets(argv[1], argv[2])) {
        return 1;
      }
    } else {
      PrintUsage();
      return 1;
    }
  }

  return 0;
}