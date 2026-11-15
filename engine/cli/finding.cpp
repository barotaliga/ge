// The definitions behind finding.hpp: the finding's printed shape and the file helpers.
//
// ge:summary The finding's printed shape, and reading, writing and hashing a repository's files
// ge:keywords cli ge finding format_finding read_text write_text fnv1a64 rel_path
// ge:skill agent-interface
// ge:test tests/cli_doctor_test.cpp
#include "cli/finding.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace ge::cli {

std::optional<std::string> read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string text = ss.str();
  std::erase(text, '\r');
  return text;
}

bool write_text(const fs::path& path, std::string_view text, std::string& error) {
  std::error_code ec;
  if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "cannot create " + path.parent_path().generic_string() + ": " + ec.message();
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!out) {
    error = "cannot write " + path.generic_string();
    return false;
  }
  return true;
}

std::string fnv1a64_hex(std::string_view bytes) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (const char c : bytes) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001b3ull;
  }
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
  return buf;
}

std::string rel_path(const fs::path& root, const fs::path& path) {
  std::error_code ec;
  const fs::path rel = fs::relative(path, root, ec);
  return (ec || rel.empty()) ? path.generic_string() : rel.generic_string();
}

bool skipped_dir(std::string_view name) {
  return name == ".git" || name.starts_with("build") || name == "_deps" || name == "previews" ||
         name == ".cache" || name == "stage" || name == "node_modules";
}

std::string regex_line(std::string_view line) { return std::string(line.substr(0, kRegexLine)); }

}  // namespace ge::cli
