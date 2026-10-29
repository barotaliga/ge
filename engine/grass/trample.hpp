// GRASS UNDERFOOT (docs/grass.md § Trampling): blades pressed down by whatever walks, rolls or
// lands on them, and standing back up over the kind's `spring` seconds. A game STAMPS a print
// where a foot, a wheel, a body or a landing skid meets the grass; the blades there read how
// pressed they are and which way, and the GPU draws exactly that (render/grass.hpp binds the
// same cells).
//
// - THE CELLS: a square of `n` x `n` cells of `cell` m that wraps round the world (a cell is
//   addressed by its world column and row modulo n, and keeps its key so a stale one reads as
//   unpressed), so a trail can be laid anywhere and the newest prints win where two places share
//   a slot. Each cell holds when it was last pressed, how hard (0..1) and the way its blades lie.
// - A STAMP (`stamp`): a disc of `radius` m, pressed flat over its middle 60 % and falling off to
//   its rim (1.6 (1 - (d / r)²), clamped to 1), its blades laid along `way` (the stamper's
//   direction of travel) and splayed outward from the middle. A cell keeps the harder of its old
//   press and the new one and starts standing up again from the new stamp's time.
// - STANDING BACK UP (`press_at`): a blade of a kind with `spring` s is pressed by
//   amount * r² with r = 1 - (t - pressed) / spring, clamped to 0..1: it springs up quickest at
//   first, then settles slowly, and is upright again after `spring` seconds. Nothing is stepped:
//   the press is a function of the cells and the clock, so a replay or a seek draws the same trail.
//
// ge:summary Trampled grass: stamps that press blades flat and the kind's spring that lifts them
// ge:keywords grass trample trampling press footprint wheel-track trail stamp spring flatten
// ge:keywords press_at stamp trample-cells
// ge:skill grass
// ge:test tests/grass_trample_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "grass/kind.hpp"

namespace ge::grass {

inline constexpr float kPressBend = 1.45f;  // rad off the vertical, a blade pressed flat

// One cell, as render/grass.wgsl's GrassPress reads it.
struct PressCell {
  uint32_t key = 0xffffffffu;  // (column & 0xffff) | (row << 16); all ones: never pressed
  float t = 0;                 // s, when last pressed
  float amount = 0;            // 0..1
  uint32_t way = 0;            // the plan way its blades lie: two snorm16s (x low, z high)
};
static_assert(sizeof(PressCell) == 16, "match grass_common.wgsl's GrassPress");

struct Trample {
  float cell = 0.25f;  // m
  int n = 256;         // cells a side (64 m at 0.25 m)
  std::vector<PressCell> cells;
  // The cells changed since `clean`: the lowest and one past the highest index (lo >= hi: none).
  size_t dirty_lo = 0, dirty_hi = 0;
};

// A blade's press: how flat (0..1) and the plan way (unit) it lies.
struct Press {
  float amount = 0;
  float way[2] = {0, 0};
};

// Every cell unpressed, `n` x `n` of `cell` m.
void init_trample(Trample& trample, float cell = 0.25f, int n = 256);

// Press a disc at (x, z) at `t` s: `radius` m, `strength` 0..1 at its middle, its blades laid
// along (way_x, way_z) (any length; zero: splayed outward only).
void stamp(Trample& trample, float x, float z, float radius, float way_x, float way_z, float t,
           float strength = 1);

// The press on a blade of `kind` rooted at (x, z) at `t` s.
Press press_at(const Trample& trample, const Kind& kind, float x, float z, float t);

// The dirty range is uploaded: forget it.
void clean(Trample& trample);

// The snorm16 pair a way is stored as, and back (render/grass.wgsl's unpack2x16snorm).
uint32_t pack_way(float x, float z);
void unpack_way(uint32_t packed, float out[2]);

}  // namespace ge::grass
