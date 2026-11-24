// Quake II's match stats, counted and shown (q2_stats.hpp states them).
//
// ge:summary Quake II's match stats counted from the fight's calls, and the table a player reads
// ge:keywords stats_shot stats_blow stats_death stats_table accuracy q2-stats
// ge:skill shooter net
// ge:test tests/shooters_edge_q2_match_test.cpp
#include "shooters_edge/server/q2_stats.hpp"

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

bool player(int slot) { return slot >= 0 && slot < 32 && slot < sh::kMaxClients; }
bool weapon(int w) { return w >= 0 && w < kSqWeaponCount; }

Q2SlotStats& of(Q2Server& s, int slot) {
  Q2SlotStats& p = s.stats.slot[static_cast<size_t>(slot)];
  p.counted = true;
  return p;
}

std::string pad(std::string text, size_t width, bool left = false) {
  if (text.size() >= width) return text;
  const std::string fill(width - text.size(), ' ');
  return left ? text + fill : fill + text;
}

}  // namespace

int stats_accuracy(const Q2WeaponTally& t) {
  if (t.shots <= 0) return -1;
  return static_cast<int>((int64_t(t.hits) * 100 + t.shots - 1) / t.shots);
}

void stats_shot(Q2Server& s, int slot, int w, int n) {
  if (!s.stats.on || !player(slot) || !weapon(w) || n <= 0) return;
  of(s, slot).weapon[static_cast<size_t>(w)].shots += n;
}

void stats_blow(Q2Server& s, int attacker, int target, int w, int points) {
  if (!s.stats.on || !player(target) || points <= 0) return;
  Q2SlotStats& victim = of(s, target);
  if (!player(attacker) || attacker == target) {
    victim.received += points;
    if (attacker == target && weapon(w)) victim.weapon[static_cast<size_t>(w)].received += points;
    return;
  }
  Q2SlotStats& by = of(s, attacker);
  if (fight_teammates(s, attacker, target)) {
    by.team_dealt += points;
    victim.team_received += points;
    return;
  }
  by.dealt += points;
  victim.received += points;
  if (!weapon(w)) return;
  const auto k = static_cast<size_t>(w);
  by.weapon[k].dealt += points;
  victim.weapon[k].received += points;
  const int64_t frame = s.fast_number + 1;
  if (w == int(SqWeapon::railgun)) {  // a slug's every player
    if (by.hit_frame[k] != frame) by.rail_hit = 0;
    by.hit_frame[k] = frame;
    const uint32_t bit = 1u << target;
    if ((by.rail_hit & bit) != 0) return;
    by.rail_hit |= bit;
    by.weapon[k].hits++;
    return;
  }
  if (by.hit_frame[k] == frame) return;  // a blast's or a blast of pellets' one hit
  by.hit_frame[k] = frame;
  by.weapon[k].hits++;
}

void stats_death(Q2Server& s, int slot, int killer, int w, bool teammate) {
  if (!s.stats.on || !player(slot)) return;
  Q2SlotStats& victim = of(s, slot);
  victim.deaths++;
  if (!player(killer) || killer == slot) {
    victim.suicides++;
    return;
  }
  Q2SlotStats& by = of(s, killer);
  if (teammate) {
    by.team_kills++;
    return;
  }
  by.kills++;
  if (!weapon(w)) return;
  by.weapon[static_cast<size_t>(w)].kills++;
  victim.weapon[static_cast<size_t>(w)].deaths++;
}

std::string stats_table(const Q2SlotStats& p, const std::string& name) {
  if (!p.counted) return {};
  std::string out = "Stats for " + name + ":\n";
  out += pad("Weapon", 15, true) + pad("Acc", 5) + pad("Hits", 6) + pad("Shots", 6) +
         pad("Kills", 6) + pad("Deaths", 7) + pad("Dealt", 6) + pad("Recvd", 6) + "\n";
  for (int w = 0; w < kSqWeaponCount; w++) {
    const Q2WeaponTally& t = p.weapon[static_cast<size_t>(w)];
    if (t.shots == 0 && t.hits == 0 && t.kills == 0 && t.deaths == 0 && t.received == 0) continue;
    const int acc = stats_accuracy(t);
    out += pad(kSqWeapons[w].name, 15, true) + pad(acc < 0 ? "-" : std::to_string(acc) + "%", 5) +
           pad(std::to_string(t.hits), 6) + pad(std::to_string(t.shots), 6) +
           pad(std::to_string(t.kills), 6) + pad(std::to_string(t.deaths), 7) +
           pad(std::to_string(t.dealt), 6) + pad(std::to_string(t.received), 6) + "\n";
  }
  out += "Kills " + std::to_string(p.kills) + ", deaths " + std::to_string(p.deaths) +
         ", suicides " + std::to_string(p.suicides) + ", team kills " +
         std::to_string(p.team_kills) + "\n";
  out += "Damage dealt " + std::to_string(p.dealt) + ", received " + std::to_string(p.received) +
         ", to teammates " + std::to_string(p.team_dealt) + ", from teammates " +
         std::to_string(p.team_received) + "\n";
  return out;
}

}  // namespace shooters_edge::server
