// Quake II's timed powerups (q2_powers.hpp).
//
// ge:summary Quake II's timed powerups as one table: the rows and their lookups
// ge:keywords q2 powerup timed-power power-table
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include "shooters_edge/server/q2_powers.hpp"

#include "shooters_edge/server/q2_fight_kit.hpp"

#include <string_view>

namespace shooters_edge::server {

namespace {

// In the game's order: quad (a blue tint, the blue shell), a pack's dual fire, invulnerability (a
// yellow tint, the red shell), the suit (green), the rebreather (a fainter pale green).
constexpr SqPower kPowers[] = {
    {"item_quad", &Q2Player::quad_until, {0, 0, 1, 0.08f}, "items/damage2.wav", "items/damage.wav",
     kSqQuadEffect, 0, "sv_dropquad", 16384},
    // The Reckoning's dual fire: a pink tint, and the quad's blue shell (it looks like the quad to
    // others); dropped at death when `dmflags` has 65536. Never held in the base game.
    {"item_quadfire", &Q2Player::dual_fire_until, {1, 0.2f, 0.5f, 0.08f}, "items/quadfire2.wav",
     "items/quadfire1.wav", kSqQuadEffect, 0, nullptr, 65536},
    // Ground Zero's double damage: a gold tint and its own shell (q2_fight_kit.hpp's multiplier).
    {"item_double", &Q2Player::double_until, {0.9f, 0.7f, 0, 0.08f}, "misc/ddamage2.wav",
     "misc/ddamage1.wav", kSqDoubleEffect, 0, nullptr, 0},
    {"item_invulnerability", &Q2Player::invulnerable_until, {1, 1, 0, 0.08f}, "items/protect2.wav",
     "items/protect.wav", kSqInvulnerableEffect, 0, "sv_droppent", 0},
    {"item_enviro", &Q2Player::suit_until, {0, 1, 0, 0.08f}, "items/airout.wav", nullptr, 0, 0,
     "sv_dropsuit", 0},
    {"item_breather", &Q2Player::breather_until, {0.4f, 1, 0.4f, 0.04f}, "items/airout.wav",
     nullptr, 0, 0, "sv_dropbreather", 0},
    // Ground Zero's IR goggles: a red tint while the client draws the living bright; last.
    {"item_ir_goggles", &Q2Player::ir_until, {1, 0, 0, 0.2f}, nullptr, "misc/ir_start.wav", 0, 0,
     nullptr, 0},
};

}  // namespace

std::span<const SqPower> sq_powers() { return kPowers; }

const SqPower* sq_power(const char* classname) {
  for (const SqPower& w : kPowers)
    if (std::string_view(w.item) == classname) return &w;
  return nullptr;
}

const SqPower* sq_first_power(const Q2Player& p, double now) {
  for (const SqPower& w : kPowers)
    if (p.*w.until > now) return &w;
  return nullptr;
}

int fight_multiplier(const Q2Server& s, const Q2Player& p) {
  const bool quad = p.quad_until > s.level_time;
  if (p.double_until <= s.level_time) return quad ? 4 : 1;
  const bool dm = s.cvars.find("deathmatch")->on();
  if (quad && (!dm || (s.cvars.find("dmflags")->integer() & 262144) != 0)) return 4;
  return quad ? 8 : 2;
}

const char* fight_multiplier_sound(int m) {
  return m >= 4 ? "items/damage3.wav" : m == 2 ? "misc/ddamage3.wav" : nullptr;
}

bool sq_power_on(const Q2Player& p, const char* item, double now) {
  const SqPower* w = sq_power(item);
  return w != nullptr && p.*w->until > now;
}

}  // namespace shooters_edge::server
