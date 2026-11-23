// Quake II's weapons and damage as data (q2_combat.hpp).
//
// ge:summary Quake II's weapons: the hand's step through its frames, the best weapon, armour
// ge:keywords q2 weapons hand-step fire-frame held-fire best-weapon armour-save knockback
// ge:skill shooter
// ge:test tests/shooters_edge_q2_combat_test.cpp
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"

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

// The hand grenade's and the trap's step through 1..15 (q2_combat.hpp states it); ammo taken at
// the throw (a trap's even with infinite ammo).
SqStep grenade_step(SqHand& h, SqBody& body, bool attack, double now, bool infinite) {
  SqStep out;
  constexpr double kEps = 1e-6;
  const bool trap = h.weapon == int(SqWeapon::trap);
  if (h.frame < 11 || (h.frame > 11 && h.frame < 15)) {
    h.frame++;
    return out;
  }
  if (h.frame == 15) {  // waits for the fuse to end
    if (now + kEps >= h.fuse_at) {
      h.fuse_at = 0;
      h.state = SqHandState::ready;
      h.frame = 16;
    }
    return out;
  }
  if (h.fuse_at == 0) {
    h.fuse_at = now + kSqGrenadeFuse;
    out.lit = true;
  }
  const bool spent = now + kEps >= h.fuse_at;
  if (attack && !spent) return out;  // cooked in the hand
  out.shots = 1;
  out.fuse_left = spent ? 0 : h.fuse_at - now;
  out.throw_speed = spent ? 0
                          : std::clamp(kSqThrowLeast + float(3 - out.fuse_left) *
                                                           (kSqThrowMost - kSqThrowLeast) / 3,
                                       kSqThrowLeast, kSqThrowMost);
  if (!infinite || trap) body.ammo[static_cast<int>(row_of(h.weapon).ammo)] -= 1;
  if (trap) h.fuse_at = now + 1;  // frame 15 waits a second after the throw
  h.frame = 12;
  return out;
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
  const int item = sq_item(kSqWeapons[w].item, kSqWeapons[w].pack);  // its own pack's row
  if (item >= 0 && kSqItems[item].kind == SqKind::ammo)  // the hand grenade: held while stocked
    return ammo_of(body, kSqItems[item].ammo) > 0;
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

SqStep sq_hand_step(SqHand& h, SqBody& body, bool attack, double now, bool infinite, int swap) {
  swap = swap > 1 ? swap : 1;
  SqStep out;
  const SqWeaponRow& r = row_of(h.weapon);
  switch (h.state) {
    case SqHandState::activating:
      if (h.frame >= r.last_activate) {
        h.state = SqHandState::ready;
        h.frame = r.last_fire + 1;
      } else {
        h.frame = std::min(h.frame + swap, r.last_activate);
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
        h.frame = std::min(h.frame + swap, r.last_deactivate);
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
      if ((!infinite || h.weapon == int(SqWeapon::trap)) && !sq_can_fire(body, h.weapon)) {
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
      if (sq_thrown(h.weapon)) return grenade_step(h, body, attack, now, infinite);
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
      // The phalanx's mag slug goes with a pull's second ball (its frame then 9).
      if (out.shots > 0 && !infinite && r.per_shot > 0 &&
          (h.weapon != int(SqWeapon::phalanx) || h.frame == 9))
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

namespace {

// The pronoun a `gender` setting names: f or F first (or one that holds "crakhor") her, m or M
// his, anything else its; `self` the reflexive (herself, himself, itself).
std::string pronoun(const std::string& gender, bool self) {
  const char c = gender.empty() ? 0 : gender[0];
  const char* base = gender.find("crakhor") != std::string::npos || c == 'f' || c == 'F' ? "her"
                     : c == 'm' || c == 'M'                                              ? "him"
                                                                                         : "it";
  if (self) return std::string(base) + "self";
  return std::string(base) == "him" ? "his" : std::string(base) == "it" ? "its" : base;
}

}  // namespace

std::string sq_obituary(const std::string& victim, const std::string& killer, bool self, int weapon,
                        int how, const std::string& gender, bool* by_killer) {
  if (by_killer != nullptr) *by_killer = false;
  // The world's causes first, whoever dealt the blow.
  const char* world = nullptr;
  switch (weapon) {
    case kSqByFall:
      world = "cratered";
      break;
    case kSqByCrush:
      world = "was squished";
      break;
    case kSqByWater:
      world = "sank like a rock";
      break;
    case kSqBySlime:
      world = "melted";
      break;
    case kSqByLava:
      world = "does a back flip into the lava";
      break;
    case kSqByExplosion:
    case kSqByBarrel:
      world = "blew up";
      break;
    case kSqByLaser:
      world = "saw the light";
      break;
    case kSqByHurt:
      world = "was in the wrong place";
      break;
    case kSqByExit:
      world = "found a way out";
      break;
    default:
      break;
  }
  if (world != nullptr) return victim + " " + world + ".";
  if (self) {
    const bool blast = how == kSqHowBlast;
    std::string words = "killed " + pronoun(gender, true);
    if (weapon < 0)
      words = "suicides";
    else if (weapon == kSqByHeldGrenade)
      words = "tried to put the pin back in";
    else if (blast &&
             (weapon == int(SqWeapon::grenade_launcher) || weapon == int(SqWeapon::hand_grenade)))
      words = "tripped on " + pronoun(gender, false) + " own grenade";
    else if (blast && weapon == int(SqWeapon::rocket_launcher))
      words = "blew " + pronoun(gender, true) + " up";
    else if (weapon == int(SqWeapon::bfg) && (blast || how == kSqHowDirect))
      words = "should have used a smaller gun";
    else if (weapon == int(SqWeapon::trap))
      words = "sucked into his own trap";  // one word for every gender
    return victim + " " + words + ".";
  }
  if (killer.empty()) return victim + " died.";
  const char* words = nullptr;
  const char* tail = "";
  const bool blast = how == kSqHowBlast;
  switch (weapon) {
    case int(SqWeapon::blaster):
      words = "was blasted by";
      break;
    case int(SqWeapon::shotgun):
      words = "was gunned down by";
      break;
    case int(SqWeapon::super_shotgun):
      words = "was blown away by", tail = "'s super shotgun";
      break;
    case int(SqWeapon::machinegun):
      words = "was machinegunned by";
      break;
    case int(SqWeapon::chaingun):
      words = "was cut in half by", tail = "'s chaingun";
      break;
    case int(SqWeapon::grenade_launcher):
      if (blast)
        words = "was shredded by", tail = "'s shrapnel";
      else
        words = "was popped by", tail = "'s grenade";
      break;
    case int(SqWeapon::rocket_launcher):
      if (blast)
        words = "almost dodged", tail = "'s rocket";
      else
        words = "ate", tail = "'s rocket";
      break;
    case int(SqWeapon::hyperblaster):
      words = "was melted by", tail = "'s hyperblaster";
      break;
    case int(SqWeapon::railgun):
      words = "was railed by";
      break;
    case int(SqWeapon::bfg):
      if (how == kSqHowLaser)
        words = "saw the pretty lights from", tail = "'s BFG";
      else if (how == kSqHowBurst)
        words = "couldn't hide from", tail = "'s BFG";
      else
        words = "was disintegrated by", tail = "'s BFG blast";
      break;
    case int(SqWeapon::hand_grenade):
      if (blast)
        words = "didn't see", tail = "'s handgrenade";
      else
        words = "caught", tail = "'s handgrenade";
      break;
    case kSqByHeldGrenade:
      words = "feels", tail = "'s pain";
      break;
    case int(SqWeapon::ion_ripper):
      words = "ripped to shreds by", tail = "'s ripper gun";
      break;
    case int(SqWeapon::phalanx):
      words = "was evaporated by";
      break;
    case int(SqWeapon::trap):
      words = "caught in trap by";
      break;
    case kSqByTelefrag:
      words = "tried to invade", tail = "'s personal space";
      break;
    default:
      break;
  }
  if (words == nullptr) return victim + " died.";
  if (by_killer != nullptr) *by_killer = true;
  return victim + " " + words + " " + killer + tail;
}

float sq_enter_box(sh::Vec3f from, sh::Vec3f to, sh::Vec3f lo, sh::Vec3f hi) {
  const float a[3] = {from.x, from.y, from.z}, b[3] = {to.x, to.y, to.z};
  const float l[3] = {lo.x, lo.y, lo.z}, h[3] = {hi.x, hi.y, hi.z};
  float t0 = 0, t1 = 1;
  for (int k = 0; k < 3; k++) {
    const float d = b[k] - a[k];
    if (d == 0) {
      if (a[k] < l[k] || a[k] > h[k]) return -1;
      continue;
    }
    float near = (l[k] - a[k]) / d, far = (h[k] - a[k]) / d;
    if (near > far) std::swap(near, far);
    t0 = std::max(t0, near);
    t1 = std::min(t1, far);
    if (t0 > t1) return -1;
  }
  return t0;
}

}  // namespace shooters_edge::server
