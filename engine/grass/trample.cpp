// The trample cells, a stamp, the press a blade reads and the way's packing. `press_at` is
// written in render/wgsl/grass_common.wgsl's order, float for float.
//
// ge:summary Trampled grass definitions: init_trample, stamp, press_at, the way's packing
// ge:keywords grass trample stamp press spring snorm
// ge:skill grass
// ge:test tests/grass_trample_test.cpp
#include "grass/trample.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ge::grass {

namespace {

// A stamp's profile is 1 - (d / r)² scaled by this and clamped to 1: flat under the middle 60 %
// of the radius, falling off to the rim, the way a sole or a tyre presses.
constexpr float kStampFlat = 1.6f;

uint32_t key_of(int32_t col, int32_t row) {
  return (static_cast<uint32_t>(col) & 0xffffu) | (static_cast<uint32_t>(row) << 16u);
}

size_t slot_of(const Trample& tr, int32_t col, int32_t row) {
  const int32_t n = tr.n;
  const int32_t i = ((col % n) + n) % n, k = ((row % n) + n) % n;
  return static_cast<size_t>(k) * static_cast<size_t>(n) + static_cast<size_t>(i);
}

int16_t to_snorm(float v) {
  const float c = std::clamp(v, -1.0f, 1.0f) * 32767.0f;
  return static_cast<int16_t>(std::floor(c + 0.5f));
}

// A cell's press at `t` for a kind that stands up in `spring` s, and its way.
float cell_press(const PressCell& c, uint32_t key, float spring, float t) {
  if (c.key != key) return 0.0f;
  const float r = std::clamp(1.0f - (t - c.t) / spring, 0.0f, 1.0f);
  return c.amount * r * r;
}

}  // namespace

uint32_t pack_way(float x, float z) {
  const uint16_t lo = static_cast<uint16_t>(to_snorm(x));
  const uint16_t hi = static_cast<uint16_t>(to_snorm(z));
  return static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 16u);
}

void unpack_way(uint32_t packed, float out[2]) {
  const auto one = [](uint16_t bits) {
    int16_t v;
    std::memcpy(&v, &bits, sizeof v);
    return std::max(static_cast<float>(v) / 32767.0f, -1.0f);
  };
  out[0] = one(static_cast<uint16_t>(packed & 0xffffu));
  out[1] = one(static_cast<uint16_t>(packed >> 16u));
}

void init_trample(Trample& tr, float cell, int n) {
  tr.cell = std::max(cell, 0.01f);
  tr.n = std::clamp(n, 2, 4096);
  tr.cells.assign(static_cast<size_t>(tr.n) * static_cast<size_t>(tr.n), PressCell{});
  tr.dirty_lo = 0;
  tr.dirty_hi = tr.cells.size();
}

void stamp(Trample& tr, float x, float z, float radius, float way_x, float way_z, float t,
           float strength) {
  if (tr.cells.empty() || radius <= 0 || strength <= 0) return;
  const float wl = std::sqrt(way_x * way_x + way_z * way_z);
  const float wx = wl > 1e-6f ? way_x / wl : 0.0f, wz = wl > 1e-6f ? way_z / wl : 0.0f;
  const int32_t c0 = static_cast<int32_t>(std::floor((x - radius) / tr.cell));
  const int32_t c1 = static_cast<int32_t>(std::floor((x + radius) / tr.cell));
  const int32_t r0 = static_cast<int32_t>(std::floor((z - radius) / tr.cell));
  const int32_t r1 = static_cast<int32_t>(std::floor((z + radius) / tr.cell));
  for (int32_t row = r0; row <= r1; ++row) {
    for (int32_t col = c0; col <= c1; ++col) {
      const float cx = (static_cast<float>(col) + 0.5f) * tr.cell - x;
      const float cz = (static_cast<float>(row) + 0.5f) * tr.cell - z;
      const float d2 = (cx * cx + cz * cz) / (radius * radius);
      if (d2 >= 1.0f) continue;
      const float amount = std::min(strength * kStampFlat * (1.0f - d2), 1.0f);
      // Laid along the travel and splayed out from the middle, more so toward the rim.
      const float d = std::sqrt(cx * cx + cz * cz);
      const float splay = d > 1e-6f ? 0.8f * std::sqrt(d2) / d : 0.0f;
      float lx = wx + cx * splay, lz = wz + cz * splay;
      const float ll = std::sqrt(lx * lx + lz * lz);
      if (ll > 1e-6f) {
        lx /= ll;
        lz /= ll;
      }
      const uint32_t key = key_of(col, row);
      const size_t s = slot_of(tr, col, row);
      PressCell& c = tr.cells[s];
      // The harder of the old press (as it stands now, on the slowest spring there is) and the
      // new one, standing up again from now.
      const float kept = c.key == key ? cell_press(c, key, 30.0f, t) : 0.0f;
      if (amount >= kept) {
        c.way = pack_way(lx, lz);
      }
      c.key = key;
      c.amount = std::max(amount, kept);
      c.t = t;
      tr.dirty_lo = tr.dirty_lo < tr.dirty_hi ? std::min(tr.dirty_lo, s) : s;
      tr.dirty_hi = std::max(tr.dirty_hi, s + 1);
    }
  }
}

Press press_at(const Trample& tr, const Kind& kind, float x, float z, float t) {
  Press out;
  if (tr.cells.empty()) return out;
  // Bilinear between the four cells' middles round the root.
  const float gx = x / tr.cell - 0.5f, gz = z / tr.cell - 0.5f;
  const float fcol = std::floor(gx), frow = std::floor(gz);
  const float fx = gx - fcol, fz = gz - frow;
  const int32_t col = static_cast<int32_t>(fcol), row = static_cast<int32_t>(frow);
  float amount = 0, way_x = 0, way_z = 0;
  for (int32_t dz = 0; dz < 2; ++dz) {
    for (int32_t dx = 0; dx < 2; ++dx) {
      const float w = (dx == 0 ? 1.0f - fx : fx) * (dz == 0 ? 1.0f - fz : fz);
      const PressCell& c = tr.cells[slot_of(tr, col + dx, row + dz)];
      const float p = cell_press(c, key_of(col + dx, row + dz), kind.spring, t) * w;
      if (p <= 0.0f) continue;
      float way[2];
      unpack_way(c.way, way);
      amount += p;
      way_x += way[0] * p;
      way_z += way[1] * p;
    }
  }
  const float wl = std::sqrt(way_x * way_x + way_z * way_z);
  out.amount = std::min(amount, 1.0f);
  if (wl > 1e-6f) {
    out.way[0] = way_x / wl;
    out.way[1] = way_z / wl;
  }
  return out;
}

void clean(Trample& tr) { tr.dirty_lo = tr.dirty_hi = 0; }

}  // namespace ge::grass
