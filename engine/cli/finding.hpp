// THE FILE HELPERS every ge command reads and writes a repository with, and ge's names for the
// finding and the outcome (toolkit/outcome.hpp: what failed, where, the next action;
// docs/architecture.md § Diagnostics). Every command reports through them, so no ge message can
// leave out the part an agent acts on.
//
// ge:summary ge's findings (what, where, next) and the repository file helpers every command uses
// ge:keywords cli ge finding diagnostics prompt severity error warn note format_finding
// ge:keywords read_text write_text fnv1a64 files skipped_dir
// ge:skill agent-interface
// ge:test tests/cli_doctor_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "toolkit/outcome.hpp"

namespace ge::cli {

namespace fs = std::filesystem;

// The finding and the outcome are the toolkit's (toolkit/outcome.hpp), shared with every game's
// player CLI; named here so every ge command reads them as its own.
using toolkit::count_of;
using toolkit::emit;
using toolkit::exit_of;
using toolkit::Finding;
using toolkit::finding_json;
using toolkit::format_finding;
using toolkit::Outcome;
using toolkit::Severity;
using toolkit::split_lines;

// A text file's contents with every '\r' dropped, or nothing when it cannot be read.
std::optional<std::string> read_text(const fs::path& path);
// Writes `text`, creating the parent directories; false (with `error` said) on failure.
bool write_text(const fs::path& path, std::string_view text, std::string& error);
// FNV-1a 64 over the bytes, as 16 lower-case hex digits: a skill's hash in the lock.
std::string fnv1a64_hex(std::string_view bytes);
// `path` relative to `root` with '/' separators, as a finding's `where` spells it.
std::string rel_path(const fs::path& root, const fs::path& path);
// A directory a walk over the repo's own text skips: build output, dependencies, pictures.
bool skipped_dir(std::string_view name);
// A log line cut to the kRegexLine characters a std::regex may scan. libstdc++'s matcher recurses
// once per character, so a line of a megabyte (a dump, a progress bar redrawn without a newline)
// overflows the stack; every finding a log is read for fits in the cut.
inline constexpr size_t kRegexLine = 1024;
std::string regex_line(std::string_view line);

}  // namespace ge::cli
