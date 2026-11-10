// EVERY PLAYER'S SHOTS AND HITS BY WEAPON, read off the server after each frame the way a match's
// statistics count them, for a team game's report (classic_team.hpp) to hold the bots to the
// bands real players reach.
//
// - A SHOT is ammunition spent: each player's shells, nails, rockets and cells read every frame,
//   a drop being shots of the weapon then held (the double scattergun and the heavy spiker spend
//   two). A scattergun shot is 6 pellets, a double 14: the scatterguns count pellets, as a
//   match's statistics do; the arc counts cells (one beam each). A frame he is dead in or comes
//   back from counts no shot: the mod empties his ammunition when he comes back in.
// - A HIT is damage an enemy took: each player's health and armour read every frame, a drop
//   being a blow from whatever last hurt him (`dmg_inflictor`): a player (his scattergun, double
//   or arc, by the weapon he holds: pellets of 4 points, beams of 30, four times either under the
//   quad) or a missile (a spike, a grenade or a rocket, counted once however many it hurt, its
//   owner the shooter). A blow from oneself, a teammate or the world counts nothing.
// - A ROCKET'S DIRECT HIT is a blow of exactly 100 (400 under the quad: the mod's direct
//   damage, which the blast then spares), or one that killed a player the rocket was within 32
//   units of the frame before; any other rocket that hurt an enemy hit by its blast alone.
// - DAMAGE GIVEN is every hit's blow summed (health and armour together, the part past a kill
//   left out), the number a match's statistics hold as `given`: per player-minute it is the
//   plainest measure of how much a player fights.
//
// A missile's hit proves its shot: when the ammunition did not show one (fired on the frame the
// shooter died, or took ammunition up), the shot is counted with the hit.
//
// Read only: it changes nothing on the server, so it moves no digest of the game itself.
//
// ge:summary Each player's shots and hits per weapon, off ammunition spent and blows taken
// ge:keywords hit-rate accuracy shots hits pellets direct-hit splash per-weapon statistics
// ge:keywords dmg_inflictor ammo divisions bands team-game damage-given
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::net {

// The weapons counted, in the order a report lists them.
enum class HitWeapon : uint8_t {
  scatter,
  dbl,
  spiker,
  heavy,
  grenades,
  rocket_direct,
  rocket_blast,
  arc
};
constexpr int kHitWeapons = 8;
const char* hit_weapon_name(HitWeapon w);

struct HitCount {
  int64_t fired = 0, hit = 0;  // pellets, spikes, grenades, rockets or beams
};

// The target's states HitBook splits shots by: speed (still, running, fast) times dry or wet.
constexpr int kTargetStates = 6;
inline constexpr const char* kTargetStateNames[kTargetStates] = {
    "still-dry", "run-dry", "fast-dry", "still-wet", "run-wet", "fast-wet"};

struct HitBook {
  std::vector<std::array<HitCount, kHitWeapons>> players;  // by client slot
  std::vector<double> given;                               // damage given, by client slot
  // THE LEAD'S MISS, by client slot: at each hitscan shot (a scattergun's, a double's, an arc's),
  // the other player nearest the shot's line within 20 degrees moving at 100 units/s or more, and
  // how far past him the line went along his motion (+ ahead of him: led too much; - behind: too
  // little), summed, its size summed, and counted.
  std::vector<double> lead_sum, lead_abs;
  std::vector<int64_t> lead_n;
  // BY THE TARGET'S STATE (`kTargetStates`: his speed across, still under 100 units/s, running
  // under 250, fast past it; dry or waist deep): the scattergun's and the double's pellets fired
  // at the player nearest the shot's line (within 20 degrees) and those that hit, by his state;
  // rockets the same (a direct hit); and the damage given by the state of who took it.
  std::array<HitCount, 6> pellets_at{}, rockets_at{};
  std::array<double, 6> given_at{};
  std::array<int64_t, 6> frames_at{};  // and the frames players lived in each state
  // What the last frame read, by entity.
  std::vector<float> life;                 // a player's health and armour
  std::vector<float> health;               // and his health alone (at or under 0: dead)
  std::vector<std::array<float, 4>> ammo;  // shells, nails, rockets, cells
  std::vector<int32_t> weapon;             // the weapon held
  std::vector<uint8_t> state;              // a player's state (kTargetStates), before any blow
  std::vector<ge::shooter::Vec3f> at;      // where each missile was
  std::vector<uint8_t> kind;               // what each entity was (0 none, else a missile kind)
  std::vector<int32_t> owner;              // and whose (an entity number)
};

// Reads the frame the server `s` just ran into `b` (the first call only takes the frame in).
void hits_frame(HitBook& b, const server::QcServer& s);

}  // namespace shooters_edge::net
