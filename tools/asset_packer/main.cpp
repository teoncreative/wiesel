//
// Created by Metehan Gezer on 12.02.2026.
//

#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include "mono_compiler.h"

namespace fs = std::filesystem;

struct FileEntry {
    std::string relative_path;
    fs::path full_path;
    uint64_t size;
};

void PackAssets(const fs::path& input_dir, const fs::path& output_pak) {
    // Collect all files
    std::vector<FileEntry> files;

    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            FileEntry fe;
            fe.full_path = entry.path();
            fe.relative_path = fs::relative(entry.path(), input_dir).generic_string();
            fe.size = fs::file_size(entry.path());
            files.push_back(fe);
        }
    }

    std::cout << "Packing " << files.size() << " files...\n";

    // Open output file
    std::ofstream pak(output_pak, std::ios::binary);

    // Write header
    pak.write("WPAK", 4);  // Magic
    uint32_t version = 1;
    pak.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write entry count
    uint32_t entry_count = static_cast<uint32_t>(files.size());
    pak.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));

    // Calculate data offset (after all metadata)
    uint64_t data_offset = 4 + sizeof(uint32_t) + sizeof(uint32_t);  // Header

    for (const auto& file : files) {
        data_offset += sizeof(uint32_t);  // Name length
        data_offset += file.relative_path.length();  // Name
        data_offset += sizeof(uint64_t) * 3;  // offset, size, compressed_size
        data_offset += sizeof(uint8_t);  // compressed flag
    }

    // Write metadata for each file
    uint64_t current_offset = data_offset;
    std::vector<uint64_t> offsets;

    for (const auto& file : files) {
        offsets.push_back(current_offset);

        // Write name length
        uint32_t name_length = static_cast<uint32_t>(file.relative_path.length());
        pak.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));

        // Write name
        pak.write(file.relative_path.c_str(), name_length);

        // Write offset
        pak.write(reinterpret_cast<const char*>(&current_offset), sizeof(current_offset));

        // Write size
        pak.write(reinterpret_cast<const char*>(&file.size), sizeof(file.size));

        // Write compressed size (same as size for now)
        pak.write(reinterpret_cast<const char*>(&file.size), sizeof(file.size));

        // Write compressed flag (false)
        uint8_t compressed = 0;
        pak.write(reinterpret_cast<const char*>(&compressed), sizeof(compressed));

        current_offset += file.size;
    }

    // Write file data
    for (size_t i = 0; i < files.size(); i++) {
        const auto& file = files[i];

        std::ifstream input(file.full_path, std::ios::binary);
        std::vector<char> buffer(file.size);
        input.read(buffer.data(), file.size);

        pak.write(buffer.data(), file.size);

        std::cout << "  [" << (i + 1) << "/" << files.size() << "] "
                  << file.relative_path << " (" << file.size << " bytes)\n";
    }

    std::cout << "Archive created: " << output_pak << "\n";
    std::cout << "Total size: " << fs::file_size(output_pak) << " bytes\n";
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

    std::cout << "Compiling " << source_files.size() << " C# scripts to " << output_dll << "\n";
    return CompileToDLL(output_dll.string(), source_files);
}

void PrintUsage() {
    std::cerr << "Usage:\n"
              << "  assetpacker pack <input_dir> <output.pak>\n"
              << "  assetpacker compile <scripts_dir> <output.dll>\n"
              << "  assetpacker bundle <engine_assets_dir> <editor_assets_dir> <output_dir>\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "pack") {
            if (argc != 4) {
                PrintUsage();
                return 1;
            }
            PackAssets(argv[2], argv[3]);
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

            // Pack assets
            PackAssets(engine_assets, output_dir / "engine.pak");
            PackAssets(editor_assets, output_dir / "editor.pak");

            // Compile core scripts
            fs::path scripts_dir = engine_assets / "scripts";
            if (fs::exists(scripts_dir)) {
                CompileScripts(scripts_dir, output_dir / "Core.dll");
            }
        } else {
            // Legacy mode: assetpacker <input_dir> <output.pak>
            if (argc == 3) {
                PackAssets(argv[1], argv[2]);
            } else {
                PrintUsage();
                return 1;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}