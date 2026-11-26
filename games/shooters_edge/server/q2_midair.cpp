// Midair under Quake II's rules (q2_fight.hpp § Midair): whether it is on, a spawn's kit, the
// tally lines told at the level's end.
//
// ge:summary Quake II's rules' midair: on or off, a spawn's kit, the tally at the level's end
// ge:keywords q2 midair k_midair midair-kit midair-tally
// ge:skill shooter
// ge:test tests/shooters_edge_midair_test.cpp
#include <string>
#include <vector>

#include "shooter/midair.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

const std::string& name_of(const Q2Server& s, int slot) {
  static const std::string kNobody = "someone";
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return kNobody;
}

}  // namespace

bool fight_midair(const Q2Server& s) {
  const auto* c = s.cvars.find("k_midair");
  return s.cvars.find("deathmatch")->on() && c != nullptr && c->on();
}

void fight_midair_kit(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  if (!fight_midair(s) || !p.alive) return;
  p.health = sh::kMidairHealth;
  p.body.health = static_cast<int>(sh::kMidairHealth);
  p.body.armour = SqArmour::body;
  p.body.armour_count = static_cast<int>(sh::kMidairArmour);
  const int rockets = static_cast<int>(SqAmmo::rockets);
  p.body.max_ammo[rockets] = 255;
  p.body.ammo[rockets] = 255;
  p.body.counts[sq_item("weapon_rocketlauncher")] = 1;
  p.hand = SqHand{};
  p.hand.weapon = int(SqWeapon::rocket_launcher);
}

std::vector<std::string> fight_midair_report(const Q2Server& s) {
  std::vector<std::string> out;
  if (!fight_midair(s)) return out;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    std::string line = sh::midair_tally_line(name_of(s, i), s.players[i].midair);
    if (!line.empty()) out.push_back(std::move(line));
  }
  return out;
}

}  // namespace shooters_edge::server
