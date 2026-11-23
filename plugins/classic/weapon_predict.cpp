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

// The ammunition index a weapon fires, and how many of it a selection needs; -1: none.
int ammo_index(int weapon) {
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
      return -1;
  }
}
int ammo_needed(int weapon) {
  return weapon == kItSuperShotgun || weapon == kItSuperNailgun ? 2 : weapon == kItAxe ? 0 : 1;
}

// Held, with the ammunition a selection needs.
bool can_select(const WeaponState& s, const WeaponWorld& w, int weapon) {
  if ((w.items & weapon) == 0) return false;
  const int a = ammo_index(weapon);
  return a < 0 || s.ammo[a] >= ammo_needed(weapon);
}

// The weapons by impulse 1..8 and in the cycle's order.
constexpr int kByImpulse[8] = {
    kItAxe,          kItShotgun,         kItSuperShotgun,   kItNailgun,
    kItSuperNailgun, kItGrenadeLauncher, kItRocketLauncher, kItLightning};

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
  const int a = ammo_index(s.weapon);
  if (a < 0 || s.ammo[a] < 1) {
    change_to(s, weapon_best(s, w, waterlevel));
    return false;
  }
  const bool lightning = s.weapon == kItLightning;
  s.frame = s.frame + 1 > (lightning ? 4 : 8) ? 1 : s.frame + 1;
  s.step = s.step == 2 ? 1 : 2;
  const int take = s.weapon == kItSuperNailgun && s.ammo[a] >= 2 ? 2 : 1;
  s.ammo[a] -= take;
  s.attack_finished = at + 2 * kThink;
  s.next_think = at + kThink;
  if (lightning)
    sound(out, room, n, WeaponSound::lightning_hit, at);
  else {
    const bool super = s.weapon == kItSuperNailgun && take == 2;
    sound(out, room, n, super ? WeaponSound::super_nailgun : WeaponSound::nailgun, at);
    // A nail from the right barrel leaves the step at 2 (the first step), the left at 1.
    if (!super && n <= room) out[n - 1].side = s.step == 2 ? 1 : -1;
  }
  return true;
}

void think(WeaponState& s, const WeaponCmd& c, const WeaponWorld& w, WeaponEvent* out, int room,
           int& n) {
  const float at = s.next_think;
  s.next_think = 0;
  if (s.weapon == kItNailgun || s.weapon == kItSuperNailgun || s.weapon == kItLightning) {
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
  if (s.weapon == kItAxe) {
    s.frame++;
    s.step = s.step + 1 >= 4 ? 0 : s.step + 1;
  } else {
    s.frame = s.step;
    s.step = s.step >= 6 ? 0 : s.step + 1;
  }
  s.next_think = at + kThink;
}

void take_impulse(WeaponState& s, const WeaponWorld& w) {
  const int i = s.impulse;
  s.impulse = 0;
  if (i >= 1 && i <= 8) {
    if (can_select(s, w, kByImpulse[i - 1])) change_to(s, kByImpulse[i - 1]);
    return;
  }
  if (i != 10 && i != 12) return;
  int at = 0;
  while (at < 8 && kByImpulse[at] != s.weapon) at++;
  if (at == 8) at = 0;
  for (int k = 0; k < 20; k++) {
    at = (at + (i == 10 ? 1 : 7)) % 8;
    if (can_select(s, w, kByImpulse[at])) {
      change_to(s, kByImpulse[at]);
      return;
    }
  }
}

void attack(WeaponState& s, const WeaponWorld& w, int waterlevel, WeaponEvent* out, int room,
            int& n) {
  const float at = s.clock;
  if (s.weapon != kItAxe && weapon_current_ammo(s) <= 0) {
    change_to(s, weapon_best(s, w, waterlevel));
    return;
  }
  switch (s.weapon) {
    case kItAxe:
      s.attack_finished = at + 0.5f;
      s.frame = axe_base(at);
      s.step = 1;
      s.next_think = at + kThink;
      sound(out, room, n, WeaponSound::axe, at);
      return;
    case kItNailgun:
    case kItSuperNailgun:
      fire_step(s, w, waterlevel, at, out, room, n);
      return;
    case kItLightning:
      sound(out, room, n, WeaponSound::lightning_start, at);
      fire_step(s, w, waterlevel, at, out, room, n);
      return;
    default:
      break;
  }
  WeaponSound what = WeaponSound::shotgun;
  float refire = 0.5f;
  int take = 1;
  switch (s.weapon) {
    case kItSuperShotgun:
      what = WeaponSound::super_shotgun;
      refire = 0.7f;
      take = s.ammo[kShells] >= 2 ? 2 : 1;
      break;
    case kItGrenadeLauncher:
      what = WeaponSound::grenade;
      refire = 0.6f;
      break;
    case kItRocketLauncher:
      what = WeaponSound::rocket;
      refire = 0.8f;
      break;
    default:
      break;
  }
  s.ammo[ammo_index(s.weapon)] -= take;
  s.attack_finished = at + refire;
  s.frame = 1;
  s.step = 2;
  s.next_think = at + kThink;
  sound(out, room, n, what, at);
}

void run_part(WeaponState& s, const WeaponCmd& c, const WeaponWorld& w, float dt, bool first,
              WeaponEvent* out, int room, int& n) {
  s.clock += dt;
  if (first && c.impulse != 0) s.impulse = c.impulse;
  if (s.clock > s.attack_finished && s.weapon != kItAxe && weapon_current_ammo(s) <= 0) {
    const int best = weapon_best(s, w, c.waterlevel);
    if (best != 0) change_to(s, best);
  }
  if (s.next_think != 0 && s.clock >= s.next_think) think(s, c, w, out, room, n);
  if (s.clock < s.attack_finished) return;
  if (s.impulse != 0) take_impulse(s, w);
  if (c.attack) attack(s, w, c.waterlevel, out, room, n);
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
    case WeaponSound::none:
      break;
  }
  return nullptr;
}

int weapon_sound_channel(WeaponSound s) { return s == WeaponSound::lightning_start ? 0 : 1; }

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
      return 64;
    case WeaponSound::rocket:
      return 128;
    case WeaponSound::lightning_start:
    case WeaponSound::lightning_hit:
      return 256;
    case WeaponSound::none:
      break;
  }
  return 0;
}

int weapon_sound_bit_of_sample(std::string_view sample) {
  for (int k = 1; k <= static_cast<int>(WeaponSound::lightning_hit); k++) {
    const auto s = static_cast<WeaponSound>(k);
    if (sample == weapon_sound_sample(s)) return weapon_sound_bit(s);
  }
  return 0;
}

int weapon_current_ammo(const WeaponState& s) {
  const int a = ammo_index(s.weapon);
  return a < 0 ? 0 : s.ammo[a];
}

const char* weapon_view_model(int weapon) {
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

int weapon_best(const WeaponState& s, const WeaponWorld& w, int waterlevel) {
  if (!w.w_rank.empty()) {
    for (const char d : w.w_rank) {
      if (d < '1' || d > '8') continue;
      const int weapon = kByImpulse[d - '1'];
      const int a = ammo_index(weapon);
      if ((w.items & weapon) != 0 && (a < 0 || s.ammo[a] >= ammo_needed(weapon))) return weapon;
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

int weapon_step_of(int weapon, int frame) {
  if (frame <= 0) return 0;
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
