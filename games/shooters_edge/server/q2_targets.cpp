// Quake II's other single-player entities (q2_targets.hpp states the rules; the numbers are
// the clean-room note's, porting/shooters-edge/q2-campaign.md).
//
// ge:summary Quake II's other entities at work: locks, counts, lasers, shakes, blasts, words
// ge:keywords q2 trigger-key laser-beam earthquake-shake explosive-brush barrel light-ramp
// ge:skill shooter
// ge:test tests/shooters_edge_q2_targets_test.cpp
#include "shooters_edge/server/q2_targets.hpp"
#include "shooters_edge/server/q2_wire.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gibs.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr float kStep = 0.1f;
constexpr uint32_t kEveryone = 0xffffffffu;
constexpr int32_t kSolid = sh::kSqSolid;

enum Kind : uint8_t {
  kNone,
  kKey,
  kCounter,
  kMonsterJump,
  kGravity,
  kKillbox,
  kSplash,
  kBlaster,
  kShake,
  kTempEntity,
  kExplosive,
  kBarrel,
  kLaser,
  kLightRamp,
  kConveyor,
  kDeadSoldier,
  kFigure,
  kString,
  kCharacter,
  kObject,
  kSpawner,
  kBomb,
  kBreach,
  kTurretBase,
  kShip,  // a fly-by ship: the map runs it as a train; only its row is drawn here
  kClock,
  kGibPart,
};

Kind kind_of(std::string_view c) {
  constexpr std::pair<std::string_view, Kind> kKinds[] = {
      {"trigger_key", kKey},
      {"trigger_counter", kCounter},
      {"trigger_monsterjump", kMonsterJump},
      {"trigger_gravity", kGravity},
      {"func_killbox", kKillbox},
      {"target_splash", kSplash},
      {"target_blaster", kBlaster},
      {"target_earthquake", kShake},
      {"target_temp_entity", kTempEntity},
      {"func_explosive", kExplosive},
      {"misc_explobox", kBarrel},
      {"target_laser", kLaser},
      {"target_lightramp", kLightRamp},
      {"func_conveyor", kConveyor},
      {"misc_deadsoldier", kDeadSoldier},
      {"misc_eastertank", kFigure},
      {"misc_easterchick", kFigure},
      {"misc_easterchick2", kFigure},
      {"misc_bigviper", kFigure},
      {"misc_blackhole", kFigure},
      {"misc_banner", kFigure},
      {"misc_satellite_dish", kFigure},
      {"light_mine1", kFigure},
      {"light_mine2", kFigure},
      {"viewthing", kFigure},
      {"misc_gib_arm", kGibPart},
      {"misc_gib_leg", kGibPart},
      {"misc_gib_head", kGibPart},
      {"target_string", kString},
      {"target_character", kCharacter},
      {"func_object", kObject},
      {"target_spawner", kSpawner},
      {"misc_viper_bomb", kBomb},
      {"turret_breach", kBreach},
      {"turret_base", kTurretBase},
      {"misc_viper", kShip},
      {"misc_strogg_ship", kShip},
      {"func_clock", kClock},
  };
  for (const auto& [name, k] : kKinds)
    if (c == name) return k;
  return kNone;
}
Kind kind_of(const Q2Entity& e) { return kind_of(e.classname); }

// The still figures: model, box, and the frames they loop at 10 a second.
struct Figure {
  std::string_view classname;
  const char* model;
  sh::Vec3f mins, maxs;
  int first, last;
  bool solid;
};
constexpr Figure kFigures[] = {
    {"misc_eastertank",
     "models/monsters/tank/tris.md2",
     {-32, -32, -16},
     {32, 32, 32},
     254,
     292,
     true},
    {"misc_easterchick",
     "models/monsters/bitch/tris.md2",
     {-32, -32, 0},
     {32, 32, 32},
     208,
     246,
     true},
    {"misc_easterchick2",
     "models/monsters/bitch/tris.md2",
     {-32, -32, 0},
     {32, 32, 32},
     248,
     286,
     true},
    {"misc_bigviper",
     "models/ships/bigviper/tris.md2",
     {-176, -120, -24},
     {176, 120, 72},
     0,
     0,
     true},
    {"misc_blackhole", "models/objects/black/tris.md2", {-64, -64, 0}, {64, 64, 8}, 0, 18, false},
    // The banner waves from a frame of its own; the dish turns once on each use (0 to 38). The
    // lamps and the banner have no size of their own.
    {"misc_banner", "models/objects/banner/tris.md2", {}, {}, 0, 15, false},
    {"misc_satellite_dish",
     "models/objects/satellite/tris.md2",
     {-64, -64, 0},
     {64, 64, 128},
     0,
     0,
     true},
    {"light_mine1", "models/objects/minelite/light1/tris.md2", {}, {}, 0, 0, true},
    {"light_mine2", "models/objects/minelite/light2/tris.md2", {}, {}, 0, 0, true},
    // A developer's model on show: the banner's first 7 frames, from half a second in.
    {"viewthing", "models/objects/banner/tris.md2", {-16, -16, -24}, {16, 16, 32}, 0, 6, true},
};
constexpr int kDishLast = 38;
const Figure* figure_of(std::string_view c) {
  for (const Figure& f : kFigures)
    if (f.classname == c) return &f;
  return nullptr;
}

const char* kBarrelModel = "models/objects/barrels/tris.md2";
const char* kBodyModel = "models/deadbods/dude/tris.md2";
const char* kBombModel = "models/objects/bomb/tris.md2";
constexpr int kSpares = 8;  // the monsters a spawner can make (made waiting as the map loads)
const char* kShakeSound =
    "world/qua"
    "ke.wav";  // the rumble (the pack's own file name)

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const Q2Server& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }
float crandom(Q2Server& s) { return float(2 * (ge::core::next(s.rng) - 0.5)); }

std::string key(const Q2Server& s, const Q2Entity& e, const char* name) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get(name);
}
float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
  const std::string v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound_at(Q2Server& s, int e, sh::Vec3f at, const char* sample, bool everywhere = false) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(e);
  ev.level = 2;
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = everywhere ? 0.0f : 1.0f;
  ev.text = sample;
  event(s, std::move(ev));
}

void centre_to(Q2Server& s, int slot, const std::string& text) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.text = text;
  event(s, std::move(ev));
}

bool alive(const Q2Server& s, int slot) {
  return slot >= 0 && slot < sh::kMaxClients && s.clients[slot].active && s.players[slot].alive;
}

void add_model(Q2Server& s, Q2Entity& e, const char* model) {
  const auto found = std::find(s.models.begin(), s.models.end(), model);
  e.modelindex = static_cast<int>(found - s.models.begin());
  if (found == s.models.end()) s.models.emplace_back(model);
}
void add_sound(Q2Server& s, const char* sample) {
  if (std::find(s.sounds.begin(), s.sounds.end(), sample) == s.sounds.end())
    s.sounds.emplace_back(sample);
}

sh::Vec3f way(float yaw) { return sh::angle_vectors({0, yaw, 0}).forward; }
sh::Vec3f way_of(const Q2Entity& e) {  // its angle: -1 up, -2 down
  if (e.angles.y == -1) return {0, 0, 1};
  if (e.angles.y == -2) return {0, 0, -1};
  return sh::angle_vectors(e.angles).forward;
}
sh::Vec3f centre(const Q2Entity& e) { return e.origin + (e.mins + e.maxs) * 0.5f; }

bool overlap(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y && alo.z < bhi.z &&
         ahi.z > blo.z;
}

}  // namespace

void targets_kill_inside(Q2Server& s, const Q2Entity& e) {
  const sh::Vec3f lo = e.origin + e.mins, hi = e.origin + e.maxs;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!alive(s, i)) continue;
    const sh::Vec3f at = unpack(s.players[i].move.origin);
    if (overlap(at + s.players[i].mins, at + s.players[i].maxs, lo, hi))
      fight_hurt(s, i, -1, {}, 100000, 0, kSqByTelefrag, 3);
  }
  for (const Q2Monster& m : s.monsters) {
    const Q2Entity& b = s.entities[size_t(m.entity)];
    if (m.dead || b.gone || !b.box) continue;
    if (overlap(b.origin + b.mins, b.origin + b.maxs, lo, hi))
      monster_damage(s, m.entity, -1, 100000, {});
  }
}

void targets_telefrag(Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == slot || !alive(s, i)) continue;
    const sh::Vec3f o = unpack(s.players[i].move.origin);
    if (overlap(o + s.players[i].mins, o + s.players[i].maxs, lo, hi))
      fight_hurt(s, i, slot, {}, 100000, 0, kSqByTelefrag, 3);
  }
  for (const Q2Monster& m : s.monsters) {
    const Q2Entity& b = s.entities[size_t(m.entity)];
    if (m.dead || b.gone || !b.box) continue;
    if (overlap(b.origin + b.mins, b.origin + b.maxs, lo, hi))
      monster_damage(s, m.entity, slot, 100000, {});
  }
}

namespace {

// A thing that breaks: its blast, its targets, and it is gone.
// The body part a spawner can throw (misc_gib_arm, misc_gib_leg, misc_gib_head; q2_gibs.hpp's
// parts), or -1.
int gib_part(std::string_view classname) {
  if (classname == "misc_gib_arm") return 0;
  if (classname == "misc_gib_leg") return 1;
  if (classname == "misc_gib_head") return 2;
  return -1;
}

// A chunk model's place on the model list (added while the map spawns, before the player's,
// which stays last).
int chunk_model(Q2Server& s, int n) {
  const std::string name = "models/objects/debris" + std::to_string(n) + "/tris.md2";
  const auto found = std::find(s.models.begin(), s.models.end(), name);
  if (found == s.models.end()) {
    s.models.push_back(name);
    return static_cast<int>(s.models.size()) - 1;
  }
  return static_cast<int>(found - s.models.begin());
}

float rnd1(Q2Server& s) { return static_cast<float>(ge::core::range(s.rng, -1, 1)); }

// Barrel or explosive `i` goes off (`from`: where the blow came from, the chunks flying away from
// it; none when used). A barrel: its blast from its foot, 2 big chunks, its 4 corners and 8 small
// ones thrown by its `dmg`, the ground's explosion (in the air, the rocket's). An explosive: its
// blast from its middle, big chunks (mass / 100 from 100, up to 8) and small ones (mass / 25, up
// to 16) inside its middle half, its targets, the explosion when it does harm.
void blow(Q2Server& s, int i, int attacker, int cause, const sh::Vec3f* from = nullptr) {
  Q2Entity& e = s.entities[size_t(i)];
  const bool barrel = kind_of(e) == kBarrel;
  const sh::Vec3f mid = centre(e), size = e.maxs - e.mins;
  e.gone = true;
  e.solid = false;
  e.box = false;
  if (barrel) {
    fight_blast(s, e.origin, attacker, e.dmg, float(e.dmg) + 40, cause);
    const float d = float(e.dmg) / 200;
    for (int k = 0; k < 2; k++)
      gibs_debris(s, chunk_model(s, 1),
                  mid + sh::Vec3f{rnd1(s) * size.x, rnd1(s) * size.y, rnd1(s) * size.z}, {},
                  1.5f * d);
    const sh::Vec3f lo = e.origin + e.mins;
    const sh::Vec3f corners[4] = {lo, lo + sh::Vec3f{size.x, 0, 0}, lo + sh::Vec3f{0, size.y, 0},
                                  lo + sh::Vec3f{size.x, size.y, 0}};
    for (const sh::Vec3f& c : corners) gibs_debris(s, chunk_model(s, 3), c, {}, 1.75f * d);
    const int small = 2 * e.dmg / 200;  // whole: the small chunks fly by an integer's speed
    for (int k = 0; k < 8; k++)
      gibs_debris(s, chunk_model(s, 2),
                  mid + sh::Vec3f{rnd1(s) * size.x, rnd1(s) * size.y, rnd1(s) * size.z}, {},
                  static_cast<float>(small));
    const ge::classic::Q2Trace t =
        map_trace(s, e.origin, e.mins, e.maxs, e.origin - sh::Vec3f{0, 0, 1}, kSolid, i);
    fight_effect(s, t.fraction < 1 ? "te_explosion2" : "te_explosion1", e.origin);
    map_use_targets(s, i, attacker);
    return;
  }
  if (e.dmg > 0) fight_blast(s, mid, attacker, e.dmg, float(e.dmg) + 40, cause);
  sh::Vec3f away{};
  if (from != nullptr) {
    away = mid - *from;
    sh::normalize(away);
    away = away * 150.0f;
  }
  const int mass = e.count;
  const sh::Vec3f quarter = size * 0.25f;
  const int big = mass >= 100 ? std::min(mass / 100, 8) : 0;
  for (int k = 0; k < big; k++)
    gibs_debris(s, chunk_model(s, 1),
                mid + sh::Vec3f{rnd1(s) * quarter.x, rnd1(s) * quarter.y, rnd1(s) * quarter.z},
                away, 1);
  for (int k = 0; k < std::min(mass / 25, 16); k++)
    gibs_debris(s, chunk_model(s, 2),
                mid + sh::Vec3f{rnd1(s) * quarter.x, rnd1(s) * quarter.y, rnd1(s) * quarter.z},
                away, 2);
  map_use_targets(s, i, attacker);
  if (e.dmg > 0) fight_effect(s, "te_explosion1", mid);
}

// The first living player whose box `lo`..`hi` overlaps, or -1.
int player_in(const Q2Server& s, sh::Vec3f lo, sh::Vec3f hi) {
  for (int p = 0; p < sh::kMaxClients; p++) {
    if (!alive(s, p)) continue;
    const sh::Vec3f at = unpack(s.players[p].move.origin);
    if (overlap(at + s.players[p].mins, at + s.players[p].maxs, lo, hi)) return p;
  }
  return -1;
}

float gravity(const Q2Server& s) { return static_cast<float>(s.cvars.find("sv_gravity")->number); }

// A falling object's frame: straight down under gravity until something holds it up; landing on
// a player or a monster from above deals its `dmg` (a crush).
void fall_object(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  e.now_speed += gravity(s) * kStep;  // its fall speed (units/s, down)
  const sh::Vec3f to = e.origin - sh::Vec3f{0, 0, e.now_speed * kStep};
  const ge::classic::Q2Trace t =
      map_trace(s, e.origin, e.mins, e.maxs, to, kSolid | sh::kSqMonster, i);
  const int p = player_in(s, t.end + e.mins, t.end + e.maxs);
  // A landing after a fall of more than a frame; a settle of a few units is no blow.
  const bool falling = e.count > 1;
  if (p < 0) {
    e.origin = t.end;
  } else {  // down onto the player's head
    const float head = float(s.players[p].move.origin[2]) * 0.125f + s.players[p].maxs.z;
    e.origin.z = std::clamp(head - e.mins.z, t.end.z, e.origin.z);
  }
  if (t.fraction >= 1 && p < 0) {
    e.count++;
    return;
  }
  if (falling && p >= 0) {
    fight_hurt(s, p, -1, {0, 0, -1}, e.dmg, 1, kSqByCrush);
  } else if (falling && t.normal.z > 0.7f && t.entity > 0) {
    const int hit = map_entity_of_row(s, t.entity);
    if (hit >= 0 && monster_index(s, hit) >= 0) monster_damage(s, hit, -1, e.dmg, {0, 0, -1});
  }
  e.count = 0;  // at rest on what holds it
  e.now_speed = 0;
  e.until = s.level_time + kStep;  // rests, and falls again if what held it goes
}

// A bomb's frame: thrown on, falling, its nose down over its first second and rolling; anything
// it touches sets it off.
void fly_bomb(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  q2_hold_speed(s, e.velocity);
  e.velocity.z -= gravity(s) * kStep;
  const float since = static_cast<float>(s.level_time - e.tick_at);
  e.angles.x = std::min(since, 1.0f) * 90;
  e.angles.z += 10;
  const sh::Vec3f to = e.origin + e.velocity * kStep;
  const ge::classic::Q2Trace t =
      map_trace(s, e.origin, e.mins, e.maxs, to, kSolid | sh::kSqMonster, i);
  e.origin = t.end;
  if (t.fraction >= 1 && player_in(s, e.origin + e.mins, e.origin + e.maxs) < 0) return;
  e.on = false;
  e.gone = true;
  map_use_targets(s, i, e.activator);
  fight_blast(s, e.origin, e.activator, e.dmg, float(e.dmg) + 40, kSqByExplosion);
  fight_effect(s, "te_explosion2", e.origin);  // the grenade-look blast
}

constexpr double kDegrees = 180.0 / 3.14159265358979323846;

// An angle brought into -180..180.
float wrap180(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// A breach's frame: once set up (its muzzle's place taken 0.1 s in), it turns toward the angles
// its driver asks for (`open`), at most `speed` degrees a second on each axis, its base with it;
// with its driver dead it levels.
// A clock's count set back: up from 0 to `count`, or down from `count` to 0 (its value in
// `state`); no one has started it.
void clock_reset(Q2Entity& e) {
  e.state = (e.spawnflags & 2) != 0 ? e.count : 0;
  e.activator = -1;
}

// A number right-aligned in two places with spaces ("5" " 5", "42", "3600").
std::string two(int v) {
  std::string t = std::to_string(v);
  if (t.size() < 2) t.insert(0, 2 - t.size(), ' ');
  return t;
}

// The text a clock shows for `seconds` in `style` (0 the seconds, 1 minutes:seconds, 2
// hours:minutes:seconds): each field two places with spaces, the tens of the later fields filled
// with a '0' at their fixed places (3, and 6), which a field grown wider pushes aside.
std::string clock_text(int seconds, int style) {
  std::string t;
  if (style == 0) return two(seconds);
  if (style == 1) {
    t = two(seconds / 60) + ":" + two(seconds % 60);
  } else if (style == 2) {
    t = two(seconds / 3600) + ":" + two(seconds / 60 % 60) + ":" + two(seconds % 60);
    if (t.size() > 6 && t[6] == ' ') t[6] = '0';
  } else {
    return {};
  }
  if (t.size() > 3 && t[3] == ' ') t[3] = '0';
  return t;
}

// Clock `i`'s second: its text onto its target_string (found the first time; none, and it is
// dead), its count stepped; past the end its pathtarget fires (after its `delay`), then it stops,
// or with MULTI_USE (8) starts over (waiting for a use again when it starts off, 4). A clock that
// neither counts up (1) nor down (2) shows the time of day, the level's own clock from midnight
// (the same in every replay of the level).
void clock_think(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  e.tick_at = 0;
  if (e.aim < 0) {
    for (size_t t = 0; t < s.entities.size() && e.aim < 0; t++)
      if (s.entities[t].targetname == e.target) e.aim = static_cast<int>(t);
    if (e.aim < 0) {  // silently stopped
      e.on = false;
      return;
    }
  }
  const bool up = (e.spawnflags & 1) != 0, down = (e.spawnflags & 2) != 0;
  std::string text;
  if (up || down) {
    text = clock_text(e.state, e.frame);
    e.state += up ? 1 : -1;
  } else {
    text = clock_text(static_cast<int>(s.level_time) % 86400, 2);
  }
  if (!text.empty()) e.message = text;  // its own text (another style keeps the last)
  s.entities[size_t(e.aim)].message = e.message;
  targets_use(s, e.aim, e.activator);
  if ((up && e.state > e.count) || (down && e.state < 0)) {
    if (!e.pathtarget.empty()) {  // its path target's, nothing printed
      const std::string target = e.target, text_was = e.message;
      e.target = e.pathtarget;
      e.message.clear();
      // A clock that started itself has no user at all to hand on.
      map_use_targets(s, i, (e.spawnflags & 4) != 0 ? e.activator : kNoActivator);
      s.entities[size_t(i)].target = target;
      s.entities[size_t(i)].message = text_was;
    }
    Q2Entity& c = s.entities[size_t(i)];
    if ((c.spawnflags & 8) == 0) {
      c.on = false;
      return;
    }
    clock_reset(c);
    if ((c.spawnflags & 4) != 0) {
      c.on = false;
      return;
    }
  }
  s.entities[size_t(i)].tick_at = s.level_time + 1;
}

// A laser's colour by its spawn flags (the first of red, green, blue, yellow, orange): four
// palette indices, low byte first, one of which a viewer draws each frame; 0 with none.
uint32_t laser_colour(int spawnflags) {
  if ((spawnflags & 2) != 0) return 0xF2F2F0F0u;
  if ((spawnflags & 4) != 0) return 0xD0D1D2D3u;
  if ((spawnflags & 8) != 0) return 0xF3F3F1F1u;
  if ((spawnflags & 16) != 0) return 0xDCDDDEDFu;
  if ((spawnflags & 32) != 0) return 0xE0E1E2E3u;
  return 0;
}

// Laser `i`'s tenth while on: its line from its origin 2048 units toward its aim's centre (or
// along its angle), burning what is on it; sparks where it lands when it was just turned on (8)
// or its aim moved (4), in the low byte's colour along the face's normal.
void laser_think(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  const int count = e.state != 0 ? 8 : 4;
  if (e.aim >= 0) {
    sh::Vec3f d = centre(s.entities[size_t(e.aim)]) - e.origin;
    sh::normalize(d);
    if (d.x != e.dir.x || d.y != e.dir.y || d.z != e.dir.z) e.state = 1;
    e.dir = d;
  }
  const LaserEnd end = fight_laser(s, e.origin, e.origin + e.dir * 2048.0f, e.dmg, e.activator, i);
  e.dest = end.at;
  if (e.state == 0) return;
  e.state = 0;
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = end.at;
  ev.values[0] = end.normal.x;
  ev.values[1] = end.normal.y;
  ev.values[2] = end.normal.z;
  ev.values[3] = static_cast<float>(count);
  ev.level = e.count & 0xff;
  ev.text = "te_laser_sparks";
  event(s, std::move(ev));
}

void turn_breach(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  if (e.until > 0 && s.level_time + 0.001 >= e.until) {  // the muzzle, from its marker
    e.until = 0;
    e.aim = -1;
    for (size_t t = 0; t < s.entities.size(); t++)
      if (!e.target.empty() && s.entities[t].targetname == e.target) e.aim = static_cast<int>(t);
    if (e.aim < 0) {
      if (e.target.empty())
        s.console.push_back(e.classname + " at " + q2_pos(e.origin) + " needs a target");
      else
        q2_pick_missing(s, e.target);
    } else {
      Q2Entity& mark = s.entities[size_t(e.aim)];
      const sh::Vec3f off = mark.origin - e.origin;
      const auto ax = sh::angle_vectors(e.angles);
      e.closed = {sh::dot(off, ax.forward), sh::dot(off, ax.right), sh::dot(off, ax.up)};
      mark.gone = true;
    }
  }
  if (e.count >= 0) {  // its driver's death levels the gun
    const int m = monster_index(s, e.count);
    if (m < 0 || s.monsters[size_t(m)].dead) e.open.x = 0;
  }
  const float step = e.speed * kStep;
  sh::Vec3f turn{std::clamp(wrap180(e.open.x - e.angles.x), -step, step),
                 std::clamp(wrap180(e.open.y - e.angles.y), -step, step), 0};
  e.avelocity = turn * (1 / kStep);
  for (Q2Entity& b : s.entities)
    if (kind_of(b) == kTurretBase && !e.team.empty() && b.team == e.team)
      b.avelocity = {0, e.avelocity.y, 0};
}

}  // namespace

bool targets_spawn(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  const Kind k = kind_of(e);
  if (k == kNone) return false;
  if (e.model.size() > 1 && e.model[0] == '*' && e.modelindex > 0 &&
      size_t(e.modelindex) < s.map->models.size()) {
    const ge::brush::Model& m = s.map->models[size_t(e.modelindex)];
    e.mins = sh::Vec3f{m.mins[0], m.mins[1], m.mins[2]};
    e.maxs = sh::Vec3f{m.maxs[0], m.maxs[1], m.maxs[2]};
  }
  e.message = map_text(key(s, e, "message"));
  e.dmg = static_cast<int>(number(s, e, "dmg", 0));
  e.speed = number(s, e, "speed", 0);
  e.delay = number(s, e, "delay", 0);
  e.health = static_cast<int>(number(s, e, "health", 0));
  e.count = static_cast<int>(number(s, e, "count", 0));
  e.sounds = static_cast<int>(number(s, e, "sounds", 0));  // a splash's colour
  e.team = key(s, e, "team");
  switch (k) {
    case kClock: {
      const int flags = e.spawnflags;
      if (e.target.empty() || ((flags & 2) != 0 && e.count == 0)) {
        s.console.push_back(e.classname +
                            (e.target.empty() ? " with no target at " : " with no count at ") +
                            q2_pos(e.origin));
        e.gone = true;
        return true;
      }
      if ((flags & 1) != 0 && e.count == 0) e.count = 3600;
      e.frame = static_cast<int>(number(s, e, "style", 0));  // the text's style
      e.pathtarget = key(s, e, "pathtarget");
      e.message.clear();  // its own text is shown, never the map's
      clock_reset(e);
      e.aim = -1;
      if ((flags & 4) == 0) {  // it starts itself a second in
        e.on = true;
        e.tick_at = s.level_time + 1;
      }
      return true;
    }
    case kKey:
      e.needs = sq_item(key(s, e, "item"), s.opt.pack);
      if (e.needs < 0 || e.target.empty()) e.gone = true;  // said at load (q2_lines.cpp)
      add_sound(s, "misc/keytry.wav");
      add_sound(s, "misc/keyuse.wav");
      e.touched_at = -100;
      return true;
    case kCounter:
      if (e.count == 0) e.count = 2;
      return true;
    case kMonsterJump:
      if (e.speed == 0) e.speed = 200;
      e.random = number(s, e, "height", 200);
      if (e.angles.y == 0) e.angles.y = 360;
      return true;
    case kGravity:
      if (key(s, e, "gravity").empty()) e.gone = true;  // said at load (q2_lines.cpp)
      e.random = static_cast<float>(std::atoi(key(s, e, "gravity").c_str()));  // whole numbers
      return true;
    case kTempEntity:
      e.sounds = static_cast<int>(number(s, e, "style", 0));  // which effect
      return true;
    case kKillbox:
    case kString:
      return true;
    case kSplash:
      if (e.count == 0) e.count = 32;
      return true;
    case kBlaster:
      if (e.dmg == 0) e.dmg = 15;
      if (e.speed == 0) e.speed = 1000;
      add_sound(s, "weapons/laser2.wav");
      return true;
    case kShake:
      if (e.speed == 0) e.speed = 200;
      if (e.count == 0) e.count = 5;
      add_sound(s, kShakeSound);
      return true;
    case kExplosive:
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      if (e.health == 0) e.health = 100;
      e.count = static_cast<int>(number(s, e, "mass", 75));
      chunk_model(s, 1);
      chunk_model(s, 2);
      e.solid = (e.spawnflags & 1) == 0;  // flag 1: hidden until a use
      e.hidden = !e.solid;
      return true;
    case kBarrel: {
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      add_model(s, e, kBarrelModel);
      for (int n = 1; n <= 3; n++) chunk_model(s, n);
      e.mins = {-16, -16, 0};
      e.maxs = {16, 16, 40};
      e.box = true;
      if (e.health == 0) e.health = 10;
      if (e.dmg == 0) e.dmg = 150;
      e.count = static_cast<int>(number(s, e, "mass", 400));
      const ge::classic::Q2Trace t = map_trace(s, e.origin + sh::Vec3f{0, 0, 1}, e.mins, e.maxs,
                                               e.origin - sh::Vec3f{0, 0, 128}, kSolid, i);
      if (!t.start_solid) e.origin = t.end;
      return true;
    }
    case kLaser:
      e.until = s.level_time + 1;  // it sets itself up a second in
      e.on = false;
      if (e.dmg == 0) e.dmg = 1;
      e.mins = {-8, -8, -8};
      e.maxs = {8, 8, 8};
      e.frame = (e.spawnflags & 64) != 0 ? 16 : 4;  // the beam's width
      e.count = static_cast<int>(laser_colour(e.spawnflags));
      return true;
    case kLightRamp:
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      if (e.message.size() != 2 || e.message[0] < 'a' || e.message[0] > 'z' || e.message[1] < 'a' ||
          e.message[1] > 'z' || e.message[0] == e.message[1] || e.speed <= 0)
        e.gone = true;  // said at load (q2_lines.cpp)
      return true;
    case kConveyor:
      e.solid = true;
      if (e.speed == 0) e.speed = 100;
      e.on = (e.spawnflags & 1) != 0;
      return true;
    case kDeadSoldier: {
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      add_model(s, e, kBodyModel);
      e.mins = {-16, -16, 0};
      e.maxs = {16, 16, 16};
      e.box = true;
      const int f = e.spawnflags;
      e.frame = (f & 2) != 0    ? 1
                : (f & 4) != 0  ? 2
                : (f & 8) != 0  ? 3
                : (f & 16) != 0 ? 4
                : (f & 32) != 0 ? 5
                                : 0;
      return true;
    }
    case kFigure: {
      const Figure* fig = figure_of(e.classname);
      add_model(s, e, fig->model);
      e.mins = fig->mins;
      e.maxs = fig->maxs;
      e.box = fig->solid;
      e.frame = fig->first;
      if (e.classname == "misc_banner") e.state = static_cast<int>(ge::core::next(s.rng) * 16) % 16;
      if (e.classname == "misc_satellite_dish") e.state = -1;  // still until used
      if (e.classname == "viewthing") e.state = -4 - static_cast<int>(s.frame_number);
      return true;
    }
    case kGibPart:  // a body part placed on the map: it falls when the level starts
      gibs_part_model(s, gib_part(e.classname));
      return true;
    case kCharacter:
      e.solid = true;
      e.frame = 12;  // blank
      return true;
    case kObject:  // a box one unit inside the brush; with any flag, hidden until a use
      e.mins = e.mins + sh::Vec3f{1, 1, 1};
      e.maxs = e.maxs - sh::Vec3f{1, 1, 1};
      if (key(s, e, "dmg").empty()) e.dmg = 100;
      if (e.spawnflags == 0) {
        e.solid = true;
        e.until = s.level_time + 0.2;  // then it falls when nothing holds it
      } else {
        e.hidden = true;
      }
      return true;
    case kSpawner: {
      const std::string made = e.target;
      if (monster_class(made)) {  // its monsters wait unseen, each a map entity of its own
        if (deathmatch(s)) return true;
        Q2Entity spare;
        spare.classname = made;
        spare.origin = e.origin;
        spare.angles = e.angles;
        spare.block = -2 - i;
        for (int n = 0; n < kSpares; n++) s.entities.push_back(spare);  // `e` may move
        return true;
      }
      if (gib_part(made) >= 0) {
        gibs_part_model(s, gib_part(made));
        return true;
      }
      return true;  // a class it cannot make: it makes nothing
    }
    case kBomb:
      add_model(s, e, kBombModel);
      e.hidden = true;
      e.mins = {-8, -8, -8};
      e.maxs = {8, 8, 8};
      if (e.dmg == 0) e.dmg = 1000;
      return true;
    case kBreach: {  // a gun that turns in pitch and yaw; 0 in a limit means its default
      e.solid = true;
      if (e.speed == 0) e.speed = 50;
      if (e.dmg == 0) e.dmg = 10;
      const float minpitch = number(s, e, "minpitch", 0), maxpitch = number(s, e, "maxpitch", 0);
      const float maxyaw = number(s, e, "maxyaw", 0);
      e.lip = minpitch == 0 ? -30 : minpitch;
      e.wait = maxpitch == 0 ? 30 : maxpitch;
      e.decel = number(s, e, "minyaw", 0);
      e.random = maxyaw == 0 ? 360 : maxyaw;
      e.open = e.angles;  // where it aims
      e.until = s.level_time + kStep;
      e.count = -1;  // its driver
      return true;   // with no target, said a frame on and firing from its middle
    }
    case kTurretBase:
      e.solid = true;
      return true;
    case kShip:
    case kNone:
      return false;
  }
  return false;
}

bool targets_use(Q2Server& s, int i, int activator) {
  Q2Entity& e = s.entities[size_t(i)];
  const Kind k = kind_of(e);
  if (k == kNone) return false;
  if (e.gone && k != kExplosive) return true;
  switch (k) {
    case kKey: {
      if (!alive(s, activator)) return true;
      Q2Player& p = s.players[activator];
      if (p.body.counts[e.needs] <= 0) {
        if (s.level_time + 0.001 < e.touched_at + 5) return true;
        e.touched_at = s.level_time;
        centre_to(s, activator, std::string("You need the ") + kSqItems[e.needs].pickup);
        sound_at(s, i, unpack(p.move.origin), "misc/keytry.wav");
        return true;
      }
      sound_at(s, i, e.origin, "misc/keyuse.wav");
      if (coop(s) && std::string_view(kSqItems[e.needs].classname) == "key_power_cube") {
        int bit = 0;  // the lowest cube he holds: everyone holding that one gives it up
        while (bit < 8 && (p.body.cubes & (1 << bit)) == 0) bit++;
        for (Q2Player& o : s.players)
          if ((o.body.cubes & (1 << bit)) != 0) {
            o.body.counts[e.needs]--;
            o.body.cubes &= ~(1 << bit);
          }
      } else if (coop(s)) {
        for (Q2Player& o : s.players) o.body.counts[e.needs] = 0;
      } else {
        p.body.counts[e.needs]--;
      }
      map_use_targets(s, i, activator);
      e.gone = true;
      return true;
    }
    case kCounter:
      if (e.count <= 0) return true;
      e.count--;
      if (e.count > 0) {
        if ((e.spawnflags & 1) == 0) {
          centre_to(s, activator, std::to_string(e.count) + " more to go...");
          if (alive(s, activator))
            sound_at(s, i, unpack(s.players[activator].move.origin), "misc/talk1.wav");
        }
        return true;
      }
      if ((e.spawnflags & 1) == 0) {
        centre_to(s, activator, "Sequence completed!");
        if (alive(s, activator))
          sound_at(s, i, unpack(s.players[activator].move.origin), "misc/talk1.wav");
      }
      map_use_targets(s, i, activator);
      e.gone = true;
      return true;
    case kKillbox:
      targets_kill_inside(s, e);
      return true;
    case kSplash: {
      const sh::Vec3f at = e.origin;
      static const char* const kSplashes[] = {
          "te_splash",       "te_splash_sparks", "te_splash_water", "te_splash_mud",
          "te_splash_slime", "te_splash_lava",   "te_splash_blood"};
      NetEvent ev;  // `count` specks along its angle
      ev.kind = EventKind::point_effect;
      ev.to = kEveryone;
      ev.origin = at;
      const sh::Vec3f along = way_of(e);
      ev.values[0] = along.x;
      ev.values[1] = along.y;
      ev.values[2] = along.z;
      ev.values[3] = static_cast<float>(std::clamp(e.count, 1, 255));
      ev.text = kSplashes[std::clamp(e.sounds, 0, 6)];
      event(s, std::move(ev));
      if (e.dmg > 0) fight_blast(s, at, activator, e.dmg, float(e.dmg) + 40, kSqByExplosion);
      return true;
    }
    case kBlaster:
      sound_at(s, i, e.origin, "weapons/laser2.wav");
      fight_monster_launch(s, i, SqShot::bolt, e.origin, way_of(e), e.dmg, e.speed,
                           int(SqWeapon::hyperblaster));
      return true;
    case kShake:
      e.until = s.level_time + double(e.count);
      e.tick_at = s.level_time + kStep;
      e.touched_at = -100;  // the rumble's last play
      return true;
    case kTempEntity:
      if (const std::string name = q2_temp_entity_name(e.sounds); !name.empty())
        fight_effect(s, name.c_str(), e.origin);
      return true;
    case kExplosive:
      if (e.hidden) {  // it appears, killing what stands in its place, and can now be shot
        e.hidden = false;
        e.gone = false;
        e.solid = true;
        targets_kill_inside(s, e);
        return true;
      }
      if (e.gone) return true;
      blow(s, i, activator, kSqByExplosion);
      return true;
    case kLaser:
      e.on = !e.on;
      e.activator = activator;
      if (e.on) {  // sparks where it first lands, and it burns at once
        e.state = 1;
        if (e.until <= 0) laser_think(s, i);
      }
      return true;
    case kLightRamp:
      if (e.aim < 0) e.aim = q2_ramp_light(s, i);  // its light, found at the first use
      if (e.aim < 0) return true;
      e.until = s.level_time;  // the ramp starts now
      e.tick_at = s.level_time;
      e.activator = activator;
      return true;
    case kConveyor:
      e.on = !e.on;
      if ((e.spawnflags & 2) == 0) e.speed = 0;  // without the toggle flag, forgotten after once
      return true;
    case kClock:
      // Started by itself, running, or used once already without MULTI_USE (8): no use.
      if ((e.spawnflags & 4) == 0 || e.on || (e.spawnflags & 0x10000) != 0) return true;
      e.on = true;
      e.activator = activator;
      if ((e.spawnflags & 8) == 0) e.spawnflags |= 0x10000;
      clock_think(s, i);  // its first text at once
      return true;
    case kString: {
      for (Q2Entity& c : s.entities) {
        if (kind_of(c) != kCharacter || c.team != e.team || c.count <= 0) continue;
        const size_t at = size_t(c.count - 1);
        const char ch = at < e.message.size() ? e.message[at] : ' ';
        c.frame = ch >= '0' && ch <= '9' ? ch - '0' : ch == '-' ? 10 : ch == ':' ? 11 : 12;
      }
      return true;
    }
    case kFigure:
      if (e.classname == "misc_blackhole") e.gone = true;
      if (e.classname == "misc_satellite_dish") e.state = static_cast<int>(s.frame_number);
      return true;
    case kObject:
      if (!e.hidden) return true;
      e.hidden = false;
      e.solid = true;
      targets_kill_inside(s, e);
      e.until = s.level_time;  // and it falls
      return true;
    case kSpawner: {
      // With a speed, what it makes is thrown along its angles (its pitch, -1 up, -2 down) and
      // faces yaw 0; without, it faces the spawner's way and keeps still.
      const sh::Vec3f velocity = e.speed != 0 ? way_of(e) * e.speed : sh::Vec3f{};
      const sh::Vec3f facing = e.speed != 0 ? sh::Vec3f{} : e.angles;
      if (const int part = gib_part(e.target); part >= 0) {
        gibs_part(s, part, e.origin, facing, velocity);
        return true;
      }
      if (monster_class(e.target)) {
        if (!monster_bring_spare(s, -2 - i, e.origin, facing, velocity, activator))
          s.console.push_back("A spawner of '" + e.target + "' has made all " +
                              std::to_string(kSpares) + " it can.");
        return true;
      }
      if (sq_item(e.target, s.opt.pack) >= 0) fight_drop_at(s, e.origin, e.target);
      return true;
    }
    case kBomb: {
      if (!e.hidden || e.on) return true;
      e.hidden = false;
      e.on = true;  // falling
      e.activator = activator;
      e.angles = {};
      for (const Q2Entity& v : s.entities)
        if (v.classname == "misc_viper") {
          e.velocity = v.velocity;
          break;
        }
      e.tick_at = s.level_time;  // its fall began
      return true;
    }
    default:
      return true;
  }
}

bool targets_damage(Q2Server& s, int i, int attacker, int damage) {
  Q2Entity& e = s.entities[size_t(i)];
  const Kind k = kind_of(e);
  if (e.gone || damage <= 0) return false;
  switch (k) {
    case kExplosive:  // shootable unless only named (then a use blows it)
      if (e.hidden || ((e.spawnflags & 1) == 0 && !e.targetname.empty())) return false;
      e.health -= damage;
      if (e.health <= 0) {  // its chunks fly away from who broke it
        const bool player = attacker >= 0 && attacker < sh::kMaxClients;
        const sh::Vec3f from = player ? unpack(s.players[attacker].move.origin) : sh::Vec3f{};
        blow(s, i, attacker, kSqByExplosion, player ? &from : nullptr);
      }
      return true;
    case kBarrel:
      if (e.health <= 0) return true;  // already lit
      e.health -= damage;
      if (e.health <= 0) {
        e.tick_at = s.level_time + 0.2;
        e.activator = attacker;
      }
      return true;
    case kDeadSoldier:
      e.health -= damage;
      if (e.health <= -80) {
        e.gone = true;
        e.box = false;
        fight_gib(s, map_row(i), e.origin,
                  {e.origin + sh::Vec3f{-16, -16, 0},
                   e.origin + sh::Vec3f{16, 16, 16},
                   {},
                   damage,
                   SqGibSet::corpse});
      }
      return true;
    default:
      return false;
  }
}

void targets_frame(Q2Server& s) {
  for (size_t n = 0; n < s.entities.size(); n++) {
    Q2Entity& e = s.entities[n];
    const int i = static_cast<int>(n);
    if (e.gone) continue;
    switch (kind_of(e)) {
      case kGibPart:  // thrown into the gibs' pool, where it falls and lies 30 s
        gibs_part(s, gib_part(e.classname), e.origin, e.angles, {});
        e.gone = true;
        break;
      case kClock:
        if (e.on && e.tick_at > 0 && s.level_time + 0.001 >= e.tick_at) clock_think(s, i);
        break;
      case kBarrel:
        if (e.tick_at > 0 && s.level_time + 0.001 >= e.tick_at) {
          e.tick_at = 0;
          blow(s, i, e.activator, kSqByBarrel);
        }
        break;
      case kShake: {
        if (e.tick_at <= 0 || s.level_time + 0.001 < e.tick_at) break;
        if (s.level_time > e.until + 0.001) {
          e.tick_at = 0;
          break;
        }
        if (s.level_time - e.touched_at > 0.5 + 0.001) {
          e.touched_at = s.level_time;
          sound_at(s, i, e.origin, kShakeSound, true);
        }
        for (int p = 0; p < sh::kMaxClients; p++) {
          if (!alive(s, p) || !s.players[p].on_ground) continue;
          const sh::Vec3f v = unpack(s.players[p].move.velocity);
          const sh::Vec3f push{crandom(s) * 150, crandom(s) * 150, e.speed * 100 / 200 - v.z};
          fight_kick(s, p, push);
        }
        e.tick_at = s.level_time + kStep;
        break;
      }
      case kLaser: {
        if (e.until > 0 && s.level_time + 0.001 >= e.until) {  // set up: its aim, maybe on
          e.until = 0;
          if (!e.target.empty()) {
            for (size_t t = 0; t < s.entities.size(); t++)
              if (s.entities[t].targetname == e.target) e.aim = static_cast<int>(t);
            if (e.aim < 0)
              s.console.push_back(e.classname + " at " + q2_pos(e.origin) + ": " + e.target +
                                  " is a bad target");
          }
          e.dir = way_of(e);
          if ((e.spawnflags & 1) != 0) {
            e.on = true;
            e.state = 1;
          }
        }
        if (e.on && e.until <= 0) laser_think(s, i);
        break;
      }
      case kObject:
        if (e.until > 0 && s.level_time + 0.001 >= e.until) fall_object(s, i);
        break;
      case kBreach:
        turn_breach(s, i);
        break;
      case kBomb:
        if (e.on) fly_bomb(s, i);
        break;
      case kLightRamp: {
        if (e.tick_at <= 0 || s.level_time + 0.001 < e.tick_at) break;
        const int light = e.aim;
        if (light < 0) {
          e.gone = true;
          break;
        }
        const int style = s.entities[size_t(light)].state;
        const int a = e.message[0] - 'a', b = e.message[1] - 'a';
        const double elapsed = s.level_time - e.until;
        const int frames = static_cast<int>(elapsed / kStep + 0.001);
        // The start plus the frames' steps, cut to a whole letter: a falling ramp is a letter on.
        const float step = float(b - a) / (e.speed / kStep);
        const int letter = static_cast<int>(float(a) + float(frames) * step);
        if (style >= 0 && style < kWorldLightStyles)
          s.world.lightstyles[style] = std::string(1, char('a' + std::clamp(letter, 0, 25)));
        if (elapsed + 0.001 >= double(e.speed)) {
          e.tick_at = 0;
          if ((e.spawnflags & 1) != 0) std::swap(e.message[0], e.message[1]);
        } else {
          e.tick_at = s.level_time + kStep;
        }
        break;
      }
      default:
        break;
    }
  }
}

void targets_monster_touch(Q2Server& s, Q2Monster& m) {
  const Q2Entity& b = s.entities[size_t(m.entity)];
  const sh::Vec3f lo = b.origin + b.mins, hi = b.origin + b.maxs;
  for (const Q2Entity& e : s.entities) {
    if (e.gone) continue;
    const Kind k = kind_of(e);
    if (k != kMonsterJump && k != kGravity) continue;
    if (!overlap(lo, hi, e.origin + e.mins, e.origin + e.maxs)) continue;
    if (k == kGravity) {
      m.gravity = e.random;
      continue;
    }
    const sh::Vec3f w = way(e.angles.y);
    m.velocity.x = w.x * e.speed;
    m.velocity.y = w.y * e.speed;
    if (m.on_ground) {
      m.on_ground = false;
      m.velocity.z = e.random;
    }
  }
}

void targets_turret(Q2Server& s, Q2Monster& m, int enemy) {
  Q2Entity& d = s.entities[size_t(m.entity)];
  int breach = -1;
  for (size_t t = 0; t < s.entities.size(); t++)
    if (!d.target.empty() && s.entities[t].targetname == d.target &&
        kind_of(s.entities[t]) == kBreach)
      breach = static_cast<int>(t);
  if (breach < 0) return;
  Q2Entity& b = s.entities[size_t(breach)];
  if (b.count < 0) {  // first: its seat, kept at its offset as the breach turns
    b.count = m.entity;
    d.closed = d.origin - b.origin;
    d.lip = b.angles.y;
  }
  // Ride: the seat turned with the breach's yaw, snapped to 1/8 unit.
  const float yaw = static_cast<float>((b.angles.y - d.lip) / kDegrees);
  const float c = static_cast<float>(ge::core::libm::cos(double(yaw)));
  const float sn = static_cast<float>(ge::core::libm::sin(double(yaw)));
  const sh::Vec3f seat{d.closed.x * c - d.closed.y * sn, d.closed.x * sn + d.closed.y * c,
                       d.closed.z};
  d.origin = b.origin + seat;
  for (float* v : {&d.origin.x, &d.origin.y, &d.origin.z})
    *v = static_cast<float>(static_cast<int>(*v * 8 + (*v < 0 ? -0.5f : 0.5f))) / 8;
  d.angles.y = b.angles.y;
  if (enemy < 0) {  // out of sight: the aim stops where it is
    m.fire_at = -1;
    b.open = b.angles;
    return;
  }
  const auto ax = sh::angle_vectors(b.angles);
  const sh::Vec3f muzzle =
      b.origin + ax.forward * b.closed.x + ax.right * b.closed.y + ax.up * b.closed.z;
  const Q2Player& p = s.players[enemy];
  const sh::Vec3f eye = unpack(p.move.origin) + sh::Vec3f{0, 0, p.view_height};
  const sh::Vec3f to = eye - muzzle;
  const double flat = double(sh::length(sh::Vec3f{to.x, to.y, 0}));
  float want_yaw = static_cast<float>(ge::core::libm::atan2(double(to.y), double(to.x)) * kDegrees);
  if (want_yaw < 0) want_yaw += 360;
  float want_pitch = static_cast<float>(-ge::core::libm::atan2(double(to.z), flat) * kDegrees);
  want_pitch = std::clamp(want_pitch, b.lip, b.wait);
  if (want_yaw < b.decel || want_yaw > b.random) {  // outside its arc: the nearer limit
    const float to_min = std::abs(wrap180(want_yaw - b.decel));
    const float to_max = std::abs(wrap180(want_yaw - b.random));
    want_yaw = to_min < to_max ? b.decel : b.random;
  }
  b.open = {want_pitch, want_yaw, 0};
  // Fire once it has seen him for (3 - skill) s, then every (3 - skill) + 1 s.
  const int skill = std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
  if (m.fire_at < 0) m.fire_at = s.level_time;
  if (s.level_time + 0.001 < m.fire_at + (3 - skill) || s.level_time + 0.001 < m.attack_finished)
    return;
  m.attack_finished = s.level_time + (3 - skill) + 1;
  const int damage = 100 + static_cast<int>(ge::core::next(s.rng) * 50);
  sound_at(s, m.entity, muzzle, "weapons/rocklf1a.wav");
  fight_monster_launch(s, m.entity, SqShot::rocket, muzzle, ax.forward, damage,
                       550.0f + 50.0f * float(skill), kSqByMonster + m.kind);
}

void targets_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  for (size_t n = 0; n < s.entities.size(); n++) {
    const Q2Entity& e = s.entities[n];
    const Kind k = kind_of(e);
    if (k != kBarrel && k != kDeadSoldier && k != kFigure && k != kCharacter && k != kExplosive &&
        k != kObject && k != kBomb && k != kShip && k != kLaser)
      continue;
    const size_t row = size_t(map_row(static_cast<int>(n)));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    if (k == kLaser) {  // a beam to where it landed, seen through, while on
      r.free = e.gone || !e.on || e.until > 0;
      r.origin = e.origin;
      r.modelindex = 0;
      r.render_fx = kSqBeamRender | kSqTranslucentRender;
      r.frame = e.frame;
      r.skin = e.count;
      r.beam_end = e.dest;
      r.mins = e.mins;
      r.maxs = e.maxs;
      continue;
    }
    r.free = e.gone || e.hidden;
    r.origin = e.origin;
    r.angles = e.angles;
    r.modelindex = e.modelindex;
    r.mins = e.mins;
    r.maxs = e.maxs;
    r.frame = e.frame;
    if (k == kObject || k == kExplosive)  // its textures turning: twice a second, or ten times
      r.effects = (e.spawnflags & 2) != 0 ? 0x1000 : (e.spawnflags & 4) != 0 ? 0x2000 : 0;
    if (k == kBomb && e.on) r.effects = kSqRocketEffect;  // a rocket's trail and glow as it falls
    if (e.classname == "misc_blackhole") r.render_fx = kSqTranslucentRender;
    if (k == kFigure) {  // its frames on a loop
      const Figure* f = figure_of(e.classname);
      const int count = f->last - f->first + 1;
      const int64_t at = static_cast<int64_t>(s.frame_number) + e.state;
      r.frame = f->first + static_cast<int>(std::max<int64_t>(at, 0) % count);
      if (e.classname == "misc_satellite_dish")
        r.frame = e.state < 0 ? 0 : std::min(kDishLast, int(s.frame_number) - e.state);
    }
  }
}

}  // namespace shooters_edge::server
