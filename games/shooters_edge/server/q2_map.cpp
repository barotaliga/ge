// Quake II's movers and triggers (q2_map.hpp states the rules; the numbers are the
// clean-room note's, porting/shooters-edge/q2-movers.md).
//
// ge:summary Quake II's movers and triggers: the moves, the push, doors, plats, triggers, uses
// ge:keywords q2 func-door func-button func-plat func-train trigger-multiple trigger-push
// ge:keywords trigger-hurt teleporter use-targets plain-move accelerated-move mover-push crush
// ge:skill shooter
// ge:test tests/shooters_edge_q2_map_test.cpp
#include "shooters_edge/server/q2_map.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "shooter/mover_motion.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map_kinds.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_targets.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr float kStep = 0.1f;
constexpr sh::MoverFrame kFrame{kStep, 10};
constexpr uint32_t kEveryone = 0xffffffffu;
constexpr int kMaxUseDepth = 16;  // a chain of uses deeper than this is a loop: it stops

using namespace q2map;

const char* kDoorStart = "doors/dr1_strt.wav";
const char* kDoorEnd = "doors/dr1_end.wav";
const char* kPlatStart = "plats/pt1_strt.wav";
const char* kPlatEnd = "plats/pt1_end.wav";
const char* kDoorMove = "doors/dr1_mid.wav";  // the loops while they move
const char* kPlatMove = "plats/pt1_mid.wav";
const char* kPadHum = "world/amb10.wav";    // a teleporter's pad
const char* kTeleSound = "misc/tele1.wav";  // its burst, at the pad and at the exit
const char* kButtonSound = "switches/butn2.wav";
constexpr float kStatic =
    3;  // a mover's start and stop: heard only near it (silent past ~410 units)
const char* kTalk = "misc/talk1.wav";
const char* kWindfly = "misc/windfly.wav";
const char* kElectro = "world/electro.wav";

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

const std::string& key(const Q2Server& s, const Q2Entity& e, const char* name) {
  static const std::string kEmpty;
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return kEmpty;
  return s.map->entities[size_t(e.block)].get_any_case(name);
}

float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
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

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound_at(Q2Server& s, int ent, sh::Vec3f at, const std::string& sample, float volume = 1,
              float attenuation = 1, int channel = 2) {
  if (sample.empty()) return;
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = channel;  // the voice by default
  ev.origin = at;
  ev.values[0] = volume * 255.0f;
  ev.values[1] = attenuation;
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

void effect(Q2Server& s, const char* name, sh::Vec3f at) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = at;
  ev.text = name;
  event(s, std::move(ev));
}

void print_all(Q2Server& s, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = kEveryone;
  ev.reliable = true;
  ev.level = 2;
  ev.text = std::move(text);
  event(s, std::move(ev));
}

std::string name_of(const Q2Server& s, int slot) {
  if (slot < 0 || slot >= sh::kMaxClients) return "someone";
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return "someone";
}

Class class_of(std::string_view c) { return class_of_name(c); }

Class class_of(const Q2Entity& e) { return class_of(e.classname); }

bool is_door(Class c) { return c == kDoor || c == kDoorRotating; }
bool is_ship(const Q2Entity& e) {
  return e.classname == "misc_viper" || e.classname == "misc_strogg_ship" ||
         e.classname == "misc_crashviper" || e.classname == "misc_transport";
}
// The Reckoning's transport: a ship that sets off at once, unseen until its first use.
bool is_transport(const Q2Entity& e) { return e.classname == "misc_transport"; }

int index_of(const Q2Server& s, const Q2Entity& e);

// A door's start or end sample; null when it moves quietly. A door of water sounds of water,
// and only when it has `sounds`. Only a team's master sounds.
const char* door_sample(const Q2Server& s, const Q2Entity& e, bool start) {
  if (e.team_master >= 0 && e.team_master != index_of(s, e)) return nullptr;
  if (e.classname == "func_water") {
    if (e.sounds == 0) return nullptr;
    return start ? "world/mov_watr.wav" : "world/stp_watr.wav";
  }
  if (e.sounds == 1) return nullptr;
  return start ? kDoorStart : kDoorEnd;
}

bool brush(const Q2Entity& e) { return !e.model.empty() && e.model[0] == '*'; }

// A turning mover's box is the cube round the sphere its corners sweep.
float reach(const Q2Entity& e) {
  const auto far = [](float a, float b) { return std::max(a < 0 ? -a : a, b < 0 ? -b : b); };
  const float x = far(e.mins.x, e.maxs.x), y = far(e.mins.y, e.maxs.y), z = far(e.mins.z, e.maxs.z);
  return sh::root_of(x * x + y * y + z * z);
}
sh::Vec3f abs_min(const Q2Entity& e) {
  if (!e.turns) return e.origin + e.mins;
  const float r = reach(e);
  return e.origin - sh::Vec3f{r, r, r};
}
sh::Vec3f abs_max(const Q2Entity& e) {
  if (!e.turns) return e.origin + e.maxs;
  const float r = reach(e);
  return e.origin + sh::Vec3f{r, r, r};
}

// A point `d` from a mover's origin, in the mover's own frame at `angles` (forward, left, up), and
// back.
sh::Vec3f to_local(sh::Vec3f d, sh::Vec3f angles) {
  const sh::ViewAxes a = sh::angle_vectors(angles);
  return {sh::dot(d, a.forward), -sh::dot(d, a.right), sh::dot(d, a.up)};
}
sh::Vec3f to_world(sh::Vec3f l, sh::Vec3f angles) {
  const sh::ViewAxes a = sh::angle_vectors(angles);
  return a.forward * l.x - a.right * l.y + a.up * l.z;
}

bool overlap(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y && alo.z < bhi.z &&
         ahi.z > blo.z;
}

// The way of an entity's `angle` (−1 up, −2 down) or `angles`: their forward axis.
sh::Vec3f way_of(const Q2Server& s, const Q2Entity& e) {
  const std::string& angle = key(s, e, "angle");
  if (!angle.empty()) {
    const float a = std::strtof(angle.c_str(), nullptr);
    if (a == -1) return {0, 0, 1};
    if (a == -2) return {0, 0, -1};
  }
  if (e.angles.x == 0 && e.angles.y == 0 && e.angles.z == 0) return {1, 0, 0};
  return sh::angle_vectors({e.angles.x, e.angles.y, 0}).forward;
}

// Whether a trigger has a way at all: an angle other than 0, or angles. With none, a push stops
// the toucher and a trigger_multiple asks no facing.
bool has_angle(const Q2Server& s, const Q2Entity& e) {
  return std::strtof(key(s, e, "angle").c_str(), nullptr) != 0 || e.angles.x != 0 ||
         e.angles.y != 0 || e.angles.z != 0;
}

// Whether a player's box at `at` fits (in no solid of the map or a mover).
bool fits(const Q2Server& s, const Q2Player& p, sh::Vec3f at) {
  const ge::classic::Q2Trace t =
      map_trace(s, at, p.mins, p.maxs, at, p.alive ? sh::kSqMaskPlayerSolid : sh::kSqMaskDeadSolid);
  return !t.start_solid && !t.all_solid;
}
// A monster's box (or a corpse's) at `at`, clear of the world, the movers and the other monsters.
bool fits_body(const Q2Server& s, const Q2Entity& b, int entity, sh::Vec3f at) {
  constexpr int32_t kBodyMask = sh::kSqSolid | sh::kSqWindow | sh::kSqMonster | 0x20000;
  const ge::classic::Q2Trace t = map_trace(s, at, b.mins, b.maxs, at, kBodyMask, entity);
  return !t.start_solid && !t.all_solid;
}

int index_of(const Q2Server& s, const Q2Entity& e) {
  return static_cast<int>(&e - s.entities.data());
}

// ---- The moves ----

// Sets mover `e` going to `dest`; `arrive` runs when it gets there (q2-movers.md § The plain
// move, § The accelerated move).
void begin_move(Q2Entity& e, sh::Vec3f dest, int arrive) {
  e.arrive = arrive;
  e.moved = true;
  e.split = false;
  sh::begin_move(e, dest, kFrame);
}

void arrived(Q2Server& s, Q2Entity& e);

// Sets turning mover `e` turning to `dest` (q2-movers.md § The angular plain move).
void begin_turn(Q2Entity& e, sh::Vec3f dest, int arrive) {
  e.arrive = arrive;
  e.moved = true;
  e.split = false;
  sh::begin_turn(e, dest, kFrame);
}

// A mover's step after a frame's push: the shooter plugin's motion (shooter/mover_motion.hpp).
void step_move(Q2Server& s, Q2Entity& e) {
  if (e.arrive == kNoAct) return;
  if (sh::step_motion(e, e.turns, kFrame) == sh::MotionStep::arrived) arrived(s, e);
}

// ---- The push ----

// Whether a box at `at` is inside mover `e`'s own brushes where it now stands (its box alone is far
// larger than a turned or an L-shaped mover: a thing beside a fan is not in the fan).
bool in_mover(const Q2Server& s, const Q2Entity& e, sh::Vec3f at, sh::Vec3f mins, sh::Vec3f maxs) {
  if (!brush(e) || e.modelindex <= 0) return true;  // a model's box is all it has
  sh::Vec3f p = at - e.origin;
  if (e.angles.x != 0 || e.angles.y != 0 || e.angles.z != 0) p = to_local(p, e.angles);
  const float a[3] = {p.x, p.y, p.z};
  const float lo[3] = {mins.x, mins.y, mins.z}, hi[3] = {maxs.x, maxs.y, maxs.z};
  const ge::brush::Trace t =
      s.tracer->trace(a, a, lo, hi, sh::kSqSolid | sh::kSqWindow, uint32_t(e.modelindex));
  return t.start_solid || t.all_solid;
}

// What is not alive in mover `e`'s way after its push by `move` (from `from`, `from_angles`): the
// items, the dropped items, the bodies and the pieces lying on its top ride it, and those its box
// now holds are pushed along; one with no room there is crushed and blows apart.
void push_things(Q2Server& s, const Q2Entity& e, sh::Vec3f move, sh::Vec3f from,
                 sh::Vec3f from_angles) {
  const sh::Vec3f lo = abs_min(e), hi = abs_max(e);
  const float top = hi.z - move.z;  // the mover's top before the push
  const bool turning =
      e.angles.x != from_angles.x || e.angles.y != from_angles.y || e.angles.z != from_angles.z;
  const auto shove = [&](sh::Vec3f& at, sh::Vec3f mins, sh::Vec3f maxs) {
    const sh::Vec3f blo = at + mins, bhi = at + maxs;
    const bool rider =
        std::abs(blo.z - top) <= 1 && blo.x < hi.x && bhi.x > lo.x && blo.y < hi.y && bhi.y > lo.y;
    if (!rider && (!overlap(blo, bhi, lo, hi) || !in_mover(s, e, at, mins, maxs))) return true;
    const sh::Vec3f to =
        turning ? e.origin + to_world(to_local(at - from, from_angles), e.angles) : at + move;
    const sh::Vec3f probe = to + sh::Vec3f{0, 0, 0.25f};  // a rider rests on the top it touches
    const ge::classic::Q2Trace t =
        map_trace(s, probe, mins, maxs, probe, sh::kSqSolid | sh::kSqWindow, -1);
    if (t.start_solid || t.all_solid) {
      effect(s, "te_explosion1", at);
      return false;
    }
    at = to;
    return true;
  };
  const sh::Vec3f item_lo{-15, -15, -15}, item_hi{15, 15, 15};
  for (Q2Entity& it : s.entities)
    if (it.item >= 0 && !it.gone && !it.hidden && !shove(it.origin, item_lo, item_hi))
      it.gone = true;
  for (Q2Drop& d : s.drops)
    if (d.live && !shove(d.origin, item_lo, item_hi)) d.live = false;
  for (Q2Body& b : s.bodies)
    if (b.live && !shove(b.origin, {-16, -16, -24}, {16, 16, -8})) b.live = false;
  for (Q2Gib& g : s.gibs)
    if (g.live && g.resting && !shove(g.origin, {}, {})) g.live = false;
}

// Pushes mover `e` by `move`: its riders and the players and monsters (and corpses) in its way
// move with it. Returns the slot that blocked it (the push undone), a monster that did as -2 - its
// entity, or -1.
int push(Q2Server& s, Q2Entity& e, sh::Vec3f move, sh::Vec3f turn, bool stop_type) {
  const sh::Vec3f from = e.origin, from_angles = e.angles;
  const bool turning = turn.x != 0 || turn.y != 0 || turn.z != 0;
  e.origin = from + move;
  e.angles = from_angles + turn;
  const sh::Vec3f lo = abs_min(e), hi = abs_max(e);
  const int row = map_row(index_of(s, e));
  int16_t before[sh::kMaxClients][3];
  bool moved[sh::kMaxClients] = {}, turned[sh::kMaxClients] = {};
  int16_t was_yaw[sh::kMaxClients] = {};
  const float yaw = turn.y;  // a rider's yaw delta gains the turn's degrees as its own units
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active || s.clients[i].spectator) continue;
    Q2Player& p = s.players[i];
    const sh::Vec3f at = unpack(p.move.origin);
    const bool rider = p.ground == row;
    if (!rider && !overlap(at + p.mins, at + p.maxs, lo, hi)) continue;
    const bool fit_after = !stop_type || rider;
    sh::Vec3f to = at + move;
    if (turning)  // carried round the origin
      to = e.origin + to_world(to_local(at - from, from_angles), e.angles);
    if (fit_after && fits(s, p, to)) {
      for (int k = 0; k < 3; k++) before[i][k] = p.move.origin[k];
      pack(p.move.origin, to);
      moved[i] = true;
      if (!rider) p.ground = -1;
      if (turn.y != 0) {  // barely turns: degrees counted as 65536ths of a turn, truncated
        was_yaw[i] = p.move.delta_angles[1];
        const float sum = float(p.move.delta_angles[1]) + yaw;
        p.move.delta_angles[1] = static_cast<int16_t>(static_cast<int>(sum));
        p.view_angles.y += float(p.move.delta_angles[1] - was_yaw[i]) * (360.0f / 65536);
        turned[i] = true;
      }
      continue;
    }
    if (fits(s, p, at)) continue;              // fits where he was: left there
    for (int j = 0; j < sh::kMaxClients; j++)  // blocked: everything back
      if (moved[j])
        for (int k = 0; k < 3; k++) s.players[j].move.origin[k] = before[j][k];
    for (int j = 0; j < sh::kMaxClients; j++)
      if (turned[j]) {
        s.players[j].view_angles.y -=
            float(s.players[j].move.delta_angles[1] - was_yaw[j]) * (360.0f / 65536);
        s.players[j].move.delta_angles[1] = was_yaw[j];
      }
    e.origin = from;
    e.angles = from_angles;
    return i;
  }
  // The monsters and corpses: a rider standing on its top, or a box in its way.
  std::vector<std::pair<int, sh::Vec3f>> carried;
  for (const Q2Monster& m : s.monsters) {
    Q2Entity& b = s.entities[size_t(m.entity)];
    if (b.gone || !b.box) continue;
    const sh::Vec3f blo = b.origin + b.mins, bhi = b.origin + b.maxs;
    const float top = hi.z - move.z;  // the mover's top before the push
    const bool rider = m.on_ground && std::abs(blo.z - top) <= 1 && blo.x < hi.x && bhi.x > lo.x &&
                       blo.y < hi.y && bhi.y > lo.y;
    if (!rider && !overlap(blo, bhi, lo, hi)) continue;
    sh::Vec3f to = b.origin + move;
    if (turning) to = e.origin + to_world(to_local(b.origin - from, from_angles), e.angles);
    if ((!stop_type || rider) && fits_body(s, b, m.entity, to)) {
      carried.emplace_back(m.entity, b.origin);
      b.origin = to;  // swung round, never turned
      continue;
    }
    if (fits_body(s, b, m.entity, b.origin)) continue;  // fits where it was: left there
    for (const auto& [n, was] : carried) s.entities[size_t(n)].origin = was;
    for (int j = 0; j < sh::kMaxClients; j++)  // blocked: everything back
      if (moved[j])
        for (int k = 0; k < 3; k++) s.players[j].move.origin[k] = before[j][k];
    for (int j = 0; j < sh::kMaxClients; j++)
      if (turned[j]) {
        s.players[j].view_angles.y -=
            float(s.players[j].move.delta_angles[1] - was_yaw[j]) * (360.0f / 65536);
        s.players[j].move.delta_angles[1] = was_yaw[j];
      }
    e.origin = from;
    e.angles = from_angles;
    return -2 - m.entity;
  }
  for (int i = 0; i < sh::kMaxClients; i++)
    if (moved[i])
      s.players[i].pushed =
          s.players[i].pushed + (unpack(s.players[i].move.origin) - unpack(before[i]));
  push_things(s, e, move, from, from_angles);
  return -1;
}

void door_go_up(Q2Server& s, Q2Entity& e, int activator);
void door_go_down(Q2Server& s, Q2Entity& e);
void plat_go_up(Q2Server& s, Q2Entity& e);
void plat_go_down(Q2Server& s, Q2Entity& e);
// Entity `target` used by `activator` (a slot, or -1), `other` the entity whose targets it is
// among (-1: none), `depth` uses deep.
void use(Q2Server& s, int target, int activator, int depth, int other = -1);

// What a mover does when a player (or a monster: -2 - its entity) blocks it: crush, then a door
// or plat turns back.
void blocked(Q2Server& s, Q2Entity& e, int slot) {
  const Class c = class_of(e);
  const auto crush = [&] {
    if (slot >= 0)
      fight_hurt(s, slot, -1, {}, e.dmg, 1, kSqByCrush);
    else
      monster_damage(s, -2 - slot, -1, e.dmg, {});
  };
  if (e.classname == "func_water") return;  // never blocked: it waits where it is
  if (c == kRotating) {                     // crushes every blocked frame and never turns back
    if (e.dmg > 0) crush();
    return;
  }
  if (c == kTrain || c == kSecretDoor) {  // harms twice a second at most, and keeps on
    if (e.dmg > 0 && s.level_time + 0.001 >= e.touched_at + 0.5) {
      e.touched_at = s.level_time;
      crush();
    }
    return;
  }
  crush();
  if (c == kPlat) {
    if (e.state == kGoing)
      plat_go_down(s, e);
    else if (e.state == kComing)
      plat_go_up(s, e);
    return;
  }
  if (!is_door(c) || e.wait < 0 || (e.spawnflags & 4) != 0) return;  // a crusher keeps pressing
  const bool up = e.state == kComing;  // the blocked door's way turns the whole team back
  for (Q2Entity& m : s.entities)
    if (&m == &e || (e.team_master >= 0 && m.team_master == e.team_master)) {
      if (up)
        door_go_up(s, m, kNoActivator);  // a door keeps no user for the way back
      else
        door_go_down(s, m);
    }
}

// ---- Doors, buttons, plats, trains ----

// The area portals door `e`'s target names, opened or shut.
void door_portals(Q2Server& s, const Q2Entity& e, bool open) {
  if (e.target.empty()) return;
  for (const Q2Entity& p : s.entities) {
    if (p.targetname != e.target || class_of(p) != kAreaPortal) continue;
    const int n = p.state;
    if (n >= 0 && size_t(n) < s.portals.size()) s.portals[size_t(n)] = open ? 1 : 0;
  }
}

void door_go_up(Q2Server& s, Q2Entity& e, int activator) {
  if (e.state == kGoing) return;
  if (e.state == kAtSecond) {  // open: the close put off
    if (e.wait >= 0) e.think_at = s.level_time + e.wait;
    return;
  }
  e.activator = activator == kNoActivator ? -1 : activator;
  if (door_sample(s, e, true) != nullptr)
    sound_at(s, map_row(index_of(s, e)), e.origin, door_sample(s, e, true), 1, kStatic);
  e.state = kGoing;
  e.think_at = 0;
  if (e.turns)
    begin_turn(e, e.open_angles, kDoorTop);
  else
    begin_move(e, e.open, kDoorTop);
  door_portals(s, e, true);
  map_use_targets(s, index_of(s, e), activator);
}

void door_go_down(Q2Server& s, Q2Entity& e) {
  if (e.max_health > 0) e.health = e.max_health;  // it can be shot again
  if (door_sample(s, e, true) != nullptr)
    sound_at(s, map_row(index_of(s, e)), e.origin, door_sample(s, e, true), 1, kStatic);
  e.state = kComing;
  e.think_at = 0;
  if (e.turns)
    begin_turn(e, e.closed_angles, kDoorBottom);
  else
    begin_move(e, e.closed, kDoorBottom);
}

// A use of a door: the team opens (or, a toggle, closes when open or opening).
void door_use(Q2Server& s, Q2Entity& e, int activator) {
  const int master = e.team_master >= 0 ? e.team_master : index_of(s, e);
  if (index_of(s, e) != master) return;  // a use on a slave does nothing
  const bool toggle = (e.spawnflags & 32) != 0;
  const bool close = toggle && (e.state == kGoing || e.state == kAtSecond);
  for (Q2Entity& m : s.entities) {
    if (index_of(s, m) != master && m.team_master != master) continue;
    // Once used, a door's message and touch are gone for good.
    m.message.clear();
    if (close)
      door_go_down(s, m);
    else
      door_go_up(s, m, activator);
  }
}

void button_press(Q2Server& s, Q2Entity& e, int activator) {
  if (e.state == kGoing || e.state == kAtSecond) return;
  e.activator = activator;
  if (e.sounds != 1) sound_at(s, map_row(index_of(s, e)), e.origin, kButtonSound, 1, kStatic);
  e.state = kGoing;
  begin_move(e, e.open, kButtonIn);
}

// A secret door used (or shot) at rest: its portals open and it slides aside, silently.
void secret_use(Q2Server& s, Q2Entity& e) {
  if (e.origin.x != e.closed.x || e.origin.y != e.closed.y || e.origin.z != e.closed.z) return;
  e.on = false;  // not shootable until it is home
  door_portals(s, e, true);
  begin_move(e, e.open, kSecretAside);
}

void plat_go_up(Q2Server& s, Q2Entity& e) {
  sound_at(s, map_row(index_of(s, e)), e.origin, kPlatStart, 1, kStatic);
  e.state = kGoing;
  e.think_at = 0;
  begin_move(e, e.closed, kPlatTop);
}

void plat_go_down(Q2Server& s, Q2Entity& e) {
  sound_at(s, map_row(index_of(s, e)), e.origin, kPlatStart, 1, kStatic);
  e.state = kComing;
  e.think_at = 0;
  e.moved = true;
  begin_move(e, e.open, kPlatBottom);
}

// The corner named `name` (one of several sharing it, picked at random), or -1.
int corner_named(Q2Server& s, const std::string& name) {
  if (name.empty()) return -1;
  int first = -1, count = 0;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].targetname == name && s.entities[i].classname == "path_corner") {
      if (count++ == 0) first = static_cast<int>(i);
    }
  if (count <= 1) return first;
  size_t pick = ge::core::pick_index(s.rng, size_t(count));
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].targetname == name && s.entities[i].classname == "path_corner" && pick-- == 0)
      return static_cast<int>(i);
  return first;
}

// A train heads for the corner its target names: a teleport corner is jumped to at once.
void train_next(Q2Server& s, Q2Entity& e) {
  for (int hops = 0; hops < 2; hops++) {
    const int c = corner_named(s, e.target);
    if (c < 0) {
      q2_pick_missing(s, e.target, "train_next: bad target " + e.target);  // none: the path's end
      e.velocity = {};
      e.arrive = kNoAct;
      return;
    }
    Q2Entity& corner = s.entities[size_t(c)];
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
  const Q2Entity& second = s.entities[size_t(e.owner)];  // two teleport corners in a row: it stops
  s.console.push_back("connected teleport path_corners, see " + second.classname + " at " +
                      q2_pos(second.origin));
  e.velocity = {};
  e.arrive = kNoAct;
}

void arrived(Q2Server& s, Q2Entity& e) {
  const int act = e.arrive;
  e.arrive = kNoAct;
  const int row = map_row(index_of(s, e));
  switch (act) {
    case kDoorTop:
      if (door_sample(s, e, false) != nullptr)
        sound_at(s, row, e.origin, door_sample(s, e, false), 1, kStatic);
      e.state = kAtSecond;
      if ((e.spawnflags & 32) == 0 && e.wait >= 0) {
        e.think_at = s.level_time + e.wait;
        e.think = kDoorClose;
      }
      return;
    case kDoorBottom:
      if (door_sample(s, e, false) != nullptr)
        sound_at(s, row, e.origin, door_sample(s, e, false), 1, kStatic);
      e.state = kAtFirst;
      door_portals(s, e, false);
      return;
    case kSecretAside:  // a second's pause beside, then back along its arrow
    case kSecretBeside:
      e.think_at = s.level_time + 1;
      e.think = act == kSecretAside ? kSecretOut : kSecretHome;
      return;
    case kSecretOpen:  // open: for good at wait -1, else for its wait
      if (e.wait >= 0) {
        e.think_at = s.level_time + e.wait;
        e.think = kSecretIn;
      }
      return;
    case kSecretShut:  // home: shootable again when it may be, the portals shut
      e.on = e.targetname.empty() || (e.spawnflags & 1) != 0;
      door_portals(s, e, false);
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
      sound_at(s, row, e.origin, kPlatEnd, 1, kStatic);
      e.state = kAtSecond;
      e.think_at = s.level_time + 3;
      e.think = kPlatDown;
      return;
    case kPlatBottom:
      sound_at(s, row, e.origin, kPlatEnd, 1, kStatic);
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
        if ((e.spawnflags & 2) != 0) {  // a toggle train: on to its next corner's aim, stopped
          train_next(s, e);
          e.velocity = {};
          e.arrive = kNoAct;
          e.spawnflags &= ~1;
        }
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

void think(Q2Server& s, Q2Entity& e) {
  const int act = e.think;
  e.think_at = 0;
  e.think = kNoAct;
  switch (act) {
    case kDoorClose:
      for (Q2Entity& m : s.entities)
        if (&m == &e || (e.team_master >= 0 && m.team_master == e.team_master &&
                         index_of(s, e) == e.team_master))
          if (m.state == kAtSecond) door_go_down(s, m);
      return;
    case kSecretOut:
      begin_move(e, e.open_angles, kSecretOpen);
      return;
    case kSecretIn:
      begin_move(e, e.open, kSecretBeside);
      return;
    case kSecretHome:
      begin_move(e, e.closed, kSecretShut);
      return;
    case kButtonReturn:
      e.state = kComing;
      begin_move(e, e.closed, kButtonOut);
      return;
    case kPlatDown:
      if (e.state == kAtSecond) plat_go_down(s, e);
      return;
    case kPlatUp:
      if (e.state == kAtFirst) plat_go_up(s, e);
      return;
    case kTrainFind: {
      const int c = corner_named(s, e.target);
      if (c < 0) {
        q2_pick_missing(s, e.target, "train_find: target " + e.target + " not found");
        return;
      }
      Q2Entity& corner = s.entities[size_t(c)];
      e.origin = corner.origin - e.mins;
      e.target = corner.target;
      e.owner = c;
      if ((!is_ship(e) || is_transport(e)) && ((e.spawnflags & 1) != 0 || e.targetname.empty())) {
        e.spawnflags |= 1;  // running
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
    case kCrossCheck:  // once: every one of its bits set, its targets fire and it goes
      if ((uint32_t(e.spawnflags) & 255u & ~s.cross_bits) != 0) return;
      map_use_targets(s, index_of(s, e), -1);
      e.think_at = s.level_time + double(e.delay) + kStep;
      e.think = kRemove;
      return;
    default:
      return;
  }
}

// ---- Triggers ----

// A trigger fires: its targets used, then dead for `wait` (a trigger_once for good).
void trigger_fire(Q2Server& s, Q2Entity& e, int activator) {
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

void place_player(Q2Server& s, int slot, sh::Vec3f origin, sh::Vec3f angles) {
  Q2Player& p = s.players[slot];
  pack(p.move.origin, origin + sh::Vec3f{0, 0, 10});
  for (int16_t& v : p.move.velocity) v = 0;
  p.move.flags |= ge::classic::kSqTimeTeleport;
  p.move.time = 20;  // 160 ms
  const int16_t want[3] = {
      static_cast<int16_t>(num::to_int32(double(angles.x) * (65536.0 / 360.0))),
      static_cast<int16_t>(num::to_int32(double(angles.y) * (65536.0 / 360.0))), 0};
  for (int k = 0; k < 3; k++) {
    p.move.delta_angles[k] = int16_t(want[k] - p.cmd_angles[k]);
    p.forced_angles[k] = want[k];
  }
  p.forced = true;
  p.changed = true;
  p.view_angles = {angles.x, angles.y, 0};
  p.ground = -1;
  targets_telefrag(s, slot);  // whoever stands there dies, monsters too
}

void teleport(Q2Server& s, Q2Entity& pad, int slot) {
  int dest = -1;
  for (size_t i = 0; i < s.entities.size() && dest < 0; i++)
    if (!pad.target.empty() && s.entities[i].targetname == pad.target) dest = static_cast<int>(i);
  if (dest < 0) return s.console.push_back("Couldn't find destination");  // each touch
  const Q2Entity& d = s.entities[size_t(dest)];
  effect(s, "te_teleport", pad.origin);
  effect(s, "te_teleport", d.origin);
  sound_at(s, map_row(index_of(s, pad)), pad.origin, kTeleSound, 1, 2, 1);  // idle, the weapon's
  place_player(s, slot, d.origin, d.angles);
  const Q2Player& p = s.players[slot];
  sound_at(s, slot + 1, unpack(p.move.origin), kTeleSound, 1, 2, 1);
}

void touch(Q2Server& s, Q2Entity& e, int slot) {
  Q2Player& p = s.players[slot];
  switch (class_of(e)) {
    case kTriggerOnce:
    case kTriggerMultiple:
      if ((e.spawnflags & 2) != 0) return;  // not by players
      if (e.dir.x == 0.0f && e.dir.y == 0.0f && e.dir.z == 0.0f) {
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
      Q2Entity& plat = s.entities[size_t(e.owner)];
      if (plat.state == kAtFirst) {
        // In coop the plat waits `coop_elevator_delay` from the first step for the others.
        const double wait = !deathmatch(s) && s.cvars.find("coop")->on()
                                ? s.cvars.find("coop_elevator_delay")->number
                                : 0;
        if (wait <= 0) {
          plat_go_up(s, plat);
        } else if (plat.think != kPlatUp || plat.think_at <= 0) {
          plat.think = kPlatUp;
          plat.think_at = s.level_time + wait;
        }
      } else if (plat.state == kAtSecond)
        plat.think_at = s.level_time + 1;  // held up while someone stands there
      return;
    }
    case kRotating:  // touch pain while it spins
      if ((e.spawnflags & 16) == 0 || e.dmg <= 0) return;
      if (e.avelocity.x == 0 && e.avelocity.y == 0 && e.avelocity.z == 0) return;
      fight_hurt(s, slot, -1, {}, e.dmg, 1, kSqByCrush);
      return;
    case kDoor:  // leaning on a named door with a message (shown until it is used)
    case kDoorRotating:
    case kSecretDoor:
      if (e.message.empty() || e.targetname.empty()) return;
      if (class_of(e) == kDoor && e.max_health > 0) return;
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
void fire_targets(Q2Server& s, int ent, int activator, int depth, bool late = false);

// A target_explosion goes off: the flash, its blast (`dmg`) and its targets at once.
void explosion(Q2Server& s, int i, int activator, int depth) {
  const Q2Entity& e = s.entities[size_t(i)];
  effect(s, "te_explosion", e.origin);
  fight_blast(s, e.origin, activator, e.dmg, float(e.dmg) + 40, kSqByExplosion);
  if (depth < kMaxUseDepth) fire_targets(s, i, activator, depth + 1);
}

void use(Q2Server& s, int target, int activator, int depth, int other) {
  Q2Entity& e = s.entities[size_t(target)];
  if (e.gone && class_of(e) != kWall) return;
  switch (class_of(e)) {
    case kDoor:
    case kDoorRotating:
      door_use(s, e, activator);
      return;
    case kButton:
      button_press(s, e, activator);
      return;
    case kSecretDoor:
      secret_use(s, e);
      return;
    case kRotating:  // toggles: stops at once, or starts at full speed
      if (e.avelocity.x != 0 || e.avelocity.y != 0 || e.avelocity.z != 0)
        e.avelocity = {};
      else
        e.avelocity = e.axis * e.speed;
      return;
    case kAreaPortal:
      e.on = !e.on;  // its own bit toggles and sets the portal (a door sets it apart from this)
      if (e.state >= 0 && size_t(e.state) < s.portals.size()) s.portals[size_t(e.state)] = e.on;
      return;
    case kLight:
      e.spawnflags ^= 1;  // bit 1: off
      s.world.lightstyles[e.state] = (e.spawnflags & 1) != 0 ? "a" : "m";
      return;
    case kPlat:
      if (!e.moved && e.state == kGoing && e.arrive == kNoAct) plat_go_down(s, e);
      return;
    case kTrain:
      if (e.hidden && is_transport(e)) {  // already on its way: it shows itself where it is
        e.hidden = false;
        return;
      }
      if (e.hidden) {  // a ship shows itself and sets off
        e.hidden = false;
        e.spawnflags |= 1;
        e.activator = activator;
        train_next(s, e);
        return;
      }
      // Once running (START_ON, 1) only a toggle train (2) hears a use: it stops where it is and
      // can be started again. Another, stopped at a wait -1 corner, never moves again.
      if ((e.spawnflags & 1) != 0) {
        if ((e.spawnflags & 2) != 0) {
          e.spawnflags &= ~1;
          e.velocity = {};
          e.arrive = kNoAct;
          e.think_at = 0;
          e.think = kNoAct;
          e.state = kAtFirst;
        }
        return;
      }
      e.spawnflags |= 1;
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
      if (appear) targets_kill_inside(s, e);            // whoever is inside dies, monsters too
      if ((e.spawnflags & 2) == 0) e.spawnflags &= ~7;  // one use, then a plain wall
      return;
    }
    case kElevator: {
      // Its train is sent to the corner the caller's pathtarget names (a floor's button), unless
      // the train is on its way or waiting to be: straight there, without a word, its next
      // corner and its wait as they were.
      int train = -1;
      for (size_t t = 0; t < s.entities.size() && train < 0; t++)
        if (s.entities[t].targetname == e.target && class_of(s.entities[t]) == kTrain)
          train = static_cast<int>(t);
      if (train < 0) return;  // said once, a frame after the load (q2_lines.cpp)
      Q2Entity& t = s.entities[size_t(train)];
      if (t.think_at > 0 || t.arrive != kNoAct) return;  // busy
      const std::string floor =
          other >= 0 ? key(s, s.entities[size_t(other)], "pathtarget") : std::string();
      const int c = corner_named(s, floor);
      if (c < 0) {
        if (!floor.empty()) q2_pick_missing(s, floor);
        s.console.push_back(floor.empty() ? std::string("elevator used with no pathtarget")
                                          : "elevator used with bad pathtarget: " + floor);
        return;
      }
      const Q2Entity& corner = s.entities[size_t(c)];
      t.owner = c;
      t.pathtarget = key(s, corner, "pathtarget");
      t.spawnflags |= 1;
      t.state = kGoing;
      begin_move(t, corner.origin - t.mins, kTrainCorner);
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
      if (s.intermission_at > 0 || !s.pending_map.empty() || s.game_over) return;
      std::string to = e.map;
      if (s.map_name == "fact1" && to == "fact3") to = "fact3$secret1";  // the secret exit's spot
      const LevelString next = level_string(to);
      s.next_spawn = next.spawn;
      s.next_unit = next.unit;
      if (s.next_unit) s.cross_bits = 0;  // a new unit
      if (deathmatch(s)) {
        if (!fight_dmflag(s, 4096)) {  // no exit in this match: it kills its toucher
          if (activator >= 0 && activator < sh::kMaxClients)
            fight_hurt(s, activator, -1, {}, 10 * s.players[activator].body.max_health, 1000,
                       kSqByExit);
          return;
        }
        print_all(s, name_of(s, activator) + " exited the level.\n");
        s.next_map = next.map;
        s.exit_now = true;
        return;
      }
      if (activator >= 0 && !s.players[activator].alive) return;
      s.next_cinematic = next.shows;
      if (to.find('*') != std::string::npos) {
        s.next_map = next.map;
        s.exit_now = true;  // a new unit: the intermission first
        return;
      }
      s.pending_map = next.map;  // "" after the game's last movie: nothing follows
      s.cinematic = next.shows;
      s.game_over = next.map.empty();
      s.pending_change = MapChange::change_level;
      return;
    }
    case kSpeaker: {  // a one-shot sounds once, at its volume and reach; a loop is turned on or off
      if ((e.spawnflags & 3) == 0) {
        const float volume = number(s, e, "volume", 1);
        const float reach = number(s, e, "attenuation", 1);  // -1 heard everywhere: no fall-off
        sound_at(s, map_row(target), e.origin, e.noise, volume > 0 ? volume : 1,
                 reach == -1 ? 0.0f
                 : reach > 0 ? reach
                             : 1.0f);
      } else {
        e.on = !e.on;
      }
      return;
    }
    case kExplosion:  // after its `delay`, the blast and its targets together
      if (e.delay > 0) {
        s.delayed.push_back({target, s.level_time + double(e.delay), activator, true});
        return;
      }
      explosion(s, target, activator, depth);
      return;
    case kNone:
      if (e.item >= 0) {  // a power cube hidden until used appears, as a thing once
        if (e.hidden && (e.spawnflags & 1) != 0) {
          e.hidden = false;
          e.spawnflags &= ~1;
        }
        return;
      }
      if (targets_use(s, target, activator)) return;
      monster_use(s, target, activator);
      return;
    case kHelp:  // the first or the second help message, and the news of it
      s.help[(e.spawnflags & 1) != 0 ? 0 : 1] = e.message.substr(0, 511);
      s.help_changed++;
      return;
    case kCrossTrigger:
      s.cross_bits |= uint32_t(e.spawnflags) & 255u;
      e.gone = true;
      return;
    case kSecret:
    case kGoal: {
      const bool secret = class_of(e) == kSecret;
      if (activator >= 0)  // its noise at whoever found it
        sound_at(s, activator + 1, unpack(s.players[activator].move.origin), e.noise);
      (secret ? s.secrets_found : s.goals_found)++;
      if (!secret && s.goals_found == s.goals) s.world.cd_track = "0";  // the last: music stops
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

// `late`: a delayed use's copy, which keeps only the message and the targets: it says talk1, and
// it uses a door's area portals too.
void fire_targets(Q2Server& s, int ent, int activator, int depth, bool late) {
  const Q2Entity& e = s.entities[size_t(ent)];
  if (!e.message.empty()) {
    centre_to(s, activator, e.message);
    const int sounds = late ? 0 : e.sounds;
    const char* sample = sounds == 1   ? "misc/secret.wav"
                         : sounds == 2 ? "misc/talk.wav"
                         : sounds == 3 ? "misc/trigger1.wav"
                                       : kTalk;
    // A secret or a goal sounds its own noise at the finder instead (its use plays it).
    const bool own = !late && (class_of(e) == kSecret || class_of(e) == kGoal);
    if (activator >= 0 && !own)
      sound_at(s, activator + 1, unpack(s.players[activator].move.origin), sample);
  }
  if (!e.killtarget.empty()) {
    for (Q2Entity& k : s.entities)
      if (k.targetname == e.killtarget) {
        k.gone = true;
        k.solid = k.trigger = false;
        k.velocity = {};
        k.arrive = kNoAct;
        k.think_at = 0;
      }
    if (s.entities[size_t(ent)].gone)
      return s.console.push_back("entity was removed while using killtargets");
  }
  const std::string target = e.target;
  if (target.empty() || depth >= kMaxUseDepth) return;
  const bool door = !late && is_door(class_of(e));
  for (size_t i = 0; i < s.entities.size(); i++) {
    if (s.entities[i].targetname != target) continue;
    if (int(i) == ent) s.console.push_back("WARNING: Entity used itself.");
    if (int(i) == ent) continue;
    if (door && class_of(s.entities[i]) == kAreaPortal) continue;  // the door sets those itself
    use(s, static_cast<int>(i), activator, depth + 1, ent);
    if (s.entities[size_t(ent)].gone)
      return s.console.push_back("entity was removed while using targets");
  }
}

// Sets up one entity of class `c` from its keys.
// The disc a teleporter's pad, every exit and a deathmatch spawn spot show, in the model list.
int disc_model(Q2Server& s) {
  const char* disc = "models/objects/dmspot/tris.md2";
  const auto found = std::find(s.models.begin(), s.models.end(), disc);
  const int index = static_cast<int>(found - s.models.begin());
  if (found == s.models.end()) s.models.emplace_back(disc);
  return index;
}

void spawn_one(Q2Server& s, Q2Entity& e, Class c) {
  e.message = map_text(key(s, e, "message"));
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
      if (e.classname == "func_water") {  // slow, toggled, flush, never doubled, never harms
        e.speed = number(s, e, "speed", 25);
        e.accel = number(s, e, "accel", e.speed);
        e.decel = number(s, e, "decel", e.speed);
        if (key(s, e, "wait").empty()) e.wait = -1;
        if (e.wait < 0) e.spawnflags |= 32;
        e.lip = number(s, e, "lip", 0);
        e.dir = way_of(s, e);
        e.angles = {};
        e.closed = e.origin;
        e.open = e.origin + e.dir * (span(e.dir) - e.lip);
        if ((e.spawnflags & 1) != 0) {
          std::swap(e.closed, e.open);
          e.origin = e.closed;
        }
        return;
      }
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
    case kSecretDoor: {  // aside (left, right or down) by its width, then along its arrow
      e.solid = true;
      e.speed = e.accel = e.decel = 50;  // even, no ramp
      if (key(s, e, "dmg").empty()) e.dmg = 2;
      if (e.wait == 0) e.wait = 5;
      const sh::ViewAxes a = sh::angle_vectors(e.angles);
      e.angles = {};
      const auto across = [&](sh::Vec3f d) { return std::abs(sh::dot(d, size)); };
      const float side = (e.spawnflags & 2) != 0 ? -1.0f : 1.0f;
      e.closed = e.origin;
      e.open = (e.spawnflags & 4) != 0 ? e.origin - a.up * across(a.up)
                                       : e.origin + a.right * (side * across(a.right));
      // Its far place, kept where a turning door keeps its open angles (the move takes `dir`).
      e.open_angles = e.open + a.forward * across(a.forward);
      e.on = e.targetname.empty() || (e.spawnflags & 1) != 0;  // shot open by any damage
      return;
    }
    case kDoorRotating: {
      e.solid = true;
      e.turns = true;
      e.speed = number(s, e, "speed", 100);  // degrees/s, never doubled
      e.accel = e.decel = e.speed;
      if (e.wait == 0) e.wait = 3;
      if (key(s, e, "dmg").empty()) e.dmg = 2;
      float distance = number(s, e, "distance", 0);
      if (distance == 0) {
        distance = 90;
        s.console.push_back(e.classname + " at " + q2_pos(e.origin) + " with no distance set");
      }
      e.axis = (e.spawnflags & 64) != 0    ? sh::Vec3f{0, 0, 1}
               : (e.spawnflags & 128) != 0 ? sh::Vec3f{1, 0, 0}
                                           : sh::Vec3f{0, 1, 0};
      if ((e.spawnflags & 2) != 0) e.axis = e.axis * -1;
      e.angles = {};
      e.closed = e.open = e.origin;
      e.closed_angles = {};
      e.open_angles = e.axis * distance;
      if ((e.spawnflags & 1) != 0) {  // start open: the angles swap
        std::swap(e.closed_angles, e.open_angles);
        e.angles = e.closed_angles;
      }
      return;
    }
    case kRotating:
      e.solid = true;
      e.turns = true;
      e.speed = number(s, e, "speed", 100);
      if (e.dmg == 0) e.dmg = 2;  // never harmless, "dmg" "0" included
      e.axis = (e.spawnflags & 4) != 0   ? sh::Vec3f{0, 0, 1}
               : (e.spawnflags & 8) != 0 ? sh::Vec3f{1, 0, 0}
                                         : sh::Vec3f{0, 1, 0};
      if ((e.spawnflags & 2) != 0) e.axis = e.axis * -1;
      e.angles = {};
      if ((e.spawnflags & 1) != 0) e.avelocity = e.axis * e.speed;
      return;
    case kAreaPortal: {
      e.hidden = true;  // never drawn, never solid
      const int n = static_cast<int>(number(s, e, "style", 0));
      e.state = n;
      if (n >= 0 && n < 1024 && size_t(n) >= s.portals.size()) s.portals.resize(size_t(n) + 1, 0);
      return;
    }
    case kLight: {
      // Only a named light with a style of 32 or more switches; the rest are the compiler's.
      const int style = static_cast<int>(number(s, e, "style", 0));
      if (e.targetname.empty() || style < 32 || style >= kWorldLightStyles) {
        e.gone = true;
        return;
      }
      e.state = style;
      s.world.lightstyles[style] = (e.spawnflags & 1) != 0 ? "a" : "m";
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
      Q2Entity f;
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
      if (is_ship(e)) {  // hidden and passable until used, then off along its path
        if (e.target.empty()) {
          // Said by its lower corner, which is not yet placed: all zeros.
          s.console.push_back(e.classname + " without a target at (0 0 0)");
          e.gone = true;
          return;
        }
        const char* model = e.classname == "misc_viper"        ? "models/ships/viper/tris.md2"
                            : e.classname == "misc_crashviper" ? "models/ships/bigviper/tris.md2"
                            : is_transport(e)                  ? "models/objects/ship/tris.md2"
                                                               : "models/ships/strogg1/tris.md2";
        if (is_transport(e)) e.spawnflags |= 1;
        const auto found = std::find(s.models.begin(), s.models.end(), model);
        e.modelindex = static_cast<int>(found - s.models.begin());
        if (found == s.models.end()) s.models.emplace_back(model);
        e.hidden = true;
        e.mins = {-16, -16, 0};
        e.maxs = {16, 16, 32};
        e.speed = number(s, e, "speed", 300);
        e.accel = e.decel = e.speed;
        e.dmg = 0;
        e.think_at = s.level_time + kStep;
        e.think = kTrainFind;
        return;
      }
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
    case kTeleporter: {
      if (e.target.empty()) {
        e.gone = true;
        return;
      }
      e.trigger = true;
      e.mins = {-8, -8, 8};
      e.maxs = {8, 8, 24};
      // The pad is drawn as the disc a player sees and walks to, with its specks rising.
      e.pad = 1;
      e.modelindex = disc_model(s);
      return;
    }
    case kChangeLevel:
      if (e.map.empty()) e.gone = true;
      return;
    case kSpeaker:
      if (!e.noise.empty() && e.noise.find(".wav") == std::string::npos) e.noise += ".wav";
      return;
    case kHelp:
      if (deathmatch(s)) {
        e.gone = true;
        return;
      }
      if (e.message.empty()) {
        s.console.push_back(e.classname + " with no message at " + q2_pos(e.origin));
        e.gone = true;
      }
      return;
    case kCrossTarget:
      if (e.delay <= 0) e.delay = 1;
      e.think_at = s.level_time + double(e.delay);
      e.think = kCrossCheck;
      return;
    case kSecret:
    case kGoal:
      if (deathmatch(s)) {
        e.gone = true;
        return;
      }
      if (e.noise.empty()) e.noise = "misc/secret.wav";
      if (c == kSecret && s.map_name == "mine3" && e.message.empty() && e.origin.x == 280 &&
          e.origin.y == -2048 && e.origin.z == -624)  // the one secret the map left unsaid
        e.message = "You have found a secret area.";
      (c == kSecret ? s.secrets : s.goals)++;
      return;
    default:
      return;
  }
}

// Each door team's master gathers its members and, for a door that opens by touch, the field.
void link_doors(Q2Server& s) {
  const size_t count = s.entities.size();
  for (size_t i = 0; i < count; i++) {
    Q2Entity& e = s.entities[i];
    if (!e.gone && class_of(e) == kTrain && !e.team.empty() && e.team_master < 0)
      for (size_t j = i; j < count; j++)  // a team of trains: one moves them, one sounds
        if (class_of(s.entities[j]) == kTrain && s.entities[j].team == e.team)
          s.entities[j].team_master = static_cast<int>(i);
    if (e.gone || !is_door(class_of(e)) || e.team_master >= 0) continue;
    e.team_master = static_cast<int>(i);
    sh::Vec3f lo = abs_min(e), hi = abs_max(e);
    bool field = e.targetname.empty() && e.max_health == 0 && e.classname != "func_water";
    if (!e.team.empty())
      for (size_t j = i + 1; j < count; j++) {
        Q2Entity& m = s.entities[j];
        if (m.gone || !is_door(class_of(m)) || m.team != e.team) continue;
        m.team_master = static_cast<int>(i);
        const sh::Vec3f mlo = abs_min(m), mhi = abs_max(m);
        lo = {std::min(lo.x, mlo.x), std::min(lo.y, mlo.y), std::min(lo.z, mlo.z)};
        hi = {std::max(hi.x, mhi.x), std::max(hi.y, mhi.y), std::max(hi.z, mhi.z)};
      }
    // The team finishes together: each member's speed (and its accel and decel where they were
    // its speed) scaled by its travel over the shortest member's.
    if (!e.team.empty()) {
      const auto travel = [](const Q2Entity& d) {
        const sh::Vec3f m =
            class_of(d) == kDoorRotating ? d.open_angles - d.closed_angles : d.open - d.closed;
        return sh::length(m);
      };
      float least = travel(e);
      for (size_t j = i + 1; j < count; j++)
        if (s.entities[j].team_master == int(i)) least = std::min(least, travel(s.entities[j]));
      for (size_t j = i; j < count && least > 0; j++) {
        Q2Entity& m = s.entities[j];
        if (m.team_master != int(i)) continue;
        const float speed = m.speed * (travel(m) / least);
        if (m.accel == m.speed) m.accel = speed;
        if (m.decel == m.speed) m.decel = speed;
        m.speed = speed;
      }
    }
    if (!field) continue;
    if ((e.spawnflags & 1) != 0) door_portals(s, e, true);  // a door that starts open
    Q2Entity f;
    f.classname = "door_field";
    f.trigger = true;
    f.mins = lo - sh::Vec3f{60, 60, 0};
    f.maxs = hi + sh::Vec3f{60, 60, 0};
    f.owner = static_cast<int>(i);
    s.entities.push_back(std::move(f));
  }
}

}  // namespace

std::string map_text(std::string text) {
  for (size_t i = text.find("\\n"); i != std::string::npos; i = text.find("\\n", i))
    text.replace(i, 2, "\n");
  return text;
}

int map_row(int e) { return 1 + sh::kMaxClients + e; }

int map_entity_of_row(const Q2Server& s, int row) {
  const int e = row - 1 - sh::kMaxClients;
  return e >= 0 && size_t(e) < s.entities.size() ? e : -1;
}

namespace {

// The sound list's index of `name`, put on the list when it is not there (as the map starts,
// before the list is told to anyone).
int sound_index(Q2Server& s, const std::string& name) {
  if (name.empty()) return 0;
  const auto found = std::find(s.sounds.begin(), s.sounds.end(), name);
  if (found != s.sounds.end()) return static_cast<int>(found - s.sounds.begin());
  s.sounds.push_back(name);
  return static_cast<int>(s.sounds.size()) - 1;
}

// The movers', the pads' and the speakers' loops, and the one-shots the movers make beside them,
// on the sound list (porting note: q2-render-fx.md § Entity loop sounds). A door grinds on
// unless its `sounds` is 1 (a door of water only splashes); a plat always; a train its `noise`;
// the pad hums; a looping speaker plays its `noise`, on from the start with spawn flag 1.
void map_loops(Q2Server& s) {
  for (Q2Entity& e : s.entities) {
    e.loop = 0;
    if (e.gone) continue;
    switch (class_of(e)) {
      case kDoor:
      case kDoorRotating:
        for (const char* one : {door_sample(s, e, true), door_sample(s, e, false)})
          if (one != nullptr) sound_index(s, one);
        if (e.classname != "func_water" && e.sounds != 1) e.loop = sound_index(s, kDoorMove);
        break;
      case kPlat:
        sound_index(s, kPlatStart);
        sound_index(s, kPlatEnd);
        e.loop = sound_index(s, kPlatMove);
        break;
      case kTrain:
        e.loop = sound_index(s, e.noise);
        break;
      case kButton:
        if (e.sounds != 1) sound_index(s, kButtonSound);
        break;
      case kTeleporter:
        if (e.pad == 1) e.loop = sound_index(s, kPadHum);
        if (e.pad == 1) sound_index(s, kTeleSound);
        break;
      case kSpeaker:
        if ((e.spawnflags & 3) == 0) {
          sound_index(s, e.noise);
          break;
        }
        e.loop = sound_index(s, e.noise);
        e.on = (e.spawnflags & 1) != 0;
        break;
      default:
        break;
    }
  }
}

// The loop entity `e` (at `i`) makes this frame: a mover only while it moves, and only its team's
// first; a speaker while on; a pad always.
int loop_now(const Q2Entity& e, int i) {
  if (e.loop == 0 || e.gone) return 0;
  switch (class_of(e)) {
    case kDoor:
    case kDoorRotating:
    case kPlat:
    case kTrain:
      if (e.team_master >= 0 && e.team_master != i) return 0;
      return e.arrive != kNoAct ? e.loop : 0;
    case kSpeaker:
      return e.on ? e.loop : 0;
    default:
      return e.loop;
  }
}

}  // namespace

void map_precache(Q2Server& s) {
  for (const Q2Entity& e : s.entities)
    if ((e.classname == "misc_teleporter" && !e.target.empty()) ||
        e.classname == "misc_teleporter_dest" ||
        (deathmatch(s) && e.classname == "info_player_deathmatch")) {
      disc_model(s);
      return;
    }
}

void map_spawn(Q2Server& s) {
  s.delayed.clear();
  s.portals.clear();
  for (int i = 32; i < kWorldLightStyles; i++) s.world.lightstyles[i].clear();  // the switchable
  // The steady light and the two that move, as the baseline mod's world sets them; a client that
  // is sent no style 0 lights the map at nothing (measured: a stock client draws it dark).
  s.world.lightstyles[0] = "m";
  s.world.lightstyles[1] = "mmnmmommommnonmmonqnmmo";
  s.world.lightstyles[2] = "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba";
  s.world.lightstyles[3] = "mmmmmaaaaammmmmaaaaaabcdefgabcdefg";          // candle
  s.world.lightstyles[4] = "mamamamamama";                                // fast strobe
  s.world.lightstyles[5] = "jklmnopqrstuvwxyzyxwvutsrqponmlkj";           // gentle pulse
  s.world.lightstyles[6] = "nmonqnmomnmomomno";                           // flicker 2
  s.world.lightstyles[7] = "mmmaaaabcdefgmmmmaaaammmaamm";                // candle 2
  s.world.lightstyles[8] = "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa";  // candle 3
  s.world.lightstyles[9] = "aaaaaaaazzzzzzzz";                            // slow strobe
  s.world.lightstyles[10] = "mmamammmmammamamaaamammma";                  // fluorescent flicker
  s.world.lightstyles[11] = "abcdefghijklmnopqrrqponmlkjihgfedcba";       // slow pulse, never black
  s.world.lightstyles[63] = "a";                                          // testing
  s.secrets = s.secrets_found = s.goals = s.goals_found = 0;
  campaign_coop_spots(s);
  campaign_unnamed_starts(s);
  const size_t count = s.entities.size();
  for (size_t i = 0; i < count; i++) {
    Q2Entity& e = s.entities[i];
    e.block = static_cast<int>(i) + 1;  // the world's block is the first
    if (e.gone || e.item >= 0) continue;
    const Class c = class_of(e);
    if (c != kNone)
      spawn_one(s, s.entities[i], c);
    else
      targets_spawn(s, static_cast<int>(i));
  }
  // Every teleporter exit, and in deathmatch every spawn spot, is a plain disc a player stands
  // on and a shot strikes, whether or not a teleporter leads there.
  for (Q2Entity& e : s.entities)
    if (!e.gone && e.pad == 0 &&
        (e.classname == "misc_teleporter_dest" ||
         (deathmatch(s) && e.classname == "info_player_deathmatch"))) {
      e.pad = 2;
      e.modelindex = disc_model(s);
    }
  link_doors(s);
  map_loops(s);
}

void map_use_targets(Q2Server& s, int e, int activator) {
  if (e < 0 || size_t(e) >= s.entities.size()) return;
  const Q2Entity& ent = s.entities[size_t(e)];
  const bool none = activator == kNoActivator;
  if (none) activator = -1;
  if (ent.delay > 0) {
    if (none) s.console.push_back("Think_Delay with no activator");  // said as it is put off
    s.delayed.push_back({e, s.level_time + double(ent.delay), activator});
    return;
  }
  fire_targets(s, e, activator, 0);
}

// A mover's share of its tenth's push when the rules run past 10 frames a second (`sv_fps`):
// the move a 10 Hz frame makes (`left`, rounded to eighths as that frame's is) is spread over the
// tenth's frames, the last one taking what is left, and the step and a blocked push's harm come at
// the tenth alone; so at every tenth a mover stands where it would at 10, and its plan holds.
// Returns the push's blocker (as `push`), or -1; `step` says whether the tenth's step is due.
int split_push(Q2Server& s, Q2Entity& e, bool& step) {
  step = false;
  if (e.gone) return -1;
  const bool still = e.velocity.x == 0 && e.velocity.y == 0 && e.velocity.z == 0 &&
                     e.avelocity.x == 0 && e.avelocity.y == 0 && e.avelocity.z == 0;
  if (still || (!e.solid && e.arrive == kNoAct)) {
    step = s.tenth && e.arrive != kNoAct;
    e.split = false;
    return -1;
  }
  if (!e.split) {  // the tenth's move, as one 10 Hz frame makes it
    e.left = e.solid ? eighths(e.velocity * kStep) : e.velocity * kStep;
    e.left_turn = e.avelocity * kStep;
    e.split = true;
  }
  const int frames = s.tenth ? 1 : s.fps / 10 - s.sub + 1;  // this one and the tenth's after it
  const float share = 1.0f / float(frames);
  const sh::Vec3f move = s.tenth ? e.left : e.solid ? eighths(e.left * share) : e.left * share;
  const sh::Vec3f turn = s.tenth ? e.left_turn : e.left_turn * share;
  if (!e.solid) {  // a passable mover (a ship) goes through everything
    e.origin = e.origin + move;
    e.angles = e.angles + turn;
  } else {
    const bool stop_type =
        class_of(e) == kButton || (class_of(e) == kRotating && (e.spawnflags & 32) != 0);
    const int by = push(s, e, move, turn, stop_type);
    if (by != -1) return by;
  }
  e.left = e.left - move;
  e.left_turn = e.left_turn - turn;
  if (!s.tenth) return -1;
  e.split = false;
  step = true;
  return -1;
}

void map_frame(Q2Server& s) {
  // The uses whose delay is up.
  for (size_t i = 0; i < s.delayed.size();) {
    if (s.level_time + 0.001 < s.delayed[i].at) {
      i++;
      continue;
    }
    const Q2Server::Delayed d = s.delayed[i];
    s.delayed.erase(s.delayed.begin() + static_cast<long>(i));
    if (d.burst)
      explosion(s, d.entity, d.activator, 0);
    else
      fire_targets(s, d.entity, d.activator, 0, true);
  }
  // Each mover pushes, then steps; a blocked push skips the step (its schedule slides).
  std::vector<TeamFrame> teams;
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    TeamFrame* team = nullptr;
    if (!e.gone && e.team_master >= 0 && !e.team.empty()) {
      if (e.team_master == int(i)) team_begin(s, teams.emplace_back(), int(i));
      for (TeamFrame& t : teams)
        if (t.master == e.team_master) team = &t;
      if (team != nullptr && team->blocked) continue;  // the parts after a blocked one: not tried
    }
    const auto held = [&](int by) {
      if (team != nullptr)
        team_undo(s, *team);
      else if (s.entities[i].think_at > 0)
        s.entities[i].think_at += kStep;
      blocked(s, s.entities[i], by);
    };
    const auto step = [&] {
      if (team != nullptr)
        team->steps.push_back(i);
      else
        step_move(s, s.entities[i]);
    };
    if (s.fps != 10) {
      bool due = false;
      const int by = split_push(s, e, due);
      if (by != -1 && s.tenth) held(by);
      if (by == -1 && due) step();
      continue;
    }
    const bool still = e.velocity.x == 0 && e.velocity.y == 0 && e.velocity.z == 0 &&
                       e.avelocity.x == 0 && e.avelocity.y == 0 && e.avelocity.z == 0;
    if (e.gone || !e.solid || still) {
      if (!e.gone && e.arrive != kNoAct) {
        if (!e.solid) {  // a passable mover (a ship) goes through everything
          e.origin = e.origin + e.velocity * kStep;
          e.angles = e.angles + e.avelocity * kStep;
        }
        step();
      }
      continue;
    }
    const bool stop_type =
        class_of(e) == kButton || (class_of(e) == kRotating && (e.spawnflags & 32) != 0);
    const int by = push(s, e, eighths(e.velocity * kStep), e.avelocity * kStep, stop_type);
    if (by != -1)
      held(by);
    else
      step();
  }
  for (const TeamFrame& t : teams)  // a team none of whose parts was blocked steps
    if (!t.blocked)
      for (size_t j : t.steps) step_move(s, s.entities[j]);
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    if (e.think_at > 0 && s.level_time + 0.001 >= e.think_at) think(s, e);
  }
  if (s.tenth) targets_frame(s);  // the specials step with the 10 Hz world
}

void map_touch(Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  if (!p.alive) return;
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  const sh::Vec3f lean{1, 1, 1};  // a brush counts as touched within a unit
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    if (e.gone) continue;
    const bool in_field = e.trigger && overlap(lo, hi, abs_min(e), abs_max(e));
    const bool leaning = !e.trigger && e.solid &&
                         (is_door(class_of(e)) || class_of(e) == kButton ||
                          (class_of(e) == kRotating && (e.spawnflags & 16) != 0)) &&
                         overlap(lo - lean, hi + lean, abs_min(e), abs_max(e));
    if (in_field || leaning) touch(s, e, slot);
    if (!s.players[slot].alive) return;
  }
}

void map_monster_touch(Q2Server& s, int entity) {
  const Q2Entity& b = s.entities[size_t(entity)];
  if (b.gone || b.health <= 0) return;
  const sh::Vec3f lo = b.origin + b.mins, hi = b.origin + b.maxs;
  const sh::Vec3f facing = sh::angle_vectors({0, b.angles.y, 0}).forward;
  for (Q2Entity& f : s.entities) {
    if (f.gone) continue;
    const Class c = class_of(f);
    // The triggers a monster sets off: a trigger_multiple with MONSTER (1), facing its way when
    // it has one; a hurt; a push.
    if (f.trigger && (c == kTriggerMultiple || c == kHurt || c == kPush)) {
      if (!overlap(lo, hi, abs_min(f), abs_max(f))) continue;
      if (c == kTriggerMultiple) {
        if ((f.spawnflags & 1) == 0) continue;
        if ((f.dir.x != 0 || f.dir.y != 0 || f.dir.z != 0) && sh::dot(facing, f.dir) < 0) continue;
        trigger_fire(s, f, -1);
      } else if (c == kHurt) {
        const double hold = (f.spawnflags & 16) != 0 ? 1 : kStep;
        if (s.level_time + 0.001 < f.touched_at + hold) continue;
        f.touched_at = s.level_time;
        monster_damage(s, entity, -1, f.dmg, {});
      } else if (const int m = monster_index(s, entity); m >= 0) {
        s.monsters[size_t(m)].velocity = f.dir * (f.speed * 10);
        s.monsters[size_t(m)].on_ground = false;
        if ((f.spawnflags & 1) != 0) {  // pushes once
          f.trigger = false;
          f.gone = true;
        }
      }
      if (s.entities[size_t(entity)].health <= 0) return;
      continue;
    }
    if (c != kDoorField || f.owner < 0) continue;
    if (!overlap(lo, hi, abs_min(f), abs_max(f))) continue;
    Q2Entity& door = s.entities[size_t(f.owner)];
    if ((door.spawnflags & 8) != 0) continue;  // NOMONSTER
    if (s.level_time + 0.001 < f.touched_at + 1) continue;
    f.touched_at = s.level_time;
    door_use(s, door, -1);
  }
}

bool map_damage(Q2Server& s, int e, int attacker, int damage, sh::Vec3f dir, bool splash,
                const sh::Vec3f* point) {
  if (e < 0 || size_t(e) >= s.entities.size()) return false;
  if (class_of(s.entities[size_t(e)]) == kNone && targets_damage(s, e, attacker, damage))
    return true;
  if (s.entities[size_t(e)].box || monster_index(s, e) >= 0)
    return monster_damage(s, e, attacker, damage, dir, splash, point);
  Q2Entity& d = s.entities[size_t(e)];
  const Class c = class_of(d);
  if (c == kSecretDoor && d.on && !d.gone && damage > 0) {
    secret_use(s, d);
    return true;
  }
  if (d.gone || d.health <= 0 || damage <= 0) return false;
  if (!is_door(c) && c != kButton) return false;
  d.health -= damage;
  if (d.health > 0) return true;
  if (c == kButton) {
    d.health = 0;
    button_press(s, d, attacker);
    return true;
  }
  const int master = d.team_master >= 0 ? d.team_master : e;
  for (Q2Entity& m : s.entities)  // the team's health back, and no more damage until it closes
    if (&m == &d || m.team_master == master) m.health = 0;
  door_use(s, s.entities[size_t(master)], attacker);
  return true;
}

void map_revisit(Q2Server& s) {
  for (Q2Entity& e : s.entities)
    if (class_of(e) == kCrossTarget && !e.gone && e.think != kRemove) {
      e.think_at = s.level_time + double(e.delay);
      e.think = kCrossCheck;
    }
}

namespace {

// The effect bits that turn a mover's animated textures (a viewer steps them by the bits, else by
// its frame): ANIM_ALL (0x1000, twice a second) or ANIM_ALLFAST (0x2000, ten times) by each
// class's spawn flags; a button ANIM01 (0x400) at rest and ANIM23 (0x800) from the moment it is
// pressed in until it is all the way back out.
int texture_effects(const Q2Entity& e) {
  constexpr int kAll = 0x1000, kFast = 0x2000;
  const int f = e.spawnflags;
  switch (class_of(e)) {
    case kDoor:
      if (e.classname != "func_door") return 0;
      return (f & 16) != 0 ? kAll : (f & 64) != 0 ? kFast : 0;
    case kDoorRotating:
      return (f & 16) != 0 ? kAll : 0;
    case kRotating:
      return (f & 64) != 0 ? kAll : (f & 128) != 0 ? kFast : 0;
    case kWall:
      return (f & 8) != 0 ? kAll : (f & 16) != 0 ? kFast : 0;
    case kButton:
      return e.state == kAtSecond || e.state == kComing ? 0x800 : 0x400;
    default:
      return 0;
  }
}

}  // namespace

void map_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& e = s.entities[i];
    const size_t row = size_t(map_row(static_cast<int>(i)));
    if (row >= w.entities.size() || e.item >= 0) continue;
    WorldEntity& r = w.entities[row];
    r.loop = loop_now(e, static_cast<int>(i));
    const Class c = class_of(e);
    if (is_door(c) || c == kRotating || c == kWall || c == kButton) r.effects = texture_effects(e);
    if (e.solid || e.gone || class_of(e) == kWall) {
      r.free = e.gone;
      r.origin = e.origin;
      r.angles = e.angles;
      r.velocity = e.velocity;
      if (brush(e)) {  // a mover's box where it stands, which the bots' plans read
        r.absmin = e.origin + e.mins;
        r.absmax = e.origin + e.maxs;
      }
    }
  }
}

void map_place_player(Q2Server& s, int slot, sh::Vec3f origin, sh::Vec3f angles) {
  place_player(s, slot, origin, angles);
}

}  // namespace shooters_edge::server
