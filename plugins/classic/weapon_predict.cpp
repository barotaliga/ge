// Quake's held weapon predicted on the client (weapon_predict.hpp).
//
// ge:summary Quake's held weapon predicted on the client: the clock, thinks, refire, ammo, sounds
// ge:keywords weapon-prediction weapon-clock think-step refire best-weapon predicted-sound
// ge:skill classic net
// ge:test tests/classic_weapon_predict_test.cpp
#include "classic/weapon_predict.hpp"

#include <cstdlib>

namespace ge::classic {

namespace {

constexpr float kThink = 0.1f;    // s between a weapon's thinks
constexpr float kPart = 0.05f;    // s: a command longer than this is split
constexpr float kFrozen = 0.05f;  // s the refire is held past the clock while frozen

enum Ammo : uint8_t { kShells, kNails, kRockets, kCells };

using R = WeaponRules;

// The axe's value under the rules.
int axe_of(R r) { return r == R::dissolution ? kItAxeEternity : kItAxe; }

// The ammunition index a weapon fires (the count the bar shows: a Dissolution pack gun's own
// ammunition stands in its base gun's place); -1: none.
int ammo_index(R r, int weapon) {
  switch (weapon) {
    case kItShotgun:
    case kItSuperShotgun:
      return kShells;
    case kItNailgun:
    case kItSuperNailgun:
      return kNails;
    case kItGrenadeLauncher:
    case kItRocketLauncher:
      return kRockets;
    case kItLightning:
      return kCells;
    default:
      break;
  }
  if (r == R::scourge) {
    if (weapon == kItLaserCannon) return kCells;
    if (weapon == kItProximityGun) return kRockets;
    return -1;  // the axe and Mjolnir
  }
  if (r == R::dissolution) switch (weapon) {
      case kItLavaNailgun:
      case kItLavaSuperNailgun:
        return kNails;
      case kItMultiGrenade:
      case kItMultiRocket:
        return kRockets;
      case kItPlasmaGun:
        return kCells;
      default:
        return -1;
    }
  return -1;
}
int ammo_needed(R r, int weapon) {
  if (weapon == kItSuperShotgun || weapon == kItSuperNailgun) return 2;
  return ammo_index(r, weapon) < 0 ? 0 : 1;
}
// Whether the held weapon is switched away from when it runs dry (the axe and Mjolnir never).
bool runs_dry(R r, int weapon) {
  return weapon != axe_of(r) && !(r == R::scourge && weapon == kItMjolnir);
}
// The weapons that fire a nail or a bolt every think while held.
bool nail_kind(R r, int weapon) {
  return weapon == kItNailgun || weapon == kItSuperNailgun ||
         (r == R::dissolution && (weapon == kItLavaNailgun || weapon == kItLavaSuperNailgun));
}
bool super_nail(R r, int weapon) {
  return weapon == kItSuperNailgun || (r == R::dissolution && weapon == kItLavaSuperNailgun);
}
bool laser(R r, int weapon) { return r == R::scourge && weapon == kItLaserCannon; }
bool mjolnir(R r, int weapon) { return r == R::scourge && weapon == kItMjolnir; }

// Held, with the ammunition a selection needs.
bool can_select(const WeaponState& s, const WeaponWorld& w, int weapon) {
  if ((w.items & weapon) == 0) return false;
  const int a = ammo_index(w.rules, weapon);
  return a < 0 || s.ammo[a] >= ammo_needed(w.rules, weapon);
}

// The weapons by impulse 1..8 and in the cycle's order.
constexpr int kByImpulse[8] = {
    kItAxe,          kItShotgun,         kItSuperShotgun,   kItNailgun,
    kItSuperNailgun, kItGrenadeLauncher, kItRocketLauncher, kItLightning};
// Scourge's cycle: the proximity gun after the launcher, the laser cannon and Mjolnir last.
constexpr int kScourgeCycle[11] = {kItAxe,          kItShotgun,        kItSuperShotgun,
                                   kItNailgun,      kItSuperNailgun,   kItGrenadeLauncher,
                                   kItProximityGun, kItRocketLauncher, kItLightning,
                                   kItLaserCannon,  kItMjolnir};

void rest(WeaponState& s) {
  s.frame = 0;
  s.step = 0;
  s.next_think = 0;
}

void change_to(WeaponState& s, int weapon) {
  if (weapon == s.weapon) return;
  s.weapon = weapon;
  rest(s);
}

// A change the prediction cannot follow: Dissolution's (its counts change meaning across 4096).
void lose_track(WeaponState& s) { change_to(s, kWeaponUnknown); }

// Changed for the best weapon when the held one runs dry.
void run_dry(WeaponState& s, const WeaponWorld& w, int waterlevel) {
  if (w.rules == R::dissolution)
    lose_track(s);
  else
    change_to(s, weapon_best(s, w, waterlevel));
}

void sound(WeaponEvent* out, int room, int& n, WeaponSound what, float at) {
  if (n < room) out[n] = {what, at};
  n++;
}

// The axe's first swing (frames 1..4) or second (5..8) by a fixed hash of the clock.
int axe_base(float clock) {
  const auto whole = static_cast<int32_t>(clock * 931.75f);
  const auto shifted = static_cast<int32_t>(static_cast<uint32_t>(whole) << 11);
  const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(shifted) +
                                           static_cast<uint32_t>(static_cast<int32_t>(clock) >> 6));
  const float r = static_cast<float>(std::abs(sum % 1000)) / 1000.0f;
  return r < 0.25f || (r >= 0.5f && r < 0.75f) ? 1 : 5;
}

// One barrel of the nailguns or one step of the lightning gun, at `at`: false when out of
// ammunition (the weapon is changed for the best one instead).
bool fire_step(WeaponState& s, const WeaponWorld& w, int waterlevel, float at, WeaponEvent* out,
               int room, int& n) {
  const int a = ammo_index(w.rules, s.weapon);
  if (a < 0 || s.ammo[a] < 1) {
    run_dry(s, w, waterlevel);
    return false;
  }
  const bool lightning = s.weapon == kItLightning;
  s.frame = s.frame + 1 > (lightning ? 4 : 8) ? 1 : s.frame + 1;
  s.step = s.step == 2 ? 1 : 2;
  const int take = super_nail(w.rules, s.weapon) && s.ammo[a] >= 2 ? 2 : 1;
  s.ammo[a] -= take;
  s.attack_finished = at + 2 * kThink;
  s.next_think = at + kThink;
  if (lightning)
    sound(out, room, n, WeaponSound::lightning_hit, at);
  else {
    const bool super = super_nail(w.rules, s.weapon) && take == 2;
    sound(out, room, n, super ? WeaponSound::super_nailgun : WeaponSound::nailgun, at);
    // A nail from the right barrel leaves the step at 2 (the first step), the left at 1.
    if (!super && n <= room) out[n - 1].side = s.step == 2 ? 1 : -1;
  }
  return true;
}

// One think of the laser cannon at `at`, the button held: a cell, the twin bolts on frame 1 and
// the single bolt on frame 4 in turn. Released, it rests (its refire still set).
void laser_step(WeaponState& s, const WeaponWorld& w, int waterlevel, bool held, float at,
                WeaponEvent* out, int room, int& n) {
  s.attack_finished = at + kThink;
  if (!held) {
    rest(s);
    return;
  }
  if (s.ammo[kCells] < 1) {
    run_dry(s, w, waterlevel);
    return;
  }
  const bool twin = s.frame != 1;
  s.frame = twin ? 1 : 4;
  s.step = s.frame;
  s.ammo[kCells]--;
  s.next_think = at + kThink;
  sound(out, room, n, WeaponSound::laser, at);
  if (twin && n <= room) out[n - 1].copies = 2;  // a sound for each bolt, one channel
}

// Mjolnir's frame at each step of its swing (step 1 the press): the blow on 4, held to 0.6 s.
constexpr int kMjolnirFrame[8] = {0, 1, 2, 3, 4, 4, 4, 0};

void think(WeaponState& s, const WeaponCmd& c, const WeaponWorld& w, WeaponEvent* out, int room,
           int& n) {
  const float at = s.next_think;
  s.next_think = 0;
  if (laser(w.rules, s.weapon)) {
    if (s.frame == 0) {
      rest(s);
      return;
    }
    laser_step(s, w, c.waterlevel, c.attack, at, out, room, n);
    return;
  }
  if (nail_kind(w.rules, s.weapon) || s.weapon == kItLightning) {
    if (s.frame == 0 || !c.attack || s.impulse != 0) {
      rest(s);
      return;
    }
    fire_step(s, w, c.waterlevel, at, out, room, n);
    return;
  }
  if (s.step == 0) {
    rest(s);
    return;
  }
  if (mjolnir(w.rules, s.weapon)) {
    s.frame = kMjolnirFrame[s.step];
    // The blow: refire 0.4 s from it (a lightning strike's 1.5 s is the server's to say).
    if (s.step == 4) s.attack_finished = at + 0.4f;
    s.step = s.step + 1 >= 8 ? 0 : s.step + 1;
    if (s.frame == 0) {
      rest(s);
      return;
    }
  } else if (s.weapon == axe_of(w.rules)) {
    s.frame++;
    s.step = s.step + 1 >= 4 ? 0 : s.step + 1;
  } else {
    s.frame = s.step;
    s.step = s.step >= 6 ? 0 : s.step + 1;
  }
  s.next_think = at + kThink;
}

void cycle(WeaponState& s, const WeaponWorld& w, bool forward) {
  const bool scourge = w.rules == R::scourge;
  const int* order = scourge ? kScourgeCycle : kByImpulse;
  const int count = scourge ? 11 : 8;
  int at = 0;
  while (at < count && order[at] != s.weapon) at++;
  if (at == count) at = 0;
  for (int k = 0; k < 2 * count; k++) {
    at = (at + (forward ? 1 : count - 1)) % count;
    if (can_select(s, w, order[at])) {
      change_to(s, order[at]);
      return;
    }
  }
}

void take_impulse(WeaponState& s, const WeaponWorld& w) {
  const int i = s.impulse;
  s.impulse = 0;
  if (w.rules == R::dissolution) {
    if ((i >= 1 && i <= 8) || i == 10 || i == 12 || i == 22 || (i >= 60 && i <= 64)) lose_track(s);
    return;
  }
  if (w.rules == R::scourge) {
    if (i == 225 || i == 226) {
      const int want = i == 225 ? kItLaserCannon : kItMjolnir;
      if (can_select(s, w, want)) change_to(s, want);
      return;
    }
    if (i == 6) {  // the launcher and the proximity gun in turn
      int want = s.weapon == kItGrenadeLauncher ? kItProximityGun : kItGrenadeLauncher;
      if (want == kItGrenadeLauncher && (w.items & kItGrenadeLauncher) == 0) want = kItProximityGun;
      if (can_select(s, w, want)) change_to(s, want);
      return;
    }
  }
  if (i >= 1 && i <= 8) {
    if (can_select(s, w, kByImpulse[i - 1])) change_to(s, kByImpulse[i - 1]);
    return;
  }
  if (i == 10 || i == 12) cycle(s, w, i == 10);
}

// A press that fires once: its sound, refire and ammunition.
struct Shot {
  WeaponSound what;
  float refire;
};
Shot shot_of(R r, int weapon) {
  switch (weapon) {
    case kItSuperShotgun:
      return {WeaponSound::super_shotgun, 0.7f};
    case kItGrenadeLauncher:
      return {WeaponSound::grenade, 0.6f};
    case kItRocketLauncher:
      return {WeaponSound::rocket, 0.8f};
    default:
      break;
  }
  if (r == R::scourge && weapon == kItProximityGun) return {WeaponSound::proximity, 0.6f};
  if (r == R::dissolution && weapon == kItMultiGrenade) return {WeaponSound::grenade, 0.6f};
  if (r == R::dissolution && weapon == kItMultiRocket) return {WeaponSound::rocket, 0.8f};
  return {WeaponSound::shotgun, 0.5f};
}

void attack(WeaponState& s, const WeaponWorld& w, int waterlevel, bool held, WeaponEvent* out,
            int room, int& n) {
  const float at = s.clock;
  if (s.weapon == kWeaponUnknown) return;
  if (runs_dry(w.rules, s.weapon) && weapon_current_ammo(s, w.rules) <= 0) {
    run_dry(s, w, waterlevel);
    return;
  }
  if (s.weapon == axe_of(w.rules)) {
    s.attack_finished = at + 0.5f;
    s.frame = axe_base(at);
    s.step = 1;
    s.next_think = at + kThink;
    sound(out, room, n, WeaponSound::axe, at);
    return;
  }
  if (nail_kind(w.rules, s.weapon)) {
    fire_step(s, w, waterlevel, at, out, room, n);
    return;
  }
  if (s.weapon == kItLightning) {
    sound(out, room, n, WeaponSound::lightning_start, at);
    fire_step(s, w, waterlevel, at, out, room, n);
    return;
  }
  if (laser(w.rules, s.weapon)) {  // its first think at the press
    laser_step(s, w, waterlevel, held, at, out, room, n);
    return;
  }
  if (mjolnir(w.rules, s.weapon)) {  // no sound until the blow, which the server's trace decides
    s.attack_finished = at + 0.8f;
    s.frame = 1;
    s.step = 2;
    s.next_think = at + kThink;
    return;
  }
  if (w.rules == R::dissolution && s.weapon == kItPlasmaGun) {  // its firing frame never shown
    s.attack_finished = at + 1.0f;
    s.ammo[kCells]--;
    rest(s);
    sound(out, room, n, WeaponSound::plasma, at);
    return;
  }
  const Shot shot = shot_of(w.rules, s.weapon);
  const int a = ammo_index(w.rules, s.weapon);
  s.ammo[a] -= s.weapon == kItSuperShotgun && s.ammo[kShells] >= 2 ? 2 : 1;
  s.attack_finished = at + shot.refire;
  s.frame = 1;
  s.step = 2;
  s.next_think = at + kThink;
  sound(out, room, n, shot.what, at);
}

void run_part(WeaponState& s, const WeaponCmd& c, const WeaponWorld& w, float dt, bool first,
              WeaponEvent* out, int room, int& n) {
  s.clock += dt;
  if (first && c.impulse != 0) s.impulse = c.impulse;
  if (s.weapon == kWeaponUnknown) return;
  // An empty weapon is changed at the frame's start (Dissolution leaves it to the next press).
  if (w.rules != R::dissolution && s.clock > s.attack_finished && runs_dry(w.rules, s.weapon) &&
      weapon_current_ammo(s, w.rules) <= 0) {
    const int best = weapon_best(s, w, c.waterlevel);
    if (best != 0) change_to(s, best);
  }
  if (s.next_think != 0 && s.clock >= s.next_think) think(s, c, w, out, room, n);
  if (s.weapon == kWeaponUnknown || s.clock < s.attack_finished) return;
  if (s.impulse != 0) take_impulse(s, w);
  if (c.attack) attack(s, w, c.waterlevel, c.attack, out, room, n);
}

}  // namespace

const char* weapon_sound_sample(WeaponSound s) {
  switch (s) {
    case WeaponSound::axe:
      return "weapons/ax1.wav";
    case WeaponSound::shotgun:
      return "weapons/guncock.wav";
    case WeaponSound::super_shotgun:
      return "weapons/shotgn2.wav";
    case WeaponSound::nailgun:
      return "weapons/rocket1i.wav";
    case WeaponSound::super_nailgun:
      return "weapons/spike2.wav";
    case WeaponSound::grenade:
      return "weapons/grenade.wav";
    case WeaponSound::rocket:
      return "weapons/sgun1.wav";
    case WeaponSound::lightning_start:
      return "weapons/lstart.wav";
    case WeaponSound::lightning_hit:
      return "weapons/lhit.wav";
    case WeaponSound::laser:
      return "hipweap/laserg.wav";
    case WeaponSound::proximity:
      return "hipweap/proxbomb.wav";
    case WeaponSound::plasma:
      return "plasma/fire.wav";
    case WeaponSound::none:
      break;
  }
  return nullptr;
}

int weapon_sound_channel(WeaponSound s) { return s == WeaponSound::lightning_start ? 0 : 1; }

float weapon_sound_volume(WeaponSound s) { return s == WeaponSound::plasma ? 0.5f : 1.0f; }

int weapon_sound_bit(WeaponSound s) {
  switch (s) {
    case WeaponSound::axe:
      return 2;
    case WeaponSound::shotgun:
      return 4;
    case WeaponSound::super_shotgun:
      return 8;
    case WeaponSound::nailgun:
      return 16;
    case WeaponSound::super_nailgun:
      return 32;
    case WeaponSound::grenade:
    case WeaponSound::proximity:
      return 64;
    case WeaponSound::rocket:
      return 128;
    case WeaponSound::lightning_start:
    case WeaponSound::lightning_hit:
    case WeaponSound::laser:
    case WeaponSound::plasma:
      return 256;
    case WeaponSound::none:
      break;
  }
  return 0;
}

int weapon_sound_bit_of_sample(std::string_view sample) {
  for (int k = 1; k <= static_cast<int>(WeaponSound::plasma); k++) {
    const auto s = static_cast<WeaponSound>(k);
    if (sample == weapon_sound_sample(s)) return weapon_sound_bit(s);
  }
  return 0;
}

int weapon_current_ammo(const WeaponState& s, WeaponRules rules) {
  const int a = ammo_index(rules, s.weapon);
  return a < 0 ? 0 : s.ammo[a];
}

const char* weapon_view_model(int weapon, WeaponRules rules) {
  if (rules == R::scourge) switch (weapon) {
      case kItLaserCannon:
        return "progs/v_laserg.mdl";
      case kItMjolnir:
        return "progs/v_hammer.mdl";
      case kItProximityGun:
        return "progs/v_prox.mdl";
      default:
        break;
    }
  if (rules == R::dissolution) switch (weapon) {
      case kItAxeEternity:
        return "progs/v_axe.mdl";
      case kItAxe:  // the lava nailgun's value there
        return "progs/v_lava.mdl";
      case kItLavaSuperNailgun:
        return "progs/v_lava2.mdl";
      case kItMultiGrenade:
        return "progs/v_multi.mdl";
      case kItMultiRocket:
        return "progs/v_multi2.mdl";
      case kItPlasmaGun:
        return "progs/v_plasma.mdl";
      default:
        break;
    }
  switch (weapon) {
    case kItAxe:
      return "progs/v_axe.mdl";
    case kItShotgun:
      return "progs/v_shot.mdl";
    case kItSuperShotgun:
      return "progs/v_shot2.mdl";
    case kItNailgun:
      return "progs/v_nail.mdl";
    case kItSuperNailgun:
      return "progs/v_nail2.mdl";
    case kItGrenadeLauncher:
      return "progs/v_rock.mdl";
    case kItRocketLauncher:
      return "progs/v_rock2.mdl";
    case kItLightning:
      return "progs/v_light.mdl";
    default:
      return nullptr;
  }
}

int weapon_on_wire(int weapon) {
  if (weapon > 32767 && (weapon & (weapon - 1)) == 0) {
    int bit = 0;
    while ((1 << bit) != weapon) bit++;
    return -bit;
  }
  return weapon < -32768 ? -32768 : weapon > 32767 ? 32767 : weapon;
}

int weapon_off_wire(int value) { return value <= -15 && value >= -30 ? 1 << -value : value; }

WeaponRules weapon_rules_of(const std::vector<std::string>& models) {
  for (const std::string& m : models) {
    if (m == "progs/v_laserg.mdl") return R::scourge;
    if (m == "progs/v_lava.mdl") return R::dissolution;
  }
  return R::quake;
}

int weapon_best(const WeaponState& s, const WeaponWorld& w, int waterlevel) {
  const auto has = [&](int bit) { return (w.items & bit) != 0; };
  if (w.rules == R::scourge) {
    if (has(kItLightning) && s.ammo[kCells] >= 1 && waterlevel <= 1) return kItLightning;
    if (has(kItLaserCannon) && s.ammo[kCells] >= 1) return kItLaserCannon;
    if (has(kItSuperNailgun) && s.ammo[kNails] >= 2) return kItSuperNailgun;
    if (has(kItSuperShotgun) && s.ammo[kShells] >= 2) return kItSuperShotgun;
    if (has(kItNailgun) && s.ammo[kNails] >= 1) return kItNailgun;
    if (has(kItShotgun) && s.ammo[kShells] >= 1) return kItShotgun;
    if (has(kItMjolnir)) return kItMjolnir;
    return kItAxe;
  }
  if (w.rules == R::dissolution) return kWeaponUnknown;  // its counts are not all known here
  if (!w.w_rank.empty()) {
    for (const char d : w.w_rank) {
      if (d < '1' || d > '8') continue;
      const int weapon = kByImpulse[d - '1'];
      const int a = ammo_index(w.rules, weapon);
      if ((w.items & weapon) != 0 && (a < 0 || s.ammo[a] >= ammo_needed(w.rules, weapon)))
        return weapon;
    }
    return (w.items & kItAxe) != 0 ? kItAxe : 0;
  }
  if ((w.items & kItLightning) != 0 && s.ammo[kCells] >= 1 && waterlevel <= 1) return kItLightning;
  if ((w.items & kItSuperNailgun) != 0 && s.ammo[kNails] >= 2) return kItSuperNailgun;
  if ((w.items & kItSuperShotgun) != 0 && s.ammo[kShells] >= 2) return kItSuperShotgun;
  if ((w.items & kItNailgun) != 0 && s.ammo[kNails] >= 1) return kItNailgun;
  if ((w.items & kItShotgun) != 0 && s.ammo[kShells] >= 1) return kItShotgun;
  return kItAxe;
}

int weapon_step_of(int weapon, int frame, WeaponRules rules) {
  if (frame <= 0) return 0;
  if (laser(rules, weapon)) return frame;  // 1 or 4: firing
  if (mjolnir(rules, weapon)) return frame >= 4 ? 5 : frame + 1;
  if (rules == R::dissolution) {
    if (nail_kind(rules, weapon)) return frame % 2 == 1 ? 2 : 1;
    if (weapon == kItAxeEternity) weapon = kItAxe;
  }
  switch (weapon) {
    case kItAxe: {
      const int k = (frame - 1) % 4 + 1;  // 1..4 within the swing
      return k >= 4 ? 0 : k;
    }
    case kItNailgun:
    case kItSuperNailgun:
    case kItLightning:
      return frame % 2 == 1 ? 2 : 1;
    default:
      return frame >= 6 ? 0 : frame + 1;
  }
}

int weapon_command(WeaponState& s, const WeaponCmd& c, const WeaponWorld& w, WeaponEvent* out,
                   int room) {
  int n = 0;
  if (c.frozen) {
    s.attack_finished = s.clock + kFrozen;
    s.impulse = 0;
    return 0;
  }
  int parts = 1;
  while (static_cast<float>(c.msec) / 1000.0f / static_cast<float>(parts) > kPart) parts *= 2;
  const float dt = static_cast<float>(c.msec) / 1000.0f / static_cast<float>(parts);
  for (int k = 0; k < parts; k++) run_part(s, c, w, dt, k == 0, out, room, n);
  return n < room ? n : room;
}

}  // namespace ge::classic
