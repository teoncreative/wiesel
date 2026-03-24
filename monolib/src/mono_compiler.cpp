#include "mono_compiler.h"
#include <filesystem>
#include <fstream>
#include <numeric>
#include <optional>

static std::optional<std::filesystem::path> ResolveMcs() {
#ifdef _WIN32
  const char* v = std::getenv("MONO_ROOT");
  if (!v) {
    return std::nullopt;
  }
  std::filesystem::path p(v);

  if (std::filesystem::is_regular_file(p) && p.filename() == "mcs.bat") {
    return p;
  }
  if (p.filename() == "bin") {
    return p / "mcs.bat";
  }
  if (p.string().find("\\lib\\mono\\") != std::string::npos) {
    return p.parent_path().parent_path() / "bin" / "mcs.bat";
  }
  return p / "bin" / "mcs.bat";
#else
  const char* v = std::getenv("MONO_ROOT");
  if (!v) {
    return std::nullopt;
  }
  std::filesystem::path p(v);
  if (std::filesystem::is_regular_file(p) && p.filename() == "mcs") {
    return p;
  }
  if (p.filename() == "bin") {
    return p / "mcs";
  }
  if (p.string().find("/lib/mono/") != std::string::npos) {
    return p.parent_path().parent_path() / "bin" / "mcs";
  }
  return p / "bin" / "mcs";
#endif
}

static std::pair<int, std::string> ExecuteAndGetOutput(
    const std::string& cmd, const std::filesystem::path& tmp) {
  int rc = std::system(cmd.c_str());
  std::ifstream in(tmp, std::ios::binary);
  std::string out((std::istreambuf_iterator<char>(in)), {});
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  return {rc, out};
}

CompileResult CompileToDLL(const std::string& output_file,
                           const std::vector<std::string>& source_files,
                           const std::string& lib_dir,
                           const std::vector<std::string>& link_libs,
                           bool debug) {
  std::filesystem::path output_dir =
      std::filesystem::path(output_file).parent_path();
  if (!output_dir.empty() && !std::filesystem::exists(output_dir) &&
      !std::filesystem::create_directories(output_dir)) {
    return {false, -1,
            "Failed to create output directory: " + output_dir.string(), ""};
  }

  auto mcs_path = ResolveMcs();
  if (!mcs_path) {
    return {false, -1,
            "MONO_ROOT environment variable is not set. "
            "Cannot locate mcs compiler.",
            ""};
  }

  std::string source =
      std::accumulate(source_files.begin(), source_files.end(), std::string(),
                      [](const std::string& a, const std::string& b) {
                        return a.empty() ? b : a + " " + b;
                      });

  std::string args;
  for (const auto& lib : link_libs) {
    args += " -reference:" + lib;
  }
  if (debug) {
    args += " -debug";
  }
  args += " -langversion:latest";
  args += " -target:library";
  args += " /nologo";
  if (!lib_dir.empty()) {
    args += " -lib:" + lib_dir;
  }
  args += " -out:" + output_file;
  args += " " + source;

  auto tmp = std::filesystem::temp_directory_path() / "wiesel_cmd_out.txt";
  std::string mcs = mcs_path->make_preferred().string();
  std::string command =
      "\"" + mcs + "\"" + args + " > \"" + tmp.string() + "\" 2>&1";

#ifdef WIN32
  command = "\"" + command + "\"";
#endif

  auto [exit_code, output] = ExecuteAndGetOutput(command, tmp);
  return {exit_code == 0, exit_code, output, command};
}