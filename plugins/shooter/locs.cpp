// A map's place names (locs.hpp): the `.loc` text read, its macros expanded, the nearest named.
//
// ge:summary A map's place names: the .loc lines parsed, the macros, the nearest point
// ge:keywords parse_locs expand_loc_name nearest_loc loc_name_at eighths
// ge:skill shooter bots
// ge:test tests/shooter_locs_test.cpp
#include "shooter/locs.hpp"

#include <cctype>
#include <cstdlib>

namespace ge::shooter {

namespace {

constexpr std::string_view kMacro = "$loc_name_";

bool word_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// One whole number at `p` in `line` (sign, digits), moving `p` past it; false on none.
bool read_int(std::string_view line, size_t& p, long& out) {
  while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) p++;
  size_t q = p;
  if (q < line.size() && (line[q] == '-' || line[q] == '+')) q++;
  const size_t digits = q;
  while (q < line.size() && std::isdigit(static_cast<unsigned char>(line[q]))) q++;
  if (q == digits) return false;
  out = std::strtol(std::string(line.substr(p, q - p)).c_str(), nullptr, 10);
  p = q;
  return true;
}

}  // namespace

std::string expand_loc_name(std::string_view raw) {
  std::string out;
  size_t i = 0;
  while (i < raw.size()) {
    if (raw.substr(i, kMacro.size()) != kMacro) {
      out += raw[i++];
      continue;
    }
    i += kMacro.size();
    std::string_view key;
    for (std::string_view k : kLocKeys)
      if (raw.substr(i, k.size()) == k && k.size() > key.size()) key = k;
    if (key.empty()) {
      const size_t from = i;
      while (i < raw.size() && word_char(raw[i])) i++;
      out += raw.substr(from, i - from);
      continue;
    }
    out += key == "separator" ? std::string_view("-") : key;
    i += key.size();
  }
  return out;
}

int parse_locs(std::string_view text, Locs& out) {
  int n = 0;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    size_t p = 0;
    long v[3];
    if (!read_int(line, p, v[0]) || !read_int(line, p, v[1]) || !read_int(line, p, v[2])) continue;
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) p++;
    size_t e = line.size();
    while (e > p && (line[e - 1] == ' ' || line[e - 1] == '\t')) e--;
    if (e == p) continue;
    Loc l;
    l.at = {static_cast<float>(v[0]) / 8.f, static_cast<float>(v[1]) / 8.f,
            static_cast<float>(v[2]) / 8.f};
    l.name = expand_loc_name(line.substr(p, e - p));
    out.points.push_back(std::move(l));
    n++;
  }
  return n;
}

const Loc* nearest_loc(const Locs& locs, Vec3f p) {
  const Loc* best = nullptr;
  float best_d = 0;
  for (const Loc& l : locs.points) {
    const Vec3f d = l.at - p;
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (!best || dd < best_d) {
      best = &l;
      best_d = dd;
    }
  }
  return best;
}

std::string loc_name_at(const Locs& locs, Vec3f p) {
  const Loc* l = nearest_loc(locs, p);
  return l ? l->name : std::string();
}

}  // namespace ge::shooter
