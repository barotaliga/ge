// OUT OF A PIT BY ROCKET: a player in lava, slime or water under a ledge he cannot climb to backs
// up against the wall under it, his back to the ledge, swims up, and as his eyes come out of the
// surface looks down and fires the rocket launcher at the pit's floor in front of him: the blast
// throws him up and back, over the lip, and he lands on the ledge behind him holding the back key.
// A pit of lava or slime with no way out (dm2's lava under its red armour) is death without it;
// in water, which always has a way out, it is one more way to the place a route wants.
//
// THE KEYS (`escape_back_keys`, `escape_flight_keys`), the same for the proof and the pilot: while
// backing, the view turned away from the ledge (`NavEscape::yaw`) and down at the shot's pitch from
// the start (in a liquid the pitched view turns the back key's wish up; the hand's stroke down is
// done on the rise), the back key and jump held (swimming up). The shot fires on the first
// command, `kEscapeBackMin_s` into the backing, that the swimmer is still rising at the escape's
// `fire_level` (3: still under, 2: eyes out, 1: waist out), or has stopped rising with his eyes out
// (a shot from a swimmer sinking back loses half the lift), or has backed `kEscapeBackMax_s` with
// his eyes out; that command presses jump and fire. In the flight the view is level, away from
// the ledge, the back key held. A shot from under the surface loses most of its push to the
// liquid, and a rocket bursting far below (a deep pit's floor) pushes little: the proofs find what
// lands.
//
// THE PROOF (`prove_escape`): a swimmer at `NavEscape::from` (backed against the wall under the
// ledge, his eyes up to `kUnder` (24) under the surface, still) flown by `player_move` under those
// keys, the rocket's burst and blast as a rocket jump's (nav.hpp `rocket_burst`, `rocket_blast`);
// held when he lands out of the liquid standing, failed once the blast is spent with him sinking
// in it or he never surfaces to shoot.
//
// THE ESCAPES (`nav_add_escapes`): every grid column with a node in a liquid at least waist deep
// is a takeoff tried, lava and slime before water (`max_proofs` a map). Its surface is found up the
// column; the ledges tried are the nodes `land` marks (dry, on the map's main part; the caller
// leaves out what is not always there, a bridge) within `far` across and `up` over the surface (at
// least `climb`: lower ones a swimmer climbs out onto), the nearest in each of eight directions,
// nearest first, at most `targets`; the takeoff backed against the wall under the ledge when one
// stands within `wall_near` toward it, else the column itself; each of the `pitches` at each shot
// level, the soonest first (the liquid burns). The first landing standing within 32 of a `land`
// node is the column's escape. Each liquid node then gets a rocket link (`NavLink::escape`) to the
// cheapest escapes it swims to straight within `reach` (one a landing node, at most `per_node`),
// costing the swim at `swim_speed`, the proof and `cost_s`. A `NavJumper` takes them only with
// `escapes` (the launcher and a rocket).
//
// Deterministic: integer column order, the move's own queries, the view's yaw through core's
// libm. Built once a map (it allocates); the keys do not.
//
// ge:summary Out of a pit of lava, slime or water by a rocket: backed up, fired down at the surface
// ge:keywords escape lava slime pit rocket-escape out-of-the-lava back-up surface prove_escape
// ge:keywords nav_add_escapes escape_back_keys escape_flight_keys liquid-escape rocket-jump
// ge:skill shooter bots
// ge:test tests/shooter_escape_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "shooter/nav.hpp"

namespace ge::shooter {

// `NavEscape` is nav.hpp's (the graph holds its escapes).

inline constexpr float kEscapeBackMin_s = 0.15f;  // backed at least this long before the shot, s
inline constexpr float kEscapeBackMax_s = 1.2f;   // and fired by this long however it stands, s

struct EscapeKeys {
  float yaw = 0;      // the view's heading, degrees
  float pitch = 0;    // degrees down
  float forward = 0;  // a share of maxspeed (negative: the back key)
  bool jump = false;
  bool fire = false;  // this command shoots
};

// The keys while backing, `backed_s` into it, with the swimmer's waterlevel and velocity.
EscapeKeys escape_back_keys(const NavEscape& e, int waterlevel, Vec3f velocity, float backed_s);
// The keys in the flight after the shot.
EscapeKeys escape_flight_keys(const NavEscape& e);

// The escape `e` (its `from`, `yaw` and `pitch`) flown by the move; reached when it lands out of
// the liquid standing anywhere (`end` says where); `damage` gets the blast's points.
NavProof prove_escape(const MoveWorld& world, const MoveSettings& move, const NavEscape& e,
                      const NavSettings& set, const RocketJumpSpec& rj = {},
                      float* damage = nullptr, std::vector<Vec3f>* path = nullptr);

struct NavEscapeSettings {
  float far = 192;       // a ledge at most this far across from the takeoff, units
  float up = 240;        // and at most this far above the surface, units
  float climb = 20;      // and at least this far above it, units
  int targets = 8;       // this many ledges tried a takeoff: the nearest in each direction
  float wall_near = 64;  // a wall within this toward the ledge: the takeoff backed against it
  float pitches[3] = {80, 70, 60};  // never past 80 down: a player's view stops there
  int max_proofs = 12000;           // the most proofs a map
  float reach = 384;                // a liquid node links escapes within this, units
  int per_node = 3;                 // at most this many a node, each landing elsewhere
  float swim_speed = 400;           // the swim to a takeoff priced at this, units/s
  float cost_s = 2;                 // and the rocket and the health
  RocketJumpSpec rj;
};
struct NavEscapeCounts {
  int64_t columns = 0, proofs = 0, held = 0, links = 0;
};
// Adds the escapes of every liquid to `g` (`NavGraph::escapes`) and their links; `land` (one byte
// a node) marks the nodes an escape may land on.
NavEscapeCounts nav_add_escapes(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                std::span<const uint8_t> land, const NavEscapeSettings& set = {});

}  // namespace ge::shooter
