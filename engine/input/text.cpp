// The input text formats' word splitter, numbers and options.
//
// ge:summary The splitter, the exact number reader and writer, the option split
// ge:keywords input text split strtod num_text
// ge:skill input
// ge:test tests/input_actions_test.cpp
#include "input/text.hpp"

#include <cstdio>
#include <cstdlib>

#include "core/num_text.hpp"

namespace ge::input::text {

std::vector<std::string> split_words(std::string_view line) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < line.size()) {
    const char c = line[i];
    if (c == ' ' || c == '\t' || c == '\r') {
      i++;
    } else if (c == '#') {
      break;
    } else if (c == '"') {
      const size_t end = line.find('"', i + 1);
      const size_t stop = end == std::string_view::npos ? line.size() : end;
      out.emplace_back(line.substr(i + 1, stop - i - 1));
      i = stop + 1;
    } else {
      size_t j = i;
      while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') j++;
      out.emplace_back(line.substr(i, j - i));
      i = j;
    }
  }
  return out;
}

std::vector<std::pair<int, std::string_view>> lines(std::string_view text) {
  std::vector<std::pair<int, std::string_view>> out;
  int n = 0;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find('\n', start);
    const size_t stop = end == std::string_view::npos ? text.size() : end;
    out.emplace_back(++n, text.substr(start, stop - start));
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return out;
}

std::optional<double> number(std::string_view word) {
  if (word.empty()) return std::nullopt;
  const std::string s(word);
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size()) return std::nullopt;
  return v;
}

std::string num(double v) { return core::num::to_string(v); }

std::string num(float v) {
  // The fewest significant digits whose double, rounded to a float, is v again.
  for (int digits = 1; digits <= 9; digits++) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*g", digits, static_cast<double>(v));
    if (static_cast<float>(std::strtod(buf, nullptr)) == v) {
      return core::num::to_string(std::strtod(buf, nullptr));
    }
  }
  return core::num::to_string(v);
}

std::pair<std::string_view, std::string_view> option(std::string_view word) {
  const size_t eq = word.find('=');
  if (eq == std::string_view::npos) return {word, {}};
  return {word.substr(0, eq), word.substr(eq + 1)};
}

std::string quote(std::string_view s) {
  const bool plain = !s.empty() && s.find_first_of(" \t\"#") == std::string_view::npos;
  return plain ? std::string(s) : "\"" + std::string(s) + "\"";
}

}  // namespace ge::input::text
