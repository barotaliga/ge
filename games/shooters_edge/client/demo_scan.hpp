// A RECORDED GAME SCANNED FOR WHAT HAPPENED IN IT: the whole demo read once, nothing drawn, into
// the events its highlights are found in (demo_highlights.hpp) and the length of its timeline
// (timedemo.hpp's `DemoTimeline`, the clock the play bar and a seek use).
//
// THE KILLS are read off what every recording of the multiplayer protocol carries, with no
// game's obituary text: a player DIES when his dead bit comes on (a multiview recording's
// players carry it, a client recording's too) or, failing that, when his health falls from
// above 0 to 0 or under; the KILLER is the other player whose frags went up within `kKillMatchS`
// of that death (the nearest in time, each frag given once); a death with no such frag is the
// world's or the victim's own (`killer` -1). For each kill the scan keeps:
//   - the killer's weapon in hand at the death (his active-weapon stat), and the distance from
//     him to his victim;
//   - for a rocket kill, the ROCKET's flight: the projectile entities whose model is a missile are
//     followed from the frame they appear to the frame they go, and the one that went nearest the
//     victim, within `kRocketReach` and `kRocketWindowS` of the death, is his; its flight is from
//     where it was first seen to where it went;
//   - whether the victim was in the AIR: over the lowest he stood in the last `kAirLookS` by at
//     least `kAirHeight`, and moving up or down at `kAirSpeed` or more over the last frames
//     (no map is read, so a player riding a lift reads as in the air);
//   - whether he was GIBBED (his gib bit);
//   - whether it was a TELEFRAG: the killer moved `kTeleportJump` or more in one frame (a
//     teleporter, a spawn) within `kTelefragS` of the death and stands within `kTelefragReach` of
//     where the victim stood;
//   - his STACK: his health and armour at their most in the last `kStackLookS` (a multiview
//     recording carries every player's stats; a client recording only its own player's, so another
//     player's stack is 0, unknown), and how long before the death that was.
//
// Times are the timeline's seconds. The scan reads its own copy of the demo; the one playing is
// not moved.
//
// ge:summary A demo scanned once, nothing drawn: its kills with weapon, distance, rocket flight,
// ge:summary air, gib and stack, and its timeline's length
// ge:keywords demo scan scan_demo kills frags death killer victim rocket flight airborne gib
// ge:keywords stack armour health timeline length highlights events multiview demo-events
// ge:keywords telefrag teleport
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "shooter/vec.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::client {

inline constexpr double kKillMatchS = 0.6;     // a death and a frag this close are one kill, s
inline constexpr float kRocketReach = 260;     // a rocket that went this near the victim, units
inline constexpr double kRocketWindowS = 0.3;  // and went this close to the death, s
inline constexpr double kAirLookS = 1.5;       // the floor is the lowest he stood in this long, s
inline constexpr float kAirHeight = 40;        // in the air: this far over it, units
inline constexpr float kAirSpeed = 60;         // and moving up or down this fast, units/s
inline constexpr double kStackLookS = 3;       // the stack is his most in this long, s
inline constexpr float kTeleportJump = 160;    // a frame's move this long is a teleport, units
inline constexpr double kTelefragS = 0.3;      // a telefrag: the killer teleported this lately, s
inline constexpr float kTelefragReach = 64;    // onto where the victim stood, within this, units

struct DemoKill {
  double t = 0;     // the death, timeline s
  int killer = -1;  // the killer's slot (-1: the world's or his own)
  int victim = 0;
  hud::Weapon weapon = hud::Weapon::melee;  // the killer's weapon in hand
  float distance = 0;                       // killer to victim, units
  float rocket_flight = 0;                  // the rocket's flight, units (0: none matched)
  bool airborne = false;
  bool gib = false;
  bool telefrag = false;  // the killer teleported onto him
  int stack = 0;          // the victim's health + armour at their most (0: unknown)
  double stack_s = 0;     // how long before the death the stack was at its most, s
  int stack_armour = 0;   // the armour part of it
};

struct DemoScan {
  double length_s = 0;  // the timeline's length
  int64_t frames = 0;
  int levels = 0;
  DemoFormat format = DemoFormat::single;
  int tracked = 0;                    // the slot the view rides by default (client, multiview)
  std::array<std::string, 32> names;  // each slot's last name
  uint32_t players = 0;               // a bit a slot seen playing
  std::vector<DemoKill> kills;        // in time order
  int64_t deaths = 0, unmatched = 0;  // every death; those with no killer
  int64_t rockets = 0;                // rockets followed
};

// `opened` (a demo as `open_demo` left it, at its first frame) read to its end. False with the
// reason and the next step when a frame does not read.
bool scan_demo(const Demo& opened, DemoScan& out, std::string& error);

}  // namespace shooters_edge::client
