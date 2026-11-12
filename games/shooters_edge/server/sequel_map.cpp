// The sequel's movers and triggers (sequel_map.hpp states the rules; the numbers are the
// clean-room note's, porting/shooters-edge/sequel-movers.md).
//
// ge:summary The sequel's movers and triggers: the moves, the push, doors, plats, triggers, uses
// ge:keywords sequel func-door func-button func-plat func-train trigger-multiple trigger-push
// ge:keywords trigger-hurt teleporter use-targets plain-move accelerated-move mover-push crush
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_map_test.cpp
#include "shooters_edge/server/sequel_map.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr float kStep = 0.1f;
constexpr uint32_t kEveryone = 0xffffffffu;
constexpr int kMaxUseDepth = 16;  // a chain of uses deeper than this is a loop: it stops

// The movers' and triggers' kinds, from their class names.
enum Class : uint8_t {
  kNone,
  kDoor,
  kButton,
  kPlat,
  kTrain,
  kWall,
  kTimer,
  kTriggerOnce,
  kTriggerMultiple,
  kRelay,
  kAlways,
  kPush,
  kHurt,
  kTeleporter,
  kChangeLevel,
  kSpeaker,
  kExplosion,
  kSecret,
  kGoal,
  kDoorField,
  kPlatField,
};

// A mover's states: at rest at its first place, going to its second, at its second, coming back.
enum State : uint8_t { kAtFirst, kGoing, kAtSecond, kComing };

// The actions a think or an arrival runs.
enum Act : uint8_t {
  kNoAct,
  kDoorClose,
  kDoorTop,
  kDoorBottom,
  kButtonIn,
  kButtonReturn,
  kButtonOut,
  kPlatDown,
  kPlatTop,
  kPlatBottom,
  kTrainFind,
  kTrainNext,
  kTrainCorner,
  kTimerFire,
  kAlwaysFire,
  kRemove,
};

struct Mover {
  Class cls = kNone;
  int corner = -1;  // a train's current corner
};

const char* kDoorStart = "doors/dr1_strt.wav";
const char* kDoorEnd = "doors/dr1_end.wav";
const char* kPlatStart = "plats/pt1_strt.wav";
const char* kPlatEnd = "plats/pt1_end.wav";
const char* kButtonSound = "switches/butn2.wav";
const char* kTalk = "misc/talk1.wav";
const char* kWindfly = "misc/windfly.wav";
const char* kElectro = "world/electro.wav";

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }

const std::string& key(const SequelServer& s, const SequelEntity& e, const char* name) {
  static const std::string kEmpty;
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return kEmpty;
  return s.map->entities[size_t(e.block)].get(name);
}

float number(const SequelServer& s, const SequelEntity& e, const char* name, float fallback) {
  const std::string& v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void pack(int16_t out[3], sh::Vec3f v) {
  const float in[3] = {v.x, v.y, v.z};
  for (int i = 0; i < 3; i++)
    out[i] = static_cast<int16_t>(std::clamp(num::to_int32(double(in[i]) * 8), -32768, 32767));
}

// A move rounded to eighths, each axis half away from zero.
sh::Vec3f eighths(sh::Vec3f v) {
  const auto r = [](float x) {
    const double q = double(x) * 8;
    return float(double(q < 0 ? -num::to_int32(-q + 0.5) : num::to_int32(q + 0.5)) * 0.125);
  };
  return {r(v.x), r(v.y), r(v.z)};
}

void event(SequelServer& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound_at(SequelServer& s, int ent, sh::Vec3f at, const std::string& sample) {
  if (sample.empty()) return;
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = 2;  // the voice channel
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = 1;
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

void effect(SequelServer& s, const char* name, sh::Vec3f at) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = at;
  ev.values[1] = 1;
  ev.text = name;
  event(s, std::move(ev));
}

void print_all(SequelServer& s, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = kEveryone;
  ev.reliable = true;
  ev.level = 2;
  ev.text = std::move(text);
  event(s, std::move(ev));
}

std::string name_of(const SequelServer& s, int slot) {
  if (slot < 0 || slot >= sh::kMaxClients) return "someone";
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return "someone";
}

Class class_of(std::string_view c) {
  constexpr std::pair<std::string_view, Class> kClasses[] = {
      {"func_door", kDoor},
      {"func_button", kButton},
      {"func_plat", kPlat},
      {"func_train", kTrain},
      {"func_wall", kWall},
      {"func_timer", kTimer},
      {"trigger_once", kTriggerOnce},
      {"trigger_multiple", kTriggerMultiple},
      {"trigger_relay", kRelay},
      {"trigger_always", kAlways},
      {"trigger_push", kPush},
      {"trigger_hurt", kHurt},
      {"misc_teleporter", kTeleporter},
      {"target_changelevel", kChangeLevel},
      {"target_speaker", kSpeaker},
      {"target_explosion", kExplosion},
      {"target_secret", kSecret},
      {"target_goal", kGoal},
      {"door_field", kDoorField},
      {"plat_field", kPlatField},
  };
  for (const auto& [name, cls] : kClasses)
    if (c == name) return cls;
  return kNone;
}

Class class_of(const SequelEntity& e) { return class_of(e.classname); }

bool brush(const SequelEntity& e) { return !e.model.empty() && e.model[0] == '*'; }

sh::Vec3f abs_min(const SequelEntity& e) { return e.origin + e.mins; }
sh::Vec3f abs_max(const SequelEntity& e) { return e.origin + e.maxs; }

bool overlap(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y && alo.z < bhi.z &&
         ahi.z > blo.z;
}

// The way of an entity's `angle` (−1 up, −2 down) or `angles`: their forward axis.
sh::Vec3f way_of(const SequelServer& s, const SequelEntity& e) {
  const std::string& angle = key(s, e, "angle");
  if (!angle.empty()) {
    const float a = std::strtof(angle.c_str(), nullptr);
    if (a == -1) return {0, 0, 1};
    if (a == -2) return {0, 0, -1};
  }
  if (e.angles.x == 0 && e.angles.y == 0 && e.angles.z == 0) return {1, 0, 0};
  return sh::angle_vectors({e.angles.x, e.angles.y, 0}).forward;
}

bool has_angle(const SequelServer& s, const SequelEntity& e) {
  return !key(s, e, "angle").empty() || e.angles.x != 0 || e.angles.y != 0 || e.angles.z != 0;
}

// Whether a player's box at `at` fits (in no solid of the map or a mover).
bool fits(const SequelServer& s, const SequelPlayer& p, sh::Vec3f at) {
  const sh::SequelTrace t =
      map_trace(s, at, p.mins, p.maxs, at, p.alive ? sh::kSqMaskPlayerSolid : sh::kSqMaskDeadSolid);
  return !t.start_solid && !t.all_solid;
}

int index_of(const SequelServer& s, const SequelEntity& e) {
  return static_cast<int>(&e - s.entities.data());
}

// ---- The moves ----

// Sets mover `e` going to `dest`; `arrive` runs when it gets there (sequel-movers.md § The plain
// move, § The accelerated move).
void begin_move(SequelEntity& e, sh::Vec3f dest, int arrive) {
  e.dest = dest;
  e.arrive = arrive;
  e.moved = true;
  sh::Vec3f way = dest - e.origin;
  const float distance = sh::normalize(way);
  e.dir = way;
  e.last = false;
  e.velocity = {};
  e.accelerated = !(e.speed == e.accel && e.speed == e.decel);
  if (!e.accelerated) {
    const float time = distance / e.speed;
    e.frames = static_cast<int>(time / kStep);
    e.rest = distance - float(e.frames) * e.speed * kStep;
    if (e.frames == 0) {
      e.velocity = way * (e.rest / kStep);
      e.last = true;
    } else {
      e.velocity = way * e.speed;
    }
    if (distance == 0) e.velocity = {};
    return;
  }
  // The accelerated move: planned on the next frame's step, from a standing start.
  e.rest = distance;
  e.now_speed = 0;
  e.next_speed = 0;
  e.top = 0;
}

void arrived(SequelServer& s, SequelEntity& e);

// The accelerated move's planning: the top speed and the span it slows over.
void plan(SequelEntity& e) {
  e.top = e.speed;
  const auto span = [](float v, float rate) { return v * (v / rate + 1) / 2; };
  const float up = span(e.speed, e.accel), down = span(e.speed, e.decel);
  e.decel_span = down;
  if (up + down > e.rest) {  // too short to reach the speed: a lower top
    const float f = (e.accel + e.decel) / (e.accel * e.decel);
    e.top = (sh::root_of(4 + 8 * f * e.rest) - 2) / (2 * f);
    e.decel_span = span(e.top, e.decel);
  }
}

// The accelerated move's step after a push: the next frame's speed.
void accelerate(SequelEntity& e) {
  if (e.top == 0) {
    if (e.rest < e.accel) {  // shorter than one step of speed: one push of all of it
      e.velocity = e.dir * (e.rest / kStep);
      e.last = true;
      return;
    }
    plan(e);
  } else {
    e.rest -= e.now_speed;
  }
  float v = e.now_speed;
  if (e.rest < e.decel_span) {  // slowing
    if (e.next_speed > 0) {
      v = e.next_speed;
      e.next_speed = 0;
    } else if (v > e.decel) {
      v = std::max(v - e.decel, e.decel);
    }
  } else if (v >= e.top) {  // at the top: past the mark within the frame, the next one slower
    if (e.rest - v < e.decel_span)
      e.next_speed = e.top - e.decel * ((e.decel_span - (e.rest - v)) / v);
  } else {  // speeding up
    const float up = std::min(v + e.accel, e.speed);
    if (e.rest - up < e.decel_span) {
      const float before = std::clamp((e.rest - e.decel_span) / up, 0.0f, 1.0f);
      const float top = std::min(up, e.top);
      v = before * (v + top) / 2 + (1 - before) * top;
      e.next_speed = top - e.decel * (1 - before);
    } else {
      v = up;
    }
  }
  if (e.rest <= v) {  // the last push: what is left
    e.velocity = e.dir * (e.rest / kStep);
    e.last = true;
    return;
  }
  e.now_speed = v;
  e.velocity = e.dir * (v * 10);
}

// A mover's step after a frame's push.
void step_move(SequelServer& s, SequelEntity& e) {
  if (e.arrive == kNoAct) return;
  if (e.last) {
    e.velocity = {};
    e.last = false;
    arrived(s, e);
    return;
  }
  if (e.accelerated) {
    accelerate(e);
    return;
  }
  if (--e.frames > 0) return;
  if (e.rest == 0) {
    e.velocity = {};
    arrived(s, e);
    return;
  }
  e.velocity = e.dir * (e.rest / kStep);
  e.last = true;
}

// ---- The push ----

// Pushes mover `e` by `move`: its riders and the players in its way move with it. Returns the
// slot that blocked it (the push undone), or -1.
int push(SequelServer& s, SequelEntity& e, sh::Vec3f move, bool stop_type) {
  const sh::Vec3f from = e.origin;
  e.origin = from + move;
  const sh::Vec3f lo = abs_min(e), hi = abs_max(e);
  const int row = map_row(index_of(s, e));
  int16_t before[sh::kMaxClients][3];
  bool moved[sh::kMaxClients] = {};
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active || s.clients[i].spectator) continue;
    SequelPlayer& p = s.players[i];
    const sh::Vec3f at = unpack(p.move.origin);
    const bool rider = p.ground == row;
    if (!rider && !overlap(at + p.mins, at + p.maxs, lo, hi)) continue;
    const bool fit_after = !stop_type || rider;
    const sh::Vec3f to = at + move;
    if (fit_after && fits(s, p, to)) {
      for (int k = 0; k < 3; k++) before[i][k] = p.move.origin[k];
      pack(p.move.origin, to);
      moved[i] = true;
      if (!rider) p.ground = -1;
      continue;
    }
    if (fits(s, p, at)) continue;              // fits where he was: left there
    for (int j = 0; j < sh::kMaxClients; j++)  // blocked: everything back
      if (moved[j])
        for (int k = 0; k < 3; k++) s.players[j].move.origin[k] = before[j][k];
    e.origin = from;
    return i;
  }
  return -1;
}

void door_go_up(SequelServer& s, SequelEntity& e, int activator);
void door_go_down(SequelServer& s, SequelEntity& e);
void plat_go_up(SequelServer& s, SequelEntity& e);
void plat_go_down(SequelServer& s, SequelEntity& e);
void use(SequelServer& s, int target, int activator, int depth);

// What a mover does when a player blocks it: crush, then a door or plat turns back.
void blocked(SequelServer& s, SequelEntity& e, int slot) {
  const Class c = class_of(e);
  if (c == kTrain) {
    if (e.dmg > 0 && s.level_time + 0.001 >= e.touched_at + 0.5) {
      e.touched_at = s.level_time;
      fight_hurt(s, slot, -1, {}, e.dmg, 1, kSqByCrush);
    }
    return;
  }
  fight_hurt(s, slot, -1, {}, e.dmg, 1, kSqByCrush);
  if (c == kPlat) {
    if (e.state == kGoing)
      plat_go_down(s, e);
    else if (e.state == kComing)
      plat_go_up(s, e);
    return;
  }
  if (c != kDoor || e.wait < 0 || (e.spawnflags & 4) != 0) return;  // a crusher keeps pressing
  for (SequelEntity& m : s.entities)                                // the whole team turns back
    if (&m == &e || (e.team_master >= 0 && m.team_master == e.team_master)) {
      if (m.state == kComing)
        door_go_up(s, m, m.activator);
      else
        door_go_down(s, m);
    }
}

// ---- Doors, buttons, plats, trains ----

void door_go_up(SequelServer& s, SequelEntity& e, int activator) {
  if (e.state == kGoing) return;
  if (e.state == kAtSecond) {  // open: the close put off
    if (e.wait >= 0) e.think_at = s.level_time + e.wait;
    return;
  }
  e.activator = activator;
  if (e.sounds != 1 && index_of(s, e) == (e.team_master >= 0 ? e.team_master : index_of(s, e)))
    sound_at(s, map_row(index_of(s, e)), e.origin, kDoorStart);
  e.state = kGoing;
  e.think_at = 0;
  begin_move(e, e.open, kDoorTop);
  map_use_targets(s, index_of(s, e), activator);
}

void door_go_down(SequelServer& s, SequelEntity& e) {
  if (e.max_health > 0) e.health = e.max_health;  // it can be shot again
  if (e.sounds != 1) sound_at(s, map_row(index_of(s, e)), e.origin, kDoorStart);
  e.state = kComing;
  e.think_at = 0;
  begin_move(e, e.closed, kDoorBottom);
}

// A use of a door: the team opens (or, a toggle, closes when open or opening).
void door_use(SequelServer& s, SequelEntity& e, int activator) {
  const int master = e.team_master >= 0 ? e.team_master : index_of(s, e);
  if (index_of(s, e) != master) return;  // a use on a slave does nothing
  const bool toggle = (e.spawnflags & 32) != 0;
  const bool close = toggle && (e.state == kGoing || e.state == kAtSecond);
  for (SequelEntity& m : s.entities) {
    if (index_of(s, m) != master && m.team_master != master) continue;
    // Once used, a door's message and touch are gone for good.
    m.message.clear();
    if (close)
      door_go_down(s, m);
    else
      door_go_up(s, m, activator);
  }
}

void button_press(SequelServer& s, SequelEntity& e, int activator) {
  if (e.state == kGoing || e.state == kAtSecond) return;
  e.activator = activator;
  if (e.sounds != 1) sound_at(s, map_row(index_of(s, e)), e.origin, kButtonSound);
  e.state = kGoing;
  begin_move(e, e.open, kButtonIn);
}

void plat_go_up(SequelServer& s, SequelEntity& e) {
  sound_at(s, map_row(index_of(s, e)), e.origin, kPlatStart);
  e.state = kGoing;
  e.think_at = 0;
  begin_move(e, e.closed, kPlatTop);
}

void plat_go_down(SequelServer& s, SequelEntity& e) {
  sound_at(s, map_row(index_of(s, e)), e.origin, kPlatStart);
  e.state = kComing;
  e.think_at = 0;
  e.moved = true;
  begin_move(e, e.open, kPlatBottom);
}

// The corner named `name` (the first of several sharing it), or -1.
int corner_named(const SequelServer& s, const std::string& name) {
  if (name.empty()) return -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].targetname == name && s.entities[i].classname == "path_corner")
      return static_cast<int>(i);
  return -1;
}

// A train heads for the corner its target names: a teleport corner is jumped to at once.
void train_next(SequelServer& s, SequelEntity& e) {
  for (int hops = 0; hops < 2; hops++) {
    const int c = corner_named(s, e.target);
    if (c < 0) {
      e.velocity = {};
      e.arrive = kNoAct;
      return;
    }
    SequelEntity& corner = s.entities[size_t(c)];
    e.wait = number(s, corner, "wait", 0);
    e.target = corner.target;
    e.pathtarget = key(s, corner, "pathtarget");
    e.owner = c;
    if ((corner.spawnflags & 1) != 0) {  // a teleport corner: there at once
      e.origin = corner.origin - e.mins;
      continue;
    }
    e.state = kGoing;
    begin_move(e, corner.origin - e.mins, kTrainCorner);
    return;
  }
  e.velocity = {};  // two teleport corners in a row: it stops
  e.arrive = kNoAct;
}

void arrived(SequelServer& s, SequelEntity& e) {
  const int act = e.arrive;
  e.arrive = kNoAct;
  const int row = map_row(index_of(s, e));
  switch (act) {
    case kDoorTop:
      if (e.sounds != 1) sound_at(s, row, e.origin, kDoorEnd);
      e.state = kAtSecond;
      if ((e.spawnflags & 32) == 0 && e.wait >= 0) {
        e.think_at = s.level_time + e.wait;
        e.think = kDoorClose;
      }
      return;
    case kDoorBottom:
      if (e.sounds != 1) sound_at(s, row, e.origin, kDoorEnd);
      e.state = kAtFirst;
      return;
    case kButtonIn:
      e.state = kAtSecond;
      map_use_targets(s, index_of(s, e), e.activator);
      if (e.wait >= 0) {
        e.think_at = s.level_time + e.wait;
        e.think = kButtonReturn;
      }
      return;
    case kButtonOut:
      e.state = kAtFirst;
      if (e.max_health > 0) e.health = e.max_health;
      return;
    case kPlatTop:
      sound_at(s, row, e.origin, kPlatEnd);
      e.state = kAtSecond;
      e.think_at = s.level_time + 3;
      e.think = kPlatDown;
      return;
    case kPlatBottom:
      sound_at(s, row, e.origin, kPlatEnd);
      e.state = kAtFirst;
      return;
    case kTrainCorner: {
      e.state = kAtSecond;
      if (!e.pathtarget.empty()) {
        const std::string saved = e.target;
        e.target = e.pathtarget;  // the corner's path target, with the train's activator
        map_use_targets(s, index_of(s, e), e.activator);
        e.target = saved;
        if (e.gone) return;
      }
      if (e.wait < 0) {
        e.think_at = 0;
        return;
      }
      if (e.wait == 0) {
        train_next(s, e);
        return;
      }
      e.think_at = s.level_time + e.wait;
      e.think = kTrainNext;
      return;
    }
    default:
      return;
  }
}

void think(SequelServer& s, SequelEntity& e) {
  const int act = e.think;
  e.think_at = 0;
  e.think = kNoAct;
  switch (act) {
    case kDoorClose:
      for (SequelEntity& m : s.entities)
        if (&m == &e || (e.team_master >= 0 && m.team_master == e.team_master &&
                         index_of(s, e) == e.team_master))
          if (m.state == kAtSecond) door_go_down(s, m);
      return;
    case kButtonReturn:
      e.state = kComing;
      begin_move(e, e.closed, kButtonOut);
      return;
    case kPlatDown:
      if (e.state == kAtSecond) plat_go_down(s, e);
      return;
    case kTrainFind: {
      const int c = corner_named(s, e.target);
      if (c < 0) {
        s.console.push_back("A train at " + std::to_string(int(e.origin.x)) + " " +
                            std::to_string(int(e.origin.y)) + " has no corner to follow.");
        return;
      }
      SequelEntity& corner = s.entities[size_t(c)];
      e.origin = corner.origin - e.mins;
      e.target = corner.target;
      e.owner = c;
      if ((e.spawnflags & 1) != 0 || e.targetname.empty()) {
        e.think_at = s.level_time + kStep;
        e.think = kTrainNext;
        e.activator = -1;
      }
      return;
    }
    case kTrainNext:
      train_next(s, e);
      return;
    case kTimerFire:
      map_use_targets(s, index_of(s, e), e.activator);
      e.think_at = s.level_time + e.wait + double(e.random) * (2 * ge::core::next(s.rng) - 1);
      e.think = kTimerFire;
      return;
    case kAlwaysFire:
      map_use_targets(s, index_of(s, e), -1);
      return;
    case kRemove:
      e.gone = true;
      e.trigger = false;
      return;
    default:
      return;
  }
}

// ---- Triggers ----

// A trigger fires: its targets used, then dead for `wait` (a trigger_once for good).
void trigger_fire(SequelServer& s, SequelEntity& e, int activator) {
  if (s.level_time + 0.001 < e.touched_at) return;  // still waiting
  map_use_targets(s, index_of(s, e), activator);
  if (class_of(e) == kTriggerOnce) {
    e.trigger = false;
    e.think_at = s.level_time + kStep;
    e.think = kRemove;
    return;
  }
  e.touched_at = s.level_time + e.wait;
}

void teleport(SequelServer& s, SequelEntity& pad, int slot) {
  int dest = -1;
  for (size_t i = 0; i < s.entities.size() && dest < 0; i++)
    if (!pad.target.empty() && s.entities[i].targetname == pad.target) dest = static_cast<int>(i);
  if (dest < 0) return;
  const SequelEntity& d = s.entities[size_t(dest)];
  SequelPlayer& p = s.players[slot];
  pack(p.move.origin, d.origin + sh::Vec3f{0, 0, 10});
  for (int16_t& v : p.move.velocity) v = 0;
  p.move.flags |= sh::kSqTimeTeleport;
  p.move.time = 20;  // 160 ms
  const int16_t want[3] = {
      static_cast<int16_t>(num::to_int32(double(d.angles.x) * (65536.0 / 360.0))),
      static_cast<int16_t>(num::to_int32(double(d.angles.y) * (65536.0 / 360.0))), 0};
  for (int k = 0; k < 3; k++) {
    p.move.delta_angles[k] = int16_t(want[k] - p.cmd_angles[k]);
    p.forced_angles[k] = want[k];
  }
  p.forced = true;
  p.changed = true;
  p.view_angles = {d.angles.x, d.angles.y, 0};
  p.ground = -1;
  effect(s, "te_teleport", pad.origin);
  effect(s, "te_teleport", d.origin);
  // Whoever stands at the destination dies.
  const sh::Vec3f at = unpack(p.move.origin);
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == slot || !s.clients[i].active || !s.players[i].alive) continue;
    const SequelPlayer& o = s.players[i];
    const sh::Vec3f oa = unpack(o.move.origin);
    if (overlap(at + p.mins, at + p.maxs, oa + o.mins, oa + o.maxs))
      fight_hurt(s, i, slot, {}, 100000, 0, kSqByTelefrag, 3);
  }
}

void touch(SequelServer& s, SequelEntity& e, int slot) {
  SequelPlayer& p = s.players[slot];
  switch (class_of(e)) {
    case kTriggerOnce:
    case kTriggerMultiple:
      if ((e.spawnflags & 2) != 0) return;  // not by players
      if (!e.dir.x && !e.dir.y && !e.dir.z) {
        trigger_fire(s, e, slot);
        return;
      }
      if (sh::dot(sh::angle_vectors(p.view_angles).forward, e.dir) >= 0) trigger_fire(s, e, slot);
      return;
    case kPush: {
      const sh::Vec3f v = e.dir * (e.speed * 10);
      const sh::Vec3f held{std::clamp(v.x, -4095.0f, 4095.0f), std::clamp(v.y, -4095.0f, 4095.0f),
                           std::clamp(v.z, -4095.0f, 4095.0f)};
      pack(p.move.velocity, held);
      p.old_fall = held.z;  // no fall from the push
      p.ground = -1;
      if (s.level_time + 0.001 >= p.windfly_at + 1.5) {
        p.windfly_at = s.level_time;
        sound_at(s, slot + 1, unpack(p.move.origin), kWindfly);
      }
      if ((e.spawnflags & 1) != 0) {  // pushes once
        e.trigger = false;
        e.gone = true;
      }
      return;
    }
    case kHurt: {
      const double hold = (e.spawnflags & 16) != 0 ? 1 : kStep;
      if (s.level_time + 0.001 < e.touched_at + hold) return;
      e.touched_at = s.level_time;
      if ((e.spawnflags & 4) == 0 && s.frame_number % 10 == 0)
        sound_at(s, slot + 1, unpack(p.move.origin), kElectro);
      fight_hurt(s, slot, -1, {}, e.dmg, e.dmg, kSqByHurt, (e.spawnflags & 8) != 0 ? 2 : 0);
      return;
    }
    case kTeleporter:
      teleport(s, e, slot);
      return;
    case kDoorField: {
      if (s.level_time + 0.001 < e.touched_at + 1) return;
      e.touched_at = s.level_time;
      if (e.owner >= 0) door_use(s, s.entities[size_t(e.owner)], slot);
      return;
    }
    case kPlatField: {
      if (e.owner < 0) return;
      SequelEntity& plat = s.entities[size_t(e.owner)];
      if (plat.state == kAtFirst)
        plat_go_up(s, plat);
      else if (plat.state == kAtSecond)
        plat.think_at = s.level_time + 1;  // held up while someone stands there
      return;
    }
    case kDoor:  // leaning on a named door with a message (shown until it is used)
      if (e.message.empty() || e.targetname.empty() || e.max_health > 0) return;
      if (s.level_time + 0.001 < e.touched_at + 5) return;
      e.touched_at = s.level_time;
      centre_to(s, slot, e.message);
      sound_at(s, slot + 1, unpack(p.move.origin), kTalk);
      return;
    case kButton:
      if (e.health > 0 || e.max_health > 0 || !e.targetname.empty()) return;
      button_press(s, e, slot);
      return;
    default:
      return;
  }
}

// A use of entity `target` by `activator`.
void use(SequelServer& s, int target, int activator, int depth) {
  SequelEntity& e = s.entities[size_t(target)];
  if (e.gone && class_of(e) != kWall) return;
  switch (class_of(e)) {
    case kDoor:
      door_use(s, e, activator);
      return;
    case kButton:
      button_press(s, e, activator);
      return;
    case kPlat:
      if (!e.moved && e.state == kGoing && e.arrive == kNoAct) plat_go_down(s, e);
      return;
    case kTrain:
      if (e.arrive != kNoAct || e.think == kTrainNext) {
        if ((e.spawnflags & 2) != 0) {  // a toggle train stops where it is
          e.velocity = {};
          e.arrive = kNoAct;
          e.think_at = 0;
          e.think = kNoAct;
          e.state = kAtFirst;
        }
        return;
      }
      e.activator = activator;
      if (e.moved && (e.origin.x != e.dest.x || e.origin.y != e.dest.y || e.origin.z != e.dest.z)) {
        begin_move(e, e.dest, kTrainCorner);  // on toward the corner it was heading for
        return;
      }
      train_next(s, e);
      return;
    case kWall: {
      if ((e.spawnflags & 7) == 0) return;  // a plain wall
      const bool appear = !e.solid;
      e.solid = appear;
      e.gone = !appear;
      if (appear)  // whoever is inside dies
        for (int i = 0; i < sh::kMaxClients; i++) {
          if (!s.clients[i].active || !s.players[i].alive) continue;
          const SequelPlayer& p = s.players[i];
          const sh::Vec3f at = unpack(p.move.origin);
          if (overlap(at + p.mins, at + p.maxs, abs_min(e), abs_max(e)))
            fight_hurt(s, i, -1, {}, 100000, 0, kSqByCrush, 3);
        }
      if ((e.spawnflags & 2) == 0) e.spawnflags &= ~7;  // one use, then a plain wall
      return;
    }
    case kTimer:
      if (e.think == kTimerFire) {
        e.think_at = 0;
        e.think = kNoAct;
        return;
      }
      e.activator = activator;
      e.think_at = s.level_time + double(e.delay);
      e.think = kTimerFire;
      if (e.delay <= 0) think(s, e);  // at once, then every wait ± random
      return;
    case kTriggerOnce:
    case kTriggerMultiple:
      if (!e.trigger && e.state == 0 && (e.spawnflags & 4) != 0) {
        e.trigger = true;  // the first use makes it live
        e.state = 1;
        return;
      }
      trigger_fire(s, e, activator);
      return;
    case kRelay:
      if (depth < kMaxUseDepth) map_use_targets(s, target, activator);
      return;
    case kHurt:
      if ((e.spawnflags & 2) != 0) e.trigger = !e.trigger;
      return;
    case kChangeLevel: {
      if (s.intermission_at > 0 || !s.pending_map.empty()) return;
      std::string next = e.map;
      const size_t spot = next.find('$');
      if (spot != std::string::npos) next.resize(spot);
      next.erase(std::remove(next.begin(), next.end(), '*'), next.end());
      if (deathmatch(s)) {
        if (!fight_dmflag(s, 4096)) return;  // no exit in this match
        print_all(s, name_of(s, activator) + " left the level.\n");
        s.next_map = next;
        s.exit_now = true;
        return;
      }
      if (activator >= 0 && !s.players[activator].alive) return;
      if (e.map.find('*') != std::string::npos) {
        s.next_map = next;
        s.exit_now = true;  // a new unit: the intermission first
        return;
      }
      s.pending_map = next;
      s.pending_change = MapChange::change_level;
      return;
    }
    case kSpeaker:
      if ((e.spawnflags & 3) == 0) sound_at(s, map_row(target), e.origin, e.noise);
      return;
    case kExplosion:
      effect(s, "te_explosion", e.origin);
      fight_blast(s, e.origin, activator, e.dmg, float(e.dmg) + 40, kSqByExplosion);
      if (depth < kMaxUseDepth) map_use_targets(s, target, activator);
      return;
    case kSecret:
    case kGoal: {
      const bool secret = class_of(e) == kSecret;
      sound_at(s, map_row(target), e.origin, e.noise);
      (secret ? s.secrets_found : s.goals_found)++;
      const float saved = e.delay;
      e.delay = 0;
      map_use_targets(s, target, activator);
      e.delay = saved;
      e.gone = true;
      return;
    }
    default:
      return;
  }
}

void fire_targets(SequelServer& s, int ent, int activator, int depth) {
  const SequelEntity& e = s.entities[size_t(ent)];
  if (!e.message.empty()) {
    centre_to(s, activator, e.message);
    const int sounds = e.sounds;
    const char* sample = sounds == 1   ? "misc/secret.wav"
                         : sounds == 2 ? "misc/talk.wav"
                         : sounds == 3 ? "misc/trigger1.wav"
                                       : kTalk;
    if (activator >= 0)
      sound_at(s, activator + 1, unpack(s.players[activator].move.origin), sample);
  }
  if (!e.killtarget.empty())
    for (SequelEntity& k : s.entities)
      if (k.targetname == e.killtarget) {
        k.gone = true;
        k.solid = k.trigger = false;
        k.velocity = {};
        k.arrive = kNoAct;
        k.think_at = 0;
      }
  const std::string target = e.target;
  if (target.empty() || depth >= kMaxUseDepth) return;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].targetname == target && int(i) != ent)
      use(s, static_cast<int>(i), activator, depth + 1);
}

// Sets up one entity of class `c` from its keys.
void spawn_one(SequelServer& s, SequelEntity& e, Class c) {
  e.message = key(s, e, "message");
  e.killtarget = key(s, e, "killtarget");
  e.team = key(s, e, "team");
  e.noise = key(s, e, "noise");
  e.map = key(s, e, "map");
  e.delay = number(s, e, "delay", 0);
  e.sounds = static_cast<int>(number(s, e, "sounds", 0));
  e.dmg = static_cast<int>(number(s, e, "dmg", 0));
  e.wait = number(s, e, "wait", 0);
  e.health = e.max_health = static_cast<int>(number(s, e, "health", 0));
  if (brush(e) && e.modelindex > 0 && size_t(e.modelindex) < s.map->models.size()) {
    const ge::brush::Model& m = s.map->models[size_t(e.modelindex)];
    e.mins = sh::Vec3f{m.mins[0], m.mins[1], m.mins[2]};
    e.maxs = sh::Vec3f{m.maxs[0], m.maxs[1], m.maxs[2]};
  }
  const sh::Vec3f size = e.maxs - e.mins;
  const auto span = [&](sh::Vec3f d) {
    return (d.x < 0 ? -d.x : d.x) * size.x + (d.y < 0 ? -d.y : d.y) * size.y +
           (d.z < 0 ? -d.z : d.z) * size.z;
  };
  switch (c) {
    case kDoor: {
      e.solid = true;
      e.speed = number(s, e, "speed", 100);
      if (deathmatch(s)) e.speed *= 2;
      e.accel = number(s, e, "accel", e.speed);
      e.decel = number(s, e, "decel", e.speed);
      if (e.wait == 0) e.wait = 3;
      e.lip = number(s, e, "lip", 8);
      if (key(s, e, "dmg").empty()) e.dmg = 2;
      e.dir = way_of(s, e);
      e.angles = {};
      e.closed = e.origin;
      e.open = e.origin + e.dir * (span(e.dir) - e.lip);
      if ((e.spawnflags & 1) != 0) {  // start open: the places swap
        std::swap(e.closed, e.open);
        e.origin = e.closed;
      }
      return;
    }
    case kButton:
      e.solid = true;
      e.speed = number(s, e, "speed", 40);
      e.accel = number(s, e, "accel", e.speed);
      e.decel = number(s, e, "decel", e.speed);
      if (e.wait == 0) e.wait = 3;
      e.lip = number(s, e, "lip", 4);
      e.dir = way_of(s, e);
      e.angles = {};
      e.closed = e.origin;
      e.open = e.origin + e.dir * (span(e.dir) - e.lip);
      return;
    case kPlat: {
      e.solid = true;
      e.speed = number(s, e, "speed", 200) * 0.1f;
      e.accel = number(s, e, "accel", 50) * 0.1f;
      e.decel = number(s, e, "decel", 50) * 0.1f;
      e.lip = number(s, e, "lip", 8);
      if (key(s, e, "dmg").empty()) e.dmg = 2;
      e.angles = {};
      e.closed = e.origin;  // the top
      const std::string& height = key(s, e, "height");
      const float travel = height.empty() ? size.z - e.lip : std::strtof(height.c_str(), nullptr);
      e.open = e.origin - sh::Vec3f{0, 0, travel};  // the bottom
      // The field above it, fixed in the world, in the raised plat's place.
      SequelEntity f;
      f.classname = "plat_field";
      f.trigger = true;
      sh::Vec3f lo = abs_min(e) + sh::Vec3f{25, 25, 0}, hi = abs_max(e) - sh::Vec3f{25, 25, 0};
      if (hi.x <= lo.x) lo.x = hi.x = (abs_min(e).x + abs_max(e).x) / 2, hi.x += 1;
      if (hi.y <= lo.y) lo.y = hi.y = (abs_min(e).y + abs_max(e).y) / 2, hi.y += 1;
      hi.z = abs_max(e).z + 8;
      lo.z = hi.z - (travel + e.lip);
      if ((e.spawnflags & 1) != 0) hi.z = lo.z + 8;  // the low trigger
      f.origin = {};
      f.mins = lo;
      f.maxs = hi;
      f.owner = index_of(s, e);
      if (e.targetname.empty()) {
        e.origin = e.open;  // rests at the bottom
        e.state = kAtFirst;
      } else {
        e.state = kGoing;  // up, its field idle, until a use sends it down
      }
      s.entities.push_back(std::move(f));  // `e` may move: nothing after this reads it
      return;
    }
    case kTrain:
      e.solid = true;
      e.speed = number(s, e, "speed", 100);
      e.accel = e.decel = e.speed;
      if (key(s, e, "dmg").empty()) e.dmg = 100;
      if ((e.spawnflags & 4) != 0) e.dmg = 0;
      e.angles = {};
      e.think_at = s.level_time + kStep;
      e.think = kTrainFind;
      return;
    case kWall:
      if ((e.spawnflags & 7) != 0) {
        e.spawnflags |= 1;
        if ((e.spawnflags & 4) != 0) e.spawnflags |= 2;
      }
      e.solid = (e.spawnflags & 7) == 0 || (e.spawnflags & 4) != 0;
      e.gone = !e.solid;
      return;
    case kTimer:
      if (e.wait == 0) e.wait = 1;
      e.random = number(s, e, "random", 0);
      if (e.random >= e.wait) e.random = e.wait - 0.1f;
      if ((e.spawnflags & 1) != 0) {
        e.think_at = s.level_time + 1 + number(s, e, "pausetime", 0) + e.delay + e.wait +
                     double(e.random) * (2 * ge::core::next(s.rng) - 1);
        e.think = kTimerFire;
      }
      return;
    case kTriggerOnce:
    case kTriggerMultiple:
      if (c == kTriggerOnce && (e.spawnflags & 1) != 0) e.spawnflags = (e.spawnflags & ~1) | 4;
      if (c == kTriggerMultiple && e.wait == 0) e.wait = 0.2f;
      e.trigger = (e.spawnflags & 4) == 0;
      e.state = e.trigger ? 1 : 0;
      e.dir = has_angle(s, e) ? way_of(s, e) : sh::Vec3f{};
      e.angles = {};
      return;
    case kAlways:
      e.think_at = s.level_time + 0.2;
      e.think = kAlwaysFire;
      return;
    case kPush:
      e.trigger = true;
      e.speed = number(s, e, "speed", 1000);
      e.dir = has_angle(s, e) ? way_of(s, e) : sh::Vec3f{};
      e.angles = {};
      return;
    case kHurt:
      e.trigger = (e.spawnflags & 1) == 0;
      if (key(s, e, "dmg").empty()) e.dmg = 5;
      return;
    case kTeleporter:
      if (e.target.empty()) {
        e.gone = true;
        return;
      }
      e.trigger = true;
      e.mins = {-8, -8, 8};
      e.maxs = {8, 8, 24};
      return;
    case kChangeLevel:
      if (e.map.empty()) e.gone = true;
      return;
    case kSpeaker:
      if (!e.noise.empty() && e.noise.find(".wav") == std::string::npos) e.noise += ".wav";
      return;
    case kSecret:
    case kGoal:
      if (deathmatch(s)) {
        e.gone = true;
        return;
      }
      if (e.noise.empty()) e.noise = "misc/secret.wav";
      (c == kSecret ? s.secrets : s.goals)++;
      return;
    default:
      return;
  }
}

// Each door team's master gathers its members and, for a door that opens by touch, the field.
void link_doors(SequelServer& s) {
  const size_t count = s.entities.size();
  for (size_t i = 0; i < count; i++) {
    SequelEntity& e = s.entities[i];
    if (e.gone || class_of(e) != kDoor || e.team_master >= 0) continue;
    e.team_master = static_cast<int>(i);
    sh::Vec3f lo = abs_min(e), hi = abs_max(e);
    bool field = e.targetname.empty() && e.max_health == 0;
    if (!e.team.empty())
      for (size_t j = i + 1; j < count; j++) {
        SequelEntity& m = s.entities[j];
        if (m.gone || class_of(m) != kDoor || m.team != e.team) continue;
        m.team_master = static_cast<int>(i);
        const sh::Vec3f mlo = abs_min(m), mhi = abs_max(m);
        lo = {std::min(lo.x, mlo.x), std::min(lo.y, mlo.y), std::min(lo.z, mlo.z)};
        hi = {std::max(hi.x, mhi.x), std::max(hi.y, mhi.y), std::max(hi.z, mhi.z)};
      }
    if (!field) continue;
    SequelEntity f;
    f.classname = "door_field";
    f.trigger = true;
    f.mins = lo - sh::Vec3f{60, 60, 0};
    f.maxs = hi + sh::Vec3f{60, 60, 0};
    f.owner = static_cast<int>(i);
    s.entities.push_back(std::move(f));
  }
}

}  // namespace

int map_row(int e) { return 1 + sh::kMaxClients + e; }

int map_entity_of_row(const SequelServer& s, int row) {
  const int e = row - 1 - sh::kMaxClients;
  return e >= 0 && size_t(e) < s.entities.size() ? e : -1;
}

void map_spawn(SequelServer& s) {
  s.delayed.clear();
  s.secrets = s.secrets_found = s.goals = s.goals_found = 0;
  const size_t count = s.entities.size();
  for (size_t i = 0; i < count; i++) {
    SequelEntity& e = s.entities[i];
    e.block = static_cast<int>(i) + 1;  // the world's block is the first
    if (e.gone || e.item >= 0) continue;
    const Class c = class_of(e);
    if (c != kNone) spawn_one(s, s.entities[i], c);
  }
  link_doors(s);
}

void map_use_targets(SequelServer& s, int e, int activator) {
  if (e < 0 || size_t(e) >= s.entities.size()) return;
  const SequelEntity& ent = s.entities[size_t(e)];
  if (ent.delay > 0) {
    s.delayed.push_back({e, s.level_time + double(ent.delay), activator});
    return;
  }
  fire_targets(s, e, activator, 0);
}

void map_frame(SequelServer& s) {
  // The uses whose delay is up.
  for (size_t i = 0; i < s.delayed.size();) {
    if (s.level_time + 0.001 < s.delayed[i].at) {
      i++;
      continue;
    }
    const SequelServer::Delayed d = s.delayed[i];
    s.delayed.erase(s.delayed.begin() + static_cast<long>(i));
    fire_targets(s, d.entity, d.activator, 0);
  }
  // Each mover pushes, then steps; a blocked push skips the step (its schedule slides).
  for (size_t i = 0; i < s.entities.size(); i++) {
    SequelEntity& e = s.entities[i];
    if (e.gone || !e.solid || (e.velocity.x == 0 && e.velocity.y == 0 && e.velocity.z == 0)) {
      if (!e.gone && e.arrive != kNoAct) step_move(s, s.entities[i]);
      continue;
    }
    const int by = push(s, e, eighths(e.velocity * kStep), class_of(e) == kButton);
    if (by >= 0) {
      if (e.think_at > 0) e.think_at += kStep;
      blocked(s, s.entities[i], by);
      continue;
    }
    step_move(s, s.entities[i]);
  }
  for (size_t i = 0; i < s.entities.size(); i++) {
    SequelEntity& e = s.entities[i];
    if (e.think_at > 0 && s.level_time + 0.001 >= e.think_at) think(s, e);
  }
}

void map_touch(SequelServer& s, int slot) {
  const SequelPlayer& p = s.players[slot];
  if (!p.alive) return;
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  const sh::Vec3f lean{1, 1, 1};  // a brush counts as touched within a unit
  for (size_t i = 0; i < s.entities.size(); i++) {
    SequelEntity& e = s.entities[i];
    if (e.gone) continue;
    const bool in_field = e.trigger && overlap(lo, hi, abs_min(e), abs_max(e));
    const bool leaning = !e.trigger && e.solid &&
                         (class_of(e) == kDoor || class_of(e) == kButton) &&
                         overlap(lo - lean, hi + lean, abs_min(e), abs_max(e));
    if (in_field || leaning) touch(s, e, slot);
    if (!s.players[slot].alive) return;
  }
}

bool map_damage(SequelServer& s, int e, int attacker, int damage) {
  if (e < 0 || size_t(e) >= s.entities.size()) return false;
  SequelEntity& d = s.entities[size_t(e)];
  if (d.gone || d.health <= 0 || damage <= 0) return false;
  const Class c = class_of(d);
  if (c != kDoor && c != kButton) return false;
  d.health -= damage;
  if (d.health > 0) return true;
  if (c == kButton) {
    d.health = 0;
    button_press(s, d, attacker);
    return true;
  }
  const int master = d.team_master >= 0 ? d.team_master : e;
  for (SequelEntity& m : s.entities)  // the team's health back, and no more damage until it closes
    if (&m == &d || m.team_master == master) m.health = 0;
  door_use(s, s.entities[size_t(master)], attacker);
  return true;
}

sh::SequelTrace map_trace(const SequelServer& s, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                          sh::Vec3f end, int32_t mask) {
  const auto one = [&](sh::Vec3f o, uint32_t model) {
    const float a[3] = {start.x - o.x, start.y - o.y, start.z - o.z};
    const float b[3] = {end.x - o.x, end.y - o.y, end.z - o.z};
    const float lo[3] = {mins.x, mins.y, mins.z}, hi[3] = {maxs.x, maxs.y, maxs.z};
    return s.tracer->trace(a, b, lo, hi, mask, model);
  };
  const ge::brush::Trace w = one({}, 0);
  sh::SequelTrace r;
  r.all_solid = w.all_solid;
  r.start_solid = w.start_solid;
  r.fraction = w.fraction;
  r.end = {w.end[0], w.end[1], w.end[2]};
  r.normal = {w.plane.n[0], w.plane.n[1], w.plane.n[2]};
  r.surface_flags = w.flags;
  r.contents = w.contents;
  r.entity = w.fraction < 1 || w.start_solid ? 0 : -1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const SequelEntity& e = s.entities[i];
    if (e.gone || !e.solid || e.modelindex <= 0 || !brush(e)) continue;
    // Only when the sweep's box can reach the mover's.
    const sh::Vec3f slo{std::min(start.x, end.x) + mins.x, std::min(start.y, end.y) + mins.y,
                        std::min(start.z, end.z) + mins.z};
    const sh::Vec3f shi{std::max(start.x, end.x) + maxs.x, std::max(start.y, end.y) + maxs.y,
                        std::max(start.z, end.z) + maxs.z};
    if (!overlap(slo - sh::Vec3f{1, 1, 1}, shi + sh::Vec3f{1, 1, 1}, abs_min(e), abs_max(e)))
      continue;
    const ge::brush::Trace t = one(e.origin, uint32_t(e.modelindex));
    if (t.all_solid) r.all_solid = true;
    if (t.start_solid || t.fraction < r.fraction) {
      r.start_solid = r.start_solid || t.start_solid;
      if (t.fraction < r.fraction) {
        r.fraction = t.fraction;
        r.end = sh::Vec3f{t.end[0], t.end[1], t.end[2]} + e.origin;
        r.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
        r.surface_flags = t.flags;
        r.contents = t.contents;
      }
      r.entity = map_row(static_cast<int>(i));
    }
  }
  return r;
}

namespace {

sh::SequelTrace world_trace(void* ctx, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                            sh::Vec3f end) {
  const auto* w = static_cast<const MapWorld*>(ctx);
  sh::SequelTrace t = map_trace(*w->s, start, mins, maxs, end, w->mask);
  if (t.entity < 0) t.entity = 0;  // the world answers every trace, hit or not
  return t;
}

int32_t world_contents(void* ctx, sh::Vec3f p) {
  const float q[3] = {p.x, p.y, p.z};
  return static_cast<const MapWorld*>(ctx)->s->tracer->contents(q);
}

}  // namespace

sh::SequelWorld MapWorld::world() { return {this, world_trace, world_contents}; }

void map_rows(SequelServer& s) {
  RulesWorld& w = s.world;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const SequelEntity& e = s.entities[i];
    const size_t row = size_t(map_row(static_cast<int>(i)));
    if (row >= w.entities.size() || e.item >= 0) continue;
    WorldEntity& r = w.entities[row];
    if (e.solid || e.gone || class_of(e) == kWall) {
      r.free = e.gone;
      r.origin = e.origin;
      r.velocity = e.velocity;
    }
  }
}

}  // namespace shooters_edge::server
