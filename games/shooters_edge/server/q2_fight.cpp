// Quake II runner's fight (q2_fight.hpp; the rules in q2_server.hpp and
// q2_combat.hpp): shots as lines and as missiles, damage through the armour, radius damage
// and what it can reach, deaths, obituaries and frags, the weapon chosen, the world's rows.
//
// ge:summary Quake II runner's fight: shots, missiles, damage, deaths, the chosen weapon
// ge:keywords q2 fight shots hitscan pellets railgun missiles grenade-bounce radius-damage
// ge:keywords can-damage obituary death frags use-weapon weapnext rocket-jump
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include "shooters_edge/server/q2_fight.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "shooter/aim_help.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_gibs.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_mods.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_screens.hpp"
#include "shooters_edge/server/shot_help.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr float kMass = 200;    // a player's
constexpr float kRange = 8192;  // a line shot's reach
constexpr int32_t kWorldMask = sh::kSqSolid | sh::kSqWindow;
constexpr int32_t kShotMask = kWorldMask | sh::kSqMonster | sh::kSqDeadMonster;  // and bodies
constexpr int32_t kSurfSky = 0x4;  // a sky surface: a shot that reaches it is gone
constexpr uint32_t kEveryone = 0xffffffffu;
// The carried weapons' names, in a client's order (it numbers them from 1).
constexpr std::pair<SqWeapon, const char*> kCarried[] = {
    {SqWeapon::blaster, "#w_blaster.md2"},
    {SqWeapon::shotgun, "#w_shotgun.md2"},
    {SqWeapon::super_shotgun, "#w_sshotgun.md2"},
    {SqWeapon::machinegun, "#w_machinegun.md2"},
    {SqWeapon::chaingun, "#w_chaingun.md2"},
    {SqWeapon::hand_grenade, "#a_grenades.md2"},
    {SqWeapon::grenade_launcher, "#w_glauncher.md2"},
    {SqWeapon::rocket_launcher, "#w_rlauncher.md2"},
    {SqWeapon::hyperblaster, "#w_hyperblaster.md2"},
    {SqWeapon::railgun, "#w_railgun.md2"},
    {SqWeapon::bfg, "#w_bfg.md2"},
};

constexpr const char* kMissileModels[kSqShotKinds] = {"models/objects/laser/tris.md2",
                                                      "",
                                                      "",
                                                      "models/objects/grenade/tris.md2",
                                                      "models/objects/rocket/tris.md2",
                                                      "",
                                                      "sprites/s_bfg1.sp2",
                                                      "models/objects/grenade2/tris.md2"};
constexpr const char* kClick = "weapons/noammo.wav";
constexpr const char* kBoom = "weapons/rocklx1a.wav";
constexpr const char* kGrenadeBoom = "weapons/grenlx1a.wav";
constexpr const char* kBounce = "weapons/grenlb1b.wav";
constexpr const char* kHandBounce = "weapons/hgrenb1a.wav";
constexpr const char* kFuse = "weapons/hgrenc1b.wav";  // the hand grenade's fuse lit
// The loops a shot or a player carries, by `SqLoop` (porting note: q2-render-fx.md § Entity
// loop sounds).
constexpr const char* kLoops[kSqLoops] = {
    "misc/lasfly.wav",      "weapons/rockfly.wav",  kFuse,
    "weapons/bfg__l1a.wav", "weapons/rg_hum.wav",   "weapons/bfg_hum.wav",
    "weapons/hyprbl1a.wav", "weapons/chngnl1a.wav", "player/fry.wav"};
constexpr const char* kGib = "misc/udeath.wav";
// A player's own voice, which each client takes from that player's model's folder (the `*`
// names), and the body's plain sounds beside it (porting note: q2-player-sounds.md).
constexpr const char* kOwnSounds[] = {
    "*death1.wav",    "*death2.wav",   "*death3.wav",   "*death4.wav",   "*fall1.wav",
    "*fall2.wav",     "*gurp1.wav",    "*gurp2.wav",    "*jump1.wav",    "*pain25_1.wav",
    "*pain25_2.wav",  "*pain50_1.wav", "*pain50_2.wav", "*pain75_1.wav", "*pain75_2.wav",
    "*pain100_1.wav", "*pain100_2.wav"};
constexpr const char* kBodySounds[] = {
    "player/gasp1.wav",   "player/gasp2.wav",   "player/watr_in.wav",   "player/watr_out.wav",
    "player/watr_un.wav", "player/lava_in.wav", "player/u_breath1.wav", "player/u_breath2.wav",
    "player/land1.wav",   "player/drown1.wav",  "player/burn1.wav",     "player/burn2.wav",
    "player/step1.wav",   "player/step2.wav",   "player/step3.wav",     "player/step4.wav",
    "items/damage2.wav",  "items/protect2.wav", "items/airout.wav",     "items/damage.wav",
    "items/protect.wav",  "items/protect4.wav", "items/damage3.wav",    "misc/power1.wav",
    "misc/power2.wav",    "player/lava1.wav",   "player/lava2.wav",     "weapons/lashit.wav"};
// The channels a player's sounds take: one sound a channel, a new one replacing it (0 stacks).
constexpr int kAutoChannel = 0, kVoiceChannel = 2, kItemChannel = 3, kBodyChannel = 4;
constexpr double kPainEvery = 0.75;  // a pain cry at most once in this long, s
constexpr float kGibHealth = -40;    // a death under this throws the body apart
constexpr float kItemHalf = 15;      // a dropped item's box, ±15 each way

bool grenade(SqShot k) { return k == SqShot::grenade || k == SqShot::hand_grenade; }

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
bool dmflag(const Q2Server& s, int bit) {
  return deathmatch(s) && (s.cvars.find("dmflags")->integer() & bit) != 0;
}
int skill(const Q2Server& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}
void pack(int16_t out[3], sh::Vec3f v) {
  const float in[3] = {v.x, v.y, v.z};
  for (int i = 0; i < 3; i++)
    out[i] = static_cast<int16_t>(std::clamp(num::to_int32(double(in[i]) * 8), -32768, 32767));
}

sh::Vec3f centre_of(const Q2Player& p) { return unpack(p.move.origin) + (p.mins + p.maxs) * 0.5f; }

bool fightable(const Q2Server& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}

float crandom(Q2Server& s) { return float(2 * (ge::core::next(s.rng) - 0.5)); }

const std::string& name_of(const Q2Server& s, int slot) {
  static const std::string kNobody = "someone";
  if (slot < 0 || slot >= sh::kMaxClients) return kNobody;
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return kNobody;
}

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound(Q2Server& s, int ent, sh::Vec3f at, const char* sample, float volume = 1) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = 1;  // the weapon's channel
  ev.origin = at;
  ev.values[0] = 255 * volume;
  ev.values[1] = 1;
  ev.text = sample;
  event(s, std::move(ev));
}

// A player's muzzle flash `value` (server/q2_flash.hpp, the silenced bit in it) at his origin.
void muzzle_flash(Q2Server& s, int slot, sh::Vec3f origin, int value) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.ent = slot + 1;
  ev.origin = origin;
  ev.values[0] = static_cast<float>(value);
  ev.text = "q2_flash";
  event(s, std::move(ev));
}

// The flash value a shot of weapon `w` sends (`shots` the chain gun's bullets this frame); -1 for
// a weapon with none (the hand grenade).
int flash_value(int w, int shots) {
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
    default:
      return -1;
  }
}

// Whether flash `value` plays `sample` (a remixed weapon's own sound is sent as a sound instead).
bool flash_plays(int value, const char* sample) {
  const Q2Flash* f = q2_player_flash(value);
  if (f == nullptr || f->sounds[0].name == nullptr || sample == nullptr) return false;
  const std::string_view shot = f->sounds[0].name, own = sample;
  const size_t wild = shot.find('%');
  return wild == std::string_view::npos ? shot == own : own.substr(0, wild) == shot.substr(0, wild);
}

// A sound from player `slot`'s body on `channel`.
void body_sound(Q2Server& s, int slot, int channel, const std::string& sample) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = slot + 1;
  ev.level = channel;
  ev.origin = unpack(s.players[slot].move.origin);
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = sample;
  event(s, std::move(ev));
}

// One of `n` variants for `slot`'s sound this frame, off a stream of its own (the rules' stream
// is left as it was, so the shots and the items fall as before).
int variant(const Q2Server& s, int slot, uint32_t salt, int n) {
  ge::core::Rng r = ge::core::make_rng(static_cast<uint32_t>(s.frame_number) * 64u +
                                       static_cast<uint32_t>(slot) + salt * 4096u + 0x50u);
  return ge::core::int_in(r, 0, n - 1);
}

// A point effect `name` at `at` thrown along `way` (the surface's normal at a hit; none: up).
void effect(Q2Server& s, const char* name, sh::Vec3f at, sh::Vec3f way = {}) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = at;
  ev.values[0] = way.x;
  ev.values[1] = way.y;
  ev.values[2] = way.z;
  ev.text = name;
  event(s, std::move(ev));
}

// A line effect `name` from `from` to `to` (a rail's): the start in `origin`, the end in `values`.
void line_effect(Q2Server& s, const char* name, sh::Vec3f from, sh::Vec3f to, int ent) {
  NetEvent ev;
  ev.kind = EventKind::trail_effect;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.origin = from;
  ev.values[0] = to.x;
  ev.values[1] = to.y;
  ev.values[2] = to.z;
  ev.text = name;
  event(s, std::move(ev));
}

void print_to(Q2Server& s, int slot, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.level = 2;
  ev.text = std::move(text);
  event(s, std::move(ev));
}

// A body thrown apart at `at`: its sound from entity `ent`, the blood and its pieces (`t`).
void gib(Q2Server& s, int ent, sh::Vec3f at, const SqGibThrow& t) {
  gibs_throw(s, t);
  sound(s, ent, at, kGib);
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = at;
  ev.values[3] = 32;
  ev.text = "te_blood";
  event(s, std::move(ev));
}

void print_all(Q2Server& s, std::string text, int level = 1) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = kEveryone;
  ev.reliable = true;
  ev.level = level;
  ev.text = std::move(text);
  event(s, std::move(ev));
}

// What a line met first: the world (a wall, the sky) or a player.
struct Line {
  float fraction = 1;
  sh::Vec3f end;
  sh::Vec3f normal;
  bool sky = false;
  int player = -1;
  int mover = -1;  // the map's entity it met (a door, a plat), when it met one
};

// A line from `from` to `to` through the walls and the living players (not those in `skip`, a
// bit per slot).
Line shoot_line(Q2Server& s, sh::Vec3f from, sh::Vec3f to, uint32_t skip, int skip_entity = -1) {
  Line out;
  const ge::classic::Q2Trace t = map_trace(s, from, {}, {}, to, kShotMask, skip_entity);
  out.fraction = t.start_solid ? 0 : t.fraction;
  out.normal = t.normal;
  out.sky = t.fraction < 1 && (t.surface_flags & kSurfSky) != 0;
  if (t.fraction < 1) out.mover = map_entity_of_row(s, t.entity);
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (((skip >> i) & 1) != 0 || !fightable(s, i)) continue;
    const Q2Player& p = s.players[i];
    sh::Vec3f at = unpack(p.move.origin), mins = p.mins, maxs = p.maxs;
    if (s.lag_shooter >= 0 && s.lag_place != nullptr) {  // where the shooter saw him
      sh::Vec3f then, lo, hi;
      if (s.lag_place(s.lag_ctx, s.lag_shooter, i, then, lo, hi)) {
        at = then;
        mins = lo;
        maxs = hi;
      }
    }
    const float f = sq_enter_box(from, to, at + mins, at + maxs);
    if (f >= 0 && f < out.fraction) {
      out.fraction = f;
      out.player = i;
      out.sky = false;
    }
  }
  if (out.player >= 0) {  // a body: the hit faces back along the line
    out.mover = -1;
    out.normal = from - to;
    sh::normalize(out.normal);
  }
  out.end = from + (to - from) * out.fraction;
  return out;
}

// Whether a blast at `at` reaches a body at `o` (player `i`'s origin): a clear line to it, or to it
// moved 15 each way in x and y.
bool reaches(const Q2Server& s, sh::Vec3f at, sh::Vec3f o) {
  const sh::Vec3f tries[5] = {o, o + sh::Vec3f{15, 15, 0}, o + sh::Vec3f{15, -15, 0},
                              o + sh::Vec3f{-15, 15, 0}, o + sh::Vec3f{-15, -15, 0}};
  for (const sh::Vec3f& t : tries)
    if (map_trace(s, at, {}, {}, t, kWorldMask).fraction >= 1) return true;
  return false;
}
bool can_damage(Q2Server& s, sh::Vec3f at, int i) {
  return reaches(s, at, unpack(s.players[i].move.origin));
}

// A blow on player `target` from `attacker` (a slot; -1 the world) along `dir`: pushed, shared
// with the armour, the rest off his health; at 0 he dies.
// Midair's level (`k_midair_minheight`, 0..4).
int midair_level(const Q2Server& s) {
  const auto* c = s.cvars.find("k_midair_minheight");
  return c != nullptr ? sh::midair_level(c->value) : sh::kMidairDefaultLevel;
}

// How far the bottom of player `i`'s box is above the floor straight under him (the world only).
float midair_height(const Q2Server& s, int i) {
  const Q2Player& p = s.players[i];
  const sh::Vec3f o = unpack(p.move.origin);
  const ge::classic::Q2Trace t =
      map_trace(s, o, {}, {}, o - sh::Vec3f{0, 0, sh::kMidairProbe}, kWorldMask);
  return sh::midair_height(o.z, p.mins.z, t.end.z);
}

// A blow in midair (plugins/shooter/midair.hpp): the push grown with the target's height and
// lifted on the floor, the armour worn as usual; then a counted rocket on another player kills
// (its frags kept for the obituary), drowning, lava, slime and a telefrag take what they take, and
// the rest takes no health.
void midair_hurt(Q2Server& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
                 bool energy, int weapon, bool no_armour) {
  Q2Player& p = s.players[target];
  const float height = midair_height(s, target);
  const bool own = attacker == target;
  if (knockback > 0 && sh::normalize(dir) > 0) {
    const float push = sq_knockback(knockback, kMass, own);
    const sh::MidairPush m = sh::midair_push(height, float(damage), own);
    sh::Vec3f v = unpack(p.move.velocity) + dir * (push * m.scale);
    v.z += dir.z * push * m.scale * m.lift;
    pack(p.move.velocity, v);
  }
  if (damage <= 0) return;
  const int saved = no_armour ? 0 : sq_armour_save(p.body, damage, energy);
  float take = 0;
  if (!std::isnan(s.rocket_from_z)) {
    if (attacker < 0 || own) return;
    const float rise = unpack(p.move.origin).z - s.rocket_from_z;
    const int frags = sh::midair_score(height, rise, midair_level(s));
    if (frags == 0) return;
    s.midair_frags = frags;
    s.midair_rise = rise;
    take = sh::kMidairKill;
  } else if (weapon == kSqByLava || weapon == kSqBySlime || weapon == kSqByWater ||
             weapon == kSqByTelefrag) {
    take = float(damage - saved);
  } else {
    return;
  }
  p.health -= take;
  if (p.health <= 0) {
    p.health = std::max(p.health, -999.0f);
    fight_kill(s, target, attacker, weapon);
  }
  s.midair_frags = 0;
}

// A blow the status bar shows for the frame (q2_bar.hpp): `bit` 1 to health, 2 to armour.
void mark_hit(const Q2Server& s, Q2Player& p, int bit, int points) {
  if (p.hit_frame != s.frame_number) p.hit_bits = 0;  // a new frame's blows
  p.hit_frame = s.frame_number;
  p.hit_bits |= bit;
  p.blow_parts[bit - 1] += points;  // blood (1) or armour (2): the screen's flash
}

void hurt(Q2Server& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
          bool energy, int weapon, bool no_armour = false, bool past_invulnerable = false) {
  if (!fightable(s, target) || damage <= 0) return;
  // A teammate's blow does nothing with `dmflags` 256 (no friendly fire) but push.
  if (attacker >= 0 && attacker != target && fight_teammates(s, attacker, target) && dmflag(s, 256))
    damage = 0;
  if (damage > 0) damage = q2_mods_damage(s, target, attacker, damage);  // the push still lands
  if (fight_midair(s)) {
    midair_hurt(s, target, attacker, dir, damage, knockback, energy, weapon, no_armour);
    return;
  }
  Q2Player& p = s.players[target];
  if (!deathmatch(s) && skill(s) == 0) damage = std::max(1, damage / 2);
  const bool along = sh::normalize(dir) > 0;
  if (knockback > 0 && along) {
    const float push = sq_knockback(knockback, kMass, attacker == target);
    pack(p.move.velocity, unpack(p.move.velocity) + dir * push);
  }
  if (damage == 0) return;                                          // the push lands
  if (p.invulnerable_until > s.level_time && !past_invulnerable) {  // the push lands
    if (s.level_time >= p.pain_sound_at + 2) {  // the blow turned aside, heard every 2 s at most
      p.pain_sound_at = s.level_time;
      body_sound(s, target, kItemChannel, "items/protect4.wav");
    }
    return;
  }
  if (p.god && !past_invulnerable) {  // `god` (q2_commands.hpp): sparks, no harm, no pain
    effect(s, "te_sparks", centre_of(p));
    return;
  }
  // What he sees of it: its points and push kick the view toward where it came from.
  ge::classic::q2_view_blow(p.view, damage, along ? knockback : 0, dir * -1.0f);
  int saved = 0;
  if (!no_armour) {
    if (along) saved += fight_power_save(s, target, dir, damage);
    const int armour = sq_armour_save(p.body, damage - saved, energy);
    saved += armour;
    if (armour > 0) mark_hit(s, p, 2, armour);
  }
  if (damage - saved > 0) mark_hit(s, p, 1, damage - saved);
  p.health -= float(damage - saved);
  if (p.health <= 0) {
    p.health = std::max(p.health, -999.0f);
    p.killing_blow = damage;
    fight_kill(s, target, attacker, weapon);
    return;
  }
  p.frame_damage += damage;  // the frame's pain cry hears it, the armour's share too
}

// Radius damage from `at`: each living player within `radius` of his box takes `damage` less half
// the distance to his box's centre (halved for the attacker), when the blast can reach him.
// Who a monster hears hurt it: a player's slot, else the monster that fired (`from`, a map
// entity) as -2 - its entity (q2_monsters.hpp's infighting), else -1.
int monster_hurt_by(int attacker, int from) {
  return attacker >= 0 ? attacker : from >= 0 ? -2 - from : -1;
}

// While a blow is dealt, how it landed (the obituary's words).
struct BlowHow {
  Q2Server& s;
  int was;
  BlowHow(Q2Server& server, int how) : s(server), was(server.blow_how) { s.blow_how = how; }
  ~BlowHow() { s.blow_how = was; }
  BlowHow(const BlowHow&) = delete;
  BlowHow& operator=(const BlowHow&) = delete;
};

void blast(Q2Server& s, sh::Vec3f at, int attacker, int damage, float radius, int ignore,
           int weapon, bool energy = false, int ignore_entity = -1, int from = -1) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == ignore || !fightable(s, i)) continue;
    const Q2Player& p = s.players[i];
    const sh::Vec3f o = unpack(p.move.origin);
    const sh::Vec3f lo = o + p.mins, hi = o + p.maxs;
    const sh::Vec3f near{std::clamp(at.x, lo.x, hi.x), std::clamp(at.y, lo.y, hi.y),
                         std::clamp(at.z, lo.z, hi.z)};
    if (sh::length(near - at) > radius) continue;
    float points = float(damage) - 0.5f * sh::length(at - centre_of(p));
    if (i == attacker) points *= 0.5f;
    if (points <= 0 || !can_damage(s, at, i)) continue;
    const BlowHow how(s, kSqHowBlast);
    hurt(s, i, attacker, o - at, static_cast<int>(points), static_cast<int>(points), energy,
         weapon);
  }
  // The map's shootable movers (a door or button with health) and the monsters: less half the
  // distance to the middle of its box, when within the radius; a monster only when the blast
  // reaches it past the walls (to its origin, or that moved 15 each way).
  for (size_t e = 0; e < s.entities.size(); e++) {
    const Q2Entity& m = s.entities[e];
    if (m.gone || (m.health <= 0 && !m.corpse) || static_cast<int>(e) == ignore_entity) continue;
    const sh::Vec3f mid = m.origin + (m.mins + m.maxs) * 0.5f;
    const float points = float(damage) - 0.5f * sh::length(mid - at);
    if (points <= 0) continue;
    const bool monster = monster_index(s, static_cast<int>(e)) >= 0;
    if (monster && !reaches(s, at, m.origin)) continue;
    map_damage(s, static_cast<int>(e), monster ? monster_hurt_by(attacker, from) : attacker,
               static_cast<int>(points), mid - at, true);
  }
  // The bodies left behind: thrown apart under −40.
  for (Q2Body& b : s.bodies) {
    if (!b.live) continue;
    const float points = float(damage) - 0.5f * sh::length(b.origin - at);
    if (points <= 0) continue;
    b.health -= points;
    if (b.health < kGibHealth) {
      b.live = false;
      gib(s, 0, b.origin,
          {b.origin + sh::Vec3f{-16, -16, -24},
           b.origin + sh::Vec3f{16, 16, -8},
           {},
           static_cast<int>(points),
           SqGibSet::corpse});
    }
  }
}

// While a rocket's blows are dealt, midair knows where its shooter stood (`rocket_from_z`).
struct RocketBlow {
  Q2Server& s;
  float was;
  RocketBlow(Q2Server& server, const Q2Missile& m) : s(server), was(server.rocket_from_z) {
    if (m.kind == SqShot::rocket) s.rocket_from_z = m.fired_z;
  }
  ~RocketBlow() { s.rocket_from_z = was; }
  RocketBlow(const RocketBlow&) = delete;
  RocketBlow& operator=(const RocketBlow&) = delete;
};

Q2Missile* new_missile(Q2Server& s) {
  for (Q2Missile& m : s.missiles)
    if (!m.live) return &m;
  return nullptr;  // every row in flight: the shot is lost
}

size_t missile_row(const Q2Server& s, const Q2Missile& m) {
  return fight_first_row(s) + size_t(&m - s.missiles);
}

// A missile explodes at `at`: its radius damage (the one it hit directly left out), the sound and
// the flash; the BFG's ball also strikes everything within 1000 it and its owner can both reach.
void explode(Q2Server& s, Q2Missile& m, sh::Vec3f at, int ignore, int ignore_entity = -1) {
  m.live = false;
  if (m.owner >= 0) monster_noise(s, m.owner, at, true);  // a player's: monsters may hear it
  const RocketBlow blow(s, m);
  sound(s, int(missile_row(s, m)), at, grenade(m.kind) ? kGrenadeBoom : kBoom);
  effect(s, m.kind == SqShot::bfg ? "te_bfg_bigexplosion" : "te_explosion", at);
  blast(s, at, m.owner, m.radius_damage, m.radius, ignore, m.weapon, m.kind == SqShot::bfg,
        ignore_entity, m.from);
  if (m.kind != SqShot::bfg) return;
  const float strength = float(m.burst > 0 ? m.burst : m.damage);
  const sh::Vec3f from = m.owner >= 0 ? unpack(s.players[m.owner].move.origin) : at;
  for (size_t e = 0; e < s.entities.size(); e++) {  // the monsters it and its owner both see
    const Q2Entity& t = s.entities[e];
    if (t.gone || t.health <= 0 || monster_index(s, static_cast<int>(e)) < 0) continue;
    const float d = sh::length(t.origin - at);
    if (d > m.reach || !reaches(s, at, t.origin) || !reaches(s, from, t.origin)) continue;
    map_damage(s, static_cast<int>(e), monster_hurt_by(m.owner, m.from),
               static_cast<int>(strength * (1 - sh::root_of(d / m.reach))), t.origin - at, true);
  }
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == m.owner || !fightable(s, i)) continue;
    const float d = sh::length(unpack(s.players[i].move.origin) - at);
    if (d > m.reach || !can_damage(s, at, i)) continue;
    if (m.owner >= 0 && !can_damage(s, unpack(s.players[m.owner].move.origin), i)) continue;
    const float points = strength * (1 - sh::root_of(d / m.reach));
    const BlowHow how(s, kSqHowBurst);
    hurt(s, i, m.owner, unpack(s.players[i].move.origin) - at, static_cast<int>(points), 0, true,
         m.weapon);
  }
}

// A player's hitscan traced as he saw the others (lag compensation), for its scope.
struct LagScope {
  Q2Server& s;
  LagScope(Q2Server& sv, int slot) : s(sv) { s.lag_shooter = slot; }
  ~LagScope() { s.lag_shooter = -1; }
  LagScope(const LagScope&) = delete;
  LagScope& operator=(const LagScope&) = delete;
};

// A bullet or a pellet from `start` along the axes, spread `h` right and `v` up.
void bullet(Q2Server& s, int slot, sh::Vec3f origin, sh::Vec3f start, const sh::ViewAxes& ax,
            float h, float v, int damage, int kick, int weapon) {
  const LagScope lag(s, slot);
  const uint32_t me = 1u << slot;
  Line l = shoot_line(s, origin, start, me);  // the shooter's own reach to the start
  if (l.fraction >= 1) {
    // The draws in a fixed order: operands of one expression are evaluated in an order the
    // compiler picks, so two draws in one would differ from compiler to compiler.
    const float across = crandom(s) * h;
    const float up = crandom(s) * v;
    const sh::Vec3f end = start + ax.forward * kRange + ax.right * across + ax.up * up;
    l = shoot_line(s, start, end, me);
  }
  if (l.player >= 0) {
    hurt(s, l.player, slot, ax.forward, damage, kick, false, weapon);
    effect(s, "te_blood", l.end, l.normal);
  } else if (l.fraction < 1 && !l.sky) {
    const bool body = monster_index(s, l.mover) >= 0;
    // A wall that takes no blow rings out: a noise a monster with no enemy may hear.
    if (!body && (l.mover < 0 || s.entities[size_t(l.mover)].health <= 0))
      monster_noise(s, slot, l.end, true);
    map_damage(s, l.mover, slot, damage, ax.forward, false, &l.end);
    effect(s, body ? "te_blood" : "te_gunshot", l.end, l.normal);
  }
}

// The railgun's line: through every player it meets to the first wall.
void rail(Q2Server& s, int slot, sh::Vec3f start, const sh::ViewAxes& ax, int damage, int kick,
          int weapon) {
  const LagScope lag(s, slot);
  uint32_t skip = 1u << slot;
  const sh::Vec3f end = start + ax.forward * kRange;
  sh::Vec3f from = start;
  int past = -1;  // the last monster crossed, which the line goes on from
  for (int n = 0; n < 64; n++) {
    const Line l = shoot_line(s, from, end, skip, past);
    if (l.player >= 0) {
      hurt(s, l.player, slot, ax.forward, damage, kick, false, weapon);
      skip |= 1u << l.player;
      continue;
    }
    if (l.fraction < 1 && monster_index(s, l.mover) >= 0) {  // through the body, on
      map_damage(s, l.mover, slot, damage, ax.forward, false, &l.end);
      past = l.mover;
      from = l.end;
      continue;
    }
    map_damage(s, l.mover, slot, damage);
    line_effect(s, "te_railtrail", start, l.end, slot + 1);
    monster_noise(s, slot, l.end, true);
    return;
  }
}

// Quake II's weapons by the names the aim help's lists use (shot_help.hpp).
constexpr const char* kSqAimNames[kSqWeaponCount] = {"blaster", "sg", "ssg", "mg",  "cg", "gl",
                                                     "rl",      "hb", "rg",  "bfg", "hg"};

// Whether the aim help acts for `slot`'s shot of weapon `w`: granted, and the weapon allowed.
bool aim_helped_weapon(const Q2Server& s, int slot, int w) {
  return static_cast<size_t>(slot) < s.sv.clients.size() && s.sv.clients[slot].aimhelp && w >= 0 &&
         w < kSqWeaponCount && aim_help_weapon_allowed(s, kSqAimNames[w]);
}

// The bend told to the player it helped: his client turns the view onto the player in slot
// `target` (entity target + 1, as the QuakeWorld endpoint numbers players; aim_turn.hpp).
void tell_bend(Q2Server& s, int slot, int target) {
  s.sv.clients[slot].aim_helped++;
  NetEvent ev;
  ev.kind = EventKind::aim_bend;
  ev.to = 1u << slot;
  ev.ent = target + 1;
  event(s, std::move(ev));
}

sh::AimLeeway aim_leeway(const Q2Server& s) {
  return {float(s.cvars.find("sv_aimhelp_x")->number), float(s.cvars.find("sv_aimhelp_y")->number)};
}

// The aim help's lead for `slot`'s missile from `start` at `velocity` (falling by `gravity` when
// thrown): the velocity that meets the player it would just miss where he will be (his run or his
// fall foreseen), or `velocity` as it was.
sh::Vec3f led_velocity(Q2Server& s, int slot, int w, sh::Vec3f start, sh::Vec3f velocity,
                       float gravity, sh::Vec3f view_forward) {
  if (!aim_helped_weapon(s, slot, w)) return velocity;
  const float fall = float(s.cvars.find("sv_gravity")->number);
  sh::AimMover targets[sh::kMaxClients];
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == slot || !fightable(s, i) || fight_teammates(s, slot, i)) continue;
    const Q2Player& o = s.players[i];
    const sh::Vec3f at = unpack(o.move.origin);
    targets[n++] = sh::lead_mover(i, at + o.mins, at + o.maxs, unpack(o.move.velocity),
                                  (o.move.flags & ge::classic::kSqOnGround) == 0, -1e30f);
  }
  sh::LeadShot shot;
  shot.origin = start;
  shot.velocity = velocity;
  shot.gravity = gravity;
  const sh::LeadBend b =
      sh::aim_lead(shot, view_forward, {targets, size_t(n)}, aim_leeway(s), fall);
  if (!b.bent) return velocity;
  if (gravity == 0 && shoot_line(s, start, b.at, 1u << slot).player != b.target) return velocity;
  tell_bend(s, slot, b.target);
  return b.velocity;
}

// The aim help (plugins/shooter/aim_help.hpp) for a player granted it: his view's forward bent
// onto the nearest other living player (not a teammate) a line just misses within the server's
// leeway, when nothing stands between; `forward` as it was otherwise. Hitscan shots only.
sh::Vec3f helped_forward(Q2Server& s, int slot, int w, sh::Vec3f eye, sh::Vec3f forward) {
  if (!aim_helped_weapon(s, slot, w)) return forward;
  const sh::AimLeeway leeway = aim_leeway(s);
  sh::AimTarget targets[sh::kMaxClients];
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == slot || !fightable(s, i) || fight_teammates(s, slot, i)) continue;
    const Q2Player& o = s.players[i];
    const sh::Vec3f at = unpack(o.move.origin);
    targets[n++] = sh::AimTarget{i, at + o.mins, at + o.maxs};
  }
  const sh::AimBend b = sh::aim_help(eye, forward, kRange, {targets, size_t(n)}, leeway);
  if (!b.bent) return forward;
  if (shoot_line(s, eye, b.aim + b.forward * 8, 1u << slot).player != b.target) return forward;
  tell_bend(s, slot, b.target);
  return b.forward;
}

// `aimfix` 1: a shot from `from` turned onto the point a line from the eye along `forward` meets
// within 8192 units (walls, glass, doors, players and monsters; not the shooter); `forward` as
// it was when the line meets nothing, and always with `aimfix` 0.
sh::Vec3f muzzle_aim(Q2Server& s, int slot, sh::Vec3f from, sh::Vec3f forward) {
  if (s.cvars.find("aimfix")->integer() == 0) return forward;
  const Q2Player& p = s.players[slot];
  const sh::Vec3f eye = unpack(p.move.origin) + sh::Vec3f{0, 0, p.view_height};
  const sh::Vec3f far = eye + forward * 8192.0f;
  const Line l = shoot_line(s, eye, far, 1u << slot);
  if (l.fraction >= 1) return forward;
  sh::Vec3f to = eye + (far - eye) * l.fraction - from;
  sh::normalize(to);
  return to;
}

// The machine gun's climb for the bullet just fired, degrees up: 1.5 for each bullet before it in
// the burst, at most 13.5; none in deathmatch or with `g_machinegun_norecoil` 1.
float machinegun_climb(const Q2Server& s, int slot) {
  if (deathmatch(s) || s.cvars.find("g_machinegun_norecoil")->integer() != 0) return 0;
  return 1.5f * static_cast<float>(std::clamp(s.players[slot].hand.held_shots - 1, 0, 9));
}

// A player's bolt, rocket or BFG ball flying from `start` along `forward` at `speed`: the first
// monster on its line may dodge it (q2-monsters-full.md § Dodging and ducking). At skill 0
// only one shot in four is offered; there are no monsters in deathmatch.
void offer_dodge(Q2Server& s, int slot, sh::Vec3f start, sh::Vec3f forward, float speed) {
  if (deathmatch(s) || s.monsters.empty()) return;
  if (skill(s) == 0 && ge::core::next(s.rng) > 0.25) return;
  const Line l = shoot_line(s, start, start + forward * 8192, 1u << slot);
  if (l.player >= 0 || l.mover < 0 || monster_index(s, l.mover) < 0) return;
  monster_dodge(s, l.mover, slot, sh::length(l.end - start), speed);
}

// A missile from `start` along the axes; the hand grenade's `speed` and `fuse` are its throw's.
void launch(Q2Server& s, int slot, SqShot kind, sh::Vec3f origin, sh::Vec3f start,
            const sh::ViewAxes& ax, int damage, int quad, int weapon, float speed = 600,
            double fuse = 2.5) {
  const Line reach = shoot_line(s, origin, start, 1u << slot);
  Q2Missile* m = new_missile(s);
  if (m == nullptr) return;
  *m = Q2Missile{};
  m->live = true;
  m->kind = kind;
  m->owner = slot;
  m->weapon = weapon;
  m->origin = start;
  m->damage = damage;
  m->fired_z = origin.z;
  switch (kind) {
    case SqShot::bolt: {  // a hyperblaster's glows on its 7th and 10th frames, else plain
      m->velocity = ax.forward * 1000;
      m->dies_at = s.level_time + 2;
      const int f = s.players[slot].hand.frame;
      if (weapon == int(SqWeapon::hyperblaster)) m->effects = f == 6 || f == 9 ? 64 : 0;
      break;
    }
    case SqShot::grenade:
    case SqShot::hand_grenade: {
      const float up = 200 + crandom(s) * 10;  // the draws in a fixed order (above)
      const float across = crandom(s) * 10;
      m->velocity = ax.forward * speed + ax.up * up + ax.right * across;
      m->dies_at = s.level_time + fuse;
      m->radius_damage = damage;
      m->radius = kind == SqShot::grenade ? 160.0f : 165.0f;  // the quad does not widen it
      break;
    }
    case SqShot::rocket:
      m->velocity = ax.forward * 650;
      m->dies_at = s.level_time + 8000.0 / 650.0;
      m->damage = damage + ge::core::int_in(s.rng, 0, 20) * quad;
      m->radius_damage = 120 * quad;
      m->radius = 120;
      break;
    default:  // the BFG's ball
      m->velocity = ax.forward * 400;
      m->dies_at = s.level_time + 20;
      m->radius_damage = 200 * quad;
      m->radius = 100;
      break;
  }
  // The aim help's lead (a thrown grenade falls; the rest fly straight).
  const float fall = grenade(kind) ? float(s.cvars.find("sv_gravity")->number) : 0.0f;
  m->velocity = led_velocity(s, slot, weapon, start, m->velocity, fall, ax.forward);
  if (reach.fraction < 1) {  // the start is behind a wall: it meets it at once
    m->origin = reach.end - ax.forward * 10;
    if (reach.player >= 0) {
      const RocketBlow blow(s, *m);
      hurt(s, reach.player, slot, ax.forward, m->damage, 1, false, weapon);
    }
    // A monster closer than the muzzle (pressed against the player) takes the blow as one a
    // flying shot meets: a blaster fired into a soldier at arm's length hurt it not at all; a
    // pane or a button the bolt meets there takes its blow too (base2's pane under water, shot
    // from against it for a minute).
    const bool monster = reach.player < 0 && reach.mover >= 0 && monster_index(s, reach.mover) >= 0;
    if (monster && !grenade(kind)) {
      monster_damage(s, reach.mover, monster_hurt_by(slot, m->from), m->damage, m->velocity, false,
                     &reach.end);
      monster_noise(s, slot, reach.end, true);
    } else if (kind == SqShot::bolt && reach.player < 0 && reach.mover >= 0) {
      map_damage(s, reach.mover, slot, m->damage);  // a pane or a button he stands against
    }
    if (kind == SqShot::bolt)
      m->live = false;
    else if (!grenade(kind))
      explode(s, *m, m->origin, reach.player, monster ? reach.mover : -1);
    return;
  }
  if (!grenade(kind)) offer_dodge(s, slot, start, ax.forward, sh::length(m->velocity));
}

// The hand grenade let go at `speed` with `fuse` left, or (no speed) held to the end of its fuse:
// it explodes in the hand.
void throw_grenade(Q2Server& s, int slot, float speed, double fuse) {
  Q2Player& p = s.players[slot];
  const int w = int(SqWeapon::hand_grenade);
  const SqWeaponRow& r = kSqWeapons[w];
  p.pose.threw = true;
  const sh::Vec3f origin = unpack(p.move.origin);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f eye = origin + sh::Vec3f{0, 0, p.view_height - 8};
  const int quad = p.quad_until > s.level_time ? 4 : 1;
  const int damage = r.damage_dm * quad;
  if (speed <= 0) {
    Q2Missile* m = new_missile(s);
    if (m == nullptr) return;
    *m = Q2Missile{};
    m->live = true;
    m->kind = SqShot::hand_grenade;
    m->owner = slot;
    m->weapon = kSqByHeldGrenade;
    m->origin = origin;
    m->radius_damage = damage;
    m->radius = 165;
    explode(s, *m, origin, -1);
    return;
  }
  sound(s, slot + 1, origin, r.sound, p.silencer_shots > 0 ? 0.2f : 1.0f);  // silenced: quieter
  if (p.silencer_shots > 0) p.silencer_shots--;
  if (quad > 1) body_sound(s, slot, kItemChannel, "items/damage3.wav");
  const sh::Vec3f start = eye + ax.forward * r.forward + ax.right * r.right;
  sh::ViewAxes aimed = ax;
  aimed.forward = muzzle_aim(s, slot, start, ax.forward);
  launch(s, slot, SqShot::hand_grenade, origin, start, aimed, damage, quad, w, speed, fuse);
}

// The view's kick a shot gives (plugins/classic/q2_view.hpp), for one rules frame: up and back
// along the aim by the weapon; the machinegun's and chaingun's a shake (dealt off the frame and the
// slot, never the rules' own stream, so the shots fall where they did); the BFG's a long heave up.
void weapon_kick(Q2Server& s, int slot, int w, sh::Vec3f forward) {
  ge::classic::Q2ViewState& v = s.players[slot].view;
  ge::core::Rng shake =
      ge::core::make_rng(static_cast<uint32_t>(s.frame_number) * 64u + uint32_t(slot) + 0x5eedu);
  const auto jitter = [&shake](float most) {
    return static_cast<float>(ge::core::range(shake, -most, most));
  };
  float up = 0, back = 0;
  switch (static_cast<SqWeapon>(w)) {
    case SqWeapon::blaster:
    case SqWeapon::hyperblaster:
    case SqWeapon::grenade_launcher:
    case SqWeapon::rocket_launcher:
      up = 1, back = 2;
      break;
    case SqWeapon::shotgun:
    case SqWeapon::super_shotgun:
      up = 2, back = 2;
      break;
    case SqWeapon::railgun:
      up = 3, back = 3;
      break;
    case SqWeapon::machinegun:
    case SqWeapon::chaingun:
      v.weapon_angles = {static_cast<SqWeapon>(w) == SqWeapon::chaingun
                             ? jitter(0.7f)
                             : -machinegun_climb(s, slot),
                         jitter(0.7f), jitter(0.7f)};
      v.weapon_origin = {jitter(0.35f), jitter(0.35f), jitter(0.35f)};
      return;
    case SqWeapon::bfg:
      v.weapon_origin = forward * -2.0f;
      ge::classic::q2_view_hard_kick(v, -40, jitter(8));
      return;
    default:
      return;
  }
  v.weapon_angles = {-up, 0, 0};
  v.weapon_origin = forward * -back;
}

void fire(Q2Server& s, int slot, int shots) {
  Q2Player& p = s.players[slot];
  const int w = p.hand.weapon;
  // A shot is a noise, but for a silenced one and a thrown grenade (heard when it bursts).
  if (p.silencer_shots <= 0 && w != int(SqWeapon::hand_grenade)) monster_noise(s, slot);
  p.hostile_until = s.level_time + 1;
  p.pose.shot = true;
  const SqWeaponRow& r = kSqWeapons[w];
  const sh::Vec3f origin = unpack(p.move.origin);
  // The machine gun's climb bends its bullets' aim as it tips the view (weapon_kick).
  const float climb = w == int(SqWeapon::machinegun) ? machinegun_climb(s, slot) : 0.0f;
  sh::ViewAxes ax = sh::angle_vectors(p.view_angles + sh::Vec3f{-climb, 0, 0});
  const sh::Vec3f eye = origin + sh::Vec3f{0, 0, p.view_height - 8};
  const sh::Vec3f view_forward = ax.forward;
  if (r.shot == SqShot::pellets || r.shot == SqShot::bullet || r.shot == SqShot::rail)
    ax.forward = helped_forward(s, slot, w, eye, ax.forward);
  const sh::Vec3f start = eye + ax.forward * r.forward + ax.right * r.right;
  const sh::Vec3f along = ax.forward;  // the aim before `aimfix` turns it onto the eye's point
  if (w != int(SqWeapon::super_shotgun) && w != int(SqWeapon::chaingun))
    ax.forward = muzzle_aim(s, slot, start, ax.forward);
  const bool dm = deathmatch(s);
  const int quad = p.quad_until > s.level_time ? 4 : 1;  // ×4 damage and kick
  const int damage = (dm ? r.damage_dm : r.damage_sp) * quad;
  const int kick = r.kick * quad;
  // The shot's sound: in its muzzle flash (the light and the tails a client plays by the value),
  // or as a sound alone for a gun with none (a remix's own sound, the hand grenade's).
  const int flash = flash_value(w, shots);
  if (flash >= 0 && flash_plays(flash, r.sound))
    muzzle_flash(s, slot, origin, flash | (p.silencer_shots > 0 ? kQ2FlashSilenced : 0));
  else
    sound(s, slot + 1, origin, r.sound, p.silencer_shots > 0 ? 0.2f : 1.0f);  // silenced: quieter
  if (p.silencer_shots > 0) p.silencer_shots--;
  if (quad > 1) body_sound(s, slot, kItemChannel, "items/damage3.wav");
  weapon_kick(s, slot, w, view_forward);
  switch (r.shot) {
    case SqShot::pellets:
      if (w == int(SqWeapon::super_shotgun)) {
        const bool fix = s.cvars.find("aimfix")->integer() != 0;
        for (const float turn : {-5.0f, 5.0f}) {
          sh::ViewAxes half = sh::angle_vectors(p.view_angles + sh::Vec3f{0, turn, 0});
          half.forward = sh::aim_bend_dir(half.forward, view_forward, along);
          // aimfix: each half from the gun on its own turned axes, onto its own point.
          const sh::Vec3f from =
              fix ? eye + half.forward * r.forward + half.right * r.right : start;
          half.forward = muzzle_aim(s, slot, from, half.forward);
          for (int i = 0; i < 10; i++)
            bullet(s, slot, origin, from, half, 1000, 500, damage, kick, w);
        }
      } else {
        for (int i = 0; i < 12; i++) bullet(s, slot, origin, start, ax, 500, 500, damage, kick, w);
      }
      return;
    case SqShot::bullet:
      for (int i = 0; i < shots; i++) {
        sh::Vec3f from = start;
        if (w == int(SqWeapon::chaingun)) {
          const float across = 7 + crandom(s) * 4;  // the draws in a fixed order (above)
          const float up = crandom(s) * 4;
          from = eye + along * r.forward + ax.right * across + ax.up * up;
          sh::ViewAxes own = ax;  // aimfix: each bullet from its own start onto the eye's point
          own.forward = muzzle_aim(s, slot, from, along);
          bullet(s, slot, origin, from, own, 300, 500, damage, kick, w);
          continue;
        }
        bullet(s, slot, origin, from, ax, 300, 500, damage, kick, w);
      }
      return;
    case SqShot::rail:
      rail(s, slot, start, ax, damage, (dm ? 200 : 250) * quad, w);
      return;
    default:
      for (int i = 0; i < shots; i++) launch(s, slot, r.shot, origin, start, ax, damage, quad, w);
      return;
  }
}

// Item `item` thrown from player `slot`: 100 forward along his view turned `turn` degrees, 300
// up, from 24 ahead and 16 below his eye (nearer when a wall is closer); his to take back after
// 1 s, gone after 30 s in deathmatch. Null when every drop's row is in use.
Q2Drop* toss(Q2Server& s, int slot, int item, float turn) {
  Q2Drop* d = nullptr;
  for (Q2Drop& free : s.drops)
    if (!free.live) {
      d = &free;
      break;
    }
  if (d == nullptr) return nullptr;
  const Q2Player& p = s.players[slot];
  const sh::Vec3f origin = unpack(p.move.origin);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles + sh::Vec3f{0, turn, 0});
  sh::Vec3f start = origin + sh::Vec3f{0, 0, p.view_height - 16} + ax.forward * 24;
  const float a[3] = {origin.x, origin.y, origin.z}, b[3] = {start.x, start.y, start.z};
  const float lo[3] = {-kItemHalf, -kItemHalf, -kItemHalf};
  const float hi[3] = {kItemHalf, kItemHalf, kItemHalf};
  const ge::brush::Trace t = s.tracer->trace(a, b, lo, hi, kWorldMask);
  if (t.fraction < 1) start = {t.end[0], t.end[1], t.end[2]};
  *d = Q2Drop{};
  d->live = true;
  d->item = item;
  d->owner = slot;
  d->origin = start;
  d->velocity = ax.forward * 100;
  d->velocity.z = 300;
  d->takeable_at = s.level_time + 1;
  d->dies_at = deathmatch(s) ? s.level_time + 30 : 0;
  return d;
}

}  // namespace

size_t fight_first_row(const Q2Server& s) { return 1 + sh::kMaxClients + s.entities.size(); }

void fight_precache(Q2Server& s) {
  const auto model = [&s](const char* name) {
    if (name[0] == '\0') return 0;
    const auto found = std::find(s.models.begin(), s.models.end(), name);
    if (found != s.models.end()) return static_cast<int>(found - s.models.begin());
    s.models.emplace_back(name);
    return static_cast<int>(s.models.size()) - 1;
  };
  for (int w = 0; w < kSqWeaponCount; w++) s.weapon_models[w] = model(kSqWeapons[w].view_model);
  // The weapons as others see them carried: names a client takes from the player's own model
  // folder ("#" and the file), in the order a client numbers them from 1 (the skin's high byte).
  for (const auto& [w, name] : kCarried) s.carried_models[static_cast<int>(w)] = model(name);
  for (int k = 0; k < kSqShotKinds; k++) s.missile_models[k] = model(kMissileModels[k]);
  gibs_precache(s);
  const auto add_sound = [&s](const char* name) {
    if (std::find(s.sounds.begin(), s.sounds.end(), name) == s.sounds.end())
      s.sounds.emplace_back(name);
  };
  for (const SqWeaponRow& r : kSqWeapons) add_sound(r.sound);
  for (const char* name : {kClick, kBoom, kGrenadeBoom, kBounce, kHandBounce, kFuse, kGib,
                           kHandSounds[0], kHandSounds[1], kHandSounds[2], kHandSounds[3]})
    add_sound(name);
  for (const char* name : kOwnSounds) add_sound(name);
  for (const char* name : kBodySounds) add_sound(name);
  for (int k = 0; k < kSqLoops; k++) {
    add_sound(kLoops[k]);
    s.loop_sounds[k] =
        static_cast<int>(std::find(s.sounds.begin(), s.sounds.end(), kLoops[k]) - s.sounds.begin());
  }
}

void fight_spawn(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  p.hand = SqHand{};
  p.stepped = false;
  p.killer = -1;
  screens_spawned(s, slot);
  p.quad_until = p.invulnerable_until = p.breather_until = p.suit_until = 0;
  p.silencer_shots = 0;
  p.power_on = false;
  p.power_flash = 0;
  p.air_until = s.level_time + 12;
  p.drown_at = 0;
  p.drown_damage = 2;
  p.old_fall = 0;
  p.frame_damage = 0;
  p.pain_sound_at = -100;
  p.old_water_level = 0;
  p.gibbed = false;
  p.view = {};
}

namespace {

// The water's sounds as the body goes in and out (the level it was at the last frame's end): in
// (lava's own), out, the head under, and the breath on surfacing by the air it had left: a gasp
// when it ran out, a breath when more than a second of it was spent.
void water_sounds(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  const int was = p.old_water_level, now_level = p.water_level;
  p.old_water_level = now_level;
  if (was == now_level) return;
  if (was == 0)
    body_sound(s, slot, kBodyChannel,
               (p.water_type & sh::kSqLava) != 0 ? "player/lava_in.wav" : "player/watr_in.wav");
  if (now_level == 0) body_sound(s, slot, kBodyChannel, "player/watr_out.wav");
  if (now_level == 3) body_sound(s, slot, kBodyChannel, "player/watr_un.wav");
  if (was == 3) {
    const double left = p.air_until - s.level_time;
    if (left < 0)
      body_sound(s, slot, kVoiceChannel, "player/gasp1.wav");
    else if (left < 11)
      body_sound(s, slot, kVoiceChannel, "player/gasp2.wav");
  }
}

// The body's frame: the air, the liquids and the fall (fight_body_frame); false when it died.
bool body_frame(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  const double now = s.level_time;
  const bool suit = p.suit_until > now;
  water_sounds(s, slot);
  // A power about to run out says so 30 frames before: the first held of these, alone.
  const std::pair<double, const char*> warn[] = {{p.quad_until, "items/damage2.wav"},
                                                 {p.invulnerable_until, "items/protect2.wav"},
                                                 {p.suit_until, "items/airout.wav"},
                                                 {p.breather_until, "items/airout.wav"}};
  for (const auto& [until, sample] : warn)
    if (until > now) {
      if (std::floor((until - now) * 10 + 0.5) == 30) body_sound(s, slot, kItemChannel, sample);
      break;
    }
  // Air: refilled by the rebreather or the suit (a breath every 2.5 s under water, counted to the
  // rebreather's end even under the suit alone); once it runs out under water, a blow each second
  // that grows by 2 to 15 (4 first), no armour against it, a gurp (the last: a drowning cry).
  if (std::max(p.breather_until, p.suit_until) > now) {
    p.air_until = std::max(p.air_until, now + 10);
    const auto left = static_cast<int64_t>(std::floor((p.breather_until - now) * 10 + 0.5));
    if (p.water_level == 3 && left % 25 == 0) {
      body_sound(s, slot, kAutoChannel,
                 p.breath_second ? "player/u_breath2.wav" : "player/u_breath1.wav");
      p.breath_second = !p.breath_second;
    }
  }
  if (p.water_level < 3) {
    p.air_until = std::max(p.air_until, now + 12);
    p.drown_damage = 2;
  } else if (now >= p.air_until && now >= p.drown_at) {
    p.drown_at = now + 1;
    p.drown_damage = std::min(p.drown_damage + 2, 15);
    const int blow = p.drown_damage;
    body_sound(
        s, slot, kVoiceChannel,
        float(blow) >= p.health ? "player/drown1.wav" : kOwnSounds[6 + variant(s, slot, 2, 2)]);
    p.pain_sound_at = now;
    hurt(s, slot, -1, {}, blow, 0, false, kSqByWater, true);
    if (!p.alive) return false;
  }
  // Lava and slime each frame he is in them, by how deep; lava burns aloud now and then.
  if (p.water_level > 0 && (p.water_type & sh::kSqLava) != 0) {
    if (p.invulnerable_until <= now && now > p.pain_sound_at) {
      body_sound(s, slot, kVoiceChannel,
                 variant(s, slot, 3, 2) == 0 ? "player/burn1.wav" : "player/burn2.wav");
      p.pain_sound_at = now + 1;
    }
    hurt(s, slot, -1, {}, (suit ? 1 : 3) * p.water_level, 0, false, kSqByLava);
    if (!p.alive) return false;
  }
  if (p.water_level > 0 && (p.water_type & sh::kSqSlime) != 0 && !suit) {
    hurt(s, slot, -1, {}, p.water_level, 0, false, kSqBySlime);
    if (!p.alive) return false;
  }
  // Falling: the fall speed lost since the last frame (the last frame's when still falling but
  // slower, else the change on landing), squared × 0.0001; less in water. A landing over 1 is a
  // step, over 15 a thud, over 30 a cry in the player's own voice (over 55 the far one) and a
  // blow of half the rest.
  const float vz = unpack(p.move.velocity).z;
  float delta = 0;
  if (p.old_fall < 0 && vz > p.old_fall && !p.on_ground)
    delta = p.old_fall;
  else if (p.on_ground)
    delta = vz - p.old_fall;
  p.old_fall = vz;
  delta = delta * delta * 0.0001f;
  if (p.water_level == 3) return true;
  if (p.water_level == 2) delta *= 0.25f;
  if (p.water_level == 1) delta *= 0.5f;
  if (delta < 1) return true;
  p.landed = true;
  ge::classic::q2_view_fall(p.view, delta);  // a hard landing dips the view
  if (delta < 15) {
    body_sound(s, slot, kBodyChannel, kBodySounds[12 + variant(s, slot, 4, 4)]);
    return true;
  }
  if (delta <= 30) {
    body_sound(s, slot, kAutoChannel, "player/land1.wav");
    return true;
  }
  body_sound(s, slot, kAutoChannel, delta >= 55 ? "*fall1.wav" : "*fall2.wav");
  p.pain_sound_at = now;
  // No harm under dmflags 8, nor under Quake's physics (`sv_physics q1`): its
  // arena hurts nobody for a fall.
  const bool no_falling =
      (deathmatch(s) && (s.cvars.find("dmflags")->integer() & 8) != 0) || s.q1_physics;
  if (no_falling) return true;
  hurt(s, slot, -1, {0, 0, -1}, std::max(1, static_cast<int>((delta - 30) / 2)), 0, false,
       kSqByFall);
  return p.alive;
}

}  // namespace

void fight_stride(Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  if (!fightable(s, slot) || !p.alive || p.landed) return;
  // g_footsteps: 1 running faster than 225 on the ground; 2 any pace on the ground; 3+ always.
  const int32_t when = s.cvars.find("g_footsteps")->integer();
  if (when < 1 || (when < 3 && !p.on_ground)) return;
  const sh::Vec3f v = unpack(p.move.velocity);
  if (when == 1 && v.x * v.x + v.y * v.y <= 225.0f * 225.0f) return;
  body_sound(s, slot, kBodyChannel, kBodySounds[12 + variant(s, slot, 9, 4)]);
}

void fight_body_frame(Q2Server& s, int slot) {
  s.players[slot].landed = false;
  if (!fightable(s, slot)) return;
  Q2Player& p = s.players[slot];
  const bool alive = body_frame(s, slot);
  // The frame's blows cry out once in the player's own voice, by the health left (at most every
  // 0.75 s; not while invulnerable).
  const double now = s.level_time;
  const int damage = p.frame_damage;
  p.frame_damage = 0;
  if (!alive || damage <= 0 || p.invulnerable_until > now || now <= p.pain_sound_at + kPainEvery)
    return;
  p.pain_sound_at = now;
  const int band = p.health < 25 ? 0 : p.health < 50 ? 1 : p.health < 75 ? 2 : 3;
  body_sound(s, slot, kVoiceChannel, kOwnSounds[9 + band * 2 + variant(s, slot, 5, 2)]);
}

void fight_moved(Q2Server& s, int slot, bool was_on_ground, int up) {
  const Q2Player& p = s.players[slot];
  if (was_on_ground && !p.on_ground && up >= 10 && p.water_level == 0) {
    body_sound(s, slot, kVoiceChannel, "*jump1.wav");
    monster_noise(s, slot, unpack(p.move.origin), false);  // a jump is heard
  }
}

namespace {

// Whether two words match ignoring case.
bool same_word(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

}  // namespace

bool fight_limits(Q2Server& s) {
  if (!s.pending_map.empty() || s.intermission_at > 0) return false;
  const int frags = static_cast<int>(s.cvars.find("fraglimit")->number);
  const double minutes = s.cvars.find("timelimit")->number;
  std::string why;
  if (minutes > 0 && s.level_time >= minutes * 60) why = "Timelimit hit.";
  for (int i = 0; i < sh::kMaxClients && why.empty() && frags > 0; i++)
    if (s.clients[i].active && s.players[i].frags >= float(frags)) why = "Fraglimit hit.";
  if (why.empty()) return false;
  print_all(s, why + "\n", 2);  // at the high level
  // The next map: the word after this one in `sv_maplist` (split at spaces, commas and line ends,
  // found ignoring case, wrapping); a list without this map is passed over; then the world's
  // `nextmap`, then the map's first exit, then this map again.
  std::vector<std::string> list;
  std::string word;
  for (const char c : s.cvars.find("sv_maplist")->value + " ") {
    if (c == ' ' || c == ',' || c == '\r' || c == '\n') {
      if (!word.empty() && word != "0") list.push_back(word);  // "0" names none
      word.clear();
    } else {
      word += c;
    }
  }
  std::string next;
  for (size_t i = 0; i < list.size() && next.empty(); i++)
    if (same_word(list[i], s.map_name)) next = list[(i + 1) % list.size()];
  if (next.empty() && s.map && !s.map->entities.empty())
    next = s.map->entities[0].get_any_case("nextmap");
  for (size_t i = 0; i < s.entities.size() && next.empty(); i++)
    if (s.entities[i].classname == "target_changelevel") next = s.entities[i].map;
  if (next.empty()) next = s.map_name;
  s.next_map = dmflag(s, 32) ? s.map_name : next;  // the same level again
  return true;
}

void fight_clear(Q2Server& s) {
  for (Q2Missile& m : s.missiles) m = Q2Missile{};
  for (Q2Drop& d : s.drops) d = Q2Drop{};
  for (Q2Body& b : s.bodies) b = Q2Body{};
  s.next_body = 0;
  gibs_clear(s);
  s.obituaries.clear();
}

void fight_step(Q2Server& s, int slot, bool attack) {
  Q2Player& p = s.players[slot];
  if (!fightable(s, slot)) return;
  // g_swap_speed: frames a lower or raise advances a frame; 1 or less, or 65535 or more, is 1.
  const int swap = s.cvars.find("g_swap_speed")->integer();
  const SqStep step = fight_hand_step(s, slot, attack, dmflag(s, 8192) || fight_midair(s),
                                      swap > 1 && swap < 65535 ? swap : 1);
  if (step.click) sound(s, slot + 1, unpack(p.move.origin), kClick);
  if (step.shots > 0 && p.hand.weapon == int(SqWeapon::hand_grenade))
    throw_grenade(s, slot, step.throw_speed, step.fuse_left);
  else if (step.shots > 0)
    fire(s, slot, step.shots);
}

void fight_missiles(Q2Server& s) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  for (Q2Missile& m : s.missiles) {
    if (!m.live) continue;
    if (s.level_time + 0.001 >= m.dies_at) {
      if (grenade(m.kind))
        explode(s, m, m.origin, -1);
      else
        m.live = false;
      continue;
    }
    if (m.resting) continue;
    q2_hold_speed(s, m.velocity);
    if (grenade(m.kind)) m.velocity.z -= gravity * s.step;
    const sh::Vec3f to = m.origin + m.velocity * s.step;
    const uint32_t owner = m.owner >= 0 ? 1u << m.owner : 0u;
    // sv_antilag 2 with sv_antilag_projectiles: it meets the players where its owner saw them.
    Line l;
    {
      const LagScope lag(s, s.lag_missiles ? m.owner : -1);
      l = shoot_line(s, m.origin, to, owner, m.from);
    }
    // A direct hit on a monster: its blow (a grenade's less half its distance from the burst),
    // then the burst leaves it out; a bolt goes on as against a wall.
    if (l.player < 0 && l.fraction < 1 && l.mover >= 0 && monster_index(s, l.mover) >= 0) {
      const Q2Entity& t = s.entities[size_t(l.mover)];
      const float d = sh::length(l.end - (t.origin + (t.mins + t.maxs) * 0.5f));
      const int direct = grenade(m.kind) ? static_cast<int>(float(m.damage) - 0.5f * d) : m.damage;
      monster_damage(s, l.mover, monster_hurt_by(m.owner, m.from), direct, m.velocity, false,
                     &l.end);
      if (m.kind == SqShot::bolt && m.owner >= 0) monster_noise(s, m.owner, l.end, true);
      if (m.kind != SqShot::bolt) {
        explode(s, m, l.end, -1, l.mover);
        continue;
      }
    }
    if (l.player >= 0) {
      const int hit = l.player;
      switch (m.kind) {
        case SqShot::bolt:
          hurt(s, hit, m.owner, m.velocity, m.damage, 1, true, m.weapon);
          if (m.owner >= 0) monster_noise(s, m.owner, l.end, true);
          m.live = false;
          break;
        case SqShot::grenade:
        case SqShot::hand_grenade: {
          const float d = sh::length(l.end - centre_of(s.players[hit]));
          hurt(s, hit, m.owner, m.velocity, static_cast<int>(float(m.damage) - 0.5f * d),
               static_cast<int>(float(m.damage) - 0.5f * d), false, m.weapon);
          explode(s, m, l.end, hit);
          break;
        }
        default: {
          const RocketBlow blow(s, m);
          hurt(s, hit, m.owner, m.velocity, m.damage, 0, m.kind == SqShot::bfg, m.weapon);
          explode(s, m, l.end, hit);
          break;
        }
      }
      continue;
    }
    if (l.fraction < 1) {
      if (l.sky) {
        m.live = false;
        continue;
      }
      switch (m.kind) {
        case SqShot::bolt:  // a mover or a pane that takes blows takes the bolt's
          if (l.mover >= 0 && monster_index(s, l.mover) < 0)
            map_damage(s, l.mover, m.owner, m.damage);
          effect(s, "te_blaster", l.end, l.normal);
          if (m.owner >= 0) monster_noise(s, m.owner, l.end, true);
          m.live = false;
          break;
        case SqShot::grenade:
        case SqShot::hand_grenade: {  // a bounce off the wall
          m.origin = l.end + l.normal * 0.1f;
          m.velocity = m.velocity - l.normal * (1.5f * sh::dot(m.velocity, l.normal));
          if (l.normal.z > 0.7f && sh::length(m.velocity) < 60) {
            m.velocity = {};
            m.resting = true;
          }
          sound(s, int(missile_row(s, m)), m.origin,
                m.kind == SqShot::hand_grenade ? kHandBounce : kBounce);
          break;
        }
        default:
          explode(s, m, l.end - m.velocity * (0.001f), -1);
          break;
      }
      continue;
    }
    m.origin = to;
    if (m.kind != SqShot::bfg) continue;
    // The ball's lasers: every player within 256 it can see takes a light blow.
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (i == m.owner || !fightable(s, i)) continue;
      const sh::Vec3f c = centre_of(s.players[i]);
      if (sh::length(c - m.origin) > 256) continue;
      if (shoot_line(s, m.origin, c, owner).player != i) continue;
      const BlowHow how(s, kSqHowLaser);
      hurt(s, i, m.owner, c - m.origin, deathmatch(s) ? 5 : 10, 0, true, m.weapon);
    }
    for (size_t e = 0; e < s.entities.size(); e++) {  // and every monster but the hornet
      const Q2Entity& t = s.entities[e];
      if (t.gone || t.health <= 0 || monster_index(s, static_cast<int>(e)) < 0 ||
          t.classname == "monster_boss2")
        continue;
      const sh::Vec3f c = t.origin + (t.mins + t.maxs) * 0.5f;
      if (sh::length(c - m.origin) > 256 || !reaches(s, m.origin, c)) continue;
      map_damage(s, static_cast<int>(e), m.owner, deathmatch(s) ? 5 : 10, c - m.origin, true);
    }
  }
}

void fight_choose(Q2Server& s, int slot, int w) {
  Q2Player& p = s.players[slot];
  if (fightable(s, slot)) sq_choose(p.hand, p.body, w);
}

void fight_took(Q2Server& s, int slot, int item) {
  const int w = sq_weapon_of_item(item);
  Q2Player& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  const bool dm = deathmatch(s);
  // The status bar's pickup (3 s), and the item selected when it has a use.
  p.picked = sq_item_number(item);
  p.picked_at = s.level_time;
  if (sq_selects(p.picked)) p.body.selected = p.picked;
  if (it.kind == SqKind::power_armour && dm && p.body.counts[item] == 1) p.power_on = true;
  if (it.kind == SqKind::powerup && dm && (s.cvars.find("dmflags")->integer() & 16) != 0)
    fight_use(s, slot, item);  // instant items
  const bool may = !dm || p.hand.weapon == int(SqWeapon::blaster);
  // Grenades, both ammunition and a weapon: the first ones held are taken in hand.
  if (it.kind == SqKind::ammo && it.ammo == SqAmmo::grenades &&
      p.body.ammo[static_cast<int>(SqAmmo::grenades)] == it.amount &&
      p.hand.weapon != int(SqWeapon::hand_grenade) && may)
    sq_choose(p.hand, p.body, int(SqWeapon::hand_grenade));
  if (w < 0 || p.body.counts[item] != 1) return;  // only a newly owned weapon
  if (may) sq_choose(p.hand, p.body, w);
}

bool fight_command(Q2Server& s, int slot, const std::string& cmd, const std::string& arg) {
  Q2Player& p = s.players[slot];
  if (cmd == "use") {
    const int w = sq_weapon_named(arg);
    // g_select_empty 0: a held weapon with no ammunition is refused and the player told.
    if (w >= 0 && fightable(s, slot) && sq_holds(p.body, w) && !sq_can_fire(p.body, w) &&
        !dmflag(s, 8192) && !s.cvars.find("g_select_empty")->on()) {
      const char* ammo = "ammo";
      for (const SqItem& it : kSqItems)
        if (it.kind == SqKind::ammo && it.ammo == kSqWeapons[w].ammo) ammo = it.pickup;
      print_to(s, slot, std::string("No ") + ammo + " for " + kSqWeapons[w].name + ".\n");
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
    const int w = sq_weapon_named(arg);
    fight_drop(s, slot,
               w >= 0 && kSqWeapons[w].item != nullptr ? sq_item(kSqWeapons[w].item) : item);
    return true;
  }
  if (cmd != "weapnext" && cmd != "weapprev") return false;
  const int way = cmd == "weapnext" ? 1 : kSqWeaponCount - 1;
  // g_quick_weap 1: from the weapon chosen but not yet raised, so presses skip ahead, its icon
  // and name shown 0.9 s (the bar's pickup line, which shows 3 s, set 2.1 s back).
  const bool quick = s.cvars.find("g_quick_weap")->on();
  const int from = quick && p.hand.pending >= 0 ? p.hand.pending : p.hand.weapon;
  for (int k = 1; k < kSqWeaponCount; k++) {
    const int w = (from + way * k) % kSqWeaponCount;
    if (sq_can_fire(p.body, w)) {
      fight_choose(s, slot, w);
      if (quick && p.hand.pending == w && kSqWeapons[w].item != nullptr) {
        p.picked = sq_item_number(sq_item(kSqWeapons[w].item));
        p.picked_at = s.level_time - 2.1;
      }
      break;
    }
  }
  return true;
}

void fight_kill(Q2Server& s, int slot, int killer, int weapon) {
  Q2Player& p = s.players[slot];
  if (!p.alive) return;
  p.alive = false;
  p.health = std::min(p.health, 0.0f);
  p.died_at = s.level_time;
  p.killer = killer;
  p.move.type = ge::classic::Q2Type::dead;
  screens_died(s, slot);
  const std::string& victim = name_of(s, slot);
  std::string line;
  if (killer >= 0 && killer != slot && fight_midair(s) && s.midair_frags > 0 &&
      !fight_teammates(s, killer, slot)) {
    // Midair: a counted hit scores by its rise and is told so (plugins/shooter/midair.hpp).
    Q2Player& k = s.players[killer];
    k.frags += float(s.midair_frags);
    sh::midair_note(k.midair, s.midair_rise);
    line =
        sh::midair_line(sh::midair_band(s.midair_rise), s.midair_rise, name_of(s, killer), victim);
    line.pop_back();
  } else {
    // Single player says only that he died; co-op and deathmatch say how. Deathmatch scores it: a
    // kill by another 1 to him (1 off for a teammate), any other death 1 off the dead.
    const bool other = killer >= 0 && killer != slot;
    std::string gender;
    for (const auto& [k, v] : s.clients[slot].userinfo)
      if (k == "gender") gender = v;
    bool by_killer = false;
    if (!deathmatch(s) && !s.cvars.find("coop")->on())
      line = victim + " died.";
    else
      line = sq_obituary(victim, other ? name_of(s, killer) : std::string(), killer == slot, weapon,
                         s.blow_how, gender, &by_killer);
    if (deathmatch(s) && !fight_midair(s)) {
      if (by_killer)
        s.players[killer].frags += fight_teammates(s, killer, slot) ? -1.0f : 1.0f;
      else
        p.frags -= 1;
    } else if (deathmatch(s) && !other) {
      p.frags -= 1;
    }
  }
  s.obituaries.push_back(line);
  print_all(s, line + "\n");
  q2_mods_death(s, slot, killer);
  const sh::Vec3f at = unpack(p.move.origin);
  if (p.health < kGibHealth) {  // thrown apart: the body is gone, his head left
    p.gibbed = true;
    p.skull = ge::core::chance(s.rng, 0.5);
    gib(s, slot + 1, at,
        {at + p.mins, at + p.maxs, unpack(p.move.velocity), p.killing_blow, SqGibSet::player});
    // He is his head now: raised 32 units and thrown, moved by the gib's box from here on.
    pack(p.move.origin, at + sh::Vec3f{0, 0, 32});
    pack(p.move.velocity, gibs_head_throw(s, unpack(p.move.velocity), p.killing_blow));
  } else {  // a death cry in the player's own voice
    body_sound(s, slot, kVoiceChannel, kOwnSounds[variant(s, slot, 1, 4)]);
  }
  // In deathmatch the weapon in hand (not the blaster, only with its ammo) is dropped, and each
  // powerup running with more than 1 s left (the silencer with shots left) whose drop setting is on
  // (`sv_dropquad` or `dmflags` 16384 for the quad), all thrown 22.5 degrees apart.
  if (deathmatch(s)) {
    const SqWeaponRow& r = kSqWeapons[p.hand.weapon];
    struct Toss {
      double left;  // a power's time left, s (0: not a power)
      int item;
      int shots;  // the silencer's shots left
    };
    Toss out[6];
    int n = 0;
    if (r.item != nullptr && r.ammo != SqAmmo::none && p.body.ammo[static_cast<int>(r.ammo)] > 0)
      out[n++] = {0, sq_item(r.item), 0};
    const auto on = [&](const char* name) { return s.cvars.find(name)->on(); };
    const auto timed = [&](bool wanted, double until, const char* item) {
      if (wanted && until > s.level_time + 1) out[n++] = {until - s.level_time, sq_item(item), 0};
    };
    timed(dmflag(s, 16384) || on("sv_dropquad"), p.quad_until, "item_quad");
    timed(on("sv_droppent"), p.invulnerable_until, "item_invulnerability");
    timed(on("sv_dropsuit"), p.suit_until, "item_enviro");
    timed(on("sv_dropbreather"), p.breather_until, "item_breather");
    if (on("sv_dropsilencer") && p.silencer_shots > 0)
      out[n++] = {0, sq_item("item_silencer"), p.silencer_shots};
    for (int i = 0; i < n; i++) {
      Q2Drop* d = toss(s, slot, out[i].item, (float(i) - float(n - 1) * 0.5f) * 22.5f);
      if (d == nullptr) continue;
      d->power_left = out[i].left;
      d->shots = out[i].shots;
    }
  }
  p.quad_until = p.invulnerable_until = p.breather_until = p.suit_until = 0;
  p.silencer_shots = 0;
}

bool fight_dmflag(const Q2Server& s, int bit) { return dmflag(s, bit); }

bool fight_midair(const Q2Server& s) {
  const auto* c = s.cvars.find("k_midair");
  return deathmatch(s) && c != nullptr && c->on();
}

void fight_midair_kit(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  if (!fight_midair(s) || !p.alive) return;
  p.health = sh::kMidairHealth;
  p.body.health = static_cast<int>(sh::kMidairHealth);
  p.body.armour = SqArmour::body;
  p.body.armour_count = static_cast<int>(sh::kMidairArmour);
  const int rockets = static_cast<int>(SqAmmo::rockets);
  p.body.max_ammo[rockets] = 255;
  p.body.ammo[rockets] = 255;
  p.body.counts[sq_item("weapon_rocketlauncher")] = 1;
  p.hand = SqHand{};
  p.hand.weapon = int(SqWeapon::rocket_launcher);
}

std::vector<std::string> fight_midair_report(const Q2Server& s) {
  std::vector<std::string> out;
  if (!fight_midair(s)) return out;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    std::string line = sh::midair_tally_line(name_of(s, i), s.players[i].midair);
    if (!line.empty()) out.push_back(std::move(line));
  }
  return out;
}

void fight_hurt(Q2Server& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
                int weapon, int flags) {
  hurt(s, target, attacker, dir, damage, knockback, false, weapon, (flags & 1) != 0,
       (flags & 2) != 0);
}

void fight_blast(Q2Server& s, sh::Vec3f at, int attacker, int damage, float radius, int weapon) {
  blast(s, at, attacker, damage, radius, -1, weapon);
}

bool fight_drop(Q2Server& s, int slot, int item) {
  if (item < 0 || item >= kSqItemCount || !fightable(s, slot)) return false;
  Q2Player& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  switch (it.kind) {
    case SqKind::weapon: {
      if (dmflag(s, 4)) {
        print_to(s, slot, "Weapons stay in this match, so none can be dropped.\n");
        return false;
      }
      if (p.body.counts[item] <= 0) return false;
      if (sq_weapon_of_item(item) == p.hand.weapon && p.body.counts[item] == 1) {
        print_to(s, slot, "You can't drop the weapon in your hand.\n");
        return false;
      }
      if (toss(s, slot, item, 0) == nullptr) return false;
      p.body.counts[item]--;
      return true;
    }
    case SqKind::ammo: {
      int& held = p.body.ammo[static_cast<int>(it.ammo)];
      const int n = std::min(held, it.amount);
      if (n <= 0) return false;
      if (p.hand.weapon == int(SqWeapon::hand_grenade) && it.ammo == SqAmmo::grenades &&
          held - n <= 0) {
        print_to(s, slot, "You can't drop the weapon in your hand.\n");
        return false;
      }
      Q2Drop* d = toss(s, slot, item, 0);
      if (d == nullptr) return false;
      d->count = n;
      held -= n;
      return true;
    }
    case SqKind::powerup:
      if (p.body.counts[item] <= 0 || toss(s, slot, item, 0) == nullptr) return false;
      p.body.counts[item]--;
      return true;
    default:
      return false;
  }
}

namespace {

// A shot in flight as the world names it: what explodes, what bounces first, what glows; the
// hitscan kinds never fly.
WorldShot shot_kind(SqShot k) {
  switch (k) {
    case SqShot::rocket:
      return WorldShot::rocket;
    case SqShot::grenade:
    case SqShot::hand_grenade:
      return WorldShot::grenade;
    case SqShot::bolt:
      return WorldShot::bolt;
    case SqShot::bfg:
      return WorldShot::ball;
    default:
      return WorldShot::none;
  }
}

}  // namespace

namespace {

int loop_of(const Q2Server& s, SqLoop k) { return s.loop_sounds[static_cast<int>(k)]; }

// The loop a player carries, the first that holds: frying in lava or slime, the railgun's or the
// BFG's hum while held, the hyperblaster's or the chaingun's spin while firing, a hand grenade's
// fuse in the hand; none dead or at the intermission.
int player_loop(const Q2Server& s, const Q2Player& p) {
  if (!p.alive || s.intermission_at > 0) return 0;
  if (p.water_level > 0 && (p.water_type & (sh::kSqLava | sh::kSqSlime)) != 0)
    return loop_of(s, SqLoop::fry);
  const auto w = static_cast<SqWeapon>(p.hand.weapon);
  const bool firing = p.hand.state == SqHandState::firing;
  if (w == SqWeapon::railgun) return loop_of(s, SqLoop::rail_hum);
  if (w == SqWeapon::bfg) return loop_of(s, SqLoop::bfg_hum);
  if (w == SqWeapon::hyperblaster && firing) return loop_of(s, SqLoop::hyper);
  if (w == SqWeapon::chaingun && firing) return loop_of(s, SqLoop::chain);
  if (w == SqWeapon::hand_grenade && p.hand.fuse_at > 0 && p.hand.frame == 11)
    return loop_of(s, SqLoop::fuse);  // lit and still in the hand
  return 0;
}

// The loop a shot carries in flight: a bolt's, a rocket's, a thrown hand grenade's fuse, the BFG's
// ball; a launcher's grenade and the rest fly quiet.
int shot_loop(const Q2Server& s, const Q2Missile& m) {
  switch (m.kind) {
    case SqShot::bolt:
      return loop_of(s, SqLoop::bolt);
    case SqShot::rocket:
      return loop_of(s, SqLoop::rocket);
    case SqShot::hand_grenade:
      return loop_of(s, SqLoop::fuse);
    case SqShot::bfg:
      return loop_of(s, SqLoop::bfg_ball);
    default:
      return 0;
  }
}

}  // namespace

void fight_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  w.weapon_frames = true;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    const Q2Player& p = s.players[i];
    PlayerNumbers& n = w.clients[i].numbers;
    const SqWeaponRow& r = kSqWeapons[p.hand.weapon];
    n.weapon = float(p.hand.weapon + 1);
    n.weapon_model = p.alive && s.intermission_at <= 0 ? s.weapon_models[p.hand.weapon] : 0;
    n.weapon_frame = float(p.hand.frame);
    n.current_ammo = r.ammo == SqAmmo::none ? 0 : float(p.body.ammo[static_cast<int>(r.ammo)]);
    w.entities[size_t(i) + 1].loop = player_loop(s, p);
  }
  const size_t first = fight_first_row(s);
  if (w.entities.size() < first + kQ2Missiles + kQ2Drops + kQ2Bodies + kQ2Gibs) return;
  gibs_rows(s, first + kQ2Missiles + kQ2Drops + kQ2Bodies);
  for (int k = 0; k < kQ2Bodies; k++) {
    const Q2Body& b = s.bodies[k];
    WorldEntity& e = w.entities[first + kQ2Missiles + kQ2Drops + size_t(k)];
    e = WorldEntity{};
    if (!b.live) continue;
    e.free = false;
    e.origin = b.origin;
    e.angles = {0, b.yaw, 0};
    e.mins = {-16, -16, -24};
    e.maxs = {16, 16, -8};
    e.absmin = e.origin + e.mins;
    e.absmax = e.origin + e.maxs;
    e.health = b.health;
    e.frame = b.frame;
    e.modelindex = static_cast<int>(s.models.size()) - 1;  // the player's model, last
  }
  for (int k = 0; k < kQ2Drops; k++) {
    const Q2Drop& d = s.drops[k];
    WorldEntity& e = w.entities[first + kQ2Missiles + size_t(k)];
    e = WorldEntity{};
    if (!d.live) continue;
    e.free = false;
    e.origin = d.origin;
    e.velocity = d.velocity;
    e.modelindex = s.item_models[size_t(d.item)];
    e.turns = sq_turns(d.item);
    e.effects = e.turns ? kSqTurnEffect : 0;
    e.render_fx = kSqGlowRender;  // a dropped item's light pulses too
    sq_dress(s, e);
  }
  for (int k = 0; k < kQ2Missiles; k++) {
    const Q2Missile& m = s.missiles[k];
    WorldEntity& e = w.entities[first + size_t(k)];
    e = WorldEntity{};
    if (!m.live) continue;
    e.free = false;
    e.origin = m.origin;
    e.velocity = m.velocity;
    const double flat =
        double(sh::root_of(m.velocity.x * m.velocity.x + m.velocity.y * m.velocity.y));
    constexpr double kDegrees = 180.0 / 3.14159265358979323846;
    e.angles = {static_cast<float>(-ge::core::libm::atan2(m.velocity.z, flat) * kDegrees),
                static_cast<float>(ge::core::libm::atan2(m.velocity.y, m.velocity.x) * kDegrees),
                0};
    e.modelindex = s.missile_models[static_cast<int>(m.kind)];
    e.shot = shot_kind(m.kind);
    e.effects = m.effects >= 0 ? m.effects : kSqShotEffects[static_cast<int>(m.kind)];
    e.loop = shot_loop(s, m);
  }
}

namespace {

// The part of `skin` ("model/skin") that names a team: the skin after the slash, or the model.
std::string team_part(const std::string& skin, bool by_model) {
  const size_t slash = skin.find('/');
  if (by_model) return slash == std::string::npos ? skin : skin.substr(0, slash);
  return slash == std::string::npos ? std::string() : skin.substr(slash + 1);
}

std::string info(const Q2Server& s, int slot, const char* key) {
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == key) return v;
  return {};
}

}  // namespace

bool fight_teammates(const Q2Server& s, int a, int b) {
  if (a < 0 || b < 0 || a >= sh::kMaxClients || b >= sh::kMaxClients) return false;
  if (s.cvars.find("coop")->on()) return true;
  if (!deathmatch(s) || !(dmflag(s, 64) || dmflag(s, 128))) return false;
  const bool by_model = dmflag(s, 128);
  const std::string ta = team_part(info(s, a, "skin"), by_model);
  return !ta.empty() && ta == team_part(info(s, b, "skin"), by_model);
}

float fight_fov(const Q2Server& s, int slot) {
  if (deathmatch(s) && dmflag(s, 32768)) return 90;
  const std::string text = info(s, slot, "fov");
  if (text.empty()) return 90;
  const float fov = std::strtof(text.c_str(), nullptr);
  return fov < 1 ? 1 : fov > 160 ? 160 : fov;
}

void fight_bury(Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  if (p.alive || p.gibbed || p.died_at <= 0) return;
  Q2Body& b = s.bodies[s.next_body];
  s.next_body = (s.next_body + 1) % kQ2Bodies;
  b.live = true;
  b.origin = unpack(p.move.origin);
  b.yaw = p.view_angles.y;
  b.health = p.health;
  b.frame = p.pose.frame;
}

std::vector<std::string> fight_scores(const Q2Server& s) {
  std::vector<int> order;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && !s.clients[i].spectator) order.push_back(i);
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return s.players[a].frags > s.players[b].frags; });
  std::vector<std::string> lines;
  for (const int i : order) {
    std::string n = std::to_string(static_cast<int>(s.players[i].frags));
    while (n.size() < 4) n.insert(n.begin(), ' ');
    lines.push_back(n + "  " + name_of(s, i));
  }
  return lines;
}

void fight_print(Q2Server& s, int slot, const std::string& text, int level) {
  if (slot < 0)
    print_all(s, text, level);
  else
    print_to(s, slot, text);
}

void fight_monster_bullet(Q2Server& s, int from, sh::Vec3f start, sh::Vec3f end, int damage,
                          int kick, int cause) {
  const Line l = shoot_line(s, start, end, 0, from);
  sh::Vec3f dir = end - start;
  sh::normalize(dir);
  if (l.player >= 0) {
    hurt(s, l.player, -1, dir, damage, kick, false, cause);
    effect(s, "te_blood", l.end, l.normal);
  } else if (l.fraction < 1 && !l.sky) {
    if (l.mover >= 0 && monster_index(s, l.mover) >= 0)
      monster_damage(s, l.mover, -2 - from, damage, dir, false, &l.end);
    else
      map_damage(s, l.mover, -1, damage);
    effect(s, "te_gunshot", l.end, l.normal);
  }
}

void fight_monster_launch(Q2Server& s, int from, SqShot kind, sh::Vec3f start, sh::Vec3f dir,
                          int damage, float speed, int cause) {
  Q2Missile* m = new_missile(s);
  if (m == nullptr) return;
  *m = Q2Missile{};
  m->live = true;
  m->kind = kind;
  m->owner = -1;
  m->from = from;
  m->weapon = cause;
  m->origin = start;
  m->damage = damage;
  m->velocity = dir * speed;
  m->dies_at = s.level_time + (kind == SqShot::grenade ? 2.5 : 8000.0 / double(speed));
  if (kind == SqShot::grenade) {  // a radius of its damage and 40
    m->velocity.z += 200;
    m->radius_damage = damage;
    m->radius = float(damage + 40);
  } else if (kind == SqShot::rocket) {  // a splash of its damage within its damage and 20
    m->radius_damage = damage;
    m->radius = float(damage + 20);
  }
}

void fight_monster_bfg(Q2Server& s, int from, sh::Vec3f start, sh::Vec3f dir, float speed,
                       int burst, float reach, int cause) {
  fight_monster_launch(s, from, SqShot::bfg, start, dir, 200, speed, cause);
  for (Q2Missile& m : s.missiles) {  // the one just launched: the player's ball's blast
    if (!m.live || m.from != from || m.kind != SqShot::bfg || m.burst != 0) continue;
    m.radius_damage = 200;
    m.radius = 100;
    m.burst = burst;
    m.reach = reach;
  }
}

void fight_monster_rail(Q2Server& s, int from, sh::Vec3f start, sh::Vec3f end, int damage, int kick,
                        int cause) {
  sh::Vec3f dir = end - start;
  sh::normalize(dir);
  const sh::Vec3f far = start + dir * kRange;
  uint32_t skip = 0;
  for (int n = 0; n <= sh::kMaxClients; n++) {
    const Line l = shoot_line(s, start, far, skip, from);
    if (l.player < 0) {
      if (l.mover >= 0 && monster_index(s, l.mover) >= 0)
        monster_damage(s, l.mover, -2 - from, damage, dir, false, &l.end);
      else
        map_damage(s, l.mover, -1, damage);
      line_effect(s, "te_railtrail", start, l.end, map_row(from));
      return;
    }
    hurt(s, l.player, -1, dir, damage, kick, false, cause);
    skip |= 1u << l.player;
  }
}

void fight_kick(Q2Server& s, int slot, sh::Vec3f push) {
  Q2Player& p = s.players[slot];
  pack(p.move.velocity, unpack(p.move.velocity) + push);
}

void fight_effect(Q2Server& s, const char* name, sh::Vec3f at, sh::Vec3f way) {
  effect(s, name, at, way);
}

void fight_monster_line(Q2Server& s, const char* name, int from, sh::Vec3f a, sh::Vec3f b,
                        int flash) {
  line_effect(s, name, a, b, map_row(from));
  s.out.events.back().values[3] = static_cast<float>(flash);
}

LaserEnd fight_laser(Q2Server& s, sh::Vec3f start, sh::Vec3f end, int damage, int attacker,
                     int skip_entity) {
  sh::Vec3f dir = end - start;
  sh::normalize(dir);
  uint32_t skip = 0;
  for (int n = 0; n < 64; n++) {  // start walks on past each body burnt
    const Line l = shoot_line(s, start, end, skip, skip_entity);
    const bool monster =
        l.player < 0 && l.fraction < 1 && l.mover >= 0 && monster_index(s, l.mover) >= 0;
    if (l.player >= 0 || monster) {  // burnt, and the line goes on through
      // The hornet is proof against lasers: the beam passes it unharmed.
      if (monster && !monster_laser_proof(s, l.mover))
        monster_damage(s, l.mover, attacker, damage, dir, false, &l.end);
      if (l.player >= 0) hurt(s, l.player, attacker, dir, damage, 1, true, kSqByLaser);
      skip |= l.player >= 0 ? 1u << l.player : 0u;
      skip_entity = monster ? l.mover : skip_entity;
      start = l.end;
      continue;
    }
    if (l.fraction < 1 && !l.sky) map_damage(s, l.mover, attacker, damage);
    return {l.end, l.fraction < 1 ? l.normal : sh::Vec3f{}};
  }
  return {start, {}};
}

void fight_gib(Q2Server& s, int ent, sh::Vec3f at, const SqGibThrow& t) { gib(s, ent, at, t); }

// A player's noise the monsters hear (his shot, its impact), outside deathmatch.
void monster_noise(Q2Server& s, int slot) {
  monster_noise(s, slot, unpack(s.players[slot].move.origin), false);
}

void monster_noise(Q2Server& s, int slot, sh::Vec3f at, bool impact) {
  if (slot < 0 || slot >= sh::kMaxClients || deathmatch(s) || s.players[slot].notarget) return;
  Q2Player& p = s.players[slot];
  (impact ? p.impact_frame : p.noise_frame) = s.frame_number;
  (impact ? p.impact_at : p.noise_at) = at;
}

}  // namespace shooters_edge::server
