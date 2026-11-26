// Quake II's items (q2_items.hpp).
//
// ge:summary Quake II's items: the pickup rule of each kind, the body's start
// ge:keywords q2 items pickup armour health ammo powerup key bandolier pack sq_pick_up
// ge:skill shooter
// ge:test tests/shooters_edge_q2_items_test.cpp
#include "shooters_edge/server/q2_items.hpp"

#include <algorithm>

#include "classic/q1_body.hpp"
#include "classic/q2_body.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"

namespace shooters_edge::server {

namespace gc = ge::classic;

namespace {

int at(SqAmmo a) { return static_cast<int>(a); }

// Ammo of `kind` added up to the limit; false when already at it.
bool add_ammo(SqBody& b, SqAmmo kind, int amount) {
  int& have = b.ammo[at(kind)];
  if (have >= b.max_ammo[at(kind)]) return false;
  have = std::min(have + amount, b.max_ammo[at(kind)]);
  return true;
}

bool take_armour(const SqItem& it, SqBody& b) {
  const int worn = static_cast<int>(b.armour);
  if (b.quake && it.amount != 0) {  // Quake's armour (classic remix): the better one replaces
    if (!gc::q1_takes_armour(worn, b.armour_count, it.amount)) return false;
    b.armour = static_cast<SqArmour>(it.amount);
    b.armour_count = gc::kQ1Armours[it.amount].value;
    return true;
  }
  const gc::Q2Armour now = gc::q2_take_armour(worn, b.armour_count, it.amount);
  if (!now.taken) return false;
  b.armour = static_cast<SqArmour>(now.tier);
  b.armour_count = now.count;
  return true;
}

bool take_health(const SqItem& it, int item, SqBody& b, bool deathmatch, double now) {
  const std::string_view name = it.classname;
  if (name == "item_adrenaline") {
    if (!deathmatch) b.max_health += 1;
    b.health = std::max(b.health, b.max_health);
    return true;
  }
  if (name == "item_ancient_head") {
    b.max_health += 2;
    return true;
  }
  const bool mega = it.amount == gc::kQ2HealthMega;
  if (b.quake) {  // Quake's boxes (classic remix): the small ones 15, the large 25, the mega to 250
    const int limit = mega ? gc::kQ1MegaLimit : b.max_health;
    if (b.health >= limit) return false;
    const int points = mega                              ? gc::kQ1HealthMega
                       : it.amount == gc::kQ2HealthLarge ? gc::kQ1HealthPlain
                                                         : gc::kQ1HealthSmall;
    b.health = std::min(b.health + points, limit);
    if (mega) b.mega_until_drain = now + gc::kQ1MegaDrainAfter;
    b.counts[item]++;
    return true;
  }
  const bool past = it.amount == gc::kQ2HealthStim || mega;  // the small one and the mega
  if (!past && b.health >= b.max_health) return false;
  b.health += it.amount;
  if (!past) b.health = std::min(b.health, b.max_health);
  if (mega) b.mega_until_drain = now + gc::kQ2MegaDrainAfter;
  b.counts[item]++;
  return true;
}

bool take_pack(const SqItem& it, SqBody& b) {
  const auto raise = [&](SqAmmo a, int limit) {
    b.max_ammo[at(a)] = std::max(b.max_ammo[at(a)], limit);
  };
  // The Reckoning's mag slugs are carried only under it (a limit of 0 elsewhere): raised there too.
  const bool magslugs = b.max_ammo[at(SqAmmo::magslugs)] > 0;
  if (it.amount == 1) {  // the bandolier
    raise(SqAmmo::bullets, 250);
    raise(SqAmmo::shells, 150);
    raise(SqAmmo::cells, 250);
    raise(SqAmmo::slugs, 75);
    if (magslugs) raise(SqAmmo::magslugs, 75);
    add_ammo(b, SqAmmo::bullets, 50);
    add_ammo(b, SqAmmo::shells, 10);
    if (b.max_ammo[at(SqAmmo::flechettes)] > 0) {  // Ground Zero's body
      raise(SqAmmo::flechettes, 250);
      raise(SqAmmo::rounds, 150);
    }
    return true;
  }
  raise(SqAmmo::bullets, 300);  // the ammo pack
  raise(SqAmmo::shells, 200);
  raise(SqAmmo::rockets, 100);
  raise(SqAmmo::grenades, 100);
  raise(SqAmmo::cells, 300);
  raise(SqAmmo::slugs, 100);
  if (magslugs) raise(SqAmmo::magslugs, 100);
  if (b.max_ammo[at(SqAmmo::flechettes)] > 0) {  // Ground Zero's body
    raise(SqAmmo::flechettes, 300);
    raise(SqAmmo::rounds, 200);
  }
  // One box of each ammo but the traps and the mines (they keep their limit and count).
  for (const SqItem& ammo : kSqItems)
    if (ammo.kind == SqKind::ammo && ammo.ammo != SqAmmo::traps && ammo.ammo != SqAmmo::prox &&
        ammo.ammo != SqAmmo::tesla)
      add_ammo(b, ammo.ammo, ammo.amount);
  return true;
}

}  // namespace

int sq_item(std::string_view classname, Q2Pack pack) {
  if (pack != Q2Pack::base)  // the pack's own row first: one may replace a base item
    for (int i = 0; i < kSqItemCount; i++)
      if (kSqItems[i].pack == pack && classname == kSqItems[i].classname) return i;
  for (int i = 0; i < kSqItemCount; i++)
    if (kSqItems[i].pack == Q2Pack::base && classname == kSqItems[i].classname) return i;
  return -1;
}

bool sq_turns(int item) {
  if (item < 0 || item >= kSqItemCount) return false;
  const SqItem& it = kSqItems[item];
  const std::string_view name = it.classname;
  switch (it.kind) {
    case SqKind::ammo:
      return false;
    case SqKind::health:
      return name == "item_adrenaline" || name == "item_ancient_head";
    case SqKind::key:
      return name != "key_commander_head";
    default:
      return true;
  }
}

SqBody sq_new_body(Q2Pack pack) {
  SqBody b;
  if (q2_pack_has(pack, Q2Pack::reckoning)) {
    b.max_ammo[at(SqAmmo::magslugs)] = 50;
    b.max_ammo[at(SqAmmo::traps)] = 5;  // nothing raises it
  }
  if (pack == Q2Pack::ground_zero) {  // its own carry limits
    b.max_ammo[at(SqAmmo::flechettes)] = 200;
    b.max_ammo[at(SqAmmo::prox)] = 50;
    b.max_ammo[at(SqAmmo::tesla)] = 50;
    b.max_ammo[at(SqAmmo::rounds)] = 100;
  }
  b.max_ammo[at(SqAmmo::bullets)] = gc::kQ2MostBullets;
  b.max_ammo[at(SqAmmo::shells)] = gc::kQ2MostShells;
  b.max_ammo[at(SqAmmo::rockets)] = gc::kQ2MostRockets;
  b.max_ammo[at(SqAmmo::grenades)] = gc::kQ2MostGrenades;
  b.max_ammo[at(SqAmmo::cells)] = gc::kQ2MostCells;
  b.max_ammo[at(SqAmmo::slugs)] = gc::kQ2MostSlugs;
  return b;
}

bool sq_pick_up(int item, SqBody& b, bool deathmatch, int skill, double now, int count) {
  if (item < 0 || item >= kSqItemCount) return false;
  const SqItem& it = kSqItems[item];
  if (b.quake) {  // Quake's weapons worn: its weapon and its amounts (q2_quake_arms.hpp)
    const SqQuakeTake t = sq_quake_take(item);
    if (t.ammo != SqAmmo::none && it.kind == SqKind::weapon) {
      add_ammo(b, t.ammo, t.amount);
      b.counts[t.item]++;
      return true;
    }
    if (t.ammo != SqAmmo::none) return add_ammo(b, t.ammo, count > 0 ? count : t.amount);
  }
  switch (it.kind) {
    case SqKind::armour:
      return take_armour(it, b);
    case SqKind::power_armour:
    case SqKind::key:
      b.counts[item]++;
      return true;
    case SqKind::weapon: {
      add_ammo(b, it.ammo, it.amount);  // a weapon is always taken, its ammo up to the limit
      b.counts[item]++;
      return true;
    }
    case SqKind::ammo:  // a map's `count` on ammo gives that many in place of the box's own
      return add_ammo(b, it.ammo, count > 0 ? count : it.amount);
    case SqKind::health:
      if (std::string_view(it.classname) == "foodcube") {  // The Reckoning's: its count, past all
        b.health += count;
        return true;
      }
      return take_health(it, item, b, deathmatch, now);
    case SqKind::powerup: {
      const std::string_view name = it.classname;
      if ((name == "ammo_nuke" || name == "item_doppleganger") && b.counts[item] >= 1)
        return false;  // one at a time
      if (name == "item_doppleganger" && !deathmatch) return false;
      const int hold = skill <= 1 ? 2 : 1;
      if (skill >= 1 && b.counts[item] >= hold) return false;
      b.counts[item]++;
      return true;
    }
    case SqKind::pack:
      return take_pack(it, b);
  }
  return false;
}

}  // namespace shooters_edge::server
