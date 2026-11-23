// The Reckoning's map things (q2_reckoning.hpp states the rules; the numbers are the clean-room
// note's, porting/shooters-edge/q2-reckoning-rules.md § 20).
//
// ge:summary The Reckoning's map things: the repair object, the rotating light, the nuke,
// ge:summary the viper's missile and the ambient hum
// ge:keywords q2 reckoning mission-pack object-repair rotating-light alarm misc-nuke
// ge:keywords viper-missile amb4 welding-sparks
// ge:skill shooter
// ge:test tests/shooters_edge_q2_reckoning_test.cpp
#include "shooters_edge/server/q2_reckoning.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr double kStep = 0.1;
constexpr uint32_t kEveryone = 0xffffffffu;

enum Thing : uint8_t { kNoThing, kRepair, kRotatingLight, kAmbient, kNuke, kViperMissile };

Thing thing_of(std::string_view c) {
  if (c == "func_object_repair" || c == "object_repair") return kRepair;
  if (c == "rotating_light") return kRotatingLight;
  if (c == "misc_amb4") return kAmbient;
  if (c == "misc_nuke") return kNuke;
  if (c == "misc_viper_missile") return kViperMissile;
  return kNoThing;
}

std::string key(const Q2Server& s, const Q2Entity& e, const char* name) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get(name);
}
float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
  const std::string v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// A sound from entity `i` on `channel` at `attenuation` (0 heard everywhere).
void sound(Q2Server& s, int i, const char* sample, int channel, float attenuation) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(i);
  ev.level = channel;
  ev.origin = s.entities[size_t(i)].origin;
  ev.values[0] = 255;
  ev.values[1] = attenuation;
  ev.text = sample;
  event(s, std::move(ev));
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

// Welding sparks (temporary entity 25): `count` of palette `colour`, along no way.
void sparks(Q2Server& s, sh::Vec3f at, int count, int colour) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = at;
  ev.values[3] = static_cast<float>(count);
  ev.level = colour;
  ev.text = "te_welding_sparks";
  event(s, std::move(ev));
}
int spark_colour(Q2Server& s) { return 0xe0 + ge::core::int_in(s.rng, 0, 7); }

constexpr const char* kLightModel = "models/objects/light/tris.md2";
constexpr const char* kAlarm = "misc/alarm.wav";
constexpr const char* kHum = "world/amb4.wav";

}  // namespace

bool reckoning_thing(std::string_view classname) { return thing_of(classname) != kNoThing; }

bool reckoning_spawn(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  switch (thing_of(e.classname)) {
    case kNoThing:
      return false;
    case kRepair:  // renamed so the repair bot finds it; sparks a `delay` (1 s) on
      e.classname = "object_repair";
      e.mins = {-8, -8, -8};
      e.maxs = {8, 8, 8};
      e.box = true;
      e.health = 100;
      e.max_health = 100;
      e.delay = number(s, e, "delay", 1);
      e.tick_at = s.level_time + 1;
      e.state = 0;  // 1 once its targets fired, 2 then regrowing
      return true;
    case kRotatingLight:
      add_model(s, e, kLightModel);
      add_sound(s, kAlarm);
      e.health = static_cast<int>(number(s, e, "health", 10));
      e.box = true;
      e.on = (e.spawnflags & 1) == 0;  // START_OFF
      e.tick_at = 0;                   // an alarm on its way (a light that starts on is silent)
      return true;
    case kAmbient:
      add_sound(s, kHum);
      e.tick_at = s.level_time + 1;
      return true;
    case kNuke:
      e.on = true;  // spent once used
      return true;
    case kViperMissile:
      e.hidden = true;
      e.dmg = static_cast<int>(number(s, e, "dmg", 250));
      return true;
  }
  return false;
}

bool reckoning_use(Q2Server& s, int i, int activator) {
  Q2Entity& e = s.entities[size_t(i)];
  switch (thing_of(e.classname)) {
    case kNoThing:
    case kRepair:
    case kAmbient:
      return thing_of(e.classname) != kNoThing;
    case kRotatingLight:
      if (e.gone || e.health <= 0) return true;
      e.on = !e.on;
      e.tick_at = e.on && (e.spawnflags & 2) != 0 ? s.level_time + kStep : 0;  // ALARM
      return true;
    case kNuke:  // every player struck past invulnerability, every monster gone outright
      if (!e.on) return true;
      e.on = false;
      for (int p = 0; p < sh::kMaxClients; p++)
        if (s.clients[p].active && s.players[p].alive)
          fight_hurt(s, p, -1, {}, 100000, 1, kSqByNuke, 2);
      for (Q2Monster& m : s.monsters)
        if (!m.dead) s.entities[size_t(m.entity)].gone = true;
      return true;
    case kViperMissile: {  // a monster's rocket at its target's place, then it goes
      if (e.gone) return true;
      for (size_t t = 0; t < s.entities.size(); t++) {
        if (s.entities[t].targetname != e.target || e.target.empty()) continue;
        sh::Vec3f d = s.entities[t].origin - e.origin;
        sh::normalize(d);
        fight_monster_line(s, "te_monster_flash.rocket", i, e.origin, e.origin + d * 16.0f, 57);
        fight_monster_launch(s, i, SqShot::rocket, e.origin, d, e.dmg, 500, kSqByExplosion);
        break;
      }
      e.think_at = 0;
      e.until = s.level_time + kStep;  // removed a frame on
      (void)activator;
      return true;
    }
  }
  return false;
}

bool reckoning_damage(Q2Server& s, int i, int damage) {
  Q2Entity& e = s.entities[size_t(i)];
  const Thing t = thing_of(e.classname);
  if (t == kRepair) {
    e.health -= damage;
    return true;
  }
  if (t != kRotatingLight || e.gone || e.health <= 0) return t == kRotatingLight;
  e.health -= damage;
  if (e.health > 0) return true;
  sparks(s, e.origin, 30, spark_colour(s));  // broken: its sparks, dark, gone a frame on
  e.on = false;
  e.tick_at = 0;
  e.until = s.level_time + kStep;
  return true;
}

void reckoning_frame(Q2Server& s) {
  for (size_t n = 0; n < s.entities.size(); n++) {
    Q2Entity& e = s.entities[n];
    const int i = static_cast<int>(n);
    const Thing t = thing_of(e.classname);
    if (t == kNoThing || e.gone) continue;
    const bool due = e.tick_at > 0 && s.level_time + 0.001 >= e.tick_at;
    switch (t) {
      case kRepair:
        if (!due) break;
        e.tick_at = s.level_time + double(e.delay);
        if (e.health < 0 && e.state == 0) {  // welded through: its targets, once
          e.state = 1;
          map_use_targets(s, i, i);
          e.tick_at = s.level_time + kStep;
          break;
        }
        if (e.state == 1) e.state = 2;
        if (e.state == 2 && e.health <= 100) {  // regrowing, quietly
          e.health++;
          break;
        }
        if (e.health >= 0) sparks(s, e.origin, 10, spark_colour(s));
        break;
      case kRotatingLight:
        if (e.until > 0 && s.level_time + 0.001 >= e.until) {
          e.gone = true;
          break;
        }
        if (due && e.on) {
          sound(s, i, kAlarm, 2, 3);
          e.tick_at = s.level_time + 1;
        }
        break;
      case kAmbient:
        if (due) {
          sound(s, i, kHum, 2, 0);
          e.tick_at = s.level_time + kAmbientEvery;
        }
        break;
      case kViperMissile:
        if (e.until > 0 && s.level_time + 0.001 >= e.until) e.gone = true;
        break;
      default:
        break;
    }
  }
}

void reckoning_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  for (size_t n = 0; n < s.entities.size(); n++) {
    const Q2Entity& e = s.entities[n];
    if (thing_of(e.classname) != kRotatingLight) continue;
    const size_t row = size_t(map_row(static_cast<int>(n)));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    r.free = e.gone;
    r.origin = e.origin;
    r.angles = e.angles;
    r.modelindex = e.modelindex;
    r.effects = e.on ? kSqSpinningLightsEffect : 0;
  }
}

}  // namespace shooters_edge::server
