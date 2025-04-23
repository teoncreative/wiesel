#include "mono_util.h"
#include <filesystem>
#include <iostream>
#include <numeric>
#include <vector>

std::pair<int, std::string> ExecuteCommandAndGetOutput(const char* command) {
  // Create a temporary file
  const char* temp_file_name = "temp_output.txt";

  // Construct the command to redirect output to the temporary file
  std::string new_command = std::string(command) + " > " + temp_file_name + " 2>&1";

  // Execute the command
  int result = std::system(new_command.c_str());

  // Read the temporary file into a string
  FILE* file = std::fopen(temp_file_name, "r");
  if (file) {
    std::string output;
    char buffer[128];
    while (std::fgets(buffer, sizeof(buffer), file) != nullptr) {
      output += buffer;
    }
    std::fclose(file);
    std::remove(temp_file_name); // Delete the temporary file
    return {result, output};
  }
  return {result, ""};
}

bool CompileToDLL(const std::string& outputFile,
                  const std::vector<std::string>& sourceFiles,
                  const std::string& libDir,
                  const std::vector<std::string>& linkLibs,
                  bool debug) {
  std::filesystem::path output_dir = std::filesystem::path(outputFile).parent_path();
  if (!std::filesystem::exists(output_dir) && !std::filesystem::create_directories(output_dir)) {
    std::cout << "Failed to create output directory: " << output_dir << std::endl;
    return false;
  }
  // I hate this, words cannot describe the disgust I have for myself for writing this code
#ifdef WIN32
  std::string command_prefix = ".\\mono\\bin\\mcs.bat";
#else
  std::string command_prefix = "mono/bin/mcs";
#endif
  std::string source = std::accumulate(sourceFiles.begin(), sourceFiles.end(), std::string(),
                                       [](const std::string& a, const std::string& b) {
                                         return a.empty() ? b : a + " " + b;
                                       });
  std::string args;
  for (const auto& lib : linkLibs) {
    args += " -reference:" + lib;
  }
  if (debug) {
    //args += " -debug:portable";
    args += " -debug";
  }
  args += " -target:library";
  //args += " /nologo";
  if (!libDir.empty()) {
    args += " -lib:" + libDir;
  }
  args += " -out:" + outputFile;
  args += " " + source;

  std::string command = command_prefix + args;
  std::pair result = ExecuteCommandAndGetOutput(command.c_str());
  if (result.first != 0) {
    std::cout << "Failed to compile C# sources to DLL (error code:" << result.first << ")" << std::endl << result.second;
    std::cout << "Compile command: " << command << std::endl;
    return false;
  }

  return true;
}
