// The sequel's items (sequel_items.hpp).
//
// ge:summary The sequel's items: the pickup rule of each kind, the body's start
// ge:keywords sequel items pickup armour health ammo powerup key bandolier pack sq_pick_up
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_items_test.cpp
#include "shooters_edge/server/sequel_items.hpp"

#include <algorithm>

namespace shooters_edge::server {

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
  if (it.amount == 0) {  // a shard: 2 onto the worn armour, or 2 jacket
    if (b.armour == SqArmour::none) b.armour = SqArmour::jacket;
    b.armour_count += 2;
    return true;
  }
  const auto tier = static_cast<SqArmour>(it.amount);
  const SqArmourRow& fresh = kSqArmours[it.amount];
  if (b.armour == SqArmour::none) {
    b.armour = tier;
    b.armour_count = fresh.base;
    return true;
  }
  const SqArmourRow& worn = kSqArmours[static_cast<int>(b.armour)];
  if (fresh.normal > worn.normal) {  // the new one is better: the old converted onto it
    const int carried = static_cast<int>(float(b.armour_count) * (worn.normal / fresh.normal));
    b.armour = tier;
    b.armour_count = std::min(fresh.base + carried, fresh.maximum);
    return true;
  }
  // The worn one is as good or better: the new converted onto it.
  const int added = static_cast<int>(float(fresh.base) * (fresh.normal / worn.normal));
  const int now = std::min(b.armour_count + added, worn.maximum);
  if (now <= b.armour_count) return false;
  b.armour_count = now;
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
  const bool past = it.amount == 2 || it.amount == 100;  // the small one and the mega
  if (!past && b.health >= b.max_health) return false;
  b.health += it.amount;
  if (!past) b.health = std::min(b.health, b.max_health);
  if (it.amount == 100) b.mega_until_drain = now + 5;
  b.counts[item]++;
  return true;
}

bool take_pack(const SqItem& it, SqBody& b) {
  const auto raise = [&](SqAmmo a, int limit) {
    b.max_ammo[at(a)] = std::max(b.max_ammo[at(a)], limit);
  };
  if (it.amount == 1) {  // the bandolier
    raise(SqAmmo::bullets, 250);
    raise(SqAmmo::shells, 150);
    raise(SqAmmo::cells, 250);
    raise(SqAmmo::slugs, 75);
    add_ammo(b, SqAmmo::bullets, 50);
    add_ammo(b, SqAmmo::shells, 10);
    return true;
  }
  raise(SqAmmo::bullets, 300);  // the ammo pack
  raise(SqAmmo::shells, 200);
  raise(SqAmmo::rockets, 100);
  raise(SqAmmo::grenades, 100);
  raise(SqAmmo::cells, 300);
  raise(SqAmmo::slugs, 100);
  for (const SqItem& ammo : kSqItems)
    if (ammo.kind == SqKind::ammo) add_ammo(b, ammo.ammo, ammo.amount);
  return true;
}

}  // namespace

int sq_item(std::string_view classname) {
  for (int i = 0; i < kSqItemCount; i++)
    if (classname == kSqItems[i].classname) return i;
  return -1;
}

SqBody sq_new_body() {
  SqBody b;
  b.max_ammo[at(SqAmmo::bullets)] = 200;
  b.max_ammo[at(SqAmmo::shells)] = 100;
  b.max_ammo[at(SqAmmo::rockets)] = 50;
  b.max_ammo[at(SqAmmo::grenades)] = 50;
  b.max_ammo[at(SqAmmo::cells)] = 200;
  b.max_ammo[at(SqAmmo::slugs)] = 50;
  return b;
}

bool sq_pick_up(int item, SqBody& b, bool deathmatch, int skill, double now) {
  if (item < 0 || item >= kSqItemCount) return false;
  const SqItem& it = kSqItems[item];
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
    case SqKind::ammo:
      return add_ammo(b, it.ammo, it.amount);
    case SqKind::health:
      return take_health(it, item, b, deathmatch, now);
    case SqKind::powerup: {
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
