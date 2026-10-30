// The builtins over entities and the world (builtins.hpp; catalogue.hpp states each rule): view
// vectors, models and sizes, traces, the entity walks (find, findradius, nextent, checkclient),
// dropping to the floor, contents, static entities, spawn parms, and the dumps and statement trace.
//
// ge:summary Builtins over entities and the world: traces, finds, models, sizes, statics, dumps
// ge:keywords makevectors setmodel setsize traceline tracebox checkclient find findradius
// ge:keywords droptofloor pointcontents aim nextent makestatic setspawnparms coredump eprint
// ge:keywords traceon traceoff trace-globals chain area-tree autoaim sv_aim
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_builtins_test.cpp
#include <cstring>

#include "core/num_text.hpp"
#include "progs/stdlib.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/server/builtins.hpp"

namespace shooters_edge::server {

namespace {

using sim::flush_to_world;
using sim::qc_of;
using sim::refresh_from_world;

constexpr int kFlagNoTarget = 128;  // a client monsters do not look for (checkclient)

sh::Vec3f vec_arg(const pg::Vm& vm, int i) {
  float v[3];
  pg::arg_vector(vm, i, v);
  return {v[0], v[1], v[2]};
}

void set_global_vec(pg::Vm& vm, int ofs, sh::Vec3f v) {
  pg::set_global_float(vm, ofs, v.x);
  pg::set_global_float(vm, ofs + 1, v.y);
  pg::set_global_float(vm, ofs + 2, v.z);
}

float field_f(const pg::Vm& vm, int e, int f) { return pg::as_float(*pg::entity_field(vm, e, f)); }
sh::Vec3f field_v(const pg::Vm& vm, int e, int f) {
  const uint32_t* w = pg::entity_field(vm, e, f);
  return {pg::as_float(w[0]), pg::as_float(w[1]), pg::as_float(w[2])};
}

bool in_use(const QcServer& s, int e) {
  return e == 0 || (e < s.sv.num_edicts && !s.sv.edicts[static_cast<size_t>(e)].free);
}

// The trace globals from a trace (its `ent` -1 when nothing was hit: the world).
void write_trace(pg::Vm& vm, const sh::HullTrace& tr) {
  const pg::LayoutGlobals& g = pg::vm_layout(vm).g;
  pg::set_global_float(vm, g.trace_allsolid, tr.allsolid ? 1.0f : 0.0f);
  pg::set_global_float(vm, g.trace_startsolid, tr.startsolid ? 1.0f : 0.0f);
  pg::set_global_float(vm, g.trace_fraction, tr.fraction);
  pg::set_global_float(vm, g.trace_inwater, tr.inwater ? 1.0f : 0.0f);
  pg::set_global_float(vm, g.trace_inopen, tr.inopen ? 1.0f : 0.0f);
  set_global_vec(vm, g.trace_endpos, tr.endpos);
  set_global_vec(vm, g.trace_plane_normal, tr.plane.normal);
  pg::set_global_float(vm, g.trace_plane_dist, tr.plane.dist);
  pg::set_global_int(vm, g.trace_ent, pg::entity_value(vm, tr.ent < 0 ? 0 : tr.ent));
}

int trace_kind(float nomonsters) {
  const int k = static_cast<int>(nomonsters);
  return k == 1 ? sh::kTraceNoMonsters : k == 2 ? sh::kTraceMissile : sh::kTraceNormal;
}

// An entity argument that may be the world; -1 after a fault.
int any_entity(pg::Vm& vm, int i, const char* name) { return sim::entity_parm(vm, i, name); }

// A field value as text, by its definition's type (eprint, coredump).
std::string field_text(pg::Vm& vm, int e, const pg::Def& d) {
  const uint32_t* w = pg::entity_field(vm, e, d.offset);
  namespace num = ge::core::num;
  switch (d.type & pg::kTypeMask) {
    case pg::kTypeString: {
      const char* t = pg::vm_string(vm, static_cast<int32_t>(w[0]));
      return t == nullptr ? "(bad string)" : t;
    }
    case pg::kTypeVector:
      return "'" + num::to_string(pg::as_float(w[0])) + " " + num::to_string(pg::as_float(w[1])) +
             " " + num::to_string(pg::as_float(w[2])) + "'";
    case pg::kTypeEntity: {
      const int n = pg::entity_number(vm, static_cast<int32_t>(w[0]));
      return "entity " + std::to_string(n);
    }
    case pg::kTypeFunction: {
      const auto fn = static_cast<int32_t>(w[0]);
      const pg::Progs& p = *vm.progs;
      if (fn > 0 && fn < static_cast<int32_t>(p.functions.size()))
        return std::string(pg::progs_string(p, p.functions[static_cast<size_t>(fn)].name)) + "()";
      return "function " + std::to_string(fn);
    }
    case pg::kTypeField:
      return "." + std::to_string(w[0]);
    default:
      return num::to_string(pg::as_float(w[0]));
  }
}

void dump_entity(QcServer& s, pg::Vm& vm, int e) {
  console_print(s, "entity " + std::to_string(e) + ":");
  for (const pg::Def& d : vm.progs->fielddefs) {
    const char* name = pg::progs_string(*vm.progs, d.name);
    const size_t n = std::strlen(name);
    if (n == 0 || (n > 2 && name[n - 2] == '_' && name[n - 1] >= 'x' && name[n - 1] <= 'z'))
      continue;  // a vector's component definitions: the vector prints whole
    const uint32_t* w = pg::entity_field(vm, e, d.offset);
    const bool zero =
        (d.type & pg::kTypeMask) == pg::kTypeVector ? (w[0] | w[1] | w[2]) == 0 : w[0] == 0;
    if (zero) continue;
    console_print(s, std::string("  ") + name + " " + field_text(vm, e, d));
  }
}

void trace_statement(void* ctx, const pg::Vm& vm, int32_t fn, int32_t statement) {
  auto& s = *static_cast<QcServer*>(ctx);
  if (s.trace_lines >= 10000) return;
  s.trace_lines++;
  const pg::Progs& p = *vm.progs;
  const pg::Statement& st = p.statements[static_cast<size_t>(statement)];
  console_print(s, std::string(pg::progs_string(p, p.functions[static_cast<size_t>(fn)].name)) +
                       " " + std::to_string(statement) + ": op " + std::to_string(st.op) + " " +
                       std::to_string(st.a) + " " + std::to_string(st.b) + " " +
                       std::to_string(st.c));
}

}  // namespace

void bi_makevectors(pg::Vm& vm) {
  const sh::ViewAxes ax = sh::angle_vectors(vec_arg(vm, 0));
  const pg::LayoutGlobals& g = pg::vm_layout(vm).g;
  set_global_vec(vm, g.v_forward, ax.forward);
  set_global_vec(vm, g.v_right, ax.right);
  set_global_vec(vm, g.v_up, ax.up);
}

void bi_setmodel(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = any_entity(vm, 0, "setmodel");
  if (e < 0) return;
  const char* m = pg::arg_string(vm, 1);
  if (vm.fault.set) return;
  int index = -1;
  for (size_t i = 0; i < s.models.size(); i++)
    if (s.models[i] == m) {
      index = static_cast<int>(i);
      break;
    }
  if (index < 0) {
    const std::string msg = std::string("no precache: ") + m;
    pg::vm_error(vm, msg.c_str());
    return;
  }
  *pg::entity_field(vm, e, pg::vm_layout(vm).f.model) = static_cast<uint32_t>(pg::arg_int(vm, 1));
  *pg::entity_field(vm, e, pg::vm_layout(vm).f.modelindex) = pg::as_word(static_cast<float>(index));
  if (m[0] == '*' && index < static_cast<int>(s.sv.models.size())) {
    flush_to_world(qc_of(vm));
    sh::set_brush_model(s.sv, e, index);
    refresh_from_world(qc_of(vm));
  }
}

void bi_setsize(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = any_entity(vm, 0, "setsize");
  if (e < 0) return;
  flush_to_world(qc_of(vm));
  sh::set_size(s.sv, e, vec_arg(vm, 1), vec_arg(vm, 2));
  refresh_from_world(qc_of(vm));
}

void bi_break(pg::Vm& vm) { pg::vm_error(vm, "break statement"); }

// The client a trace shoots for (lag compensation, QcServer::lag_rewind): its pass entity when a
// client, else the pass entity's owner when a client; 0 for none or with no rewind installed.
int lag_shooter(QcServer& s, int pass) {
  if (s.lag_rewind == nullptr || pass <= 0) return 0;
  if (client_slot_of(s, pass) >= 0) return pass;
  const int owner = s.sv.edicts[static_cast<size_t>(pass)].owner;
  return owner > 0 && client_slot_of(s, owner) >= 0 ? owner : 0;
}

void bi_traceline(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int pass = any_entity(vm, 3, "traceline");
  if (pass < 0) return;
  flush_to_world(qc_of(vm));
  const int shooter = lag_shooter(s, pass);
  if (shooter) s.lag_rewind(s.lag_ctx, shooter, true);
  const sh::HullTrace tr = sh::trace_move(s.sv, vec_arg(vm, 0), {}, {}, vec_arg(vm, 1),
                                          trace_kind(pg::arg_float(vm, 2)), pass);
  if (shooter) s.lag_rewind(s.lag_ctx, shooter, false);
  write_trace(vm, tr);
}

void bi_tracebox(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int pass = any_entity(vm, 5, "tracebox");
  if (pass < 0) return;
  flush_to_world(qc_of(vm));
  const int shooter = lag_shooter(s, pass);
  if (shooter) s.lag_rewind(s.lag_ctx, shooter, true);
  const sh::HullTrace tr = sh::trace_move(s.sv, vec_arg(vm, 0), vec_arg(vm, 1), vec_arg(vm, 2),
                                          vec_arg(vm, 3), trace_kind(pg::arg_float(vm, 4)), pass);
  if (shooter) s.lag_rewind(s.lag_ctx, shooter, false);
  write_trace(vm, tr);
}

void bi_checkclient(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int self = pg::entity_number(vm, pg::global_int(vm, pg::vm_layout(vm).g.self));
  // Every 0.1 s of server time the candidate moves on to the next client in use, alive and not
  // flagged no-target (wrapping 1..32); none: it stays.
  if (s.check_time < 0 || s.sv.time - s.check_time >= 0.1) {
    s.check_time = s.sv.time;
    int c = s.check_client;
    for (int i = 0; i < sh::kMaxClients; i++) {
      c = c % sh::kMaxClients + 1;
      if (!in_use(s, c) || client_slot_of(s, c) < 0) continue;
      if (field_f(vm, c, pg::vm_layout(vm).f.health) <= 0) continue;
      if ((static_cast<int>(field_f(vm, c, pg::vm_layout(vm).f.flags)) & kFlagNoTarget) != 0)
        continue;
      s.check_client = c;
      break;
    }
  }
  const int c = s.check_client;
  if (c <= 0 || !in_use(s, c) || field_f(vm, c, pg::vm_layout(vm).f.health) <= 0 || self < 0) {
    pg::return_int(vm, 0);
    return;
  }
  pg::return_int(vm, sees(s, eye_of(s, c), eye_of(s, self)) ? pg::entity_value(vm, c) : 0);
}

void bi_find(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int start = any_entity(vm, 0, "find");
  if (start < 0) return;
  const int32_t f = pg::arg_int(vm, 1);
  if (f < 0 || f >= vm.entityfields) {
    pg::vm_error(vm, "find: the field argument is not a field of the entity");
    return;
  }
  const char* want = pg::arg_string(vm, 2);
  if (vm.fault.set) return;
  for (int e = start + 1; e < s.sv.num_edicts; e++) {
    if (!in_use(s, e)) continue;
    const char* have = pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, e, f)));
    if (have != nullptr && std::strcmp(have, want) == 0) {
      pg::return_int(vm, pg::entity_value(vm, e));
      return;
    }
  }
  pg::return_int(vm, 0);
}

namespace {

// findradius's walk: each node's solid list then its trigger list, then the children the box
// reaches; every entity prepended to the chain (so the chain is the visit order reversed).
void radius_walk(const QcServer& s, pg::Vm& vm, int32_t n, sh::Vec3f lo, sh::Vec3f hi,
                 sh::Vec3f org, float rad2, int32_t& head) {
  const sh::AreaNode& node = s.sv.area.nodes[n];
  for (const sh::AreaList* list : {&node.solids, &node.triggers}) {
    for (int32_t e = list->head; e != -1; e = s.sv.edicts[static_cast<size_t>(e)].link_next) {
      const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
      if (ed.solid == sh::kSolidNot) continue;
      if (ed.absmin.x > hi.x || ed.absmin.y > hi.y || ed.absmin.z > hi.z || ed.absmax.x < lo.x ||
          ed.absmax.y < lo.y || ed.absmax.z < lo.z)
        continue;
      const sh::Vec3f c = ed.origin + (ed.mins + ed.maxs) * 0.5f;
      const sh::Vec3f d = c - org;
      if ((d.x * d.x + d.y * d.y) + d.z * d.z > rad2) continue;
      *pg::entity_field(vm, e, pg::vm_layout(vm).f.chain) = static_cast<uint32_t>(head);
      head = pg::entity_value(vm, e);
    }
  }
  if (node.axis < 0) return;
  if (hi[node.axis] > node.dist) radius_walk(s, vm, node.children[0], lo, hi, org, rad2, head);
  if (lo[node.axis] < node.dist) radius_walk(s, vm, node.children[1], lo, hi, org, rad2, head);
}

}  // namespace

void bi_findradius(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const sh::Vec3f org = vec_arg(vm, 0);
  const float rad = pg::arg_float(vm, 1);
  flush_to_world(qc_of(vm));
  const float grow = rad + 1;
  const sh::Vec3f lo = org - sh::Vec3f{grow, grow, grow}, hi = org + sh::Vec3f{grow, grow, grow};
  int32_t head = 0;
  radius_walk(s, vm, 0, lo, hi, org, rad * rad, head);
  pg::return_int(vm, head);
}

void bi_droptofloor(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int self = pg::entity_number(vm, pg::global_int(vm, pg::vm_layout(vm).g.self));
  if (self < 0) {
    pg::vm_error(vm, "droptofloor: self is not an entity in use");
    return;
  }
  flush_to_world(qc_of(vm));
  sh::Edict& ed = s.sv.edicts[static_cast<size_t>(self)];
  sh::Vec3f end = ed.origin;
  end.z = end.z - 256;
  const sh::HullTrace tr =
      sh::trace_move(s.sv, ed.origin, ed.mins, ed.maxs, end, sh::kTraceNormal, self);
  if (tr.fraction == 1 || tr.allsolid) {
    pg::return_float(vm, 0);
    return;
  }
  ed.origin = tr.endpos;
  sh::link_edict(s.sv, self, false);
  ed.flags |= sh::kFlagOnGround;
  ed.groundentity = tr.ent < 0 ? 0 : tr.ent;
  refresh_from_world(qc_of(vm));
  pg::return_float(vm, 1);
}

void bi_pointcontents(pg::Vm& vm) {
  const QcServer& s = server_of(vm);
  pg::return_float(vm, static_cast<float>(sh::world_contents(s.sv, vec_arg(vm, 0))));
}

namespace {

constexpr float kAimTakedamage = 2;  // takedamage's "aimed at" value
constexpr float kAimEye = 20;        // the line starts this far above the shooter's origin
constexpr float kAimRange = 2048;

sh::Vec3f box_centre(const pg::Vm& vm, const pg::LayoutFields& f, int e) {
  return field_v(vm, e, f.origin) + (field_v(vm, e, f.mins) + field_v(vm, e, f.maxs)) * 0.5f;
}

// The single-player form's autoaim: the entity straight ahead along the line of sight when it
// takes aimed damage; else the one nearest the line of sight (a cosine of at least `sv_aim`) with
// a clear line to it, aimed at by correcting the pitch alone; else straight ahead.
sh::Vec3f autoaim(QcServer& s, pg::Vm& vm, int shooter, sh::Vec3f forward) {
  const ge::console::Cvar* c = s.cvars.find("sv_aim");
  const float least = c != nullptr ? static_cast<float>(c->number) : 0.93f;
  if (least >= 1) return forward;
  const pg::LayoutFields& f = pg::vm_layout(vm).f;
  const ge::console::Cvar* tp = s.cvars.find("teamplay");
  const bool teams = tp != nullptr && tp->on();
  const float team = field_f(vm, shooter, f.team);
  auto aimed_at = [&](int e) {
    if (e <= 0 || e == shooter || !in_use(s, e)) return false;
    if (field_f(vm, e, f.takedamage) != kAimTakedamage) return false;
    return !(teams && team > 0 && field_f(vm, e, f.team) == team);
  };
  flush_to_world(qc_of(vm));
  const sh::Vec3f start = field_v(vm, shooter, f.origin) + sh::Vec3f{0, 0, kAimEye};
  const sh::HullTrace ahead =
      sh::trace_move(s.sv, start, {}, {}, start + forward * kAimRange, sh::kTraceNormal, shooter);
  if (aimed_at(ahead.ent)) return forward;
  float best = least;
  int target = -1;
  for (int e = 1; e < s.sv.num_edicts; e++) {
    if (!aimed_at(e)) continue;
    sh::Vec3f to = box_centre(vm, f, e) - start;
    sh::normalize(to);
    const float cosine = sh::dot(to, forward);
    if (cosine < best) continue;
    const sh::HullTrace tr =
        sh::trace_move(s.sv, start, {}, {}, box_centre(vm, f, e), sh::kTraceNormal, shooter);
    if (tr.ent == e) {
      best = cosine;
      target = e;
    }
  }
  if (target < 0) return forward;
  const sh::Vec3f to = box_centre(vm, f, target) - start;
  const float along = sh::dot(to, forward);
  sh::Vec3f aim{forward.x * along, forward.y * along, to.z};
  sh::normalize(aim);
  return aim;
}

}  // namespace

void bi_aim(pg::Vm& vm) {
  const int forward_at = pg::vm_layout(vm).g.v_forward;
  const sh::Vec3f forward{pg::global_float(vm, forward_at), pg::global_float(vm, forward_at + 1),
                          pg::global_float(vm, forward_at + 2)};
  sh::Vec3f aim = forward;
  if (pg::vm_layout(vm).single_player) {
    const int shooter = any_entity(vm, 0, "aim");
    if (shooter < 0) return;
    aim = autoaim(server_of(vm), vm, shooter, forward);
  }
  pg::return_vector(vm, aim.x, aim.y, aim.z);
}

void bi_nextent(pg::Vm& vm) {
  const QcServer& s = server_of(vm);
  const int start = any_entity(vm, 0, "nextent");
  if (start < 0) return;
  for (int e = start + 1; e < s.sv.num_edicts; e++) {
    if (in_use(s, e)) {
      pg::return_int(vm, pg::entity_value(vm, e));
      return;
    }
  }
  pg::return_int(vm, 0);
}

void bi_makestatic(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = any_entity(vm, 0, "makestatic");
  if (e < 0) return;
  if (e == 0) {
    pg::vm_error(vm, "makestatic: the world cannot be made static");
    return;
  }
  const char* model =
      pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, e, pg::vm_layout(vm).f.model)));
  int index = -1;
  for (size_t i = 1; model != nullptr && model[0] != 0 && i < s.models.size(); i++)
    if (s.models[i] == model) index = static_cast<int>(i);
  if (index > 0 && static_cast<int>(s.statics.size()) < kMaxStatics) {
    StaticEntity st;
    st.modelindex = index;
    st.frame = field_f(vm, e, pg::vm_layout(vm).f.frame);
    st.colormap = field_f(vm, e, pg::vm_layout(vm).f.colormap);
    st.skin = field_f(vm, e, pg::vm_layout(vm).f.skin);
    st.origin = field_v(vm, e, pg::vm_layout(vm).f.origin);
    st.angles = field_v(vm, e, pg::vm_layout(vm).f.angles);
    st.extras = s.extras[static_cast<size_t>(e)];
    s.statics.push_back(st);
    NetEvent ev;
    ev.kind = EventKind::static_entity;
    ev.to = all_clients(s);
    ev.reliable = true;
    ev.time = s.sv.time;
    ev.ent = index;
    ev.origin = st.origin;
    ev.text = model;
    s.out.events.push_back(std::move(ev));
    s.out.events_total++;
  }
  sim::qc_remove(vm);
}

void bi_setspawnparms(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = any_entity(vm, 0, "setspawnparms");
  if (e < 0) return;
  const int slot = client_slot_of(s, e);
  if (slot < 0) {
    pg::vm_error(vm, "setspawnparms: not a client (entities 1..32 in use)");
    return;
  }
  for (int i = 0; i < 16; i++)
    pg::set_global_float(vm, pg::vm_layout(vm).g.parm1 + i, s.clients[slot].spawn_parms[i]);
}

void bi_coredump(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  for (int e = 0; e < s.sv.num_edicts; e++)
    if (in_use(s, e)) dump_entity(s, vm, e);
}

void bi_traceon(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  s.tracing = true;
  s.trace_hooks.ctx = &s;
  s.trace_hooks.statement = &trace_statement;
  vm.trace = &s.trace_hooks;  // from the next engine call on
}

void bi_traceoff(pg::Vm& vm) {
  server_of(vm).tracing = false;
  vm.trace = nullptr;
}

void bi_eprint(pg::Vm& vm) {
  const int e = any_entity(vm, 0, "eprint");
  if (e >= 0) dump_entity(server_of(vm), vm, e);
}

}  // namespace shooters_edge::server
