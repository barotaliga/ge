// The definition behind fetch.hpp: `curl` run through the shell, its status read off stdout.
//
// ge:summary Lab tooling: the download through curl, written beside its name and renamed in
// ge:keywords fetch curl download popen
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/fetch.hpp"

#include <cstdio>
#include <filesystem>

namespace ge::mountain::labs {

namespace fs = std::filesystem;

std::string fetch_to(const std::string& url, const std::string& file) {
  const fs::path target(file);
  if (target.has_parent_path()) fs::create_directories(target.parent_path());
  const fs::path part = target.string() + ".part";
  // Double quotes, which both a POSIX shell and the Windows command line read as one word.
  const std::string cmd = "curl -sS -A \"ge-lab-bake/1.0 (offline bake, run by hand)\" -o \"" +
                          part.string() + "\" -w \"%{http_code}\" \"" + url + "\"";
#ifdef _WIN32
  FILE* p = _popen(cmd.c_str(), "r");
#else
  FILE* p = popen(cmd.c_str(), "r");
#endif
  if (!p) return "";
  char code[8] = {};
  const size_t got = std::fread(code, 1, 3, p);
#ifdef _WIN32
  const int status = _pclose(p);
#else
  const int status = pclose(p);
#endif
  std::string http(code, got);
  if (status != 0 || http != "200") {
    std::error_code ignored;
    fs::remove(part, ignored);
    return status != 0 && http.empty() ? "" : http;
  }
  fs::rename(part, target);
  return http;
}

}  // namespace ge::mountain::labs
