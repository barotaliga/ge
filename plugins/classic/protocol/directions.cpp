// The direction table built and ordered (directions.hpp).
//
// ge:summary The direction table built from a divided icosahedron and the pick by largest dot
// ge:keywords direction-table icosahedron normals direction_index
// ge:skill classic shooter net
// ge:test tests/classic_protocol_test.cpp
#include "classic/protocol/directions.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ge::classic::protocol {

namespace {

using V = std::array<double, 3>;

// For each of the protocol's entries, the point of the divided icosahedron (as `divided` orders
// them) it is, measured against a stock program's table.
constexpr uint8_t kOrder[kDirections] = {
    25,  26,  29,  27,  30,  32,  0,   28,  5,   31,  9,   14,  34,  12,  33,  77,  78,  81,
    80,  79,  83,  82,  85,  86,  84,  89,  88,  91,  51,  87,  52,  90,  53,  54,  92,  1,
    57,  2,   55,  56,  58,  4,   3,   60,  59,  6,   8,   7,   11,  13,  10,  18,  17,  21,
    15,  16,  19,  20,  22,  24,  23,  70,  69,  68,  64,  67,  63,  66,  62,  61,  65,  71,
    72,  74,  73,  75,  76,  106, 105, 109, 103, 104, 107, 108, 110, 112, 111, 115, 114, 117,
    93,  113, 97,  116, 100, 102, 118, 94,  95,  98,  96,  99,  101, 147, 150, 148, 38,  152,
    47,  151, 46,  45,  149, 153, 154, 156, 119, 155, 141, 157, 144, 146, 158, 120, 121, 142,
    122, 35,  143, 36,  37,  145, 39,  44,  42,  43,  41,  40,  49,  50,  48,  128, 127, 126,
    125, 123, 124, 134, 140, 132, 135, 138, 136, 129, 137, 139, 133, 130, 131, 159, 161, 160,
};

V unit(const V& v) {
  const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  return {v[0] / l, v[1] / l, v[2] / l};
}

double d2(const V& a, const V& b) {
  double s = 0;
  for (size_t k = 0; k < 3; k++) s += (a[k] - b[k]) * (a[k] - b[k]);
  return s;
}

// The divided icosahedron's 162 points, in the order they are first met: the twelve vertices
// (for a in +1, −1, for b in +phi, −phi: (0, a, b), (a, b, 0), (b, 0, a)), its twenty faces as the
// vertex triples three edges long in index order, each face's points i/4 A + j/4 B + k/4 C (i, then
// j, rising; k = 4 − i − j), normalized. Then x and y are swapped: the protocol's solid is the
// mirror image of this one.
std::vector<V> divided() {
  const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
  std::vector<V> v;
  for (double a : {1.0, -1.0})
    for (double b : {phi, -phi}) {
      v.push_back({0, a, b});
      v.push_back({a, b, 0});
      v.push_back({b, 0, a});
    }
  std::vector<V> out;
  for (size_t x = 0; x < 12; x++)
    for (size_t y = x + 1; y < 12; y++)
      for (size_t z = y + 1; z < 12; z++) {
        if (std::fabs(d2(v[x], v[y]) - 4) > 1e-9 || std::fabs(d2(v[x], v[z]) - 4) > 1e-9 ||
            std::fabs(d2(v[y], v[z]) - 4) > 1e-9)
          continue;
        for (int i = 0; i <= 4; i++)
          for (int j = 0; j <= 4 - i; j++) {
            const int k = 4 - i - j;
            V p;
            for (size_t c = 0; c < 3; c++) p[c] = (i * v[x][c] + j * v[y][c] + k * v[z][c]) / 4;
            p = unit(p);
            bool seen = false;
            for (const V& q : out) seen = seen || d2(p, q) < 1e-12;
            if (!seen) out.push_back(p);
          }
      }
  for (V& p : out) std::swap(p[0], p[1]);
  return out;
}

// A component as the protocol's table holds it: six decimals, cos 18° one unit toward zero.
float held(double c) {
  double r = std::round(c * 1e6) / 1e6;
  if (std::fabs(std::fabs(r) - 0.951057) < 1e-9) r = r < 0 ? -0.951056 : 0.951056;
  return static_cast<float>(r);
}

std::array<Direction, kDirections> build() {
  const std::vector<V> pts = divided();
  std::array<Direction, kDirections> t{};
  for (size_t n = 0; n < size_t(kDirections) && kOrder[n] < pts.size(); n++)
    for (size_t c = 0; c < 3; c++) t[n][c] = held(pts[kOrder[n]][c]);
  return t;
}

}  // namespace

const std::array<Direction, kDirections>& directions() {
  static const std::array<Direction, kDirections> table = build();
  return table;
}

int direction_index(const float d[3]) {
  const std::array<Direction, kDirections>& t = directions();
  int best = 0;
  float most = 0;
  for (int n = 0; n < kDirections; n++) {
    const Direction& e = t[size_t(n)];
    const float dot = d[0] * e[0] + d[1] * e[1] + d[2] * e[2];
    if (dot > most) {
      most = dot;
      best = n;
    }
  }
  return best;
}

}  // namespace ge::classic::protocol
