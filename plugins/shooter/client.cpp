// A player's packet and command on the server (client.hpp states the rules).
//
// ge:summary A player's packet, speed check, command pieces, touches and the move's solid list
// ge:keywords run_packet run_player_command speed_check build_player_world move_type_of
// ge:keywords connect_client disconnect_client landing-guard touches
// ge:skill shooter
// ge:test tests/shooter_client_test.cpp
#include "shooter/client.hpp"

#include "core/num.hpp"
#include "shooter/physics.hpp"

namespace ge::shooter {

namespace {

constexpr int kSplitMs = 50;
constexpr double kMaxCommandDt = 0.1;  // s

Edict& ent(Server& sv, int e) { return sv.edicts[static_cast<size_t>(e)]; }

// The program's hook with self the player and the server's time.
void call_hook(Server& sv, int e, Func fn) {
  sv.g.self = e;
  sv.g.other = 0;
  sv.g.time = static_cast<float>(sv.time);
  call_program(sv, fn);
}

// A program cannot cancel a landing's physics: a player on the ground falling, whose hook set the
// vertical speed to exactly 0 and left the horizontal alone, gets it back.
struct LandingGuard {
  bool armed = false;
  Vec3f before;

  explicit LandingGuard(const Edict& ed)
      : armed((ed.flags & kFlagOnGround) != 0 && ed.velocity.z < 0), before(ed.velocity) {}

  void apply(Edict& ed) const {
    if (armed && ed.velocity.z == 0 && ed.velocity.x == before.x && ed.velocity.y == before.y)
      ed.velocity.z = before.z;
  }
};

void add_solid(const Server& sv, int e, MoveWorld& w, int* ents) {
  const Edict& ed = sv.edicts[static_cast<size_t>(e)];
  Solid& s = w.solids[w.count];
  s.origin = ed.origin;
  s.mins = ed.mins;
  s.maxs = ed.maxs;
  if (ed.solid == kSolidBsp) {
    const BrushModel& m = sv.models[static_cast<size_t>(ed.modelindex)];
    s.kind = SolidKind::brush;
    s.point_hull = m.hulls[0];
    s.clip_hull = m.hulls[1];
  } else {
    s.kind = SolidKind::box;
  }
  ents[w.count] = e;
  w.count++;
}

void list_solids(const Server& sv, int32_t n, int player, Vec3f lo, Vec3f hi, MoveWorld& w,
                 int* ents) {
  const AreaNode& node = sv.area.nodes[n];
  for (int32_t e = node.solids.head; e != -1; e = sv.edicts[static_cast<size_t>(e)].link_next) {
    const Edict& ed = sv.edicts[static_cast<size_t>(e)];
    if (e == player || ed.owner == player) continue;
    if (ed.solid != kSolidBsp && ed.solid != kSolidBbox && ed.solid != kSolidSlidebox) continue;
    if (ed.absmin.x > hi.x || ed.absmin.y > hi.y || ed.absmin.z > hi.z || ed.absmax.x < lo.x ||
        ed.absmax.y < lo.y || ed.absmax.z < lo.z)
      continue;
    if (w.count >= kMaxSolids) return;
    add_solid(sv, e, w, ents);
  }
  if (node.axis < 0) return;
  if (hi[node.axis] > node.dist) list_solids(sv, node.children[0], player, lo, hi, w, ents);
  if (lo[node.axis] < node.dist) list_solids(sv, node.children[1], player, lo, hi, w, ents);
}

// One command piece of at most 50 ms: steps 1 to 7.
void run_piece(Server& sv, int slot, const MoveCmd& in) {
  ClientSlot& c = sv.clients[static_cast<size_t>(slot)];
  const int e = slot + 1;
  sv.counts.commands++;
  {
    Edict& ed = ent(sv, e);
    ed.button0 = static_cast<float>(in.buttons & kButtonAttack);
    ed.button2 = static_cast<float>((in.buttons & kButtonJump) >> 1);
    ed.button1 = static_cast<float>((in.buttons & kButtonUse) >> 2);
    if (in.impulse != 0) ed.impulse = in.impulse;
  }

  MoveCmd cmd = in;
  if (cmd.angles.x < sv.set.minpitch) cmd.angles.x = sv.set.minpitch;
  if (cmd.angles.x > sv.set.maxpitch) cmd.angles.x = sv.set.maxpitch;
  {
    Edict& ed = ent(sv, e);
    if (!ed.fixangle) ed.v_angle = cmd.angles;
    if (ed.health > 0) {
      ed.angles.x = -ed.v_angle.x / 3;
      if (!ed.fixangle) ed.angles.y = ed.v_angle.y;
      ed.angles.z = 0;
    }
  }

  double dt = cmd.msec * 0.001;
  if (dt > kMaxCommandDt) dt = kMaxCommandDt;

  if (!c.spectator) {
    sv.g.frametime = static_cast<float>(dt);
    const LandingGuard guard(ent(sv, e));
    call_hook(sv, e, sv.program.player_pre_think);
    guard.apply(ent(sv, e));
    run_think(sv, e, dt);
  }

  // The move, from the fields.
  Edict& ed = ent(sv, e);
  const Vec3f offset = ed.mins - kPlayerMins;
  MoveState ms;
  ms.origin = ed.origin + offset;
  ms.velocity = ed.velocity;
  ms.angles = ed.v_angle;
  ms.type = move_type_of(ed, c.old_spectator);
  ms.jump_held = c.jump_held;
  ms.waterjump_time = ed.teleport_time;
  ms.waterlevel = ed.waterlevel;
  ms.watertype = ed.watertype == 0 ? kContentsEmpty : ed.watertype;
  ms.entgravity = ed.gravity != 0 ? ed.gravity : 1.0f;
  ms.maxspeed = ed.maxspeed != 0 ? ed.maxspeed : sv.set.move.maxspeed;
  ms.ground_normal = c.ground_normal;

  MoveWorld world;
  int ents[kMaxSolids];
  build_player_world(sv, e, ms.origin, world, ents);
  ms.ground = -1;
  if ((ed.flags & kFlagOnGround) != 0) {
    ms.ground = 0;
    for (int i = 0; i < world.count; i++)
      if (ents[i] == ed.groundentity) ms.ground = i;
  }
  const MoveResult r = player_move(ms, cmd, sv.set.move, world);

  // Back into the fields.
  c.jump_held = ms.jump_held;
  c.ground_normal = ms.ground_normal;
  ed.teleport_time = ms.waterjump_time;
  ed.waterlevel = ms.waterlevel;
  ed.watertype = ms.watertype;
  if (ms.ground != -1) {
    ed.flags |= kFlagOnGround;
    ed.groundentity = ents[ms.ground];
  } else {
    ed.flags &= ~kFlagOnGround;
  }
  ed.origin = ms.origin - offset;
  ed.velocity = ms.velocity;
  if (!ed.fixangle) ed.v_angle = ms.angles;
  c.last_cmd_time = sv.time;

  if (ed.solid == kSolidNot) return;
  link_edict(sv, e, true);
  for (int i = 0; i < r.touches.count; i++) {
    const int t = ents[r.touches.ents[i]];
    if (sv.touched[static_cast<size_t>(t)] == sv.touch_stamp) continue;
    const Edict& te = ent(sv, t);
    if (te.free || te.touch == kNoFunc) continue;
    sv.touched[static_cast<size_t>(t)] = sv.touch_stamp;
    sv.g.self = t;
    sv.g.other = e;
    sv.g.time = static_cast<float>(sv.time);
    sv.counts.player_touches++;
    call_program(sv, te.touch);
  }
}

void run_split(Server& sv, int slot, const MoveCmd& cmd) {
  if (cmd.msec <= kSplitMs) {
    run_piece(sv, slot, cmd);
    return;
  }
  MoveCmd half = cmd;
  half.msec = static_cast<uint8_t>(cmd.msec / 2);
  run_split(sv, slot, half);
  half.impulse = 0;
  run_split(sv, slot, half);
}

}  // namespace

MoveType move_type_of(const Edict& ed, bool old_spectator) {
  if (ed.movetype == kMovetypeNoclip)
    return old_spectator ? MoveType::old_spectator : MoveType::spectator;
  if (ed.movetype == kMovetypeNone) return MoveType::none;
  if (ed.movetype == kMovetypeLock) return MoveType::lock;
  if (ed.health <= 0) return MoveType::dead;
  if (ed.movetype == kMovetypeFly) return MoveType::fly;
  return MoveType::normal;
}

void connect_client(Server& sv, int slot, bool spectator, bool bot) {
  ClientSlot& c = sv.clients[static_cast<size_t>(slot)];
  c = ClientSlot{};
  c.active = true;
  c.spectator = spectator;
  c.bot = bot;
  c.speed_check_time = sv.time;
  c.last_cmd_time = sv.time;
  const int e = slot + 1;
  ent(sv, e) = Edict{};
  ent(sv, e).free = false;
  call_hook(sv, e, sv.program.set_new_parms);
  if (spectator) {
    call_hook(sv, e, sv.program.spectator_connect);
  } else {
    call_hook(sv, e, sv.program.client_connect);
    call_hook(sv, e, sv.program.put_client_in_server);
  }
}

void disconnect_client(Server& sv, int slot) {
  ClientSlot& c = sv.clients[static_cast<size_t>(slot)];
  const int e = slot + 1;
  call_hook(sv, e, c.spectator ? sv.program.spectator_disconnect : sv.program.client_disconnect);
  free_edict(sv, e);
  c = ClientSlot{};
}

int speed_check(ClientSlot& c, double now, int msec) {
  const auto elapsed = static_cast<int>(core::num::round((now - c.speed_check_time) * 1000));
  c.speed_check_time = now;
  if (msec > elapsed) {
    const int over = msec - elapsed;
    if (c.speed_bank_ms >= over) {
      c.speed_bank_ms -= over;
    } else {
      msec = elapsed + c.speed_bank_ms;
      c.speed_bank_ms = 0;
    }
  } else {
    c.speed_bank_ms += elapsed - msec;
  }
  if (c.speed_bank_ms > kSpeedBankMaxMs) c.speed_bank_ms = kSpeedBankMaxMs;
  if (c.speed_bank_ms < 0) c.speed_bank_ms = 0;
  return msec;
}

void run_player_command(Server& sv, int slot, const MoveCmd& cmd, bool checked) {
  ClientSlot& c = sv.clients[static_cast<size_t>(slot)];
  MoveCmd run = cmd;
  if (checked && !c.bot && sv.set.speedcheck) {
    const int allowed = speed_check(c, sv.time, cmd.msec);
    if (allowed < cmd.msec) sv.counts.speed_cuts++;
    run.msec = static_cast<uint8_t>(allowed < 0 ? 0 : allowed > 255 ? 255 : allowed);
  }
  run_split(sv, slot, run);
}

int run_packet(Server& sv, const Packet& p) {
  ClientSlot& c = sv.clients[static_cast<size_t>(p.client)];
  if (!c.active) return -1;
  if (c.any_packet && p.sequence <= c.last_sequence) return -1;  // late or duplicated
  const int lost = c.any_packet ? static_cast<int>(p.sequence - c.last_sequence - 1) : 0;
  c.last_sequence = p.sequence;
  c.any_packet = true;
  sv.touch_stamp++;

  if (lost < kMaxRepeatLost) {
    for (int i = lost; i > 2; i--) {
      sv.counts.commands_lost++;
      run_player_command(sv, p.client, c.last_cmd);
    }
    if (lost > 1) {
      sv.counts.commands_lost++;
      run_player_command(sv, p.client, p.cmds[0]);
    }
    if (lost > 0) {
      sv.counts.commands_lost++;
      run_player_command(sv, p.client, p.cmds[1]);
    }
  }
  run_player_command(sv, p.client, p.cmds[2]);

  const int e = p.client + 1;
  if (!c.spectator) {
    const LandingGuard guard(ent(sv, e));
    call_hook(sv, e, sv.program.player_post_think);
    guard.apply(ent(sv, e));
    run_new_missile(sv);
  } else {
    call_hook(sv, e, sv.program.spectator_think);
  }
  c.last_cmd = p.cmds[2];
  c.last_cmd.buttons = 0;
  return lost;
}

void build_player_world(const Server& sv, int player, Vec3f origin, MoveWorld& world, int* ents) {
  world.count = 0;
  add_solid(sv, 0, world, ents);
  const Vec3f reach{kSolidsReach, kSolidsReach, kSolidsReach};
  list_solids(sv, 0, player, origin - reach, origin + reach, world, ents);
}

}  // namespace ge::shooter
