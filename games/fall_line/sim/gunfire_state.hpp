// THE GUNFIRE AS A RUN HOLDS IT (gunfire.hpp): the guns armed, the rounds in flight, the stream
// every spread is drawn from and the tallies the labs read. Its own header so `state.hpp` can
// carry it without the step that moves it.
//
// ge:summary The gunfire as a run holds it: Gun, Round, GunfireState, GunfirePhase
// ge:keywords gunfire gun round burst gunfire-state gunfire-phase shot hit impact snap
// ge:skill sim
// ge:test tests/fall_line_gunfire_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "fall_line/sim/wanted_rules.hpp"
#include "fall_line/sim/weapons_rules.hpp"
#include "skier/anatomy.hpp"

namespace fall_line::sim {

// WHAT A BURST IS AIMED AT: the hunted man, or a point (with its velocity, to lead it).
struct AimTarget {
  bool quarry = true;
  double x = 0, y = 0, z = 0;
  double vx = 0, vy = 0, vz = 0;
  template <class Io>
  friend void describe(Io& io, AimTarget& v) {
    io(v.quarry);
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.vx);
    io(v.vy);
    io(v.vz);
  }
};

// ONE GUN: a rifle in a soldier's hands, the light machine gun on a snowmobile's pillion, a door
// gun. Its owner arms it (`arm_gun`), writes its muzzle and the platform's velocity every step,
// and pulls the trigger with `fire_at`; `step_gunfire` fires the burst's rounds at the cyclic
// rate.
struct Gun {
  int32_t id = 0;
  WeaponKind weapon = WeaponKind::rifle;
  double x = 0, y = 0, z = 0;     // the muzzle, world frame, m
  double vx = 0, vy = 0, vz = 0;  // the platform's way, m/s: the rounds leave with it
  int32_t left = 0;               // rounds still to fire of this burst
  // s to the next round of the burst, or between bursts to when it is ready again (the pause)
  double wait = 0;
  int32_t shot = 0;   // rounds this burst has fired (the climb grows with them)
  int32_t fired = 0;  // rounds it has fired in all (every `tracer`th glows)
  FireRule rules = FireRule::hold;
  AimTarget target;
  // The burst's aim error, drawn once a burst: across and up, mil, and the share of the lead
  // judged; for warning fire (`spot`) the offset off him it is put down in the snow at, m.
  double ex = 0, ey = 0, el = 1;
  bool spot = false;
  double ox = 0, oz = 0;
  template <class Io>
  friend void describe(Io& io, Gun& v) {
    io(v.id);
    io(v.weapon);
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.vx);
    io(v.vy);
    io(v.vz);
    io(v.left);
    io(v.wait);
    io(v.shot);
    io(v.fired);
    io(v.rules);
    io(v.target);
    io(v.ex);
    io(v.ey);
    io(v.el);
    io(v.spot);
    io(v.ox);
    io(v.oz);
  }
};

// ONE ROUND IN FLIGHT.
struct Round {
  int32_t id = 0;
  int32_t gun = 0;
  WeaponKind weapon = WeaponKind::rifle;
  double x = 0, y = 0, z = 0;
  double vx = 0, vy = 0, vz = 0;
  double age = 0;    // s since it left
  double flown = 0;  // m
  bool tracer = false;
  bool warn = false;  // warning fire: never tested against his body
  bool snapped = false;
  template <class Io>
  friend void describe(Io& io, Round& v) {
    io(v.id);
    io(v.gun);
    io(v.weapon);
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.vx);
    io(v.vy);
    io(v.vz);
    io(v.age);
    io(v.flown);
    io(v.tracer);
    io(v.warn);
    io(v.snapped);
  }
};

// What a gunfire event says (`EventKind::gunfire`, its `gate` the phase in the low byte and, on a
// hit, the part over it; its `value` the energy, J, or the weapon): a gun fired a round (its
// report and flash), a round struck him, struck the snow or a trunk, or cracked past him.
enum class GunfirePhase : uint8_t { shot, hit, snow, trunk, snap };
inline constexpr int32_t kGunfirePhases = 5;

struct GunfireState {
  bool active = false;  // a gun was armed this run: only then is any of it hashed
  std::vector<Gun> guns;
  std::vector<Round> rounds;
  ge::core::Rng stream{0};  // its own stream, seeded off the map: nothing draws `state.rng`
  int32_t next = 0;         // the next round's id
  // THE TALLIES the labs and the tests read: rounds fired, rounds into him, the energy they
  // carried, J, rounds into the snow and the trunks, cracks past him; and every hit by part.
  int32_t fired = 0;
  int32_t hits = 0;
  double energy = 0;
  int32_t snow = 0;
  int32_t trunks = 0;
  int32_t snaps = 0;
  std::array<int32_t, ge::skier::kParts> by_part{};
  template <class Io>
  friend void describe(Io& io, GunfireState& v) {
    io(v.active);
    io(v.guns);
    io(v.rounds);
    io(v.stream);
    io(v.next);
    io(v.fired);
    io(v.hits);
    io(v.energy);
    io(v.snow);
    io(v.trunks);
    io(v.snaps);
    io(v.by_part);
  }
};

}  // namespace fall_line::sim
