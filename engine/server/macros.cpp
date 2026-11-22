// The dedicated server's macro list and map list (macros.hpp).
//
// ge:summary A dedicated server's macro list and $com_maplist
// ge:keywords macrolist com_maplist wildcard_match console_prefix terminal_prefix
// ge:skill net console
// ge:test tests/server_macros_test.cpp
#include "server/macros.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace ge::server {

namespace {

char fold(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string folded(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = fold(c);
  return out;
}

}  // namespace

std::string terminal_prefix(std::string_view prefix, LineLevel level, const std::tm& now) {
  if (prefix.empty()) return {};
  std::string text(prefix);
  const char digit = level == LineLevel::error ? '3' : level == LineLevel::warning ? '4' : '6';
  const char letter = level == LineLevel::error ? 'E' : level == LineLevel::warning ? 'W' : 'A';
#if !defined(_WIN32)
  if (text.size() >= 3 && text[0] == '<' && text[1] == '?' && text[2] == '>') text[1] = digit;
#else
  (void)digit;
#endif
  if (const size_t at = text.find('@'); at != std::string::npos) text[at] = letter;
  if (text.find('%') == std::string::npos) return text;
  char out[256] = {};
  if (std::strftime(out, sizeof out, text.c_str(), &now) == 0) return {};
  return out;
}

bool wildcard_match(std::string_view pattern, std::string_view text) {
  size_t p = 0, t = 0, star = std::string_view::npos, back = 0;
  while (t < text.size()) {
    if (p < pattern.size() && pattern[p] == '*') {
      star = ++p;
      back = t;
      continue;
    }
    if (p < pattern.size()) {
      char want = pattern[p];
      size_t step = 1;
      if (want == '\\' && p + 1 < pattern.size()) {
        want = pattern[p + 1];
        step = 2;
      } else if (want == '?') {
        p++;
        t++;
        continue;
      }
      if (want == text[t]) {
        p += step;
        t++;
        continue;
      }
    }
    if (star == std::string_view::npos) return false;
    p = star;
    t = ++back;
  }
  while (p < pattern.size() && pattern[p] == '*') p++;
  return p == pattern.size();
}

std::string server_macro_list(const console::Console& console, std::string_view filter) {
  const std::vector<std::string> names = console.macro_names();
  std::string out;
  int shown = 0;
  for (const std::string& n : names) {
    if (!filter.empty() && !wildcard_match(filter, n)) continue;
    std::string value = console.expand_text("$" + n);
    if (value.size() > 63) value.resize(63);
    char head[32];
    std::snprintf(head, sizeof head, "%-16s ", n.c_str());
    out += head + value + "\n";
    shown++;
  }
  return out + std::to_string(shown) + " of " + std::to_string(names.size()) + " macros";
}

std::string server_map_list(std::vector<std::string> maps) {
  std::stable_sort(maps.begin(), maps.end(), [](const std::string& a, const std::string& b) {
    return folded(a) < folded(b);
  });
  std::string out, last;
  for (const std::string& m : maps) {
    const std::string key = folded(m);
    if (!out.empty() && key == last) continue;
    if (!out.empty()) out += ' ';
    out += m;
    last = key;
  }
  return out;
}

}  // namespace ge::server
