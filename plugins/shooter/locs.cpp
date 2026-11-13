// A map's place names (locs.hpp): the `.loc` text read, its macros expanded, the nearest named.
//
// ge:summary A map's place names: the .loc lines parsed, the macros, the nearest point
// ge:keywords parse_locs expand_loc_name nearest_loc loc_name_at eighths
// ge:skill shooter bots
// ge:test tests/shooter_locs_test.cpp
#include "shooter/locs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
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
    l.raw = std::string(line.substr(p, std::min(e - p, kLocNameMax)));
    l.name = expand_loc_name(l.raw);
    out.points.push_back(std::move(l));
    n++;
  }
  return n;
}

std::vector<Loc> loc_places(const Locs& locs) {
  std::vector<Loc> out;
  std::vector<int> count;
  for (const Loc& l : locs.points) {
    size_t i = 0;
    while (i < out.size() && out[i].name != l.name) i++;
    if (i == out.size()) {
      out.push_back({l.name, {0, 0, 0}, {}});
      count.push_back(0);
    }
    out[i].at = out[i].at + l.at;
    count[i]++;
  }
  for (size_t i = 0; i < out.size(); i++)
    out[i].at = out[i].at * (1.f / static_cast<float>(count[i]));
  return out;
}

const Loc* nearest_loc(const Locs& locs, Vec3f p, float range, const LocSight& seen) {
  const Loc* best = nullptr;
  float best_d = 0;
  for (const Loc& l : locs.points) {
    const Vec3f d = l.at - p;
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (range > 0 && dd > range * range) continue;
    if (best && dd >= best_d) continue;
    if (seen && !seen(p, l.at)) continue;  // asked only of a point that would be nearer
    best = &l;
    best_d = dd;
  }
  return best;
}

std::string loc_name_at(const Locs& locs, Vec3f p, float range, const LocSight& seen) {
  const Loc* l = nearest_loc(locs, p, range, seen);
  return l ? l->name : std::string();
}

void add_loc(Locs& locs, Vec3f at, std::string_view raw) {
  Loc l;
  l.at = at;
  l.raw = std::string(raw);
  l.name = expand_loc_name(raw);
  locs.points.push_back(std::move(l));
}

bool remove_nearest_loc(Locs& locs, Vec3f at, Loc* removed, float range, const LocSight& seen) {
  const Loc* near = nearest_loc(locs, at, range, seen);
  if (near == nullptr) return false;
  const auto i = locs.points.begin() + (near - locs.points.data());
  if (removed != nullptr) *removed = *i;
  locs.points.erase(i);
  return true;
}

std::string locs_text(const Locs& locs) {
  const auto eighths = [](float v) {
    const double x = static_cast<double>(v) * 8.0;
    return static_cast<long>(x < 0 ? x - 0.5 : x + 0.5);
  };
  std::string out;
  for (const Loc& l : locs.points) {
    char nums[64];
    std::snprintf(nums, sizeof nums, "%4ld %4ld %4ld ", eighths(l.at.x), eighths(l.at.y),
                  eighths(l.at.z));
    out += nums;
    out += l.raw.empty() ? l.name : l.raw;
    out += '\n';
  }
  return out;
}

std::vector<std::string> recorded_loc_lines(const Locs& locs) {
  std::vector<std::string> out;
  const std::string text = locs_text(locs);
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find('\n', at);
    out.push_back(std::string(kRecordedLoc) + text.substr(at, end - at) + "\n");
    at = end + 1;
  }
  return out;
}

bool read_recorded_loc(std::string_view line, Locs& out) {
  if (line.substr(0, kRecordedLoc.size()) != kRecordedLoc) return false;
  return parse_locs(line.substr(kRecordedLoc.size()), out) > 0;
}

}  // namespace ge::shooter
