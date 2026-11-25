// Ground Zero's plat that goes either way (`func_plat2`; q2_gz.hpp states the rules, the numbers
// are the clean-room note's, porting/shooters-edge/gz-world.md § 1). The map's movers (q2_map.cpp)
// hand it its spawn, its field's touch, its uses, its arrivals, its thinks and its blocks.
//
// ge:summary Ground Zero's func_plat2: called to either end, toggles, rests at the top, crushes
// ge:keywords q2 ground-zero rogue mission-pack func-plat2 plat2 plat-toggle plat-top box-lift
// ge:keywords called-plat
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_plat_test.cpp
#include "shooters_edge/server/q2_gz.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "brush/map.hpp"
#include "shooter/mover_motion.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_map_kinds.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

using namespace q2map;

constexpr sh::MoverFrame kFrame{0.1f, 10};
constexpr uint32_t kEveryone = 0xffffffffu;
const char* kStart = "plats/pt1_strt.wav";
const char* kEnd = "plats/pt1_end.wav";

// Spawn flags: the low field, stays where it is sent, rests at the top, the box lift's touch.
constexpr int kLowTrigger = 1, kToggle = 2, kTop = 4, kBoxLift = 32;
// `count`'s bits: it was called to an end; it waits for a first use (named).
constexpr int kCalled = 1, kDormant = 2;

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

std::string key(const Q2Server& s, const Q2Entity& e, const char* name) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get_any_case(name);
}
float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
  const std::string v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

int index_of(const Q2Server& s, const Q2Entity& e) {
  return static_cast<int>(&e - s.entities.data());
}

void sound(Q2Server& s, Q2Entity& e, const char* sample) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.time = s.level_time;
  ev.ent = map_row(index_of(s, e));
  ev.level = 2;
  ev.origin = e.origin;
  ev.values[0] = 255;
  ev.values[1] = 3;  // static
  ev.text = sample;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// Its field (the entity it owns), or -1.
int field_of(const Q2Server& s, const Q2Entity& plat) {
  const int me = index_of(s, plat);
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].owner == me && s.entities[i].classname == "plat2_field")
      return static_cast<int>(i);
  return -1;
}

bool at_top(const Q2Entity& e) { return e.state == kAtSecond; }
bool moving(const Q2Entity& e) { return e.state == kGoing || e.state == kComing; }

// The field round the plat as it stands at the top, the travel `travel` and lip `lip` below.
void place_field(const Q2Entity& plat, Q2Entity& f, float travel, float lip) {
  const sh::Vec3f amin = plat.closed + plat.mins, amax = plat.closed + plat.maxs;
  sh::Vec3f lo = amin + sh::Vec3f{25, 25, 0}, hi = amax - sh::Vec3f{25, 25, 0};
  if (hi.x <= lo.x) lo.x = hi.x = (amin.x + amax.x) / 2, hi.x += 1;
  if (hi.y <= lo.y) lo.y = hi.y = (amin.y + amax.y) / 2, hi.y += 1;
  hi.z = amax.z + 8;
  lo.z = hi.z - (travel + lip);
  if ((plat.spawnflags & kLowTrigger) != 0) hi.z = lo.z + 8;
  lo = lo - sh::Vec3f{10, 10, 0};  // grown by 10 on each side
  hi = hi + sh::Vec3f{10, 10, 0};
  f.origin = {};
  f.mins = lo;
  f.maxs = hi;
}

// Off to the other end at once.
void go(Q2Server& s, Q2Entity& e) {
  sound(s, e, kStart);
  e.think_at = 0;
  e.touched_at = s.level_time;  // its last move
  e.arrive = kPlat2Arrive;
  e.moved = true;
  e.split = false;
  if (at_top(e)) {
    e.state = kComing;
    sh::begin_move(e, e.open, kFrame);
  } else {
    e.state = kGoing;
    sh::begin_move(e, e.closed, kFrame);
  }
}

void go_after(Q2Server& s, Q2Entity& e, double seconds) {
  e.think = kPlat2Go;
  e.think_at = s.level_time + seconds;
}

// A body at height `z` operates the plat (a touch of its field, or a use).
void operate(Q2Server& s, Q2Entity& e, const Q2Entity& f, float z) {
  if (moving(e) || e.think == kPlat2Go || s.level_time + 0.001 < e.touched_at + 2) return;
  const float mid = (f.mins.z + f.maxs.z) / 2;
  bool wants_top;
  if (!at_top(e))
    wants_top = z > mid;
  else
    wants_top = (e.spawnflags & kBoxLift) != 0 ? z >= mid : z >= f.maxs.z;
  if (wants_top != at_top(e)) {  // called to the other end
    e.count |= kCalled;
    go_after(s, e, 0.1);
  } else {  // riding it already
    e.count &= ~kCalled;
    go_after(s, e, deathmatch(s) ? 0.3 : 0.5);
  }
}

}  // namespace

void gz_plat2_spawn(Q2Server& s, Q2Entity& e) {
  e.solid = true;
  const float scale = deathmatch(s) ? 2 : 1;  // twice as quick in deathmatch
  e.speed = number(s, e, "speed", 200) * 0.1f * scale;
  e.accel = number(s, e, "accel", 50) * 0.1f * scale;
  e.decel = number(s, e, "decel", 50) * 0.1f * scale;
  e.lip = number(s, e, "lip", 0);
  if (key(s, e, "dmg").empty()) e.dmg = 2;
  e.angles = {};
  e.closed = e.origin;  // the top
  const std::string height = key(s, e, "height");
  const float travel =
      (height.empty() ? e.maxs.z - e.mins.z : std::strtof(height.c_str(), nullptr)) - e.lip;
  e.open = e.origin - sh::Vec3f{0, 0, travel};  // the bottom
  e.count = 0;
  e.touched_at = -100;
  Q2Entity f;
  f.classname = "plat2_field";
  f.owner = index_of(s, e);
  if (!e.targetname.empty()) {  // at the top, idle, until a first use
    e.count = kDormant;
    e.state = kAtSecond;
    place_field(e, f, travel, 0);  // its lip forgotten by then
    f.trigger = false;
  } else {
    place_field(e, f, travel, e.lip);
    f.trigger = true;
    if ((e.spawnflags & kTop) != 0) {
      e.state = kAtSecond;
    } else {
      e.origin = e.open;
      e.state = kAtFirst;
    }
  }
  s.entities.push_back(std::move(f));  // `e` may move: nothing after this reads it
}

void gz_plat2_touch(Q2Server& s, Q2Entity& field, float z) {
  if (field.owner < 0) return;
  operate(s, s.entities[size_t(field.owner)], field, z);
}

void gz_plat2_use(Q2Server& s, Q2Entity& e, int activator) {
  const int f = field_of(s, e);
  if (f < 0) return;
  if ((e.count & kDormant) != 0) {  // its first use: an ordinary plat2, sent down at once
    e.count &= ~kDormant;
    s.entities[size_t(f)].trigger = true;
    go(s, e);
    return;
  }
  // As if its user had touched its field; nobody's use calls it to the other end.
  const Q2Entity& field = s.entities[size_t(f)];
  float z = at_top(e) ? field.mins.z - 1 : field.maxs.z + 1;
  if (activator >= 0 && activator < sh::kMaxClients)
    z = float(s.players[activator].move.origin[2]) * 0.125f;
  operate(s, e, field, z);
}

void gz_plat2_go(Q2Server& s, Q2Entity& e) {
  if (!moving(e)) go(s, e);
}

void gz_plat2_arrived(Q2Server& s, Q2Entity& e) {
  sound(s, e, kEnd);
  e.state = e.state == kGoing ? kAtSecond : kAtFirst;
  const bool top = at_top(e), toggle = (e.spawnflags & kToggle) != 0;
  if ((e.count & kCalled) != 0) {  // called: back to where it came from 5 s on
    e.count &= ~kCalled;
    if (!toggle) go_after(s, e, 5);
    e.touched_at -= deathmatch(s) ? 1 : 2;
  } else if (!toggle && top != ((e.spawnflags & kTop) != 0)) {  // off the end it rests at
    go_after(s, e, 2);
  }
  map_use_targets(s, index_of(s, e), e.activator);  // its targets at either end
}

void gz_plat2_blocked(Q2Server& s, Q2Entity& e, int slot) {
  // A dead body is gibbed first; then the blocker takes `dmg` and the plat turns back.
  if (slot >= 0) {
    if (!s.players[slot].alive) fight_hurt(s, slot, -1, {}, 100, 1, kSqByCrush);
    fight_hurt(s, slot, -1, {}, e.dmg, 1, kSqByCrush);
  } else {
    monster_damage(s, -2 - slot, -1, e.dmg, {});
  }
  if (e.state == kGoing) {
    e.state = kAtSecond;
    go(s, e);
  } else if (e.state == kComing) {
    e.state = kAtFirst;
    go(s, e);
  }
}

}  // namespace shooters_edge::server
