// The native game program's functions by number (rules.hpp states the rules of the match).
//
// ge:summary The native program's functions: weapons, explosions, the door, the plat, health
// ge:keywords rules call dispatch fire_rocket throw_grenade explode calc_move door_go_up
// ge:keywords door_go_down plat health_touch put_client_in_server
// ge:skill shooter
// ge:test tests/shooters_edge_match_test.cpp
#include "shooters_edge/sim/rules.hpp"

#include "shooter/physics.hpp"

namespace shooters_edge::sim {

namespace {

constexpr float kMinTravel = 0.1f;  // s: a shorter move just waits this long
constexpr float kMinHealth = 1;     // the test's players never die
constexpr int kStateBottom = 0, kStateRising = 1, kStateTop = 2, kStateFalling = 3;

sh::Edict& ent(sh::Server& sv, int e) { return sv.edicts[static_cast<size_t>(e)]; }

void record_event(sh::Server& sv, Rules& r, EventKind kind, int e, int other, sh::Vec3f where,
                  float amount = 0) {
  if (static_cast<int>(r.events.size()) >= kMaxEvents) {
    r.events_dropped++;
    return;
  }
  r.events.push_back({sv.time, kind, e, other, where, amount});
}

bool is_player(const sh::Server& sv, int e) { return e >= 1 && e <= sv.set.maxclients; }

// The move to `dest` at `speed` on the mover's own clock, then `done`.
void calc_move(sh::Server& sv, Rules& r, int e, sh::Vec3f dest, float speed, sh::Func done) {
  sh::Edict& ed = ent(sv, e);
  r.dest[static_cast<size_t>(e)] = dest;
  r.move_done[static_cast<size_t>(e)] = done;
  const sh::Vec3f delta = dest - ed.origin;
  const float travel = sh::length(delta) / speed;
  ed.think = kFnMoveDone;
  if (travel < kMinTravel) {
    ed.velocity = {};
    ed.nextthink = ed.ltime + kMinTravel;
    return;
  }
  ed.velocity = delta * (1 / travel);
  ed.nextthink = ed.ltime + travel;
}

void explode(sh::Server& sv, Rules& r, int e) {
  const sh::Edict& m = ent(sv, e);
  const sh::Vec3f at = m.origin;
  const int owner = m.owner;
  record_event(sv, r, EventKind::explosion, e, owner, at);
  sh::start_sound(sv, e, 0, "weapon/explosion", 1, 1);
  for (int p = 1; p <= sv.set.maxclients; p++) {
    sh::Edict& pl = ent(sv, p);
    if (pl.free || pl.health <= 0 || pl.solid == sh::kSolidNot) continue;
    const sh::Vec3f centre = pl.origin + (pl.mins + pl.maxs) * 0.5f;
    sh::Vec3f dir = centre - at;
    const float dist = sh::normalize(dir);
    float points = kBlastReach - 0.5f * dist;
    if (p == owner) points = points * 0.5f;
    if (points <= 0) continue;
    pl.health = pl.health - points;
    if (pl.health < kMinHealth) pl.health = kMinHealth;
    pl.velocity = sh::mad(pl.velocity, points * kKnockback, dir);
    pl.flags &= ~sh::kFlagOnGround;
    record_event(sv, r, EventKind::hurt, p, e, centre, points);
  }
  sh::free_edict(sv, e);
}

int spawn_missile(sh::Server& sv, int owner, int movetype) {
  const int e = sh::spawn_edict(sv);
  if (e < 0) return e;
  sh::Edict& m = ent(sv, e);
  m.owner = owner;
  m.movetype = movetype;
  m.solid = sh::kSolidBbox;
  sh::set_size(sv, e, {}, {});
  return e;
}

void fire(sh::Server& sv, Rules& r, int p) {
  sh::Edict& pl = ent(sv, p);
  const auto slot = static_cast<size_t>(p);
  if (pl.impulse == kImpulseRocket || pl.impulse == kImpulseGrenade)
    r.weapon[slot] = static_cast<int>(pl.impulse);
  pl.impulse = 0;
  if (pl.button0 == 0 || sv.g.time < r.attack_finished[slot]) return;

  const sh::ViewAxes ax = sh::angle_vectors(pl.v_angle);
  const sh::Vec3f muzzle = {pl.origin.x + ax.forward.x * 8, pl.origin.y + ax.forward.y * 8,
                            pl.origin.z + ax.forward.z * 8 + 16};
  if (r.weapon[slot] == kImpulseRocket) {
    const int e = spawn_missile(sv, p, sh::kMovetypeFlyMissile);
    if (e < 0) return;
    sh::Edict& m = ent(sv, e);
    m.velocity = ax.forward * kRocketSpeed;
    m.touch = kFnRocketTouch;
    m.think = kFnRemove;
    m.nextthink = sv.g.time + kRocketLife;
    sh::set_origin(sv, e, muzzle);
    r.attack_finished[slot] = sv.g.time + kRocketReload;
    sh::start_sound(sv, p, 1, "weapon/rocket_fire", 1, 1);
    record_event(sv, r, EventKind::rocket_fired, e, p, muzzle);
    sv.g.newmis = e;
  } else {
    const int e = spawn_missile(sv, p, sh::kMovetypeBounce);
    if (e < 0) return;
    sh::Edict& m = ent(sv, e);
    m.velocity = ax.forward * kGrenadeSpeed;
    m.velocity.z = m.velocity.z + kGrenadeLift;
    m.avelocity = {300, 300, 300};
    m.touch = kFnGrenadeTouch;
    m.think = kFnGrenadeExplode;
    m.nextthink = sv.g.time + kGrenadeFuse;
    sh::set_origin(sv, e, muzzle);
    r.attack_finished[slot] = sv.g.time + kGrenadeReload;
    sh::start_sound(sv, p, 1, "weapon/grenade_launch", 1, 1);
    record_event(sv, r, EventKind::grenade_thrown, e, p, muzzle);
    sv.g.newmis = e;
  }
}

// The spawn point for player `p`: its slot's own (slot modulo the arena's), or the next one round
// the list that no other living player stands within kSpawnClear of on every axis.
int pick_spawn(const sh::Server& sv, const Rules& r, int p) {
  const int slot = p - 1;
  for (int i = 0; i < kArenaSpawns; i++) {
    const int s = (slot + i) % kArenaSpawns;
    const sh::Vec3f at = r.arena->spawn[s];
    bool clear = true;
    for (int o = 1; o <= sv.set.maxclients && clear; o++) {
      const sh::Edict& other = sv.edicts[static_cast<size_t>(o)];
      if (o == p || other.free || other.solid == sh::kSolidNot || other.health <= 0) continue;
      const sh::Vec3f d = other.origin - at;
      clear = d.x > kSpawnClear || d.x < -kSpawnClear || d.y > kSpawnClear || d.y < -kSpawnClear ||
              d.z > kSpawnClear || d.z < -kSpawnClear;
    }
    if (clear) return s;
  }
  return slot % kArenaSpawns;
}

void put_in_server(sh::Server& sv, Rules& r, int p) {
  const int spawn = pick_spawn(sv, r, p);
  sh::Edict& pl = ent(sv, p);
  pl.movetype = sh::kMovetypeWalk;
  pl.solid = sh::kSolidSlidebox;
  pl.health = 100;
  pl.v_angle = {0, r.arena->spawn_yaw[spawn], 0};
  pl.angles = pl.v_angle;
  r.weapon[static_cast<size_t>(p)] = p == 1 ? kImpulseRocket : kImpulseGrenade;
  r.attack_finished[static_cast<size_t>(p)] = 0;
  pl.origin = r.arena->spawn[spawn];
  sh::set_size(sv, p, sh::kPlayerMins, sh::kPlayerMaxs);
}

void door_go_up(sh::Server& sv, Rules& r, int d) {
  r.state[static_cast<size_t>(d)] = kStateRising;
  record_event(sv, r, EventKind::door_opening, d, sv.g.other, ent(sv, d).origin);
  calc_move(sv, r, d, r.pos2[static_cast<size_t>(d)], kDoorSpeed, kFnDoorHitTop);
}

void call(void* ctx, sh::Server& sv, sh::Func fn) {
  Rules& r = *static_cast<Rules*>(ctx);
  const int self = sv.g.self;
  const int other = sv.g.other;
  const auto s = static_cast<size_t>(self);
  switch (fn) {
    case kFnStartFrame:
    case kFnPreThink:
    case kFnClientConnect:
      break;
    case kFnPostThink:
      fire(sv, r, self);
      break;
    case kFnPutClientInServer:
      put_in_server(sv, r, self);
      break;
    case kFnRemove:
      sh::free_edict(sv, self);
      break;
    case kFnRocketTouch:
      if (other == ent(sv, self).owner) break;
      explode(sv, r, self);
      break;
    case kFnGrenadeTouch:
      if (other == ent(sv, self).owner) break;
      if (is_player(sv, other) && ent(sv, other).health > 0) {
        explode(sv, r, self);
        break;
      }
      record_event(sv, r, EventKind::grenade_bounce, self, other, ent(sv, self).origin);
      sh::start_sound(sv, self, 1, "weapon/grenade_bounce", 1, 1);
      break;
    case kFnGrenadeExplode:
      explode(sv, r, self);
      break;
    case kFnDoorTrigger: {
      if (!is_player(sv, other) || ent(sv, other).health <= 0) break;
      const int d = r.door_of[s];
      const int st = r.state[static_cast<size_t>(d)];
      if (st == kStateBottom) {
        door_go_up(sv, r, d);
      } else if (st == kStateTop) {
        ent(sv, d).nextthink = ent(sv, d).ltime + kDoorWait;  // held open
      }
      break;
    }
    case kFnMoveDone: {
      sh::Edict& m = ent(sv, self);
      sh::set_origin(sv, self, r.dest[s]);
      m.velocity = {};
      const sh::Func done = r.move_done[s];
      r.move_done[s] = kFnNone;
      if (done != kFnNone) call(ctx, sv, done);
      break;
    }
    case kFnDoorHitTop: {
      r.state[s] = kStateTop;
      sh::Edict& d = ent(sv, self);
      d.think = kFnDoorGoDown;
      d.nextthink = d.ltime + kDoorWait;
      record_event(sv, r, EventKind::door_open, self, 0, d.origin);
      sh::start_sound(sv, self, 2, "door/stone_stop", 1, 1);
      break;
    }
    case kFnDoorGoDown:
      r.state[s] = kStateFalling;
      record_event(sv, r, EventKind::door_closing, self, 0, ent(sv, self).origin);
      calc_move(sv, r, self, r.pos1[s], kDoorSpeed, kFnDoorHitBottom);
      break;
    case kFnDoorHitBottom:
      r.state[s] = kStateBottom;
      record_event(sv, r, EventKind::door_closed, self, 0, ent(sv, self).origin);
      sh::start_sound(sv, self, 2, "door/stone_stop", 1, 1);
      break;
    case kFnDoorBlocked:
      record_event(sv, r, EventKind::door_blocked, self, other, ent(sv, self).origin);
      if (r.state[s] == kStateFalling) door_go_up(sv, r, self);
      break;
    case kFnPlatGoUp:
      r.state[s] = kStateRising;
      record_event(sv, r, EventKind::plat_up, self, 0, ent(sv, self).origin);
      calc_move(sv, r, self, r.pos2[s], kPlatSpeed, kFnPlatHitTop);
      break;
    case kFnPlatHitTop: {
      r.state[s] = kStateTop;
      sh::Edict& p = ent(sv, self);
      record_event(sv, r, EventKind::plat_top, self, 0, p.origin);
      p.think = kFnPlatGoDown;
      p.nextthink = p.ltime + kPlatWait;
      break;
    }
    case kFnPlatGoDown:
      r.state[s] = kStateFalling;
      record_event(sv, r, EventKind::plat_down, self, 0, ent(sv, self).origin);
      calc_move(sv, r, self, r.pos1[s], kPlatSpeed, kFnPlatHitBottom);
      break;
    case kFnPlatHitBottom: {
      r.state[s] = kStateBottom;
      sh::Edict& p = ent(sv, self);
      record_event(sv, r, EventKind::plat_bottom, self, 0, p.origin);
      p.think = kFnPlatGoUp;
      p.nextthink = p.ltime + kPlatWait;
      break;
    }
    case kFnHealthTouch: {
      if (!is_player(sv, other)) break;
      sh::Edict& pl = ent(sv, other);
      if (pl.health <= 0 || pl.health >= 100) break;
      const float gain = pl.health + kHealthGain > 100 ? 100 - pl.health : kHealthGain;
      pl.health = pl.health + gain;
      record_event(sv, r, EventKind::health_taken, self, other, ent(sv, self).origin, gain);
      sh::start_sound(sv, other, 3, "pickup/health", 1, 1);
      sh::free_edict(sv, self);
      break;
    }
    default:
      break;
  }
}

}  // namespace

const char* event_name(EventKind k) {
  switch (k) {
    case EventKind::rocket_fired:
      return "rocket fired";
    case EventKind::grenade_thrown:
      return "grenade thrown";
    case EventKind::grenade_bounce:
      return "grenade bounce";
    case EventKind::explosion:
      return "explosion";
    case EventKind::hurt:
      return "hurt";
    case EventKind::door_opening:
      return "door opening";
    case EventKind::door_open:
      return "door open";
    case EventKind::door_closing:
      return "door closing";
    case EventKind::door_closed:
      return "door closed";
    case EventKind::door_blocked:
      return "door blocked";
    case EventKind::plat_up:
      return "plat up";
    case EventKind::plat_top:
      return "plat top";
    case EventKind::plat_down:
      return "plat down";
    case EventKind::plat_bottom:
      return "plat bottom";
    case EventKind::health_taken:
      return "health taken";
  }
  return "?";
}

void create_rules(Rules& r, const Arena& arena, int max_edicts) {
  r = Rules{};
  r.arena = &arena;
  const auto n = static_cast<size_t>(max_edicts);
  r.attack_finished.assign(n, 0);
  r.weapon.assign(n, 0);
  r.pos1.assign(n, {});
  r.pos2.assign(n, {});
  r.dest.assign(n, {});
  r.move_done.assign(n, kFnNone);
  r.state.assign(n, 0);
  r.door_of.assign(n, 0);
  r.events.reserve(kMaxEvents);
}

sh::Program rules_program(Rules& r) {
  sh::Program p;
  p.ctx = &r;
  p.call = call;
  p.start_frame = kFnStartFrame;
  p.player_pre_think = kFnPreThink;
  p.player_post_think = kFnPostThink;
  p.client_connect = kFnClientConnect;
  p.put_client_in_server = kFnPutClientInServer;
  return p;
}

void spawn_arena_entities(sh::Server& sv, Rules& r) {
  const Arena& a = *r.arena;
  r.door = sh::spawn_edict(sv);
  {
    sh::Edict& d = ent(sv, r.door);
    d.movetype = sh::kMovetypePush;
    d.solid = sh::kSolidBsp;
    d.blocked = kFnDoorBlocked;
    sh::set_brush_model(sv, r.door, kDoorModel);
    r.pos1[static_cast<size_t>(r.door)] = {};
    r.pos2[static_cast<size_t>(r.door)] = {0, 0, kDoorLift};
  }
  r.trigger = sh::spawn_edict(sv);
  {
    sh::Edict& t = ent(sv, r.trigger);
    t.solid = sh::kSolidTrigger;
    t.touch = kFnDoorTrigger;
    r.door_of[static_cast<size_t>(r.trigger)] = r.door;
    sh::set_size(sv, r.trigger, a.trigger_mins, a.trigger_maxs);
  }
  r.plat = sh::spawn_edict(sv);
  {
    sh::Edict& p = ent(sv, r.plat);
    p.movetype = sh::kMovetypePush;
    p.solid = sh::kSolidBsp;
    sh::set_brush_model(sv, r.plat, kPlatModel);
    r.pos1[static_cast<size_t>(r.plat)] = {};
    r.pos2[static_cast<size_t>(r.plat)] = {0, 0, kPlatRise};
    p.think = kFnPlatGoUp;
    p.nextthink = p.ltime + kPlatWait;
  }
  r.health = sh::spawn_edict(sv);
  {
    sh::Edict& h = ent(sv, r.health);
    h.solid = sh::kSolidTrigger;
    h.flags = sh::kFlagItem;
    h.touch = kFnHealthTouch;
    h.origin = a.health_origin;
    sh::set_size(sv, r.health, {-16, -16, 0}, {16, 16, 56});
  }
}

}  // namespace shooters_edge::sim
