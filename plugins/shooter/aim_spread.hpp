// THE AIM SPREAD: how tightly a player's shots sit on the players they hit, a bot-likeness number
// the server keeps for each player and the match report carries (docs/networking.md §
// Anti-cheat). A person's aim wanders over the target: where the view's line crosses a player it
// hits, it falls all over his box, a little differently every frame. An aim bot puts the line on
// the same point every time (the box's middle, or the head), whatever the range or the run.
//
// A SAMPLE is one frame of a player holding the attack with the view's line passing through
// another player's box: the nearest such along the line. Where it passes is taken as the point
// of the line closest to the box's middle, measured from that middle ACROSS (sideways, in the
// level's plan) and UP, each over the box's half-width and half-height, so -1..1 is edge to edge
// on every box at every range. The spread (`aim_spread_rms`) is the root mean square distance of
// the samples from their own mean point: a line that falls evenly over the box is about 0.8; one
// held on a single point is near 0. Fewer than `kAimSpreadLeast` samples say nothing (-1).
//
// It is a number to read, not a verdict: a steady player at long range tracking a slow target
// is tight too, so the report shows it beside the player (and the hub with it) and nothing is
// banned on it. Float math on the view's own numbers and `sqrt`, a pure function of the world.
//
//   aim_spread_sample(s, eye, forward, boxes)     a frame of one player firing
//   aim_spread_frame(sv, spreads)                 every firing player on the server, once a frame
//   aim_spread_rms(s)                             the spread, or -1 with too few samples
//
// ge:summary The aim spread: how tightly a player's shots sit on the players they cross
// ge:keywords aim-spread aimbot aim-bot bot-likeness anti-cheat spread tightness sample
// ge:keywords aim_spread_sample aim_spread_frame aim_spread_rms
// ge:skill shooter
// ge:test tests/shooter_aim_spread_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "shooter/server.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter {

constexpr int kAimSpreadLeast = 30;  // samples before the spread says anything

struct AimSpread {
  int64_t samples = 0;
  double sum_x = 0, sum_y = 0, sum_xx = 0, sum_yy = 0;
};

// A player who may be crossed: his box in the world.
struct AimBox {
  Vec3f lo, hi;
};

// One frame of a player firing from `eye` along `forward` (unit length): a sample when the line
// passes through one of `boxes` ahead of the eye (the nearest along the line). True when taken.
bool aim_spread_sample(AimSpread& s, Vec3f eye, Vec3f forward, std::span<const AimBox> boxes);
// Every living player holding the attack, against every other living player's box: slot i's
// sample into `spreads[i]` (null: that slot is not kept).
void aim_spread_frame(const Server& sv, std::span<AimSpread* const> spreads);
// The root mean square distance of the samples from their mean, in half-boxes; -1 under
// kAimSpreadLeast samples.
double aim_spread_rms(const AimSpread& s);

}  // namespace ge::shooter
