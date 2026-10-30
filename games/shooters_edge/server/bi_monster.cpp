// THE MONSTER MOVES: walkmove (#32), checkbottom (#40), changeyaw (#49) and movetogoal (#67), on
// the loop's entities through its traces.
//
// THE STEP MOVE of entity E by m: FLYING OR SWIMMING (flag fly or swim) tries twice: the first try,
// when E has an enemy, corrects the height toward it (E more than 40 above it: 8 down; less than
// 30: 8 up); E's box is traced from its origin to origin + m (+ the correction); a full move
// succeeds (a swimmer whose end is in empty contents fails instead: it left the water), moving E
// to the end (relinked with trigger touches when asked); with no enemy there is no second try.
// WALKING: from origin + m raised 18, E's box is traced down 36; all solid fails; started in solid,
// it retries from 18 lower and fails if still in solid; hitting nothing, a partial-ground E moves
// by m, loses on-ground and succeeds, any other fails (it walked off an edge); otherwise E moves to
// the end, and when `checkbottom` then fails a partial-ground E succeeds anyway and any other is
// put back and fails; success clears partial-ground and sets `groundentity` to what was hit.
//
// CHECKBOTTOM: the four points 1 unit below the box's bottom corners all in solid: yes. Otherwise
// a line from the bottom centre down 36 units must hit, and one from each bottom corner down to the
// same depth must hit within 18 units below the centre's hit.
//
// CHANGEYAW turns the yaw toward `ideal_yaw` by at most `yaw_speed`, the short way round, each
// angle wrapped into 0..360 on the protocol's 16-bit grid (360/65536 degrees; `wrap_angle`).
//
// MOVETOGOAL (dist): see the steps in `new_chase_dir`. Every draw (one in four, the coins) comes
// from the game's generator in the order the rules make them, so a replay holds.
//
// The flag numbers the rules read: fly 1, swim 2, on-ground 512, partial-ground 1024.
//
// ge:summary The monster moves: the step move, walkmove, checkbottom, changeyaw, movetogoal
// ge:keywords walkmove movetogoal checkbottom changeyaw step-move chase-direction partial-ground
// ge:keywords ideal_yaw yaw_speed goalentity enemy wrap_angle monster-ai
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_monster_test.cpp
#include <cmath>

#include "core/libm/libm.hpp"
#include "progs/stdlib.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/server/builtins.hpp"

namespace shooters_edge::server {

namespace {

using sim::flush_to_world;
using sim::qc_of;
using sim::refresh_from_world;

constexpr int kFlagPartialGround = 1024;
constexpr float kStepSize = 18;
constexpr double kRadians = 3.14159265358979323846 * 2 / 360;

float vm_f(const pg::Vm& vm, int e, int f) { return pg::as_float(*pg::entity_field(vm, e, f)); }
void vm_set_f(pg::Vm& vm, int e, int f, float v) { *pg::entity_field(vm, e, f) = pg::as_word(v); }
int vm_e(const pg::Vm& vm, int e, int f) {
  const int n = pg::entity_number(vm, static_cast<int32_t>(*pg::entity_field(vm, e, f)));
  return n < 0 ? 0 : n;
}

}  // namespace

// An angle wrapped into 0..360 on the 16-bit grid.
float wrap_angle(float a) {
  const double steps = static_cast<double>(a) * (65536.0 / 360.0);
  const auto k = static_cast<int64_t>(steps) & 65535;
  return static_cast<float>((360.0 / 65536.0) * static_cast<double>(k));
}

// The yaw turned toward `ideal` by at most `speed` (changeyaw's rule).
float turned_yaw(float yaw, float ideal, float speed) {
  const float current = wrap_angle(yaw);
  if (current == ideal) return yaw;
  float move = ideal - current;
  if (ideal > current) {
    if (move >= 180) move = move - 360;
  } else {
    if (move <= -180) move = move + 360;
  }
  if (move > 0) {
    if (move > speed) move = speed;
  } else {
    if (move < -speed) move = -speed;
  }
  return wrap_angle(current + move);
}

bool check_bottom(QcServer& s, int e) {
  const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
  const sh::Vec3f mins = ed.origin + ed.mins, maxs = ed.origin + ed.maxs;
  sh::Vec3f start;
  start.z = mins.z - 1;
  bool all_solid = true;
  for (int x = 0; x <= 1 && all_solid; x++)
    for (int y = 0; y <= 1 && all_solid; y++) {
      start.x = x != 0 ? maxs.x : mins.x;
      start.y = y != 0 ? maxs.y : mins.y;
      if (sh::world_contents(s.sv, start) != sh::kContentsSolid) all_solid = false;
    }
  if (all_solid) return true;

  start.z = mins.z;
  start.x = (mins.x + maxs.x) * 0.5f;
  start.y = (mins.y + maxs.y) * 0.5f;
  sh::Vec3f stop = start;
  stop.z = start.z - 2 * kStepSize;
  sh::HullTrace tr = sh::trace_move(s.sv, start, {}, {}, stop, sh::kTraceNoMonsters, e);
  if (tr.fraction == 1) return false;
  const float mid = tr.endpos.z;
  for (int x = 0; x <= 1; x++)
    for (int y = 0; y <= 1; y++) {
      start.x = stop.x = x != 0 ? maxs.x : mins.x;
      start.y = stop.y = y != 0 ? maxs.y : mins.y;
      tr = sh::trace_move(s.sv, start, {}, {}, stop, sh::kTraceNoMonsters, e);
      if (tr.fraction == 1 || mid - tr.endpos.z > kStepSize) return false;
    }
  return true;
}

// The step move of `e` by `move` (the rules above); `enemy` is E's enemy (0 none).
bool step_move(QcServer& s, int e, sh::Vec3f move, int enemy, bool relink) {
  sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
  if ((ed.flags & (sh::kFlagFly | sh::kFlagSwim)) != 0) {
    for (int i = 0; i < 2; i++) {
      sh::Vec3f to = ed.origin + move;
      if (i == 0 && enemy != 0) {
        const float dz = ed.origin.z - s.sv.edicts[static_cast<size_t>(enemy)].origin.z;
        if (dz > 40) to.z = to.z - 8;
        if (dz < 30) to.z = to.z + 8;
      }
      const sh::HullTrace tr =
          sh::trace_move(s.sv, ed.origin, ed.mins, ed.maxs, to, sh::kTraceNormal, e);
      if (tr.fraction == 1) {
        if ((ed.flags & sh::kFlagSwim) != 0 &&
            sh::world_contents(s.sv, tr.endpos) == sh::kContentsEmpty)
          return false;  // a swimmer may not leave the water
        ed.origin = tr.endpos;
        if (relink) sh::link_edict(s.sv, e, true);
        return true;
      }
      if (enemy == 0) break;
    }
    return false;
  }

  sh::Vec3f from = ed.origin + move;
  from.z = from.z + kStepSize;
  sh::Vec3f end = from;
  end.z = end.z - 2 * kStepSize;
  sh::HullTrace tr = sh::trace_move(s.sv, from, ed.mins, ed.maxs, end, sh::kTraceNormal, e);
  if (tr.allsolid) return false;
  if (tr.startsolid) {
    from.z = from.z - kStepSize;
    tr = sh::trace_move(s.sv, from, ed.mins, ed.maxs, end, sh::kTraceNormal, e);
    if (tr.allsolid || tr.startsolid) return false;
  }
  if (tr.fraction == 1) {
    if ((ed.flags & kFlagPartialGround) != 0) {
      ed.origin = ed.origin + move;
      if (relink) sh::link_edict(s.sv, e, true);
      s.sv.edicts[static_cast<size_t>(e)].flags &= ~sh::kFlagOnGround;
      return true;
    }
    return false;  // walked off an edge
  }
  const sh::Vec3f old = ed.origin;
  ed.origin = tr.endpos;
  if (!check_bottom(s, e)) {
    if ((ed.flags & kFlagPartialGround) != 0) {
      if (relink) sh::link_edict(s.sv, e, true);
      return true;
    }
    s.sv.edicts[static_cast<size_t>(e)].origin = old;
    return false;
  }
  sh::Edict& after = s.sv.edicts[static_cast<size_t>(e)];
  after.flags &= ~kFlagPartialGround;
  after.groundentity = tr.ent < 0 ? 0 : tr.ent;
  if (relink) sh::link_edict(s.sv, e, true);
  return true;
}

namespace {

// Steps `self` toward `yaw` by `dist`: ideal_yaw set, turned, moved without relinking; a move
// that leaves the yaw more than 45 degrees off the ideal is undone (turned, not moved). Relinked.
bool step_direction(QcServer& s, pg::Vm& vm, int self, float yaw, float dist) {
  vm_set_f(vm, self, pg::vm_layout(vm).f.ideal_yaw, yaw);
  sh::Edict& ed = s.sv.edicts[static_cast<size_t>(self)];
  ed.angles.y = turned_yaw(ed.angles.y, yaw, vm_f(vm, self, pg::vm_layout(vm).f.yaw_speed));
  const double r = static_cast<double>(yaw) * kRadians;
  const sh::Vec3f move{static_cast<float>(ge::core::libm::cos(r) * dist),
                       static_cast<float>(ge::core::libm::sin(r) * dist), 0};
  const sh::Vec3f old = ed.origin;
  const bool ok = step_move(s, self, move, vm_e(vm, self, pg::vm_layout(vm).f.enemy), false);
  sh::Edict& now = s.sv.edicts[static_cast<size_t>(self)];
  if (ok) {
    const float delta = now.angles.y - vm_f(vm, self, pg::vm_layout(vm).f.ideal_yaw);
    if (delta > 45 && delta < 315) now.origin = old;
  }
  sh::link_edict(s.sv, self, true);
  return ok;
}

bool draw_is(QcServer& s, int32_t hi, int32_t want) {
  return ge::core::int_in(s.q.rng, 0, hi) == want;
}

void new_chase_dir(QcServer& s, pg::Vm& vm, int self, int goal, float dist) {
  constexpr float kNone = -1;
  const float olddir = wrap_angle(static_cast<float>(
      static_cast<int>(vm_f(vm, self, pg::vm_layout(vm).f.ideal_yaw) / 45) * 45));
  const float turnaround = wrap_angle(olddir - 180);
  const sh::Edict& me = s.sv.edicts[static_cast<size_t>(self)];
  const sh::Edict& g = s.sv.edicts[static_cast<size_t>(goal)];
  const float dx = g.origin.x - me.origin.x, dy = g.origin.y - me.origin.y;
  float d1 = dx > 10 ? 0.0f : dx < -10 ? 180.0f : kNone;
  float d2 = dy < -10 ? 270.0f : dy > 10 ? 90.0f : kNone;
  if (d1 != kNone && d2 != kNone) {
    // The diagonal; 215 rather than 225 for south-west, as the rule has it.
    const float tdir = d1 == 0 ? (d2 == 90 ? 45.0f : 315.0f) : (d2 == 90 ? 135.0f : 215.0f);
    if (tdir != turnaround && step_direction(s, vm, self, tdir, dist)) return;
  }
  const bool coin = draw_is(s, 1, 1);
  if (coin || std::fabs(dy) > std::fabs(dx)) {
    const float t = d1;
    d1 = d2;
    d2 = t;
  }
  if (d1 != kNone && d1 != turnaround && step_direction(s, vm, self, d1, dist)) return;
  if (d2 != kNone && d2 != turnaround && step_direction(s, vm, self, d2, dist)) return;
  if (step_direction(s, vm, self, olddir, dist)) return;
  if (draw_is(s, 1, 1)) {
    for (int t = 0; t <= 315; t += 45)
      if (static_cast<float>(t) != turnaround &&
          step_direction(s, vm, self, static_cast<float>(t), dist))
        return;
  } else {
    for (int t = 315; t >= 0; t -= 45)
      if (static_cast<float>(t) != turnaround &&
          step_direction(s, vm, self, static_cast<float>(t), dist))
        return;
  }
  if (step_direction(s, vm, self, turnaround, dist)) return;
  vm_set_f(vm, self, pg::vm_layout(vm).f.ideal_yaw, olddir);
  if (!check_bottom(s, self)) s.sv.edicts[static_cast<size_t>(self)].flags |= kFlagPartialGround;
}

// `self` for a monster move; -1 after a fault.
int self_of(pg::Vm& vm, const char* name) {
  const int self = pg::entity_number(vm, pg::global_int(vm, pg::vm_layout(vm).g.self));
  if (self < 0) {
    const std::string m = std::string(name) + ": self is not an entity in use";
    pg::vm_error(vm, m.c_str());
  }
  return self;
}

}  // namespace

void bi_walkmove(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int self = self_of(vm, "walkmove");
  if (self < 0) return;
  const float yaw = pg::arg_float(vm, 0), dist = pg::arg_float(vm, 1);
  flush_to_world(qc_of(vm));
  const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(self)];
  if ((ed.flags & (sh::kFlagOnGround | sh::kFlagFly | sh::kFlagSwim)) == 0) {
    pg::return_float(vm, 0);
    return;
  }
  const double r = static_cast<double>(yaw) * kRadians;
  const sh::Vec3f move{static_cast<float>(ge::core::libm::cos(r) * dist),
                       static_cast<float>(ge::core::libm::sin(r) * dist), 0};
  const SavedGlobals saved = save_globals(vm);
  const bool ok = step_move(s, self, move, vm_e(vm, self, pg::vm_layout(vm).f.enemy), true);
  restore_globals(vm, saved);
  refresh_from_world(qc_of(vm));
  pg::return_float(vm, ok ? 1.0f : 0.0f);
}

void bi_checkbottom(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = sim::entity_parm(vm, 0, "checkbottom");
  if (e < 0) return;
  flush_to_world(qc_of(vm));
  pg::return_float(vm, check_bottom(s, e) ? 1.0f : 0.0f);
}

void bi_changeyaw(pg::Vm& vm) {
  const int self = self_of(vm, "changeyaw");
  if (self < 0) return;
  uint32_t* yaw = pg::entity_field(vm, self, pg::vm_layout(vm).f.angles + 1);
  *yaw = pg::as_word(turned_yaw(pg::as_float(*yaw), vm_f(vm, self, pg::vm_layout(vm).f.ideal_yaw),
                                vm_f(vm, self, pg::vm_layout(vm).f.yaw_speed)));
}

void bi_movetogoal(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int self = self_of(vm, "movetogoal");
  if (self < 0) return;
  const float dist = pg::arg_float(vm, 0);
  const int goal = vm_e(vm, self, pg::vm_layout(vm).f.goalentity);
  flush_to_world(qc_of(vm));
  const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(self)];
  pg::return_float(vm, 0);
  if ((ed.flags & (sh::kFlagOnGround | sh::kFlagFly | sh::kFlagSwim)) == 0) return;
  const SavedGlobals saved = save_globals(vm);
  // With an enemy, close enough (its box grown by dist meets the goal's): stop.
  const sh::Edict& g = s.sv.edicts[static_cast<size_t>(goal)];
  bool close = vm_e(vm, self, pg::vm_layout(vm).f.enemy) != 0;
  for (int i = 0; i < 3 && close; i++)
    if (g.absmin[i] > ed.absmax[i] + dist || g.absmax[i] < ed.absmin[i] - dist) close = false;
  if (!close) {
    if (draw_is(s, 3, 1) ||
        !step_direction(s, vm, self, vm_f(vm, self, pg::vm_layout(vm).f.ideal_yaw), dist))
      new_chase_dir(s, vm, self, goal, dist);
  }
  restore_globals(vm, saved);
  refresh_from_world(qc_of(vm));
}

}  // namespace shooters_edge::server
