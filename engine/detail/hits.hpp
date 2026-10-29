// MARKS A FIGHT LEAVES, FROM ITS EVENTS: what a game calls when a round lands or someone is
// hurt, turned into decals (decals.hpp) on the level's receivers. The game owns the event (its
// shot, its damage, its seed); this file owns where the marks go and how big they are.
//
// - A SHOT (`shot_hit`): the round's ray is cast against the receivers and a bullet hole is
//   pressed where it lands, facing back out of the surface toward the shooter. A shotgun calls it
//   once a pellet, each with its own ray and seed.
// - A WOUND (`bleed`): blood in proportion to the health lost. The share of the victim's health
//   the hit took (`health_lost / max_health`, 0..1) sets everything: how many spatters are thrown
//   on along the shot (onto the wall or floor behind the victim, in a cone that widens and reaches
//   further with the share), how big each is, the pool on the floor under the wound and how many
//   drops fall round it. A graze leaves a spot or two; a hit that takes the whole bar paints the
//   wall behind and leaves a pool a metre across.
//
// Deterministic: every draw is from a `core::Rng` made of the event's `seed`; nothing is read
// from a clock or a global stream, so the same event leaves the same marks on every machine.
//
// ge:summary The marks a fight's events leave: bullet holes where shots land, blood by health lost
// ge:keywords detail decal hit shot bullet-hole pellet shotgun wound damage health blood spatter
// ge:keywords bleed shot_hit pool-of-blood game-event
// ge:skill detail
// ge:test tests/detail_hits_test.cpp
#pragma once

#include <cstdint>

#include "detail/decals.hpp"
#include "detail/receivers.hpp"

namespace ge::detail {

struct ShotAt {
  Vec3 from{0, 0, 0};
  Vec3 dir{0, 0, 1};  // the round's flight; normalised here
  double far_m = 100;
  double size_m = 0.14;  // the hole's decal across, before a +-15 % spread
  double now_s = 0;
  uint32_t seed = 0;
};
struct ShotResult {
  bool hit = false;
  int slot = -1;  // the hole's pool slot; -1 when it hit nothing or no surface took the mark
  Vec3 at{0, 0, 0};
  Vec3 normal{0, 1, 0};  // out of the surface, toward the shooter
  // The struck triangle of the receivers' mesh, -1 on a miss: its `mesh.tag` names the surface,
  // so a game finds the texture and its class (`classify_surface`) for the impact's look.
  int tri = -1;
};
// The round of `s` flown against `receivers` and its hole pressed into `pool`.
ShotResult shot_hit(DecalPool& pool, const Receivers& receivers, const ShotAt& s);

struct WoundAt {
  Vec3 at{0, 1, 0};        // where the round struck the victim, m
  Vec3 shot_dir{0, 0, 1};  // the round's flight; the spatter carries on along it
  double health_lost = 0;
  double max_health = 100;
  double now_s = 0;
  uint32_t seed = 0;
};
struct BleedReport {
  double share = 0;             // the health lost over the max, 0..1
  int spatters = 0, drops = 0;  // thrown on along the shot, fallen round the feet
  int pressed = 0;  // decals that took (a spatter that flew into empty air presses none)
};
// The blood the wound `w` leaves pressed into `pool`.
BleedReport bleed(DecalPool& pool, const Receivers& receivers, const WoundAt& w);

}  // namespace ge::detail
