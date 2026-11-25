// QUAKE II'S MISSION PACKS UNDER CLASSIC ENHANCE: what the enhanced effects draw for the packs'
// own temporary entities, as data (world_fx_q2.cpp throws them). Every blast is a kin of the common
// one (app/weapon_fx.hpp's `BlastOf`): explosion1's big one (53) the same ball larger, the nuke's
// (51) a white-hot ball that lights a hall, the tracker's (47) a dark ball that pulls black specks
// in, the plain one (35) and explosion1 without particles (54) the ball alone. The hits that are
// not a bullet's shine in their own colours (`q2_pack_hit_of`): the green bolt's (30), the heat
// beam's sparks (43) and steam (44), the electric sparks (46, which light the face round them as a
// nail's sparks off metal do) and a flechette's glints (55). A heat beam (38, 39) is a hot strip
// `kQ2HeatBeamU` units wide in `kQ2HeatBeamRgb`, never the arc gun's bolt. These are our own
// looks, chosen to sit beside the base game's enhanced effects; the classic look keeps the game's.
//
// ge:summary Quake II's mission-pack blasts, hits and heat beams as classic enhance draws them
// ge:keywords q2 mission-pack classic-enhance ce_explosions ce_hits ce_beams tracker nuke
// ge:keywords explosion1-big heat-beam electric-sparks flechette blaster2 q2_blast_of
// ge:keywords q2_pack_hit_of
// ge:skill render shooter
// ge:test tests/shooters_edge_q2_model_fx_test.cpp
#pragma once

#include "shooters_edge/app/weapon_fx.hpp"

namespace shooters_edge::client {

// The kind of blast explosion code `code` is drawn as.
constexpr effects::BlastOf q2_blast_of(int code) {
  switch (code) {
    case 53:  // explosion1, big
      return effects::BlastOf::big;
    case 51:  // the nuke's
      return effects::BlastOf::huge;
    case 47:  // the tracker's
      return effects::BlastOf::dark;
    case 35:  // the plain one and explosion1 without particles
    case 54:
      return effects::BlastOf::bare;
    default:
      return effects::BlastOf::common;
  }
}

// A pack's hit: its sparks, and whether their flick lights the face.
struct Q2PackHit {
  effects::TintedHit hit;
  bool flick = false;
};
// The hit of code `code`; false (and `out` untouched) for a code that is not one.
constexpr bool q2_pack_hit_of(int code, Q2PackHit* out) {
  switch (code) {
    case 30:  // the green bolt's hit
      *out = {{{0.35f, 1.0f, 0.4f}, 24, 4.0, 0.6, 0.35, 0.03}, false};
      return true;
    case 43:  // the heat beam's sparks: hot, many, fast
      *out = {{{1.0f, 0.5f, 0.15f}, 30, 6.0, 0.8, 0.4, 0.025}, false};
      return true;
    case 44:  // the heat beam's steam: pale, slow, rising
      *out = {{{0.8f, 0.8f, 0.85f}, 16, 1.5, -0.1, 0.7, 0.08}, false};
      return true;
    case 46:  // electric sparks: blue-white, crackling, lighting the face
      *out = {{{0.55f, 0.75f, 1.0f}, 32, 7.0, 0.7, 0.3, 0.02}, true};
      return true;
    case 55:  // a flechette's hit: a few steel-blue glints
      *out = {{{0.6f, 0.8f, 1.0f}, 10, 3.0, 0.8, 0.25, 0.02}, false};
      return true;
    default:
      return false;
  }
}

inline constexpr float kQ2HeatBeamRgb[3] = {1.0f, 0.42f, 0.1f};
inline constexpr float kQ2HeatBeamU = 3;

}  // namespace shooters_edge::client
