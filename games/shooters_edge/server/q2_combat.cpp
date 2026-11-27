// Quake II's weapons and damage as data (q2_combat.hpp).
//
// ge:summary Quake II's weapons: the hand's step through its frames, the best weapon, armour
// ge:keywords q2 weapons hand-step fire-frame held-fire best-weapon armour-save knockback
// ge:skill shooter
// ge:test tests/shooters_edge_q2_combat_test.cpp
#include "shooters_edge/server/q2_combat.hpp"

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"

#include <algorithm>

#include "classic/q2_arms.hpp"

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

int sq_weapon_of_item(int item, bool quake) {
  if (item < 0 || item >= kSqItemCount) return -1;
  for (int w = 0; w < kSqWeaponCount; w++)
    if (kSqWeapons[w].quake == quake && kSqWeapons[w].item != nullptr &&
        std::string_view(kSqWeapons[w].item) == kSqItems[item].classname)
      return w;
  return -1;
}

int sq_weapon_named(std::string_view name, bool quake) {
  for (int w = 0; w < kSqWeaponCount; w++)
    if (kSqWeapons[w].quake == quake && fold_equal(name, kSqWeapons[w].name)) return w;
  return -1;
}

int sq_item_named(std::string_view name) {
  for (int i = 0; i < kSqItemCount; i++)
    if (fold_equal(name, kSqItems[i].pickup)) return i;
  return -1;
}

bool sq_holds(const SqBody& body, int w) {
  if (w < 0 || w >= kSqWeaponCount) return false;
  if (kSqWeapons[w].quake != body.quake) return false;  // Quake's alone when worn, else never
  if (kSqWeapons[w].item == nullptr) return body.quake || body.blaster;
  const int item = sq_item(kSqWeapons[w].item, kSqWeapons[w].pack);
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
  if (body.quake) {  // Quake's choice: the lightning gun down to the axe, the launchers never
    constexpr SqWeapon kOrder[] = {SqWeapon::q1_lightning, SqWeapon::q1_super_nailgun,
                                   SqWeapon::q1_super_shotgun, SqWeapon::q1_nailgun,
                                   SqWeapon::q1_shotgun};
    for (SqWeapon w : kOrder)
      if (sq_can_fire(body, int(w))) return int(w);
    return int(SqWeapon::q1_axe);
  }
  if (body.max_ammo[static_cast<int>(SqAmmo::flechettes)] > 0) return gz_best_weapon(body);
  constexpr SqWeapon kOrder[] = {SqWeapon::railgun,    SqWeapon::hyperblaster,  SqWeapon::chaingun,
                                 SqWeapon::machinegun, SqWeapon::super_shotgun, SqWeapon::shotgun};
  for (SqWeapon w : kOrder)
    if (sq_can_fire(body, int(w))) return int(w);
  return int(SqWeapon::blaster);
}

void sq_hand_hasten(SqHand& h, int mode) {
  if (mode <= 0) return;
  const SqWeaponRow& r = row_of(h.weapon);
  if (h.state == SqHandState::ready && h.pending >= 0 && h.pending != h.weapon) {
    h.state = SqHandState::dropping;
    h.frame = r.last_deactivate;  // lowered: this step swaps
  } else if (h.state == SqHandState::dropping) {
    h.frame = r.last_deactivate;
  } else if (mode >= 2 && h.state == SqHandState::activating) {
    h.frame = r.last_activate;  // raised: this step makes it ready
  }
}

bool sq_choose(SqHand& hand, const SqBody& body, int w) {
  if (!sq_holds(body, w)) return false;
  if (w == hand.weapon && hand.pending < 0) return false;
  hand.pending = w == hand.weapon ? -1 : w;
  return hand.pending >= 0;
}

SqStep sq_hand_step(SqHand& h, SqBody& body, bool attack, double now, bool infinite, int swap,
                    uint32_t idle_draw) {
  swap = swap > 1 ? swap : 1;
  SqStep out;
  const SqWeaponRow& r = row_of(h.weapon);
  switch (h.state) {
    case SqHandState::activating:
      if (ge::classic::q2_raise_step(sq_runs(r), h.frame, swap)) h.state = SqHandState::ready;
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
        ge::classic::q2_idle_step(sq_runs(r), sq_pauses(h.weapon), h.frame, idle_draw);
        return out;
      }
      if ((!infinite || h.weapon == int(SqWeapon::trap) || h.weapon == int(SqWeapon::disruptor)) &&
          !sq_can_fire(body, h.weapon)) {
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
      if (r.pack == Q2Pack::ground_zero) return gz_hand_fire(h, body, attack, now, infinite);
      if (sq_thrown(h.weapon)) return grenade_step(h, body, attack, now, infinite);
      {
        const int have = infinite || r.per_shot == 0 ? 1 << 20 : ammo_of(body, r.ammo) / r.per_shot;
        const bool again = attack && (infinite || sq_can_fire(body, h.weapon));
        const ge::classic::Q2Firing f = ge::classic::q2_firing_step(
            sq_runs(r), sq_held_fire(h.weapon), h.frame, h.held_shots, attack, have, again);
        if (f.done) {
          h.state = SqHandState::ready;
          return out;
        }
        out.shots = f.shots;
      }
      // The phalanx's mag slug goes with a pull's second ball (its frame then 9).
      if (out.shots > 0 && !infinite && r.per_shot > 0 &&
          (h.weapon != int(SqWeapon::phalanx) || h.frame == 9))
        body.ammo[static_cast<int>(r.ammo)] -= out.shots * r.per_shot;
      return out;
  }
  return out;
}

int sq_hand_sound(int weapon, int was, bool was_firing, const SqHand& after) {
  if (after.weapon != weapon || (after.frame == was && was_firing)) return -1;
  if (weapon < ge::classic::kQ2ArmCount)  // the base game's: the shared hand's (q2_arms.hpp)
    return ge::classic::q2_arm_sound(static_cast<ge::classic::Q2ArmId>(weapon), was, was_firing,
                                     after.frame, after.state == SqHandState::firing);
  switch (static_cast<SqWeapon>(weapon)) {
    case SqWeapon::trap:  // The Reckoning's trap cocked
      return after.frame == 5 ? 4 : -1;
    default:
      return -1;
  }
}

const char* sq_hand_loop(const SqHand& h) {
  const auto w = static_cast<SqWeapon>(h.weapon);
  const bool firing = h.state == SqHandState::firing;
  if (w == SqWeapon::railgun) return "weapons/rg_hum.wav";
  if (w == SqWeapon::bfg) return "weapons/bfg_hum.wav";
  if (w == SqWeapon::hyperblaster && firing) return "weapons/hyprbl1a.wav";
  if (w == SqWeapon::chaingun && firing) return "weapons/chngnl1a.wav";
  if (w == SqWeapon::hand_grenade && h.fuse_at > 0 && h.frame == 11)
    return "weapons/hgrenc1b.wav";  // lit and still in the hand
  if (w == SqWeapon::trap && h.fuse_at > 0 && h.frame == 11) return "weapons/traploop.wav";
  if (w == SqWeapon::phalanx) return "weapons/phaloop.wav";
  return nullptr;
}

int sq_flash_value(int w, int shots) {
  switch (static_cast<SqWeapon>(w)) {
    case SqWeapon::blaster:
      return 0;
    case SqWeapon::machinegun:
      return 1;
    case SqWeapon::shotgun:
      return 2;
    case SqWeapon::chaingun:
      return 3 + std::clamp(shots, 1, 3) - 1;
    case SqWeapon::railgun:
      return 6;
    case SqWeapon::rocket_launcher:
      return 7;
    case SqWeapon::grenade_launcher:
      return 8;
    case SqWeapon::bfg:
      return 12;
    case SqWeapon::super_shotgun:
      return 13;
    case SqWeapon::hyperblaster:
      return 14;
    case SqWeapon::ion_ripper:
      return 16;
    case SqWeapon::phalanx:  // ball one's only
      return shots > 0 ? 18 : -1;
    default:
      return -1;
  }
}

bool sq_flash_plays(int value, const char* sample) {
  const Q2Flash* f = q2_player_flash(value);
  if (f == nullptr || f->sounds[0].name == nullptr || sample == nullptr) return false;
  const std::string_view shot = f->sounds[0].name, own = sample;
  const size_t wild = shot.find('%');
  return wild == std::string_view::npos ? shot == own : own.substr(0, wild) == shot.substr(0, wild);
}

int sq_armour_save(SqBody& body, int damage, bool energy) {
  if (body.armour == SqArmour::none || body.armour_count <= 0 || damage <= 0) return 0;
  const SqArmourRow& a = kSqArmours[static_cast<int>(body.armour)];
  const float share = energy && !body.quake ? a.energy : a.normal;  // Quake's knows no energy
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
  return ge::classic::q2_push(knockback, mass, self);
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
    case kSqByGotta:
      world = "that's gotta hurt";
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
    else if (blast && (weapon == int(SqWeapon::rocket_launcher) ||
                       weapon == int(SqWeapon::q1_rocket_launcher) ||
                       weapon == int(SqWeapon::q1_grenade_launcher)))
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
  if (weapon >= 0 && weapon < ge::classic::kQ2ArmCount &&
      weapon != int(SqWeapon::bfg)) {  // the base game's (classic/q2_arms.hpp)
    const ge::classic::Q2ArmWords w =
        ge::classic::q2_arm_words(static_cast<ge::classic::Q2ArmId>(weapon), blast);
    words = w.words, tail = w.tail;
  }
  if (sq_quake_weapon(weapon)) {  // Quake's (q2_quake_arms.hpp)
    const SqQuakeWords w = sq_quake_words(weapon);
    words = w.words, tail = w.tail;
  }
  switch (weapon) {
    case int(SqWeapon::bfg):
      if (how == kSqHowLaser)
        words = "saw the pretty lights from", tail = "'s BFG";
      else if (how == kSqHowBurst)
        words = "couldn't hide from", tail = "'s BFG";
      else
        words = "was disintegrated by", tail = "'s BFG blast";
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
    // Ground Zero's (q2_gz.hpp).
    case int(SqWeapon::chainfist):
      words = "was shredded by", tail = "'s ripsaw";
      break;
    case int(SqWeapon::etf_rifle):
      words = "was perforated by";
      break;
    case int(SqWeapon::plasma_beam):
      words = "was scorched by", tail = "'s plasma beam";
      break;
    case int(SqWeapon::tesla):
      words = "was enlightened by", tail = "'s tesla mine";
      break;
    case int(SqWeapon::prox_launcher):
      words = "got too close to", tail = "'s proximity mine";
      break;
    case kSqByAmBomb:
      words = "was nuked by", tail = "'s antimatter bomb";
      break;
    case kSqByVengeance:
      words = "was purged by", tail = "'s vengeance sphere";
      break;
    case kSqByDefender:
      words = "had a blast with", tail = "'s defender sphere";
      break;
    case kSqByHunter:
      words = "was killed like a dog by", tail = "'s hunter sphere";
      break;
    case kSqByDoppleVengeance:
      words = "was purged by", tail = "'s doppleganger";
      break;
    case kSqByDoppleHunter:
      words = "was hunted down by", tail = "'s doppleganger";
      break;
    case int(SqWeapon::disruptor):
      words = "was annihilated by", tail = "'s disruptor";
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
