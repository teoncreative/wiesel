//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include "mono_compiler.h"
#include "wpak/wpak.h"

namespace fs = std::filesystem;

// Returns the number of pak files written, or -1 on error.
int PackAssets(const fs::path& input_dir, const fs::path& output_dir,
               const std::string& prefix, int start_index = 1) {
  auto files_result = Wpak::CollectFiles(input_dir);
  if (!files_result) {
    std::cerr << "Error: " << Wpak::ErrorToString(files_result.error())
              << "\n";
    return -1;
  }

  std::cout << "Packing " << files_result->size() << " files with prefix '"
            << prefix << "'...\n";

  auto result = Wpak::WriteArchive(
      output_dir, *files_result, prefix, start_index,
      [](size_t i, size_t total, const std::string& name) {
        std::cout << "  [" << (i + 1) << "/" << total << "] " << name << "\n";
      });

  if (!result) {
    std::cerr << "Error: " << Wpak::ErrorToString(result.error()) << "\n";
    return -1;
  }

  for (const auto& path : *result) {
    std::cout << "Archive created: " << path << " (" << fs::file_size(path)
              << " bytes)\n";
  }
  return static_cast<int>(result->size());
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
  DotNetProject project(output_dll.stem().string());
  project.SetOutputPath(output_dll.string());
  project.SetSources(source_files);
  auto result = project.Build();
  if (!result.success) {
    std::cerr << "Compilation failed (exit code " << result.exit_code << "):\n"
              << result.output << "\n";
  }
  return result.success;
}

int main(int argc, char** argv) {
  cxxopts::Options options("assetpacker", "Wiesel Engine Asset Packer");

  options.add_options()("command", "Command to run (pack, compile, bundle)",
                        cxxopts::value<std::string>())(
      "args", "Positional arguments",
      cxxopts::value<std::vector<std::string>>())(
      "prefix", "VFS prefix for pack entries (e.g. engine://)",
      cxxopts::value<std::string>())("h,help", "Print usage");

  options.parse_positional({"command", "args"});
  options.positional_help("<command> [arguments...]");

  try {
    auto result = options.parse(argc, argv);

    if (result.count("help") || !result.count("command")) {
      std::cout << options.help() << "\n"
                << "Commands:\n"
                << "  pack <input_dir> <output_dir> --prefix <scheme>\n"
                << "  compile <scripts_dir> <output.dll>\n"
                << "  bundle <engine_assets> <editor_assets> <output_dir>\n";
      return result.count("help") ? 0 : 1;
    }

    std::string command = result["command"].as<std::string>();
    std::vector<std::string> args;
    if (result.count("args")) {
      args = result["args"].as<std::vector<std::string>>();
    }

    if (command == "pack") {
      if (args.size() < 2) {
        std::cerr << "Usage: assetpacker pack <input_dir> <output_dir>"
                  << " --prefix <scheme>\n";
        return 1;
      }
      std::string prefix;
      if (result.count("prefix")) {
        prefix = result["prefix"].as<std::string>();
      }
      if (prefix.empty()) {
        std::cerr << "Error: --prefix is required (e.g. --prefix engine://)\n";
        return 1;
      }
      if (PackAssets(args[0], args[1], prefix) < 0) {
        return 1;
      }
    } else if (command == "compile") {
      if (args.size() < 2) {
        std::cerr << "Usage: assetpacker compile <scripts_dir> <output.dll>\n";
        return 1;
      }
      if (!CompileScripts(args[0], args[1])) {
        return 1;
      }
    } else if (command == "bundle") {
      if (args.size() < 3) {
        std::cerr << "Usage: assetpacker bundle <engine_assets>"
                  << " <editor_assets> <output_dir>\n";
        return 1;
      }
      fs::path engine_assets = args[0];
      fs::path editor_assets = args[1];
      fs::path output_dir = args[2];

      fs::create_directories(output_dir);

      int engine_count = PackAssets(engine_assets, output_dir, "engine://");
      if (engine_count < 0) {
        return 1;
      }
      if (PackAssets(editor_assets, output_dir, "editor://", 1 + engine_count) <
          0) {
        return 1;
      }

      fs::path scripts_dir = engine_assets / "scripts";
      if (fs::exists(scripts_dir)) {
        if (!CompileScripts(scripts_dir, output_dir / "Core.dll")) {
          return 1;
        }
      }
    } else {
      std::cerr << "Unknown command: " << command << "\n";
      std::cout << options.help() << "\n";
      return 1;
    }
  } catch (const cxxopts::exceptions::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
