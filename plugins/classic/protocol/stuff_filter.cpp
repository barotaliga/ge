// Which stuffed console lines a classic client runs (stuff_filter.hpp).
//
// ge:summary cl_ignore_stufftext's filter: the whitelist's patterns and the signon's own steps
// ge:keywords stufftext whitelist wildcard cl_ignore_stufftext
// ge:skill classic net
// ge:test tests/classic_stuff_filter_test.cpp
#include "classic/protocol/stuff_filter.hpp"

namespace ge::classic {

namespace {

constexpr size_t kLongestPattern = 1024;  // a line this long or longer is dropped

// The command's first word.
std::string_view first_word(std::string_view line) {
  const size_t from = line.find_first_not_of(" \t");
  if (from == std::string_view::npos) return {};
  const size_t to = line.find_first_of(" \t", from);
  return line.substr(from, to == std::string_view::npos ? std::string_view::npos : to - from);
}

}  // namespace

bool stuff_pattern_match(std::string_view p, std::string_view t) {
  size_t pi = 0, ti = 0;
  size_t star = std::string_view::npos, back = 0;  // the last `*` and where its run would end
  while (ti < t.size()) {
    if (pi < p.size() && p[pi] == '*') {
      star = pi++;
      back = ti;
      continue;
    }
    if (pi < p.size()) {
      const bool escaped = p[pi] == '\\' && pi + 1 < p.size();
      const char want = escaped ? p[pi + 1] : p[pi];
      if ((!escaped && want == '?') || want == t[ti]) {
        pi += escaped ? 2 : 1;
        ti++;
        continue;
      }
    }
    if (star == std::string_view::npos) return false;
    pi = star + 1;  // the star takes one character more
    ti = ++back;
  }
  while (pi < p.size() && p[pi] == '*') pi++;
  return pi == p.size();
}

std::vector<std::string> read_stuff_whitelist(std::string_view text) {
  std::vector<std::string> out;
  size_t from = 0;
  while (from < text.size()) {
    size_t end = text.find('\n', from);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(from, end - from);
    from = end + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty() || line.size() >= kLongestPattern) continue;
    out.emplace_back(line);
  }
  return out;
}

bool stuff_allowed(const StuffFilter& f, std::string_view line) {
  if (f.level <= 0) return true;
  const std::string_view word = first_word(line);
  if (word == "changing" || word == "precache" || word == "reconnect") return true;
  if (word == "cmd" && f.patterns.empty()) return true;
  for (const std::string& p : f.patterns)
    if (stuff_pattern_match(p, line)) return true;
  return false;
}

std::string stuff_ignored_line(const StuffFilter& f, std::string_view line) {
  return f.level >= 2 ? "Ignored stufftext: " + std::string(line) : std::string();
}

}  // namespace ge::classic
