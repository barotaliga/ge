// The QC adapter (qc_program.hpp).
//
// ge:summary The QC adapter: the mirror, nested calls, the hooks, the seam's builtins
// ge:keywords qc progs adapter mirror sync hooks spawn remove setorigin nested-call
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_program_test.cpp
#include "shooters_edge/sim/qc_program.hpp"

#include "progs/file.hpp"
#include "progs/layout.hpp"

namespace shooters_edge::sim {

namespace pg = ge::progs;

namespace {

uint32_t* field(QcProgram& q, int e, int f) { return pg::entity_field(q.vm, e, f); }

void put_f(QcProgram& q, int e, int f, float v) { *field(q, e, f) = pg::as_word(v); }
void put_i(QcProgram& q, int e, int f, int32_t v) { *field(q, e, f) = static_cast<uint32_t>(v); }
void put_v(QcProgram& q, int e, int f, sh::Vec3f v) {
  uint32_t* w = field(q, e, f);
  w[0] = pg::as_word(v.x);
  w[1] = pg::as_word(v.y);
  w[2] = pg::as_word(v.z);
}
void put_e(QcProgram& q, int e, int f, int number) {
  put_i(q, e, f, pg::entity_value(q.vm, number));
}
float get_f(QcProgram& q, int e, int f) { return pg::as_float(*field(q, e, f)); }
int32_t get_i(QcProgram& q, int e, int f) { return static_cast<int32_t>(*field(q, e, f)); }
sh::Vec3f get_v(QcProgram& q, int e, int f) {
  const uint32_t* w = field(q, e, f);
  return {pg::as_float(w[0]), pg::as_float(w[1]), pg::as_float(w[2])};
}
// An entity field's number; a value that names no entity in use reads as the world.
int get_e(QcProgram& q, int e, int f) {
  const int n = pg::entity_number(q.vm, get_i(q, e, f));
  return n < 0 ? 0 : n;
}
// The loop keeps these as ints; QC as floats.
int float_int(float v) { return v != v ? 0 : static_cast<int>(v); }
float int_float(int v) { return static_cast<float>(v); }

void mirror_in(QcProgram& q, int e) {
  const sh::Edict& d = q.sv->edicts[static_cast<size_t>(e)];
  put_f(q, e, q.layout->f.modelindex, int_float(d.modelindex));
  put_v(q, e, q.layout->f.absmin, d.absmin);
  put_v(q, e, q.layout->f.absmax, d.absmax);
  put_f(q, e, q.layout->f.ltime, d.ltime);
  put_f(q, e, q.layout->f.movetype, int_float(d.movetype));
  put_f(q, e, q.layout->f.solid, int_float(d.solid));
  put_v(q, e, q.layout->f.origin, d.origin);
  put_v(q, e, q.layout->f.velocity, d.velocity);
  put_v(q, e, q.layout->f.angles, d.angles);
  put_v(q, e, q.layout->f.avelocity, d.avelocity);
  put_v(q, e, q.layout->f.mins, d.mins);
  put_v(q, e, q.layout->f.maxs, d.maxs);
  put_v(q, e, q.layout->f.size, d.size);
  put_i(q, e, q.layout->f.touch, d.touch);
  put_i(q, e, q.layout->f.think, d.think);
  put_i(q, e, q.layout->f.blocked, d.blocked);
  put_f(q, e, q.layout->f.nextthink, d.nextthink);
  put_e(q, e, q.layout->f.groundentity, d.groundentity);
  put_f(q, e, q.layout->f.health, d.health);
  put_f(q, e, q.layout->f.button0, d.button0);
  put_f(q, e, q.layout->f.button1, d.button1);
  put_f(q, e, q.layout->f.button2, d.button2);
  put_f(q, e, q.layout->f.impulse, d.impulse);
  put_f(q, e, q.layout->f.fixangle, d.fixangle ? 1.0f : 0.0f);
  put_v(q, e, q.layout->f.v_angle, d.v_angle);
  if (q.layout->f.punchangle >= 0) put_v(q, e, q.layout->f.punchangle, d.punchangle);
  put_f(q, e, q.layout->f.flags, int_float(d.flags));
  put_f(q, e, q.layout->f.teleport_time, d.teleport_time);
  put_f(q, e, q.layout->f.waterlevel, int_float(d.waterlevel));
  put_f(q, e, q.layout->f.watertype, int_float(d.watertype));
  put_e(q, e, q.layout->f.owner, d.owner);
  if (q.fields.gravity >= 0) put_f(q, e, q.fields.gravity, d.gravity);
  if (q.fields.maxspeed >= 0) put_f(q, e, q.fields.maxspeed, d.maxspeed);
  if (q.fields.movement >= 0) put_v(q, e, q.fields.movement, d.movement);
}

// The loop's own (absmin, absmax: link_edict's) are not read back.
void mirror_out(QcProgram& q, int e) {
  sh::Edict& d = q.sv->edicts[static_cast<size_t>(e)];
  d.modelindex = float_int(get_f(q, e, q.layout->f.modelindex));
  d.ltime = get_f(q, e, q.layout->f.ltime);
  d.movetype = float_int(get_f(q, e, q.layout->f.movetype));
  d.solid = float_int(get_f(q, e, q.layout->f.solid));
  d.origin = get_v(q, e, q.layout->f.origin);
  d.velocity = get_v(q, e, q.layout->f.velocity);
  d.angles = get_v(q, e, q.layout->f.angles);
  d.avelocity = get_v(q, e, q.layout->f.avelocity);
  d.mins = get_v(q, e, q.layout->f.mins);
  d.maxs = get_v(q, e, q.layout->f.maxs);
  d.size = get_v(q, e, q.layout->f.size);
  d.touch = get_i(q, e, q.layout->f.touch);
  d.think = get_i(q, e, q.layout->f.think);
  d.blocked = get_i(q, e, q.layout->f.blocked);
  d.nextthink = get_f(q, e, q.layout->f.nextthink);
  d.groundentity = get_e(q, e, q.layout->f.groundentity);
  d.health = get_f(q, e, q.layout->f.health);
  d.button0 = get_f(q, e, q.layout->f.button0);
  d.button1 = get_f(q, e, q.layout->f.button1);
  d.button2 = get_f(q, e, q.layout->f.button2);
  d.impulse = get_f(q, e, q.layout->f.impulse);
  d.fixangle = get_f(q, e, q.layout->f.fixangle) != 0;
  d.v_angle = get_v(q, e, q.layout->f.v_angle);
  if (q.layout->f.punchangle >= 0) d.punchangle = get_v(q, e, q.layout->f.punchangle);
  d.view_ofs = get_v(q, e, q.layout->f.view_ofs);  // the program's: read, never written
  d.flags = float_int(get_f(q, e, q.layout->f.flags));
  d.teleport_time = get_f(q, e, q.layout->f.teleport_time);
  d.waterlevel = float_int(get_f(q, e, q.layout->f.waterlevel));
  d.watertype = float_int(get_f(q, e, q.layout->f.watertype));
  d.owner = get_e(q, e, q.layout->f.owner);
  if (q.fields.gravity >= 0) d.gravity = get_f(q, e, q.fields.gravity);
  if (q.fields.maxspeed >= 0) d.maxspeed = get_f(q, e, q.fields.maxspeed);
}

// The progs' free rule on the VM's record: these cleared, nextthink -1, the rest kept.
void free_rule(QcProgram& q, int e) {
  for (int f : {q.layout->f.model, q.layout->f.takedamage, q.layout->f.modelindex,
                q.layout->f.colormap, q.layout->f.skin, q.layout->f.frame, q.layout->f.health,
                q.layout->f.classname, q.layout->f.solid}) {
    *field(q, e, f) = 0;
  }
  put_v(q, e, q.layout->f.origin, {});
  put_v(q, e, q.layout->f.angles, {});
  put_f(q, e, q.layout->f.nextthink, -1);
}

// Every entity in use into the VM: one the loop allocated since is cleared first, one it freed
// gets the free rule.
void sync_entities_in(QcProgram& q) {
  sh::Server& sv = *q.sv;
  q.vm.num_entities = sv.num_edicts;
  for (int e = 0; e < sv.num_edicts; e++) {
    const bool used = !sv.edicts[static_cast<size_t>(e)].free || e == 0;
    uint8_t& was = q.live[static_cast<size_t>(e)];
    if (used && was == 0) pg::clear_entity(q.vm, e);
    if (!used && was != 0) free_rule(q, e);
    was = used ? 1 : 0;
    if (used) mirror_in(q, e);
  }
}

void sync_in(QcProgram& q) {
  sh::Server& sv = *q.sv;
  sync_entities_in(q);
  pg::set_global_int(q.vm, q.layout->g.self, pg::entity_value(q.vm, sv.g.self));
  pg::set_global_int(q.vm, q.layout->g.other, pg::entity_value(q.vm, sv.g.other));
  pg::set_global_int(q.vm, q.layout->g.world, 0);
  pg::set_global_float(q.vm, q.layout->g.time, sv.g.time);
  pg::set_global_float(q.vm, q.layout->g.frametime, sv.g.frametime);
  if (q.layout->g.newmis >= 0) {  // the single-player form has none
    pg::set_global_int(q.vm, q.layout->g.newmis, pg::entity_value(q.vm, sv.g.newmis));
  }
  pg::set_global_float(q.vm, q.layout->g.force_retouch, sv.g.force_retouch);
}

int global_entity(QcProgram& q, int ofs) {
  const int n = pg::entity_number(q.vm, pg::global_int(q.vm, ofs));
  return n < 0 ? 0 : n;
}

void sync_out(QcProgram& q) {
  sh::Server& sv = *q.sv;
  for (int e = 1; e < sv.num_edicts; e++) {
    if (q.live[static_cast<size_t>(e)] != 0 && !sv.edicts[static_cast<size_t>(e)].free) {
      mirror_out(q, e);
    }
  }
  sv.g.self = global_entity(q, q.layout->g.self);
  sv.g.other = global_entity(q, q.layout->g.other);
  sv.g.newmis = q.layout->g.newmis >= 0 ? global_entity(q, q.layout->g.newmis) : 0;
  sv.g.force_retouch = pg::global_float(q.vm, q.layout->g.force_retouch);
}

void call(void* ctx, sh::Server& sv, sh::Func fn) {
  auto& q = *static_cast<QcProgram*>(ctx);
  if (q.before_call != nullptr) {
    sh::Server* const was = q.sv;
    q.sv = &sv;  // the hook reads the server it is called under
    q.before_call(q, fn);
    q.sv = was;
  }
  enter_qc(q, sv);
  run_qc(q, fn);
  leave_qc(q);
  if (q.after_call != nullptr) q.after_call(q, fn);
}

QcProgram& of(pg::Vm& vm) { return qc_of(vm); }

}  // namespace

void enter_qc(QcProgram& q, sh::Server& sv) {
  if (q.nesting < kMaxQcNesting) q.outer[q.nesting] = q.sv;
  q.nesting++;
  q.sv = &sv;
  q.calls++;
  q.vm.rng = &q.rng;
  sync_in(q);
}

bool run_qc(QcProgram& q, int32_t fn) {
  if (pg::call_function(q.vm, fn)) return true;
  q.faults++;
  q.last_fault = pg::fault_text(q.vm);
  pg::clear_fault(q.vm);
  return false;
}

void leave_qc(QcProgram& q) {
  sync_out(q);
  q.nesting--;
  q.sv = q.nesting < kMaxQcNesting ? q.outer[q.nesting] : nullptr;
}

void flush_to_world(QcProgram& q) { sync_out(q); }

void refresh_from_world(QcProgram& q) { sync_entities_in(q); }

int entity_parm(pg::Vm& vm, int i, const char* builtin) {
  const int n = pg::entity_number(vm, pg::global_int(vm, pg::parm(i)));
  if (n < 0) {
    const std::string m = std::string(builtin) + ": not an entity in use";
    pg::vm_error(vm, m.c_str());
  }
  return n;
}

int qc_spawn_entity(QcProgram& q) {
  const int e = sh::spawn_edict(*q.sv);
  if (e < 0) return -1;
  q.vm.num_entities = q.sv->num_edicts;
  pg::clear_entity(q.vm, e);
  q.live[static_cast<size_t>(e)] = 1;
  mirror_in(q, e);
  return e;
}

void qc_free_entity(QcProgram& q, int e) {
  sh::free_edict(*q.sv, e);
  free_rule(q, e);
  q.live[static_cast<size_t>(e)] = 0;
}

void qc_spawn(pg::Vm& vm) {
  const int e = qc_spawn_entity(of(vm));
  if (e < 0) {
    pg::vm_error(vm, "spawn: no free entity (the pool is full)");
    return;
  }
  pg::set_global_int(vm, pg::kGlobalReturn, pg::entity_value(vm, e));
}

void qc_remove(pg::Vm& vm) {
  const int e = entity_parm(vm, 0, "remove");
  if (e < 0) return;
  if (e == 0) {
    pg::vm_error(vm, "remove: the world cannot be removed");
    return;
  }
  qc_free_entity(of(vm), e);
}

void qc_setorigin(pg::Vm& vm) {
  QcProgram& q = of(vm);
  const int e = entity_parm(vm, 0, "setorigin");
  if (e < 0) return;
  const uint32_t* w = &vm.globals[static_cast<size_t>(pg::parm(1))];
  const sh::Vec3f o{pg::as_float(w[0]), pg::as_float(w[1]), pg::as_float(w[2])};
  if (e != 0 && q.live[static_cast<size_t>(e)] != 0) mirror_out(q, e);
  sh::set_origin(*q.sv, e, o);
  put_v(q, e, q.layout->f.origin, o);
  put_v(q, e, q.layout->f.absmin, q.sv->edicts[static_cast<size_t>(e)].absmin);
  put_v(q, e, q.layout->f.absmax, q.sv->edicts[static_cast<size_t>(e)].absmax);
}

namespace {

sh::Func hook_global(const pg::Progs& p, int ofs) {
  return static_cast<sh::Func>(p.globals[static_cast<size_t>(ofs)]);
}

sh::Func optional_hook(const pg::Progs& p, const char* name) {
  const int fn = pg::find_function(p, name);
  return fn < 0 ? sh::kNoFunc : fn;
}

int optional_field(const pg::Progs& p, const char* name) {
  const pg::Def* d = pg::find_field(p, name);
  return d == nullptr ? -1 : static_cast<int>(d->offset);
}

}  // namespace

bool create_qc_program(QcProgram& q, const pg::Progs& progs, int max_edicts, std::string& error) {
  if (max_edicts < 1 || max_edicts > pg::kMaxEntitiesDefault) {
    error = "qc program: a pool of " + std::to_string(max_edicts) + " entities is past the VM's " +
            std::to_string(pg::kMaxEntitiesDefault) + "; lower ServerSettings::max_edicts";
    return false;
  }
  pg::VmLimits limits;
  limits.max_entities = max_edicts;
  limits.max_builtins = kQcBuiltins;
  pg::create_vm(q.vm, progs, limits);
  q.vm.user = &q;
  q.layout = progs.layout != nullptr ? progs.layout : &pg::kMultiplayerLayout;
  q.live.assign(static_cast<size_t>(max_edicts), 0);
  OptionalFields& f = q.fields;
  f.items2 = optional_field(progs, "items2");
  f.maxspeed = optional_field(progs, "maxspeed");
  f.gravity = optional_field(progs, "gravity");
  f.movement = optional_field(progs, "movement");
  f.vw_index = optional_field(progs, "vw_index");
  f.hideentity = optional_field(progs, "hideentity");
  f.trackent = optional_field(progs, "trackent");
  f.visclients = optional_field(progs, "visclients");
  f.hideplayers = optional_field(progs, "hideplayers");
  f.teleported = optional_field(progs, "teleported");
  sh::Program& h = q.hooks;
  h = {};
  h.start_frame = hook_global(progs, q.layout->g.start_frame);
  h.player_pre_think = hook_global(progs, q.layout->g.player_pre_think);
  h.player_post_think = hook_global(progs, q.layout->g.player_post_think);
  h.client_kill = hook_global(progs, q.layout->g.client_kill);
  h.client_connect = hook_global(progs, q.layout->g.client_connect);
  h.put_client_in_server = hook_global(progs, q.layout->g.put_client_in_server);
  h.client_disconnect = hook_global(progs, q.layout->g.client_disconnect);
  h.set_new_parms = hook_global(progs, q.layout->g.set_new_parms);
  h.set_change_parms = hook_global(progs, q.layout->g.set_change_parms);
  h.spectator_connect = optional_hook(progs, "SpectatorConnect");
  h.spectator_think = optional_hook(progs, "SpectatorThink");
  h.spectator_disconnect = optional_hook(progs, "SpectatorDisconnect");
  q.calls = 0;
  q.faults = 0;
  q.last_fault.clear();
  return true;
}

sh::Program qc_program(QcProgram& q) {
  sh::Program p = q.hooks;
  p.ctx = &q;
  p.call = &call;
  return p;
}

}  // namespace shooters_edge::sim
