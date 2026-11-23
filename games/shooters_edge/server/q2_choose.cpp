// Quake II's weapon choosing (q2_fight.hpp's `fight_took` and `fight_command`): the weapon a
// pickup takes in hand, `use` and `drop` by name, `weapnext` and `weapprev`. The Reckoning
// (q2_pack.hpp) adds its own: its cycle by the item list's order, `use hyperblaster` toggling to the
// Ionripper and `use railgun` to the Phalanx (porting note: q2-reckoning-rules.md § 7).
//
// ge:summary Quake II's weapon choosing: the pickup's switch, use and drop by name, the cycle,
// ge:summary The Reckoning's hyperblaster and railgun toggles
// ge:keywords q2 use-weapon weapnext weapprev weapon-cycle g_select_empty g_quick_weap pickup-switch
// ge:keywords reckoning ionripper phalanx trap toggle
// ge:skill shooter
// ge:test tests/shooters_edge_q2_reckoning_test.cpp
#include <iterator>
#include <string>
#include <string_view>

#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

bool fightable(const Q2Server& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}

// The weapon called `name` that plays under the server's pack; -1 none.
int weapon_named(const Q2Server& s, std::string_view name) {
  const int w = sq_weapon_named(name);
  return sq_weapon_plays(w, s.opt.pack) ? w : -1;
}

// The weapon a thrown ammunition is (the grenades, The Reckoning's traps); -1 none.
int thrown_of(SqAmmo a) {
  return a == SqAmmo::grenades ? int(SqWeapon::hand_grenade)
         : a == SqAmmo::traps  ? int(SqWeapon::trap)
                               : -1;
}

// The cycle `weapnext` walks: the base game's by its weapons' order, The Reckoning's by its item
// list's (the trap after the grenades, each pack weapon after the one it shares a key with).
constexpr SqWeapon kReckoningCycle[] = {
    SqWeapon::blaster,      SqWeapon::shotgun,          SqWeapon::super_shotgun,
    SqWeapon::machinegun,   SqWeapon::chaingun,         SqWeapon::hand_grenade,
    SqWeapon::trap,         SqWeapon::grenade_launcher, SqWeapon::rocket_launcher,
    SqWeapon::hyperblaster, SqWeapon::ion_ripper,       SqWeapon::railgun,
    SqWeapon::phalanx,      SqWeapon::bfg};
constexpr int kReckoningCycleSize = static_cast<int>(std::size(kReckoningCycle));

int cycle_size(const Q2Server& s) {
  return s.opt.pack == Q2Pack::reckoning ? kReckoningCycleSize : int(SqWeapon::hand_grenade) + 1;
}
int cycle_at(const Q2Server& s, int k) {
  return s.opt.pack == Q2Pack::reckoning ? int(kReckoningCycle[k]) : k;
}
int cycle_place(const Q2Server& s, int w) {
  for (int k = 0; k < cycle_size(s); k++)
    if (cycle_at(s, k) == w) return k;
  return 0;
}

// The line a refused weapon prints: "No Cells for HyperBlaster.", or "Not enough" with too few.
std::string short_of(const SqBody& b, int w) {
  std::string say = b.ammo[int(kSqWeapons[w].ammo)] > 0 ? "Not enough " : "No ";
  for (const SqItem& it : kSqItems)
    if (it.kind == SqKind::ammo && it.ammo == kSqWeapons[w].ammo) {
      say += it.pickup;
      break;
    }
  return say + " for " + kSqWeapons[w].name + ".\n";
}

// The Reckoning's toggles: the weapon `use` of `w` (the HyperBlaster or the Railgun) chooses in its
// place, or -1 to choose `w` as the base game does; `refused` the line when nothing is chosen.
int reckoning_toggle(const Q2Server& s, int slot, int w, std::string& refused) {
  if (s.opt.pack != Q2Pack::reckoning) return -1;
  const Q2Player& p = s.players[slot];
  const SqBody& b = p.body;
  const auto ammo = [&b](SqAmmo a) { return b.ammo[static_cast<int>(a)]; };
  if (w == int(SqWeapon::hyperblaster)) {
    const int ripper = int(SqWeapon::ion_ripper);
    if (!sq_holds(b, w)) return sq_holds(b, ripper) ? ripper : -1;
    if (p.hand.weapon != w) return -1;
    // In hand: the Ionripper when held with any cells at all; else nothing.
    return sq_holds(b, ripper) && ammo(SqAmmo::cells) > 0 ? ripper : -2;
  }
  if (w != int(SqWeapon::railgun)) return -1;
  const int phalanx = int(SqWeapon::phalanx);
  if (!sq_holds(b, w)) return sq_holds(b, phalanx) ? phalanx : -1;
  if (ammo(SqAmmo::slugs) <= 0) {
    if (p.hand.weapon == phalanx) return -2;  // kept
    if (ammo(SqAmmo::magslugs) > 0 && sq_holds(b, phalanx)) return phalanx;
    refused = short_of(b, w);
    return -2;
  }
  if (p.hand.weapon != w) return w;
  if (!sq_holds(b, phalanx)) return -2;
  if (ammo(SqAmmo::magslugs) <= 0) {
    refused = short_of(b, phalanx);
    return -2;
  }
  return phalanx;
}

}  // namespace

void fight_took(Q2Server& s, int slot, int item, int before) {
  const int w = sq_weapon_of_item(item);
  Q2Player& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  const bool dm = deathmatch(s);
  // The status bar's pickup (3 s), and the item selected when it has a use.
  p.picked = sq_item_number(item, s.opt.pack);
  p.picked_at = s.level_time;
  if (sq_selects(p.picked, s.opt.pack)) p.body.selected = p.picked;
  if (it.kind == SqKind::power_armour && dm && p.body.counts[item] == 1) p.power_on = true;
  if (it.kind == SqKind::powerup && dm && (s.cvars.find("dmflags")->integer() & 16) != 0)
    fight_use(s, slot, item);  // instant items
  const bool may = !dm || p.hand.weapon == int(SqWeapon::blaster);
  // Grenades (and traps), both ammunition and a weapon: the first held (none before) are taken in
  // hand.
  const int thrown = thrown_of(it.ammo);
  if (it.kind == SqKind::ammo && thrown >= 0 &&
      (before >= 0 ? before == 0 : p.body.ammo[static_cast<int>(it.ammo)] == it.amount) &&
      p.hand.weapon != thrown && may)
    sq_choose(p.hand, p.body, thrown);
  if (w < 0 || p.body.counts[item] != 1) return;  // only a newly owned weapon
  if (may) sq_choose(p.hand, p.body, w);
}

bool fight_command(Q2Server& s, int slot, const std::string& cmd, const std::string& arg) {
  Q2Player& p = s.players[slot];
  if (cmd == "use") {
    int w = weapon_named(s, arg);
    if (w >= 0 && fightable(s, slot)) {
      std::string refused;
      const int toggled = reckoning_toggle(s, slot, w, refused);
      if (toggled == -2) {
        if (!refused.empty()) fight_print(s, slot, refused);
        return true;
      }
      if (toggled >= 0) w = toggled;
    }
    // g_select_empty 0: a held weapon short of a shot's ammunition is refused and the player told.
    if (w >= 0 && !sq_thrown(w) && fightable(s, slot) && sq_holds(p.body, w) &&
        !sq_can_fire(p.body, w) && !fight_anyflag(s, 8192) &&
        !s.cvars.find("g_select_empty")->on()) {
      fight_print(s, slot, short_of(p.body, w));
      return true;
    }
    if (w >= 0)
      fight_choose(s, slot, w);
    else
      fight_use(s, slot, sq_item_named(arg));
    return true;
  }
  if (cmd == "drop") {
    const int item = sq_item_named(arg);
    const int w = weapon_named(s, arg);
    fight_drop(s, slot,
               w >= 0 && kSqWeapons[w].item != nullptr ? sq_item(kSqWeapons[w].item, s.opt.pack)
                                                       : item);
    return true;
  }
  if (cmd != "weapnext" && cmd != "weapprev") return false;
  const int n = cycle_size(s);
  const int way = cmd == "weapnext" ? 1 : n - 1;
  // g_quick_weap 1: from the weapon chosen but not yet raised, so presses skip ahead, its icon
  // and name shown 0.9 s (the bar's pickup line, which shows 3 s, set 2.1 s back).
  const bool quick = s.cvars.find("g_quick_weap")->on();
  const int from = cycle_place(s, quick && p.hand.pending >= 0 ? p.hand.pending : p.hand.weapon);
  for (int k = 1; k < n; k++) {
    const int w = cycle_at(s, (from + way * k) % n);
    if (sq_can_fire(p.body, w)) {
      fight_choose(s, slot, w);
      if (quick && p.hand.pending == w && kSqWeapons[w].item != nullptr) {
        p.picked = sq_item_number(sq_item(kSqWeapons[w].item, s.opt.pack), s.opt.pack);
        p.picked_at = s.level_time - 2.1;
      }
      break;
    }
  }
  return true;
}

}  // namespace shooters_edge::server
