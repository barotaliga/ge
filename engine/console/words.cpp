// A setting's words (words.hpp states the grammar).
//
// ge:summary The values grammar parsed, a value matched against it, the words checked
// ge:keywords parse_values pattern_fits value_fits words_fault
// ge:skill console
// ge:test tests/console_words_test.cpp
#include "console/words.hpp"

#include <cctype>
#include <limits>

namespace ge::console {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

std::vector<std::string_view> words_of(std::string_view s) {
  std::vector<std::string_view> out;
  size_t at = 0;
  while (at < s.size()) {
    while (at < s.size() && s[at] == ' ') at++;
    const size_t end = std::min(s.find(' ', at), s.size());
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end;
  }
  return out;
}

// A range word `a..b`, either end `N` for no bound that way ("1..N": one and up); false when the
// word is no range.
bool range_of(std::string_view word, double& lo, double& hi) {
  const size_t dots = word.find("..");
  if (dots == std::string_view::npos) return false;
  const std::string_view a = word.substr(0, dots), b = word.substr(dots + 2);
  lo = -std::numeric_limits<double>::infinity();
  hi = std::numeric_limits<double>::infinity();
  return (a == "N" || parse_number(a, lo)) && (b == "N" || parse_number(b, hi)) && lo <= hi;
}

// One pattern word against one value word.
bool word_fits(std::string_view pattern, std::string_view value) {
  if (pattern == "TEXT") return true;
  double v = 0;
  if (pattern == "N") return parse_number(value, v);
  if (double lo = 0, hi = 0; range_of(pattern, lo, hi))
    return parse_number(value, v) && v >= lo && v <= hi;
  return pattern == value;
}

}  // namespace

bool parse_values(std::string_view values, std::vector<ValueEntry>& out) {
  out.clear();
  size_t at = 0;
  while (at < values.size()) {
    size_t end = values.find("; ", at);
    if (end == std::string_view::npos) end = values.size();
    const std::string_view entry = values.substr(at, end - at);
    const size_t colon = entry.find(": ");
    if (colon == std::string_view::npos) return false;
    const std::string_view pattern = trim(entry.substr(0, colon));
    const std::string_view meaning = trim(entry.substr(colon + 2));
    if (pattern.empty() || meaning.empty()) return false;
    out.push_back({std::string(pattern), std::string(meaning)});
    at = end + 2;
  }
  return !out.empty();
}

bool pattern_fits(std::string_view pattern, std::string_view value) {
  if (pattern == "\"\"") return trim(value).empty();
  if (pattern == "TEXT") return true;  // any text, spaces and all
  const std::vector<std::string_view> p = words_of(pattern), v = words_of(value);
  if (p.size() != v.size()) return false;
  for (size_t i = 0; i < p.size(); i++)
    if (!word_fits(p[i], v[i])) return false;
  return true;
}

bool value_fits(const Cvar& cvar, std::string_view value) {
  if (value.empty() && cvar.fallback.empty()) return true;  // unset: the owner's own default
  if (cvar.kind != CvarKind::text) {
    double v = 0;
    if (cvar.kind == CvarKind::boolean) {
      if (value != "0" && value != "1") return false;
    } else if (!parse_number(value, v) || v < cvar.min || v > cvar.max) {
      return false;
    }
  }
  std::vector<ValueEntry> entries;
  if (!parse_values(cvar.values, entries)) return true;
  for (const ValueEntry& e : entries)
    if (pattern_fits(e.pattern, value)) return true;
  return false;
}

std::string value_meaning(const Cvar& cvar, std::string_view value) {
  std::vector<ValueEntry> entries;
  if (!parse_values(cvar.values, entries)) return "";
  for (const ValueEntry& e : entries)
    if (pattern_fits(e.pattern, value)) return e.meaning;
  return "";
}

std::string words_fault(const Cvar& cvar) {
  const std::string_view h = cvar.help;
  if (h.empty()) return "no help: say what it does in one sentence";
  const auto first = static_cast<unsigned char>(h.front());
  if (!std::isupper(first) && !std::isdigit(first) && first != '"')
    return "the help starts with \"" + std::string(1, h.front()) + "\": start it with a capital";
  if (h.back() != '.') return "the help does not end with a full stop: end its one sentence";
  for (size_t i = 0; i + 2 < h.size(); i++)
    if (h[i] == '.' && h[i + 1] == ' ' && std::isupper(static_cast<unsigned char>(h[i + 2])))
      return "the help has more than one sentence: say it in one, the values say the rest";
  if (cvar.values.empty()) return "no values: say what each value does (\"0: off; 1: on\")";
  std::vector<ValueEntry> entries;
  if (!parse_values(cvar.values, entries))
    return "the values do not parse: write \"<value>: <what it does>\" entries with \"; \" "
           "between";
  for (const ValueEntry& e : entries) {
    for (const std::string_view w : words_of(e.pattern)) {
      double lo = 0, hi = 0;
      if (w.find("..") != std::string_view::npos && !range_of(w, lo, hi))
        return "the pattern \"" + std::string(w) +
               "\" is no range: write \"<low>..<high>\", N for no bound";
    }
  }
  if (!cvar.fallback.empty() && !value_fits(cvar, cvar.fallback))
    return "the default \"" + cvar.fallback + "\" is not one of its values";
  return "";
}

}  // namespace ge::console
