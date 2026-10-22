// The definitions behind finding.hpp: the finding's printed shape and the file helpers.
//
// ge:summary The finding's printed shape, and reading, writing and hashing a repository's files
// ge:keywords cli ge finding format_finding read_text write_text fnv1a64 rel_path split_lines
// ge:skill agent-interface
// ge:test tests/cli_doctor_test.cpp
#include "cli/finding.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace ge::cli {

namespace {

std::string_view severity_name(Severity s) {
  switch (s) {
    case Severity::error:
      return "error";
    case Severity::warn:
      return "warn";
    case Severity::note:
      return "note";
  }
  return "error";
}

}  // namespace

std::string format_finding(std::string_view command, const Finding& f) {
  std::string out;
  out += command;
  out += ": ";
  out += severity_name(f.severity);
  out += ": ";
  out += f.what;
  out += "\n    where: ";
  out += f.where.empty() ? "the repository" : f.where;
  out += "\n    next: ";
  out += f.next;
  out += '\n';
  return out;
}

int count_of(const std::vector<Finding>& findings, Severity severity) {
  int n = 0;
  for (const Finding& f : findings) n += f.severity == severity ? 1 : 0;
  return n;
}

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

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(text.substr(start));
      break;
    }
    lines.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

}  // namespace ge::cli
