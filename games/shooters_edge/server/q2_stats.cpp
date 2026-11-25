// Quake II's match stats, counted into the shared columns (q2_stats.hpp states them).
//
// ge:summary Quake II's match stats counted from the fight's calls into the shared columns
// ge:keywords stats_shot stats_blow stats_death stats_item stats_tick q2-stats
// ge:skill shooter net
// ge:test tests/shooters_edge_q2_match_test.cpp
#include "shooters_edge/server/q2_stats.hpp"

#include <cstring>

#include "core/num.hpp"

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

bool player(int slot) { return slot >= 0 && slot < 32 && slot < sh::kMaxClients; }
// The base game's eleven weapons: a mission pack's own (the ion ripper, the phalanx, the trap)
// are not counted, so every Quake II match report keeps one layout.
constexpr int kStatQ2Weapons = int(SqWeapon::hand_grenade) + 1;
bool weapon(int w) { return w >= 0 && w < kStatQ2Weapons; }

Q2SlotStats& of(Q2Server& s, int slot) {
  Q2SlotStats& p = s.stats.slot[static_cast<size_t>(slot)];
  p.counted = true;
  return p;
}

constexpr int kRocket = int(SqWeapon::rocket_launcher), kGrenade = int(SqWeapon::grenade_launcher);

}  // namespace

const StatsGame& q2_stats_game() {
  static const StatsGame g = [] {
    StatsGame out;
    out.weapons = kStatQ2Weapons;
    out.label = {"bl", "sg", "ssg", "mg", "cg", "gl", "rl", "hb", "rg", "bfg", "hg", nullptr};
    out.rl = kRocket;
    out.gl = kGrenade;
    int k = 0;
    for (SqWeapon w :
         {SqWeapon::railgun, SqWeapon::rocket_launcher, SqWeapon::grenade_launcher,
          SqWeapon::hyperblaster, SqWeapon::chaingun, SqWeapon::machinegun, SqWeapon::super_shotgun,
          SqWeapon::shotgun, SqWeapon::bfg, SqWeapon::hand_grenade, SqWeapon::blaster})
      out.shown[static_cast<size_t>(k++)] = static_cast<uint8_t>(w);
    out.shown_count = k;
    out.item = {"ja", "ca", "ba", "mh", "quad", "invuln", "power"};
    out.words = {"jacket", "combat", "body", "mega", "quad", "invulnerability", "power armour"};
    return out;
  }();
  return g;
}

void stats_shot(Q2Server& s, int slot, int w, int n) {
  if (!s.stats.on || !player(slot) || !weapon(w) || n <= 0) return;
  const int each = w == int(SqWeapon::shotgun) ? 12 : w == int(SqWeapon::super_shotgun) ? 20 : 1;
  stats_attack(&of(s, slot).tally, w, int64_t(n) * each);
}

void stats_blow(Q2Server& s, int attacker, int target, int w, int points) {
  if (!s.stats.on || !player(target) || !player(attacker) || points <= 0) return;
  const StatsGame& g = q2_stats_game();
  Q2SlotStats& victim = of(s, target);
  Q2SlotStats& by = of(s, attacker);
  const bool self = attacker == target;
  const bool enemy = !self && !fight_teammates(s, attacker, target);
  const Q2Player& body = s.players[target];
  const bool big = sq_holds(body.body, kRocket) || sq_holds(body.body, int(SqWeapon::railgun));
  stats_blow(g, &by.tally, &victim.tally, self, enemy, points, big);
  if (self || !weapon(w) || s.blow_how == kSqHowLaser) return;
  const bool blast = s.blow_how == kSqHowBlast || s.blow_how == kSqHowBurst;
  if (w == kRocket || w == kGrenade) {
    if (!blast) stats_hit(&by.tally, w, 1);  // the direct touch
    if (enemy) stats_splash(g, &by.tally, w, points);
    if (enemy && w == kRocket && !body.on_ground && body.water_level < 2)
      stats_midair(g, &by.tally,
                   static_cast<int64_t>(ge::core::num::round(double(fight_floor_gap(s, target)))));
    return;
  }
  if (w == int(SqWeapon::hand_grenade) || w == int(SqWeapon::bfg)) {  // once a player a frame
    const auto k = static_cast<size_t>(w);
    const int64_t frame = s.fast_number + 1;
    if (by.hit_frame[k] != frame) by.hit_mask[k] = 0;
    by.hit_frame[k] = frame;
    const uint32_t bit = 1u << target;
    if ((by.hit_mask[k] & bit) != 0) return;
    by.hit_mask[k] |= bit;
  }
  stats_hit(&by.tally, w, 1);
}

void stats_death(Q2Server& s, int slot, int killer, int w, bool teammate) {
  if (!s.stats.on || !player(slot)) return;
  Q2SlotStats& victim = of(s, slot);
  victim.deaths++;
  const bool other = player(killer) && killer != slot;
  const bool enemy = other && !teammate;
  if (!other)
    victim.suicides++;
  else if (teammate)
    of(s, killer).team_kills++;
  else
    of(s, killer).kills++;
  const bool quad = enemy && s.players[killer].quad_until > s.level_time;
  stats_died(q2_stats_game(), &victim.tally, enemy ? &of(s, killer).tally : nullptr, enemy,
             weapon(w) ? w : -1, quad, s.level_time);
}

void stats_spawn(Q2Server& s, int slot) {
  if (player(slot)) stats_spawned(&s.stats.slot[static_cast<size_t>(slot)].tally, s.level_time);
}

void stats_item(Q2Server& s, int slot, int item) {
  if (!s.stats.on || !player(slot) || item < 0) return;
  const SqItem& it = kSqItems[item];
  const StatsGame& g = q2_stats_game();
  PlayerStats* p = &of(s, slot).tally;
  if (it.kind == SqKind::armour && it.amount >= 1 && it.amount <= 3)
    stats_took(g, p, static_cast<StatItem>(it.amount - 1));
  else if (it.kind == SqKind::power_armour)
    stats_took(g, p, StatItem::power_c);
  else if (std::strcmp(it.classname, "item_health_mega") == 0)
    stats_took(g, p, StatItem::mega);
  else if (std::strcmp(it.classname, "item_quad") == 0)
    stats_took(g, p, StatItem::quad);
  else if (std::strcmp(it.classname, "item_invulnerability") == 0)
    stats_took(g, p, StatItem::power_b);
}

void stats_tick(Q2Server& s) {
  const double now = s.level_time, dt = now - s.stats.last_time;
  const bool step = s.stats.last_time >= 0 && dt > 0 && dt <= 1;
  s.stats.last_time = now;
  if (!s.stats.on || !step) return;
  for (int i = 0; i < 32 && i < sh::kMaxClients; i++) {
    const Q2Player& p = s.players[i];
    if (!s.clients[i].active || !p.alive || p.quad_until <= now) continue;
    stats_quad_held(q2_stats_game(), &of(s, i).tally, dt);
  }
}

}  // namespace shooters_edge::server
