// The sequel's weapons and damage as data (sequel_combat.hpp).
//
// ge:summary The sequel's weapons: the hand's step through its frames, the best weapon, armour
// ge:keywords sequel weapons hand-step fire-frame held-fire best-weapon armour-save knockback
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_combat_test.cpp
#include "shooters_edge/server/sequel_combat.hpp"

#include <algorithm>

namespace shooters_edge::server {

namespace {

const SqWeaponRow& row_of(int w) { return kSqWeapons[w]; }

int ammo_of(const SqBody& b, SqAmmo a) { return b.ammo[static_cast<int>(a)]; }

bool fold_equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    const auto low = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
    if (low(a[i]) != low(b[i])) return false;
  }
  return true;
}

// The shots of the held-fire and single weapons at the hand's frame, the frame moved on.
int fire_frame(SqHand& h, const SqBody& b, bool attack, bool infinite) {
  const SqWeaponRow& r = row_of(h.weapon);
  const int idle = r.last_fire + 1;
  const int have = infinite || r.per_shot == 0 ? 1 << 20 : ammo_of(b, r.ammo) / r.per_shot;
  switch (static_cast<SqWeapon>(h.weapon)) {
    case SqWeapon::machinegun:  // 4 and 5 in turn while held and stocked
      if (!attack || have < 1) {
        h.frame = idle;
        h.held_shots = 0;
        return 0;
      }
      h.frame = h.frame == 4 ? 5 : 4;
      h.held_shots++;
      return 1;
    case SqWeapon::chaingun: {
      if (h.frame == 14 && !attack) {  // let go before the spin: it ends here
        h.frame = idle;
        return 0;
      }
      const int want = h.frame <= 9 ? 1 : h.frame <= 14 ? 2 : 3;
      const int shots = std::min(want, have);
      if (shots == 0) {
        h.frame = idle;
        return 0;
      }
      h.frame = h.frame == 21 ? (attack && have > shots ? 15 : 22) : h.frame + 1;
      return shots;
    }
    case SqWeapon::hyperblaster:
      if (!attack || have < 1) {
        h.frame = 12;
        return 0;
      }
      h.frame++;
      return 1;
    default:
      h.frame++;
      return have >= 1 ? 1 : 0;
  }
}

}  // namespace

int sq_weapon_of_item(int item) {
  if (item < 0 || item >= kSqItemCount) return -1;
  for (int w = 0; w < kSqWeaponCount; w++)
    if (kSqWeapons[w].item != nullptr &&
        std::string_view(kSqWeapons[w].item) == kSqItems[item].classname)
      return w;
  return -1;
}

int sq_weapon_named(std::string_view name) {
  for (int w = 0; w < kSqWeaponCount; w++)
    if (fold_equal(name, kSqWeapons[w].name)) return w;
  return -1;
}

int sq_item_named(std::string_view name) {
  for (int i = 0; i < kSqItemCount; i++)
    if (fold_equal(name, kSqItems[i].pickup)) return i;
  return -1;
}

bool sq_holds(const SqBody& body, int w) {
  if (w < 0 || w >= kSqWeaponCount) return false;
  if (kSqWeapons[w].item == nullptr) return body.blaster;
  const int item = sq_item(kSqWeapons[w].item);
  return item >= 0 && body.counts[item] > 0;
}

bool sq_can_fire(const SqBody& body, int w) {
  if (!sq_holds(body, w)) return false;
  const SqWeaponRow& r = kSqWeapons[w];
  return r.per_shot == 0 || ammo_of(body, r.ammo) >= r.per_shot;
}

int sq_best_weapon(const SqBody& body) {
  constexpr SqWeapon kOrder[] = {SqWeapon::railgun,    SqWeapon::hyperblaster,  SqWeapon::chaingun,
                                 SqWeapon::machinegun, SqWeapon::super_shotgun, SqWeapon::shotgun};
  for (SqWeapon w : kOrder)
    if (sq_can_fire(body, int(w))) return int(w);
  return int(SqWeapon::blaster);
}

bool sq_choose(SqHand& hand, const SqBody& body, int w) {
  if (!sq_holds(body, w)) return false;
  if (w == hand.weapon && hand.pending < 0) return false;
  hand.pending = w == hand.weapon ? -1 : w;
  return hand.pending >= 0;
}

SqStep sq_hand_step(SqHand& h, SqBody& body, bool attack, double now, bool infinite) {
  SqStep out;
  const SqWeaponRow& r = row_of(h.weapon);
  switch (h.state) {
    case SqHandState::activating:
      if (h.frame >= r.last_activate) {
        h.state = SqHandState::ready;
        h.frame = r.last_fire + 1;
      } else {
        h.frame++;
      }
      return out;
    case SqHandState::dropping:
      if (h.frame >= r.last_deactivate) {
        h.weapon = h.pending >= 0 ? h.pending : h.weapon;
        h.pending = -1;
        h.state = SqHandState::activating;
        h.frame = 0;
        h.held_shots = 0;
        out.changed = true;
      } else {
        h.frame++;
      }
      return out;
    case SqHandState::ready:
      if (h.pending >= 0 && h.pending != h.weapon) {
        h.state = SqHandState::dropping;
        h.frame = r.last_idle + 1;
        return out;
      }
      if (!attack) {
        h.frame = h.frame >= r.last_idle ? r.last_fire + 1 : h.frame + 1;
        return out;
      }
      if (!infinite && !sq_can_fire(body, h.weapon)) {
        if (now >= h.click_at + 1) {
          out.click = true;
          h.click_at = now;
        }
        const int best = sq_best_weapon(body);
        if (best != h.weapon) h.pending = best;
        return out;
      }
      h.state = SqHandState::firing;  // from the first fire frame, in this same step
      h.frame = r.last_activate + 1;
      h.held_shots = 0;
      [[fallthrough]];
    case SqHandState::firing:
      if (h.frame > r.last_fire) {
        h.state = SqHandState::ready;
        return out;
      }
      if (h.weapon == int(SqWeapon::hyperblaster) && h.frame == 12 && attack &&
          (infinite || sq_can_fire(body, h.weapon))) {
        h.frame = 6;  // held: round again, firing at once
        out.shots = fire_frame(h, body, attack, infinite);
      } else if (h.frame >= r.fire_from && h.frame <= r.fire_to) {
        out.shots = fire_frame(h, body, attack, infinite);
      } else {
        h.frame++;
      }
      if (out.shots > 0 && !infinite && r.per_shot > 0)
        body.ammo[static_cast<int>(r.ammo)] -= out.shots * r.per_shot;
      return out;
  }
  return out;
}

int sq_armour_save(SqBody& body, int damage, bool energy) {
  if (body.armour == SqArmour::none || body.armour_count <= 0 || damage <= 0) return 0;
  const SqArmourRow& a = kSqArmours[static_cast<int>(body.armour)];
  const float share = energy ? a.energy : a.normal;
  const float exact = share * float(damage);
  int saved = static_cast<int>(exact);
  if (float(saved) < exact) saved++;  // rounded up
  saved = std::min(saved, body.armour_count);
  body.armour_count -= saved;
  if (body.armour_count == 0) body.armour = SqArmour::none;
  return saved;
}

float sq_knockback(int knockback, float mass, bool self) {
  if (knockback <= 0) return 0;
  return (self ? 1600.0f : 500.0f) * float(knockback) / std::max(mass, 50.0f);
}

}  // namespace shooters_edge::server
