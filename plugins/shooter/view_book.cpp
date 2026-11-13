// The view book read and looked up (view_book.hpp).
//
// ge:summary The view book's parser and its lookup by cell and heading octant
// ge:keywords view-book viewbook parse_view_book view_book_yaw
// ge:skill shooter bots
// ge:test tests/shooter_view_book_test.cpp
#include "shooter/view_book.hpp"

#include <algorithm>
#include <cmath>

namespace ge::shooter {

namespace {

int64_t pack(int64_t cx, int64_t cy, int64_t cz, int64_t oct) {
  // 20 bits a coordinate (cells to +-500 000), 4 for the octant.
  return ((cx + (1 << 19)) << 44) | ((cy + (1 << 19)) << 24) | ((cz + (1 << 19)) << 4) | oct;
}

int64_t floor_div(float v, int32_t cell) {
  return static_cast<int64_t>(std::floor(v / static_cast<float>(cell)));
}

// Whole numbers off `s` from `at`, spaces and tabs between; false past the line's end.
bool next_int(std::string_view s, size_t& at, int64_t& v) {
  while (at < s.size() && (s[at] == ' ' || s[at] == '\t')) at++;
  bool neg = false;
  if (at < s.size() && (s[at] == '-' || s[at] == '+')) neg = s[at++] == '-';
  if (at >= s.size() || s[at] < '0' || s[at] > '9') return false;
  int64_t n = 0;
  while (at < s.size() && s[at] >= '0' && s[at] <= '9') n = n * 10 + (s[at++] - '0');
  v = neg ? -n : n;
  return true;
}

}  // namespace

int parse_view_book(std::string_view text, ViewBook& out) {
  out.entries.clear();
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(pos, end - pos);
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty() || line[0] == '#') continue;
    size_t at = 0;
    if (line.substr(0, 5) == "cell ") {
      at = 5;
      int64_t xy = 0, z = 0;
      if (next_int(line, at, xy) && next_int(line, at, z) && xy > 0 && z > 0) {
        out.cell_xy = static_cast<int32_t>(xy);
        out.cell_z = static_cast<int32_t>(z);
      }
      continue;
    }
    int64_t v[7];
    bool ok = true;
    for (int64_t& x : v) ok = ok && next_int(line, at, x);
    if (!ok || v[3] < 0 || v[3] > 7) continue;
    out.entries.push_back({pack(v[0], v[1], v[2], v[3]), static_cast<int32_t>(v[4]),
                           static_cast<int32_t>(v[5]), static_cast<int32_t>(v[6])});
  }
  std::sort(out.entries.begin(), out.entries.end(),
            [](const ViewBookEntry& a, const ViewBookEntry& b) { return a.key < b.key; });
  return static_cast<int>(out.entries.size());
}

int view_book_octant(float heading_deg) {
  float h = heading_deg + 22.5f;
  h -= 360.f * std::floor(h / 360.f);
  return std::min(7, static_cast<int>(h / 45.f));
}

bool view_book_yaw(const ViewBook& book, Vec3f origin, float heading_deg, int32_t min_frames,
                   int32_t min_consistency, float& yaw_deg) {
  if (book.entries.empty()) return false;
  const int64_t cx = floor_div(origin.x, book.cell_xy), cy = floor_div(origin.y, book.cell_xy);
  const int64_t cz = floor_div(origin.z, book.cell_z);
  const int64_t oct = view_book_octant(heading_deg);
  for (int64_t dz : {0, -1, 1}) {
    const int64_t key = pack(cx, cy, cz + dz, oct);
    const auto it = std::lower_bound(
        book.entries.begin(), book.entries.end(), key,
        [](const ViewBookEntry& e, int64_t k) { return e.key < k; });
    if (it == book.entries.end() || it->key != key) continue;
    if (it->frames < min_frames || it->consistency < min_consistency) continue;
    yaw_deg = static_cast<float>(it->yaw_tenths) / 10.f;
    return true;
  }
  return false;
}

}  // namespace ge::shooter
