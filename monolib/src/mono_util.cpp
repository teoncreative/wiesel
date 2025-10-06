#include "mono_util.h"
#include <filesystem>
#include <iostream>
#include <numeric>
#include <vector>
#include <fstream>

static std::filesystem::path ResolveMcs() {
#ifdef _WIN32
  const char* v = std::getenv("MONO_ROOT"); // <- use MONO_ROOT, not MONO_PATH
  if (!v) throw std::runtime_error("MONO_ROOT not set");
  std::filesystem::path p(v);

  // tolerate people passing lib\mono\4.x or bin\ or the bat itself
  if (std::filesystem::is_regular_file(p) && p.filename() == "mcs.bat") return p;
  if (p.filename() == "bin") return p / "mcs.bat";
  if (p.string().find("\\lib\\mono\\") != std::string::npos) {
    return p.parent_path().parent_path() / "bin" / "mcs.bat";
  }
  return p / "bin" / "mcs.bat";
#else
  const char* v = std::getenv("MONO_ROOT");
  if (!v) throw std::runtime_error("MONO_ROOT not set");
  std::filesystem::path p(v);
  if (std::filesystem::is_regular_file(p) && p.filename() == "mcs") return p;
  if (p.filename() == "bin") return p / "mcs";
  if (p.string().find("/lib/mono/") != std::string::npos) {
    return p.parent_path().parent_path() / "bin" / "mcs";
  }
  return p / "bin" / "mcs";
#endif
}

static std::pair<int,std::string> ExecuteAndGetOutput(const std::string& cmd, std::filesystem::path tmp) {
  int rc = std::system(cmd.c_str());
  std::ifstream in(tmp, std::ios::binary);
  std::string out((std::istreambuf_iterator<char>(in)), {});
  std::error_code ec; std::filesystem::remove(tmp, ec);
  return {rc, out};
}

bool CompileToDLL(const std::string& output_file,
                  const std::vector<std::string>& source_files,
                  const std::string& lib_dir,
                  const std::vector<std::string>& link_libs,
                  bool debug) {
  std::filesystem::path output_dir = std::filesystem::path(output_file).parent_path();
  if (!std::filesystem::exists(output_dir) && !std::filesystem::create_directories(output_dir)) {
    std::cout << "Failed to create output directory: " << output_dir << std::endl;
    return false;
  }
  std::string source = std::accumulate(source_files.begin(), source_files.end(), std::string(),
                                       [](const std::string& a, const std::string& b) {
                                         return a.empty() ? b : a + " " + b;
                                       });
  std::string args;
  for (const auto& lib : link_libs) {
    args += " -reference:" + lib;
  }
  if (debug) {
    //args += " -debug:portable";
    args += " -debug";
  }
  args += " -target:library";
  //args += " /nologo";
  if (!lib_dir.empty()) {
    args += " -lib:" + lib_dir;
  }
  args += " -out:" + output_file;
  args += " " + source;

  auto tmp = std::filesystem::temp_directory_path() / "wiesel_cmd_out.txt";
  std::string mcs = ResolveMcs().make_preferred().string();
  std::string command = "\"" + mcs + "\"" + args + " > \"" + tmp.string() + "\" 2>&1";

#ifdef WIN32
  command = "\"" + command + "\"";
#endif
  std::pair result = ExecuteAndGetOutput(command,tmp);
  if (result.first != 0) {
    std::cout << "Failed to compile C# sources to DLL (error code:" << result.first << ")" << std::endl << result.second;
    std::cout << "Compile command: " << command << std::endl;
    return false;
  }

  return true;
}
