// A BODY THROUGH THE SURFACE (docs/water.md § Wakes): what a body falling in does to the water,
// sized by the body itself, its mass, its volume (so its density), its footprint and how fast
// it comes in, and cheap enough to set off for every body a player sees: nothing is stepped on
// a grid; the surface's answer is one ring (wake.hpp) and the spray is particles.
//
// The model, from the body's kinetic energy E = 1/2 m v^2 at the surface (v its speed down
// through it):
// - THE CRATER. The water dug out of a hemisphere of radius R and lifted to the surface stores
//   pi rho g R^4 / 4; a share `kCraterShare` of E goes into it (the rest into the spray, the
//   sound and the water's own motion), so R = (4 share E / (pi rho g))^(1/4), never narrower
//   than the body's footprint. Its depth is 0.6 R for a body as dense as the water, deeper for a
//   denser one (it carries on down), shallower for a lighter one (it bobs up), as the square
//   root of the density ratio between a half and one and a half. A 90 kg player in his gear
//   (0.088 m^3, 1.8 m tall) jumping in off a metre and a half (5.4 m/s) opens one about a metre
//   across and 0.3 m deep, closing in under half a second, its jet after it a fifth of a metre
//   high over the surface (the thin column of drops thrown up with it goes higher).
// - THE RING: the waves the crater sends out are the far field of the water it pushed aside,
//   the cavity's volume (2/3 pi R^2 x its depth), set off over a source as wide as the crater:
//   so a heavier or faster body throws longer, higher waves (the player's, about 2.5 cm at 1.5 m
//   after a second), a pebble short ripples.
// - THE SPRAY (`Entry::drops`, `Entry::speed`): a crown of drops round the crater's rim thrown
//   up and out at up to 0.8 of the entry speed (two drops a joule's square root, 8 to 160), and,
//   as the crater closes (`Entry::jet_at`), the jet's column of drops straight up at half the
//   entry speed.
// A body coming out of the water (`leaving`) sets off a ring of its volume's share alone and a
// curtain of drops falling off it, no crater.
//
// ge:summary A body through the surface: the crater, the ring and the spray by its mass and speed
// ge:keywords water entry splash crater cavity jet worthington body mass density footprint
// ge:keywords kinetic-energy crown spray entry_of jump-in dive cannonball
// ge:skill water
// ge:test tests/water_entry_test.cpp
#pragma once

#include "water/wake.hpp"

namespace ge::water {

inline constexpr double kCraterShare = 0.35;  // of the entry's kinetic energy

// The body going through the surface.
struct Entrant {
  double mass = 90;               // kg, with what it carries
  double volume = 0.088;          // m^3 (mass / volume is its density)
  double footprint = 0.15;        // m^2: its section in the plane of the surface as it goes in
  double x = 0, y = 0, z = 0;     // where it crosses the surface, m
  double vx = 0, vy = 0, vz = 0;  // its velocity, m/s (vy < 0 going in)
  bool leaving = false;           // coming out of the water rather than going in
};

// What the water does about it.
struct Entry {
  Ring ring;             // the waves and, going in, the crater (set off at `ring.t0`)
  double energy = 0;     // J: the kinetic energy down through the surface
  double radius = 0;     // m: the crater's
  int drops = 0;         // the crown's drops (going in) or the curtain's (leaving)
  double speed = 0;      // m/s: the fastest drop's
  double jet_at = 0;     // s after t0: the crater closed and the jet thrown up (0: none)
  double jet_speed = 0;  // m/s: the jet's
  double y = 0;          // the surface's height, m
};

// The water's answer to `body` crossing the surface of `water_body` (its density `rho`) at `t`.
Entry entry_of(const Entrant& body, int water_body, double t, double rho = 1000);

}  // namespace ge::water
