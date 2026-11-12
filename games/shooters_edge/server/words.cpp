// A game's own words, filled in: the rules in words.hpp's header.
//
// ge:summary Fills a game's word templates: gold digits, red text, padding, the local info lookup
// ge:keywords fill_words red_text gold_digits words_of say_words words_fault bi_words
// ge:skill shooter progs
// ge:test tests/shooters_edge_words_test.cpp
#include "shooters_edge/server/words.hpp"

#include <cstdint>

#include "progs/stdlib.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/builtins.hpp"

namespace shooters_edge::server {

namespace {

constexpr char kGoldOpen = static_cast<char>(144);
constexpr char kGoldClose = static_cast<char>(145);

char redden(char c) {
  const auto u = static_cast<uint8_t>(c);
  return u >= 33 && u <= 127 ? static_cast<char>(u + 128) : c;
}

}  // namespace

std::string red_text(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = redden(c);
  return out;
}

std::string gold_digits(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= '0' && c <= '9') c = static_cast<char>(c - '0' + 146);
  return out;
}

std::string fill_words(std::string_view t, std::span<const std::string> args) {
  std::string out;
  out.reserve(t.size() + 32);
  bool red = false;
  size_t i = 0;
  while (i < t.size()) {
    const char c = t[i];
    if (c == '^') {
      if (i + 1 < t.size() && t[i + 1] == '^') {
        out.push_back(red ? redden('^') : '^');
        i += 2;
      } else {
        red = !red;
        i++;
      }
      continue;
    }
    if (c != '%' || i + 1 >= t.size()) {
      out.push_back(red ? redden(c) : c);
      i++;
      continue;
    }
    const char d = t[i + 1];
    if (d == 'n') {
      out.push_back('\n');
      i += 2;
      continue;
    }
    if (d == '%') {
      out.push_back(red ? redden('%') : '%');
      i += 2;
      continue;
    }
    if (d == '.') {
      out.push_back(static_cast<char>(143));
      i += 2;
      continue;
    }
    if (d == 'l') {
      out.push_back(static_cast<char>(157));
      out.append(36, static_cast<char>(158));
      out.push_back(static_cast<char>(159));
      i += 2;
      continue;
    }
    if (d == '[' || d == ']') {
      out.push_back(d == '[' ? kGoldOpen : kGoldClose);
      i += 2;
      continue;
    }
    // An argument: [>w][g|r]digit.
    size_t j = i + 1;
    size_t width = 0;
    if (t[j] == '>' && j + 1 < t.size() && t[j + 1] >= '1' && t[j + 1] <= '9') {
      width = static_cast<size_t>(t[j + 1] - '0');
      j += 2;
    }
    char look = 0;
    if (j < t.size() && (t[j] == 'g' || t[j] == 'r')) look = t[j++];
    if (j >= t.size() || t[j] < '1' || t[j] > '9') {  // not an argument: kept as it is
      out.push_back(red ? redden('%') : '%');
      i++;
      continue;
    }
    const size_t a = static_cast<size_t>(t[j] - '1');
    const std::string_view arg = a < args.size() ? std::string_view(args[a]) : std::string_view{};
    std::string v = look == 'g' ? gold_digits(arg) : look == 'r' ? red_text(arg) : std::string(arg);
    if (v.size() < width) out.append(width - v.size(), ' ');
    out += v;
    i = j + 1;
  }
  return out;
}

std::string_view words_of(const std::vector<std::pair<std::string, std::string>>& info,
                          std::string_view key) {
  for (const auto& [k, v] : info)
    if (k == key) return v;
  return {};
}

std::string say_words(const std::vector<std::pair<std::string, std::string>>& info,
                      std::string_view key, std::span<const std::string> args) {
  const std::string_view t = words_of(info, key);
  return t.empty() ? std::string{} : fill_words(t, args);
}

std::string words_fault(std::string_view t) {
  for (size_t i = 0; i < t.size(); i++) {
    const auto u = static_cast<uint8_t>(t[i]);
    if (u == '"' || u == '\\' || u < 32 || u > 126)
      return "byte " + std::to_string(u) + " at " + std::to_string(i) +
             " cannot travel in a server line (write it with the template's marks: %n, ^, "
             "%[, %g1)";
  }
  return {};
}

// #640 words(key, a, b, c, d): the template under `key` in the server's local info filled with
// the four arguments, or "" when the game names no such words.
void bi_words(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const char* key = pg::arg_string(vm, 0);
  std::string args[4];
  for (int i = 0; i < 4; i++) args[i] = pg::arg_string(vm, i + 1);
  if (vm.fault.set) return;
  const std::string text = say_words(s.localinfo, key, args);
  pg::return_int(vm, pg::builtin_temp_string(vm, text.c_str()));
}

}  // namespace shooters_edge::server
