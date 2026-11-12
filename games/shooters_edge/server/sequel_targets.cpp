// The sequel's other single-player entities (sequel_targets.hpp states the rules; the numbers are
// the clean-room note's, porting/shooters-edge/sequel-campaign.md).
//
// ge:summary The sequel's other entities at work: locks, counts, lasers, shakes, blasts, words
// ge:keywords sequel trigger-key laser-beam earthquake-shake explosive-brush barrel light-ramp
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_targets_test.cpp
#include "shooters_edge/server/sequel_targets.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_monsters.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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
      {"target_string", kString},
      {"target_character", kCharacter},
      {"func_object", kObject},
      {"target_spawner", kSpawner},
      {"misc_viper_bomb", kBomb},
      {"turret_breach", kBreach},
      {"turret_base", kTurretBase},
      {"misc_viper", kShip},
      {"misc_strogg_ship", kShip},
  };
  for (const auto& [name, k] : kKinds)
    if (c == name) return k;
  return kNone;
}
Kind kind_of(const SequelEntity& e) { return kind_of(e.classname); }

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
};
const Figure* figure_of(std::string_view c) {
  for (const Figure& f : kFigures)
    if (f.classname == c) return &f;
  return nullptr;
}

const char* kBarrelModel = "models/objects/barrels/tris.md2";
const char* kBodyModel = "models/deadbods/dude/tris.md2";
const char* kBombModel = "models/objects/bomb/tris.md2";
constexpr int kSpares = 4;  // the monsters a spawner can make
const char* kShakeSound =
    "world/qua"
    "ke.wav";  // the rumble (the pack's own file name)

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const SequelServer& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }
float crandom(SequelServer& s) { return float(2 * (ge::core::next(s.rng) - 0.5)); }

std::string key(const SequelServer& s, const SequelEntity& e, const char* name) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get(name);
}
float number(const SequelServer& s, const SequelEntity& e, const char* name, float fallback) {
  const std::string v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void event(SequelServer& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound_at(SequelServer& s, int e, sh::Vec3f at, const char* sample, bool everywhere = false) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(e);
  ev.level = 2;
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = everywhere ? 0 : 1;
  ev.text = sample;
  event(s, std::move(ev));
}

void centre_to(SequelServer& s, int slot, const std::string& text) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.text = text;
  event(s, std::move(ev));
}

bool alive(const SequelServer& s, int slot) {
  return slot >= 0 && slot < sh::kMaxClients && s.clients[slot].active && s.players[slot].alive;
}

void add_model(SequelServer& s, SequelEntity& e, const char* model) {
  const auto found = std::find(s.models.begin(), s.models.end(), model);
  e.modelindex = static_cast<int>(found - s.models.begin());
  if (found == s.models.end()) s.models.emplace_back(model);
}
void add_sound(SequelServer& s, const char* sample) {
  if (std::find(s.sounds.begin(), s.sounds.end(), sample) == s.sounds.end())
    s.sounds.emplace_back(sample);
}

sh::Vec3f way(float yaw) { return sh::angle_vectors({0, yaw, 0}).forward; }
sh::Vec3f way_of(const SequelEntity& e) {  // its angle: -1 up, -2 down
  if (e.angles.y == -1) return {0, 0, 1};
  if (e.angles.y == -2) return {0, 0, -1};
  return sh::angle_vectors(e.angles).forward;
}
sh::Vec3f centre(const SequelEntity& e) { return e.origin + (e.mins + e.maxs) * 0.5f; }

bool overlap(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y && alo.z < bhi.z &&
         ahi.z > blo.z;
}

// Kills every player and monster in `e`'s box (a killbox, a thing appearing in place).
void kill_inside(SequelServer& s, const SequelEntity& e) {
  const sh::Vec3f lo = e.origin + e.mins, hi = e.origin + e.maxs;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!alive(s, i)) continue;
    const sh::Vec3f at = unpack(s.players[i].move.origin);
    if (overlap(at + s.players[i].mins, at + s.players[i].maxs, lo, hi))
      fight_hurt(s, i, -1, {}, 100000, 0, kSqByTelefrag, 3);
  }
  for (const SequelMonster& m : s.monsters) {
    const SequelEntity& b = s.entities[size_t(m.entity)];
    if (m.dead || b.gone || !b.box) continue;
    if (overlap(b.origin + b.mins, b.origin + b.maxs, lo, hi))
      monster_damage(s, m.entity, -1, 100000, {});
  }
}

// A thing that breaks: its blast, its targets, and it is gone.
void blow(SequelServer& s, int i, int attacker, int cause) {
  SequelEntity& e = s.entities[size_t(i)];
  const sh::Vec3f at = kind_of(e) == kBarrel ? e.origin + sh::Vec3f{0, 0, 20} : centre(e);
  e.gone = true;
  e.solid = false;
  e.box = false;
  if (e.dmg > 0) {
    fight_blast(s, at, attacker, e.dmg, float(e.dmg) + 40, cause);
    fight_effect(s, "te_explosion", at);
  }
  fight_effect(s, "te_debris", at);
  map_use_targets(s, i, attacker);
}

// The first living player whose box `lo`..`hi` overlaps, or -1.
int player_in(const SequelServer& s, sh::Vec3f lo, sh::Vec3f hi) {
  for (int p = 0; p < sh::kMaxClients; p++) {
    if (!alive(s, p)) continue;
    const sh::Vec3f at = unpack(s.players[p].move.origin);
    if (overlap(at + s.players[p].mins, at + s.players[p].maxs, lo, hi)) return p;
  }
  return -1;
}

float gravity(const SequelServer& s) {
  return static_cast<float>(s.cvars.find("sv_gravity")->number);
}

// A falling object's frame: straight down under gravity until something holds it up; landing on
// a player or a monster from above deals its `dmg` (a crush).
void fall_object(SequelServer& s, int i) {
  SequelEntity& e = s.entities[size_t(i)];
  e.now_speed += gravity(s) * kStep;  // its fall speed (units/s, down)
  const sh::Vec3f to = e.origin - sh::Vec3f{0, 0, e.now_speed * kStep};
  const sh::SequelTrace t = map_trace(s, e.origin, e.mins, e.maxs, to, kSolid | sh::kSqMonster, i);
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
void fly_bomb(SequelServer& s, int i) {
  SequelEntity& e = s.entities[size_t(i)];
  e.velocity.z -= gravity(s) * kStep;
  const float since = static_cast<float>(s.level_time - e.tick_at);
  e.angles.x = std::min(since, 1.0f) * 90;
  e.angles.z += 10;
  const sh::Vec3f to = e.origin + e.velocity * kStep;
  const sh::SequelTrace t = map_trace(s, e.origin, e.mins, e.maxs, to, kSolid | sh::kSqMonster, i);
  e.origin = t.end;
  if (t.fraction >= 1 && player_in(s, e.origin + e.mins, e.origin + e.maxs) < 0) return;
  e.on = false;
  e.gone = true;
  map_use_targets(s, i, e.activator);
  fight_blast(s, e.origin, e.activator, e.dmg, float(e.dmg) + 40, kSqByExplosion);
  fight_effect(s, "te_explosion", e.origin);
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
void turn_breach(SequelServer& s, int i) {
  SequelEntity& e = s.entities[size_t(i)];
  if (e.until > 0 && s.level_time + 0.001 >= e.until) {  // the muzzle, from its marker
    e.until = 0;
    e.aim = -1;
    for (size_t t = 0; t < s.entities.size(); t++)
      if (!e.target.empty() && s.entities[t].targetname == e.target) e.aim = static_cast<int>(t);
    if (e.aim < 0) {
      s.console.push_back("A turret's breach has no muzzle marker named '" + e.target + "'.");
    } else {
      SequelEntity& mark = s.entities[size_t(e.aim)];
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
  for (SequelEntity& b : s.entities)
    if (kind_of(b) == kTurretBase && !e.team.empty() && b.team == e.team)
      b.avelocity = {0, e.avelocity.y, 0};
}

}  // namespace

bool targets_spawn(SequelServer& s, int i) {
  SequelEntity& e = s.entities[size_t(i)];
  const Kind k = kind_of(e);
  if (k == kNone) return false;
  if (e.model.size() > 1 && e.model[0] == '*' && e.modelindex > 0 &&
      size_t(e.modelindex) < s.map->models.size()) {
    const ge::brush::Model& m = s.map->models[size_t(e.modelindex)];
    e.mins = sh::Vec3f{m.mins[0], m.mins[1], m.mins[2]};
    e.maxs = sh::Vec3f{m.maxs[0], m.maxs[1], m.maxs[2]};
  }
  e.message = key(s, e, "message");
  e.dmg = static_cast<int>(number(s, e, "dmg", 0));
  e.speed = number(s, e, "speed", 0);
  e.delay = number(s, e, "delay", 0);
  e.health = static_cast<int>(number(s, e, "health", 0));
  e.count = static_cast<int>(number(s, e, "count", 0));
  e.sounds = static_cast<int>(number(s, e, "sounds", 0));  // a splash's colour
  e.team = key(s, e, "team");
  switch (k) {
    case kKey:
      e.needs = sq_item(key(s, e, "item"));
      if (e.needs < 0 || e.target.empty()) {
        s.console.push_back("A key lock at " + std::to_string(int(e.origin.x)) + " " +
                            std::to_string(int(e.origin.y)) +
                            " has no item it knows or no target; it does nothing.");
        e.gone = true;
      }
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
      if (key(s, e, "gravity").empty()) {
        s.console.push_back("A gravity trigger has no gravity; it is left out.");
        e.gone = true;
      }
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
      e.solid = (e.spawnflags & 1) == 0;  // flag 1: hidden until a use
      e.hidden = !e.solid;
      return true;
    case kBarrel: {
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      add_model(s, e, kBarrelModel);
      e.mins = {-16, -16, 0};
      e.maxs = {16, 16, 40};
      e.box = true;
      if (e.health == 0) e.health = 10;
      if (e.dmg == 0) e.dmg = 150;
      e.count = static_cast<int>(number(s, e, "mass", 400));
      const sh::SequelTrace t = map_trace(s, e.origin + sh::Vec3f{0, 0, 1}, e.mins, e.maxs,
                                          e.origin - sh::Vec3f{0, 0, 128}, kSolid, i);
      if (!t.start_solid) e.origin = t.end;
      return true;
    }
    case kLaser:
      e.until = s.level_time + 1;  // it sets itself up a second in
      e.on = false;
      if (e.dmg == 0) e.dmg = 1;
      return true;
    case kLightRamp:
      if (deathmatch(s)) {
        e.gone = true;
        return true;
      }
      if (e.message.size() != 2 || e.message[0] < 'a' || e.message[0] > 'z' || e.message[1] < 'a' ||
          e.message[1] > 'z' || e.message[0] == e.message[1] || e.speed <= 0) {
        s.console.push_back(
            "A light ramp has no two different letters or no time; it is left "
            "out.");
        e.gone = true;
      }
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
      return true;
    }
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
        SequelEntity spare;
        spare.classname = made;
        spare.origin = e.origin;
        spare.angles = e.angles;
        spare.block = -2 - i;
        for (int n = 0; n < kSpares; n++) s.entities.push_back(spare);  // `e` may move
        return true;
      }
      if (sq_item(made) < 0)
        s.console.push_back("A spawner makes '" + made +
                            "', which is no monster or item it can make; it makes nothing.");
      return true;
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
      if (e.target.empty())
        s.console.push_back("A turret's breach has no muzzle marker; it fires from its middle.");
      return true;
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

bool targets_use(SequelServer& s, int i, int activator) {
  SequelEntity& e = s.entities[size_t(i)];
  const Kind k = kind_of(e);
  if (k == kNone) return false;
  if (e.gone && k != kExplosive) return true;
  switch (k) {
    case kKey: {
      if (!alive(s, activator)) return true;
      SequelPlayer& p = s.players[activator];
      if (p.body.counts[e.needs] <= 0) {
        if (s.level_time + 0.001 < e.touched_at + 5) return true;
        e.touched_at = s.level_time;
        centre_to(s, activator, std::string("You need the ") + kSqItems[e.needs].pickup + ".");
        sound_at(s, i, unpack(p.move.origin), "misc/keytry.wav");
        return true;
      }
      sound_at(s, i, e.origin, "misc/keyuse.wav");
      if (coop(s)) {
        for (SequelPlayer& o : s.players) o.body.counts[e.needs] = 0;
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
          centre_to(s, activator,
                    e.count == 1 ? "Only one more to go..."
                                 : "Only " + std::to_string(e.count) + " more to go...");
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
      kill_inside(s, e);
      return true;
    case kSplash: {
      const sh::Vec3f at = e.origin;
      static const char* const kSplashes[] = {
          "te_splash",       "te_splash_sparks", "te_splash_water", "te_splash_mud",
          "te_splash_slime", "te_splash_lava",   "te_splash_blood"};
      fight_effect(s, kSplashes[std::clamp(e.sounds, 0, 6)], at);
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
      fight_effect(s, ("te_style_" + std::to_string(e.sounds)).c_str(), e.origin);
      return true;
    case kExplosive:
      if (e.hidden) {  // it appears, killing what stands in its place, and can now be shot
        e.hidden = false;
        e.gone = false;
        e.solid = true;
        kill_inside(s, e);
        return true;
      }
      if (e.gone) return true;
      blow(s, i, activator, kSqByExplosion);
      return true;
    case kLaser:
      e.on = !e.on;
      e.activator = activator;
      if (e.on) fight_effect(s, "te_laser_sparks", e.origin);
      return true;
    case kLightRamp:
      e.until = s.level_time;  // the ramp starts now
      e.tick_at = s.level_time;
      e.activator = activator;
      return true;
    case kConveyor:
      e.on = !e.on;
      if ((e.spawnflags & 2) == 0) e.speed = 0;  // without the toggle flag, forgotten after once
      return true;
    case kString: {
      for (SequelEntity& c : s.entities) {
        if (kind_of(c) != kCharacter || c.team != e.team || c.count <= 0) continue;
        const size_t at = size_t(c.count - 1);
        const char ch = at < e.message.size() ? e.message[at] : ' ';
        c.frame = ch >= '0' && ch <= '9' ? ch - '0' : ch == '-' ? 10 : ch == ':' ? 11 : 12;
      }
      return true;
    }
    case kFigure:
      if (e.classname == "misc_blackhole") e.gone = true;
      return true;
    case kObject:
      if (!e.hidden) return true;
      e.hidden = false;
      e.solid = true;
      kill_inside(s, e);
      e.until = s.level_time;  // and it falls
      return true;
    case kSpawner: {
      const sh::Vec3f velocity = e.speed != 0 ? way(e.angles.y) * e.speed : sh::Vec3f{};
      if (monster_class(e.target)) {
        if (!monster_bring_spare(s, -2 - i, e.origin, e.angles, velocity, activator))
          s.console.push_back("A spawner of '" + e.target + "' has made all " +
                              std::to_string(kSpares) + " it can.");
        return true;
      }
      if (sq_item(e.target) >= 0) fight_drop_at(s, e.origin, e.target);
      return true;
    }
    case kBomb: {
      if (!e.hidden || e.on) return true;
      e.hidden = false;
      e.on = true;  // falling
      e.activator = activator;
      e.angles = {};
      for (const SequelEntity& v : s.entities)
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

bool targets_damage(SequelServer& s, int i, int attacker, int damage) {
  SequelEntity& e = s.entities[size_t(i)];
  const Kind k = kind_of(e);
  if (e.gone || damage <= 0) return false;
  switch (k) {
    case kExplosive:  // shootable unless only named (then a use blows it)
      if (e.hidden || ((e.spawnflags & 1) == 0 && !e.targetname.empty())) return false;
      e.health -= damage;
      if (e.health <= 0) blow(s, i, attacker, kSqByExplosion);
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
        fight_gib(s, map_row(i), e.origin);
      }
      return true;
    default:
      return false;
  }
}

void targets_frame(SequelServer& s) {
  for (size_t n = 0; n < s.entities.size(); n++) {
    SequelEntity& e = s.entities[n];
    const int i = static_cast<int>(n);
    if (e.gone) continue;
    switch (kind_of(e)) {
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
              s.console.push_back("A laser's target '" + e.target + "' is not on the map.");
          }
          if ((e.spawnflags & 1) != 0) e.on = true;
        }
        if (!e.on || e.until > 0) break;
        const sh::Vec3f dir =
            e.aim >= 0 ? centre(s.entities[size_t(e.aim)]) - e.origin : way_of(e) * 2048.0f;
        sh::Vec3f d = dir;
        sh::normalize(d);
        const sh::Vec3f end =
            fight_laser(s, e.origin, e.origin + d * 2048.0f, e.dmg, e.activator, i);
        NetEvent ev;
        ev.kind = EventKind::trail_effect;
        ev.to = kEveryone;
        ev.origin = e.origin;
        ev.values[0] = end.x;
        ev.values[1] = end.y;
        ev.values[2] = end.z;
        ev.ent = map_row(i);
        ev.text = (e.spawnflags & 64) != 0 ? "te_laser_fat" : "te_laser";
        event(s, std::move(ev));
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
        int light = -1;
        for (size_t t = 0; t < s.entities.size(); t++)
          if (s.entities[t].targetname == e.target && s.entities[t].classname == "light")
            light = static_cast<int>(t);
        if (light < 0) {
          e.gone = true;
          break;
        }
        const int style = s.entities[size_t(light)].state;
        const int a = e.message[0] - 'a', b = e.message[1] - 'a';
        const double elapsed = s.level_time - e.until;
        const int frames = static_cast<int>(elapsed / kStep + 0.001);
        const int letter = a + static_cast<int>(float(frames) * float(b - a) / (e.speed / kStep));
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

void targets_monster_touch(SequelServer& s, SequelMonster& m) {
  const SequelEntity& b = s.entities[size_t(m.entity)];
  const sh::Vec3f lo = b.origin + b.mins, hi = b.origin + b.maxs;
  for (const SequelEntity& e : s.entities) {
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

void targets_turret(SequelServer& s, SequelMonster& m, int enemy) {
  SequelEntity& d = s.entities[size_t(m.entity)];
  int breach = -1;
  for (size_t t = 0; t < s.entities.size(); t++)
    if (!d.target.empty() && s.entities[t].targetname == d.target &&
        kind_of(s.entities[t]) == kBreach)
      breach = static_cast<int>(t);
  if (breach < 0) return;
  SequelEntity& b = s.entities[size_t(breach)];
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
  const SequelPlayer& p = s.players[enemy];
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

void targets_rows(SequelServer& s) {
  RulesWorld& w = s.world;
  for (size_t n = 0; n < s.entities.size(); n++) {
    const SequelEntity& e = s.entities[n];
    const Kind k = kind_of(e);
    if (k != kBarrel && k != kDeadSoldier && k != kFigure && k != kCharacter && k != kExplosive &&
        k != kObject && k != kBomb && k != kShip)
      continue;
    const size_t row = size_t(map_row(static_cast<int>(n)));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    r.free = e.gone || e.hidden;
    r.origin = e.origin;
    r.angles = e.angles;
    r.modelindex = e.modelindex;
    r.mins = e.mins;
    r.maxs = e.maxs;
    r.frame = e.frame;
    if (k == kFigure) {  // its frames on a loop
      const Figure* f = figure_of(e.classname);
      const int count = f->last - f->first + 1;
      r.frame = f->first + static_cast<int>(s.frame_number % count);
    }
  }
}

}  // namespace shooters_edge::server
