// A FINDING: one thing ge has to tell an agent, in the diagnostics' three parts (what failed,
// where, the next action; docs/architecture.md § Diagnostics). Every command reports through
// it, so no ge message can leave out the part an agent acts on, and the files' helpers ge reads
// and writes a repository with sit beside it.
//
// ge:summary ge's findings (what, where, next) and the repository file helpers every command uses
// ge:keywords cli ge finding diagnostics prompt severity error warn note format_finding
// ge:keywords read_text write_text fnv1a64 files
// ge:skill agent-interface
// ge:test tests/cli_doctor_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ge::cli {

namespace fs = std::filesystem;

enum class Severity : uint8_t {
  error,  // the repository breaks a rule; the command exits 1
  warn,   // odd but allowed; worth a look
  note,   // information the next step needs
};

struct Finding {
  Severity severity;
  std::string what;   // one line, with the values involved
  std::string where;  // a repository-relative path (":line" when there is one), or "" for the repo
  std::string next;   // the command to run, the skill to load or the file to read
};

// "<command>: error: <what>\n    where: <where>\n    next: <next>\n"
std::string format_finding(std::string_view command, const Finding& f);
int count_of(const std::vector<Finding>& findings, Severity severity);

// A text file's contents with every '\r' dropped, or nothing when it cannot be read.
std::optional<std::string> read_text(const fs::path& path);
// Writes `text`, creating the parent directories; false (with `error` said) on failure.
bool write_text(const fs::path& path, std::string_view text, std::string& error);
// FNV-1a 64 over the bytes, as 16 lower-case hex digits: a skill's hash in the lock.
std::string fnv1a64_hex(std::string_view bytes);
// `path` relative to `root` with '/' separators, as a finding's `where` spells it.
std::string rel_path(const fs::path& root, const fs::path& path);
// Lines without their '\n'.
std::vector<std::string_view> split_lines(std::string_view text);

}  // namespace ge::cli
