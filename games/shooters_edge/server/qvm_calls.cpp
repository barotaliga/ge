// The compiled module's system calls (qvm_server.hpp's `qvm_syscall`): the module's view of the
// server, by number. Arguments arrive as raw words (`a[1]` the first): ints, float bits, VM
// addresses; a vector is three float arguments, a pointer to one three floats in memory. Every
// address the server reads or writes is checked against the module's memory (a VM fault naming
// the call otherwise), a null string reads as "", an entity number must be below the entity limit
// and an entity address must land exactly on a record of an entity in use.
//
// THE WORLD: a call that reads where entities are (the traces, dropping to the floor, the step
// move) first writes the module's records to the loop (`qvm_flush`); one that moves something
// writes the loop back into the records after (`qvm_refresh`). A call that may run module code
// again (a touch from a move, the console's `mod`, a user info change) keeps the module's `self`,
// `other` and `time` across it.
//
// What each call does is the server's builtin of the same purpose with the differences the module
// form has: entity numbers in and out (find, findradius and nextclient take and give record
// addresses), results written into buffers the module passes, findradius one step a call, ints
// for the message writes, flags on the prints and stuffcmd, changelevel checking the map exists.
// The extension calls bound through map_extension are the three per-entity value calls (alpha and
// colormod); every other unbound number past 93 stops the module ("bad system call").
//
// ge:summary The compiled module's system calls: entities, traces, prints, messages, cvars, files
// ge:keywords qvm syscall system-call entity_token spawn remove setmodel traceline tracecapsule
// ge:keywords findradius find nextclient infokey cvar argv tokenize readcmd executecmd strlcpy
// ge:keywords strlcat strftime fs_open fs_filelist map_extension extfieldptr alpha colormod
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/libm/libm.hpp"
#include "progs/layout.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/server/builtins.hpp"
#include "shooters_edge/server/qvm_server.hpp"

namespace shooters_edge::server {

namespace {

namespace pg = ge::progs;
namespace libm = ge::core::libm;

const pg::LayoutFields& F = pg::kMultiplayerLayout.f;
const pg::LayoutGlobals& G = pg::kMultiplayerLayout.g;

constexpr int kExtAlpha = 1, kExtColormod = 2;  // MapExtFieldPtr's tokens: 0x4500 | id
constexpr int kBindMapExt = 1, kBindSetExt = 2, kBindGetExt = 3;
constexpr size_t kCaptureBytes = 1340;  // a redirected print's piece
constexpr int kPrintHigh = 2;

const char* const kNames[94] = {"api_version",
                                "dprint",
                                "error",
                                "entity_token",
                                "spawn",
                                "remove",
                                "precache_sound",
                                "precache_model",
                                "lightstyle",
                                "setorigin",
                                "setsize",
                                "setmodel",
                                "bprint",
                                "sprint",
                                "centerprint",
                                "ambientsound",
                                "sound",
                                "traceline",
                                "checkclient",
                                "stuffcmd",
                                "localcmd",
                                "cvar",
                                "cvar_set",
                                "findradius",
                                "walkmove",
                                "droptofloor",
                                "checkbottom",
                                "pointcontents",
                                "nextent",
                                "aim",
                                "makestatic",
                                "setspawnparms",
                                "changelevel",
                                "logfrag",
                                "infokey",
                                "multicast",
                                "disableupdates",
                                "writebyte",
                                "writechar",
                                "writeshort",
                                "writelong",
                                "writeangle",
                                "writecoord",
                                "writestring",
                                "writeentity",
                                "flushsignon",
                                "memset",
                                "memcpy",
                                "strncpy",
                                "sin",
                                "cos",
                                "atan2",
                                "sqrt",
                                "floor",
                                "ceil",
                                "acos",
                                "argc",
                                "argv",
                                "tracecapsule",
                                "fs_open",
                                "fs_close",
                                "fs_read",
                                "fs_write",
                                "fs_seek",
                                "fs_tell",
                                "fs_filelist",
                                "cvar_set_float",
                                "cvar_string",
                                "map_extension",
                                "strcmp",
                                "strncmp",
                                "stricmp",
                                "strnicmp",
                                "find",
                                "executecmd",
                                "conprint",
                                "readcmd",
                                "redirectcmd",
                                "add_bot",
                                "remove_bot",
                                "set_bot_userinfo",
                                "set_bot_cmd",
                                "strftime",
                                "args",
                                "tokenize",
                                "strlcpy",
                                "strlcat",
                                "makevectors",
                                "nextclient",
                                "precache_vwep_model",
                                "setpause",
                                "setuserinfo",
                                "movetogoal",
                                "visibleto"};

QvmServer& srv(void* user) { return *static_cast<QvmServer*>(user); }
pg::Qvm& vm_of(QvmServer& s) { return s.q.vm; }
float fa(const int32_t* a, int i) { return pg::qvm_float(a[i]); }
sh::Vec3f va(const int32_t* a, int i) { return {fa(a, i), fa(a, i + 1), fa(a, i + 2)}; }
int32_t fr(float f) { return pg::qvm_bits(f); }

int32_t fail(QvmServer& s, const std::string& why) {
  pg::qvm_raise(vm_of(s), why);
  return 0;
}

std::string text(QvmServer& s, int32_t addr, const char* what) {
  return std::string(pg::qvm_string(vm_of(s), addr, what));
}

// Three floats at a pointer; zeros after a fault.
sh::Vec3f vec_at(QvmServer& s, int32_t addr, const char* what) {
  const uint8_t* p = pg::qvm_ptr(vm_of(s), addr, 12, what);
  if (p == nullptr) return {};
  float f[3];
  std::memcpy(f, p, 12);
  return {f[0], f[1], f[2]};
}

bool put(QvmServer& s, int32_t buf, int32_t size, std::string_view t, const char* what) {
  if (size < 0) {
    fail(s, std::string(what) + ": a negative buffer size");
    return false;
  }
  return pg::qvm_put_string(vm_of(s), buf, size, t, what);
}

bool has_entities(QvmServer& s, const char* what) {
  if (s.q.data.ents != 0 && s.q.sv != nullptr) return true;
  fail(s, std::string(what) + ": no entities yet (init has not returned the game data block)");
  return false;
}

bool in_use(const QvmServer& s, int e) {
  return e == 0 || (e < s.sv.num_edicts && !s.sv.edicts[static_cast<size_t>(e)].free);
}

// An entity number argument: below the limit (and in use when `used`); -1 after a fault.
int number_arg(QvmServer& s, int32_t n, const char* what, bool used = true) {
  if (!has_entities(s, what)) return -1;
  if (n < 0 || n >= s.q.max_edicts || (used && !in_use(s, n))) {
    fail(s, std::string(what) + ": entity " + std::to_string(n) + " is not an entity in use");
    return -1;
  }
  return n;
}

// An entity address argument (exactly a record of an entity in use); -1 after a fault.
int address(QvmServer& s, int32_t addr, const char* what) {
  if (!has_entities(s, what)) return -1;
  const int n = sim::entity_from_address(s.q, addr);
  if (n < 0) {
    fail(s, std::string(what) + ": address " + std::to_string(addr) + " is not an entity's record");
    return -1;
  }
  return n;
}

int32_t addr_of(const QvmServer& s, int e) { return sim::record_of(s.q, e); }

void push(QvmServer& s, NetEvent ev) {
  ev.time = s.sv.time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

int client_slot(const QvmServer& s, int e) {
  if (e < 1 || e > sh::kMaxClients) return -1;
  return s.clients[e - 1].active ? e - 1 : -1;
}

uint32_t everyone(const QvmServer& s) {
  uint32_t to = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) to |= 1u << i;
  return to;
}

sh::Vec3f eye(const QvmServer& s, int e) {
  return sim::field_v(s.q, e, F.origin) + sim::field_v(s.q, e, F.view_ofs);
}

bool sees_point(const QvmServer& s, sh::Vec3f from, sh::Vec3f to, bool see) {
  return s.vis == nullptr || s.vis(s.vis_ctx, from, to, see);
}

// The client a spectator in `slot` tracks (its trackent, a client number), or -1.
int tracked_by(const QvmServer& s, int slot) {
  if (s.q.fields.trackent == 0) return -1;
  const int n = sim::ext_i(s.q, slot + 1, s.q.fields.trackent);
  return n >= 1 && n <= sh::kMaxClients ? n : -1;
}

uint32_t reaching(const QvmServer& s, sh::Vec3f where, bool see) {
  constexpr float kHearing = 1024;
  uint32_t to = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& ci = s.clients[i];
    if (!ci.active) continue;
    int viewer = i + 1;
    if (ci.spectator) {
      const int t = tracked_by(s, i);
      if (t > 0) viewer = t;
    }
    const sh::Vec3f from = eye(s, viewer);
    bool reach = sees_point(s, from, where, see);
    if (!see && !reach) {
      const sh::Vec3f d = from - where;
      reach = (d.x * d.x + d.y * d.y) + d.z * d.z <= kHearing * kHearing;
    }
    if (reach) to |= 1u << i;
  }
  return to;
}

int list_index(const std::vector<std::string>& list, std::string_view name) {
  for (size_t i = 1; i < list.size(); i++)
    if (list[i] == name) return static_cast<int>(i);
  return -1;
}

// The module's self, other and time, kept across a call that may run module code.
struct Kept {
  int32_t self, other, time;
};
Kept keep(const QvmServer& s) {
  return {sim::global_i(s.q, G.self), sim::global_i(s.q, G.other), sim::global_i(s.q, G.time)};
}
void restore(QvmServer& s, const Kept& k) {
  sim::set_global_i(s.q, G.self, k.self);
  sim::set_global_i(s.q, G.other, k.other);
  sim::set_global_i(s.q, G.time, k.time);
}

// ---- the entity text ----

int32_t entity_token(QvmServer& s, int32_t buf, int32_t size) {
  const std::string& t = s.entities;
  size_t& i = s.entity_at;
  std::string tok;
  for (;;) {
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n')) i++;
    if (i + 1 < t.size() && t[i] == '/' && t[i + 1] == '/') {
      while (i < t.size() && t[i] != '\n') i++;
      continue;
    }
    break;
  }
  if (i >= t.size()) {
    put(s, buf, size, "", "entity_token");
    return 0;
  }
  if (t[i] == '"') {
    i++;
    while (i < t.size() && t[i] != '"') {
      if (tok.size() < 1023) tok += t[i];
      i++;
    }
    if (i < t.size()) i++;
  } else {
    while (i < t.size() && t[i] != ' ' && t[i] != '\t' && t[i] != '\r' && t[i] != '\n') {
      if (tok.size() < 1023) tok += t[i];
      i++;
    }
  }
  put(s, buf, size, tok, "entity_token");
  return 1;
}

// ---- entities ----

int32_t spawn(QvmServer& s) {
  if (!has_entities(s, "spawn")) return 0;
  const Kept k = keep(s);
  const int e = sim::qvm_spawn_entity(s.q);
  restore(s, k);
  if (e < 0) return fail(s, "spawn: no free entity (the pool is full)");
  s.extras[static_cast<size_t>(e)] = pg::EntityExtras{};
  return e;
}

void remove(QvmServer& s, int32_t n) {
  const int e = number_arg(s, n, "remove", false);
  if (e < 0) return;
  if (e == 0) {
    fail(s, "remove: the world cannot be removed");
    return;
  }
  if (!in_use(s, e)) return;
  sim::qvm_free_entity(s.q, e);
  s.extras[static_cast<size_t>(e)] = pg::EntityExtras{};
}

void setorigin(QvmServer& s, int e, sh::Vec3f o) {
  sim::qvm_flush_entity(s.q, e);
  sh::set_origin(s.sv, e, o);
  sim::qvm_refresh_entity(s.q, e);
}

void setsize(QvmServer& s, int e, sh::Vec3f mins, sh::Vec3f maxs) {
  sim::qvm_flush_entity(s.q, e);
  sh::set_size(s.sv, e, mins, maxs);
  sim::qvm_refresh_entity(s.q, e);
}

void setmodel(QvmServer& s, int e, int32_t name_at) {
  const std::string m = text(s, name_at, "setmodel");
  if (!vm_of(s).fault.empty()) return;
  const int index = m.empty() ? 0 : list_index(s.models, m);
  if (index < 0) {
    fail(s, "setmodel: no precache: " + m);
    return;
  }
  const int32_t slot = sim::text_slot(s.q, e, F.model);
  if (slot >= 0) pg::qvm_set_word(vm_of(s), slot, name_at);
  sim::set_field_f(s.q, e, F.modelindex, static_cast<float>(index));
  if (!m.empty() && m[0] == '*' && index < static_cast<int>(s.sv.models.size())) {
    sim::qvm_flush_entity(s.q, e);
    sh::set_brush_model(s.sv, e, index);
    sim::qvm_refresh_entity(s.q, e);
  }
}

int32_t nextent(QvmServer& s, int32_t n) {
  if (!has_entities(s, "nextent")) return 0;
  for (int e = (n < 0 ? 0 : n) + 1; e < s.sv.num_edicts && e < s.q.max_edicts; e++)
    if (in_use(s, e)) return e;
  return 0;
}

int32_t nextclient(QvmServer& s, int32_t addr) {
  const int start = address(s, addr, "nextclient");
  if (start < 0) return 0;
  for (int e = start + 1; e <= sh::kMaxClients; e++)
    if (client_slot(s, e) >= 0 && in_use(s, e)) return addr_of(s, e);
  return 0;
}

int32_t find(QvmServer& s, int32_t start_at, int32_t fofs, int32_t want_at) {
  const int start = address(s, start_at, "find");
  if (start < 0) return 0;
  if (want_at == 0) return fail(s, "find: a null string to look for");
  if (fofs < 0 || fofs + 4 > s.q.data.entity_size)
    return fail(s, "find: the field offset " + std::to_string(fofs) + " is outside the record");
  const std::string want = text(s, want_at, "find");
  for (int e = start + 1; e < s.sv.num_edicts && e < s.q.max_edicts; e++) {
    if (!in_use(s, e)) continue;
    const int32_t at = pg::qvm_word(vm_of(s), sim::record_of(s.q, e) + fofs);
    if (at <= 0) continue;
    const std::string_view have = pg::qvm_string(vm_of(s), at, "find");
    if (!vm_of(s).fault.empty()) return 0;
    if (have == want) return addr_of(s, e);
  }
  return 0;
}

int32_t findradius(QvmServer& s, int32_t start_at, int32_t org_at, float rad) {
  const int start = address(s, start_at, "findradius");
  if (start < 0) return 0;
  const sh::Vec3f org = vec_at(s, org_at, "findradius");
  if (!vm_of(s).fault.empty()) return 0;
  for (int e = start + 1; e < s.sv.num_edicts && e < s.q.max_edicts; e++) {
    if (!in_use(s, e) || e == 0) continue;
    if (static_cast<int>(sim::field_f(s.q, e, F.solid)) == sh::kSolidNot) continue;
    const sh::Vec3f c = sim::field_v(s.q, e, F.origin) +
                        (sim::field_v(s.q, e, F.mins) + sim::field_v(s.q, e, F.maxs)) * 0.5f;
    const sh::Vec3f d = org - c;
    const float len2 = (d.x * d.x + d.y * d.y) + d.z * d.z;
    const auto len = static_cast<float>(std::sqrt(static_cast<double>(len2)));
    if (len > rad) continue;
    return addr_of(s, e);
  }
  return 0;
}

int32_t checkclient(QvmServer& s) {
  if (!has_entities(s, "checkclient")) return 0;
  constexpr int kFlagNoTarget = 128;
  const int self = sim::global_entity(s.q, G.self);
  auto alive = [&](int c) {
    return in_use(s, c) && client_slot(s, c) >= 0 && sim::field_f(s.q, c, F.health) > 0 &&
           (static_cast<int>(sim::field_f(s.q, c, F.flags)) & kFlagNoTarget) == 0;
  };
  if (s.check_time < 0 || s.sv.time - s.check_time >= 0.1) {
    s.check_time = s.sv.time;
    int c = s.check_client;
    for (int i = 0; i < sh::kMaxClients; i++) {
      c = c % sh::kMaxClients + 1;
      if (!alive(c)) continue;
      s.check_client = c;
      break;
    }
  }
  const int c = s.check_client;
  if (c <= 0 || !alive(c)) return 0;
  return sees_point(s, eye(s, c), eye(s, self), true) ? c : 0;
}

void makestatic(QvmServer& s, int e) {
  if (e == 0) {
    fail(s, "makestatic: the world cannot be made static");
    return;
  }
  const std::string model(sim::entity_text(s.q, e, F.model));
  const int index = model.empty() ? -1 : list_index(s.models, model);
  if (index > 0 && static_cast<int>(s.statics.size()) < kMaxStatics) {
    StaticEntity st;
    st.modelindex = index;
    st.frame = sim::field_f(s.q, e, F.frame);
    st.colormap = sim::field_f(s.q, e, F.colormap);
    st.skin = sim::field_f(s.q, e, F.skin);
    st.origin = sim::field_v(s.q, e, F.origin);
    st.angles = sim::field_v(s.q, e, F.angles);
    st.extras = s.extras[static_cast<size_t>(e)];
    s.statics.push_back(st);
    NetEvent ev;
    ev.kind = EventKind::static_entity;
    ev.to = everyone(s);
    ev.reliable = true;
    ev.ent = index;
    ev.origin = st.origin;
    ev.text = model;
    push(s, std::move(ev));
  }
  remove(s, e);
}

void setspawnparms(QvmServer& s, int e) {
  const int slot = client_slot(s, e);
  if (slot < 0) {
    fail(s, "setspawnparms: not a client (entities 1..32 in use)");
    return;
  }
  for (int i = 0; i < 16; i++) sim::set_global_f(s.q, G.parm1 + i, s.clients[slot].spawn_parms[i]);
}

void makevectors(QvmServer& s, int32_t at) {
  const sh::Vec3f angles = vec_at(s, at, "makevectors");
  if (!vm_of(s).fault.empty() || s.q.data.globals == 0) return;
  const sh::ViewAxes ax = sh::angle_vectors(angles);
  sim::set_global_v(s.q, G.v_forward, ax.forward);
  sim::set_global_v(s.q, G.v_right, ax.right);
  sim::set_global_v(s.q, G.v_up, ax.up);
}

int32_t visibleto(QvmServer& s, int viewer, int first, int32_t len, int32_t buf) {
  if (len < 0) return fail(s, "visibleto: a negative length");
  uint8_t* out = pg::qvm_ptr(vm_of(s), buf, static_cast<uint32_t>(len), "visibleto");
  if (out == nullptr) return 0;
  std::memset(out, 0, static_cast<size_t>(len));
  const sh::Vec3f from = eye(s, viewer);
  const int stop = std::min(first + len, std::min(s.sv.num_edicts, s.q.max_edicts));
  for (int e = first < 0 ? 0 : first; e < stop; e++) {
    if (!in_use(s, e)) continue;
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (e != 0 && ed.area_node < 0) continue;
    if (e >= 1 && e <= sh::kMaxClients && client_slot(s, e) < 0) continue;
    const sh::Vec3f c = (ed.absmin + ed.absmax) * 0.5f;
    if (sees_point(s, from, c, true)) out[e - first] = 1;
  }
  return 0;
}

// ---- movement and traces ----

int lag_shooter(QvmServer& s, int pass) {
  if (s.lag_rewind == nullptr || pass <= 0) return 0;
  const int slot = client_slot(s, pass);
  if (slot >= 0) return s.clients[slot].bot ? 0 : pass;
  const int owner = s.sv.edicts[static_cast<size_t>(pass)].owner;
  return owner > 0 && client_slot(s, owner) >= 0 && !s.clients[owner - 1].bot ? owner : 0;
}

int trace_kind(int32_t nomonsters) {
  return nomonsters == 1   ? sh::kTraceNoMonsters
         : nomonsters == 2 ? sh::kTraceMissile
                           : sh::kTraceNormal;
}

void write_trace(QvmServer& s, const sh::HullTrace& tr) {
  sim::QvmProgram& q = s.q;
  sim::set_global_f(q, G.trace_allsolid, tr.allsolid ? 1.0f : 0.0f);
  sim::set_global_f(q, G.trace_startsolid, tr.startsolid ? 1.0f : 0.0f);
  sim::set_global_f(q, G.trace_fraction, tr.fraction);
  sim::set_global_f(q, G.trace_inwater, tr.inwater ? 1.0f : 0.0f);
  sim::set_global_f(q, G.trace_inopen, tr.inopen ? 1.0f : 0.0f);
  sim::set_global_v(q, G.trace_endpos, tr.endpos);
  sim::set_global_v(q, G.trace_plane_normal, tr.plane.normal);
  sim::set_global_f(q, G.trace_plane_dist, tr.plane.dist);
  sim::set_global_i(q, G.trace_ent, sim::entity_value(q, tr.ent < 0 ? 0 : tr.ent));
}

void trace(QvmServer& s, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs, sh::Vec3f end,
           int32_t nomonsters, int32_t pass_n, bool lag) {
  const int pass = number_arg(s, pass_n, "traceline");
  if (pass < 0) return;
  sim::qvm_flush(s.q);
  const int shooter = lag ? lag_shooter(s, pass) : 0;
  if (shooter) s.lag_rewind(s.lag_ctx, shooter, true);
  const sh::HullTrace tr =
      sh::trace_move(s.sv, start, mins, maxs, end, trace_kind(nomonsters), pass);
  if (shooter) s.lag_rewind(s.lag_ctx, shooter, false);
  write_trace(s, tr);
}

int32_t walkmove(QvmServer& s, int e, float yaw, float dist) {
  constexpr double kRadians = 3.14159265358979323846 * 2 / 360;
  sim::qvm_flush(s.q);
  const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
  if ((ed.flags & (sh::kFlagOnGround | sh::kFlagFly | sh::kFlagSwim)) == 0) return 0;
  const double r = static_cast<double>(yaw) * kRadians;
  const sh::Vec3f move{static_cast<float>(libm::cos(r) * dist),
                       static_cast<float>(libm::sin(r) * dist), 0};
  const Kept k = keep(s);
  const int enemy = sim::entity_from_value(s.q, sim::field_i(s.q, e, F.enemy));
  const bool ok = step_move(s.sv, e, move, enemy < 0 ? 0 : enemy, true);
  sim::qvm_refresh(s.q);
  restore(s, k);
  return ok ? 1 : 0;
}

int32_t droptofloor(QvmServer& s, int e) {
  sim::qvm_flush(s.q);
  sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
  sh::Vec3f end = ed.origin;
  end.z = end.z - 256;
  const sh::HullTrace tr =
      sh::trace_move(s.sv, ed.origin, ed.mins, ed.maxs, end, sh::kTraceNormal, e);
  if (tr.fraction == 1 || tr.allsolid) return 0;
  ed.origin = tr.endpos;
  sh::link_edict(s.sv, e, false);
  ed.flags |= sh::kFlagOnGround;
  ed.groundentity = tr.ent < 0 ? 0 : tr.ent;
  sim::qvm_refresh(s.q);
  return 1;
}

// ---- sound and precache ----

bool good_name(std::string_view n) { return !n.empty() && static_cast<unsigned char>(n[0]) > ' '; }

void precache(QvmServer& s, int32_t at, std::vector<std::string>& list, int cap, const char* what) {
  const std::string name = text(s, at, what);
  if (!vm_of(s).fault.empty()) return;
  if (!s.loading) {
    fail(s, std::string(what) + ": only while the map loads");
    return;
  }
  if (!good_name(name)) {
    fail(s, std::string(what) + ": Bad string");
    return;
  }
  if (list_index(list, name) > 0) return;
  if (static_cast<int>(list.size()) >= cap) {
    fail(s, std::string(what) + ": the list is full (" + std::to_string(cap) + ")");
    return;
  }
  list.push_back(name);
}

int32_t precache_vwep(QvmServer& s, int32_t at) {
  const std::string name = text(s, at, "precache_vwep_model");
  if (!vm_of(s).fault.empty()) return 0;
  if (!s.loading) return fail(s, "precache_vwep_model: only while the map loads");
  if (!good_name(name) || name.find_first_of("\"; \n\t") != std::string::npos)
    return fail(s, "precache_vwep_model: bad name (empty, or a quote, ';', space or newline)");
  if (static_cast<int>(s.vwep.size()) >= kMaxVwep)
    return fail(s, "precache_vwep_model: the list is full (32)");
  s.vwep.push_back(name);
  return static_cast<int32_t>(s.vwep.size() - 1);
}

void sound(QvmServer& s, int e, int32_t channel, int32_t sample_at, float volume,
           float attenuation) {
  const std::string sample = text(s, sample_at, "sound");
  if (!vm_of(s).fault.empty()) return;
  const auto vol255 = static_cast<int32_t>(static_cast<double>(volume) * 255);
  if (vol255 < 0 || vol255 > 255) {
    fail(s, "sound: volume out of 0..1");
    return;
  }
  if (!(attenuation >= 0 && attenuation <= 4)) {
    fail(s, "sound: attenuation out of 0..4");
    return;
  }
  if (channel < 0 || channel > 15) {
    fail(s, "sound: channel out of 0..15");
    return;
  }
  const int index = list_index(s.sounds, sample);
  if (index < 0) {
    qvm_print(s, "no precache: " + sample);
    return;
  }
  sh::Vec3f at = sim::field_v(s.q, e, F.origin);
  const int solid = static_cast<int>(sim::field_f(s.q, e, F.solid));
  const int modelindex = static_cast<int>(sim::field_f(s.q, e, F.modelindex));
  if (solid == sh::kSolidBsp || (solid == sh::kSolidTrigger && modelindex == 0))
    at = at + (sim::field_v(s.q, e, F.mins) + sim::field_v(s.q, e, F.maxs)) * 0.5f;
  const bool all = (channel & 8) != 0 || !s.cvars.find("sv_phs")->on();
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.reliable = (channel & 8) != 0;
  ev.to = all ? everyone(s) : reaching(s, at, false);
  ev.ent = e;
  ev.level = channel & 7;
  ev.origin = at;
  ev.values[0] = static_cast<float>(vol255);
  ev.values[1] = attenuation;
  ev.text = sample;
  push(s, std::move(ev));
  sh::start_sound(s.sv, e, channel & 7, s.sounds[static_cast<size_t>(index)].c_str(), volume,
                  attenuation);
}

void ambientsound(QvmServer& s, sh::Vec3f pos, int32_t sample_at, float vol, float att) {
  const std::string sample = text(s, sample_at, "ambientsound");
  if (!vm_of(s).fault.empty()) return;
  if (list_index(s.sounds, sample) < 0) {
    qvm_print(s, "no precache: " + sample);
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::ambient;
  ev.to = everyone(s);
  ev.reliable = true;
  ev.origin = pos;
  ev.values[0] = static_cast<float>(static_cast<int32_t>(static_cast<double>(vol) * 255));
  ev.values[1] = static_cast<float>(static_cast<int32_t>(static_cast<double>(att) * 64));
  ev.text = sample;
  push(s, std::move(ev));
}

void lightstyle(QvmServer& s, int32_t style, int32_t at) {
  const std::string value = text(s, at, "lightstyle");
  if (!vm_of(s).fault.empty()) return;
  if (style < 0 || style >= kLightStyles) {
    fail(s, "lightstyle: style out of 0..63");
    return;
  }
  s.lightstyles[style] = value;
  if (s.loading) return;  // the signon carries the styles a loading map sets
  NetEvent ev;
  ev.kind = EventKind::lightstyle;
  ev.to = everyone(s);
  ev.reliable = true;
  ev.level = style;
  ev.text = value;
  push(s, std::move(ev));
}

// ---- prints ----

void bprint(QvmServer& s, int32_t level, int32_t at, int32_t flags) {
  const std::string t = text(s, at, "bprint");
  if (!vm_of(s).fault.empty()) return;
  if ((flags & 8) == 0 && !t.empty()) qvm_print(s, t);
  if ((flags & 2) != 0) return;  // not to clients
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = everyone(s);
  ev.reliable = true;
  ev.level = level;
  ev.text = t;
  push(s, std::move(ev));
}

void sprint(QvmServer& s, int32_t n, int32_t level, int32_t at) {
  const std::string t = text(s, at, "sprint");
  if (!vm_of(s).fault.empty()) return;
  if (client_slot(s, n) < 0) {
    qvm_print(s, "tried to sprint to a non-client");
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = 1u << (n - 1);
  ev.reliable = true;
  ev.ent = n;
  ev.level = level;
  ev.text = t;
  push(s, std::move(ev));
}

void centerprint(QvmServer& s, int32_t n, int32_t at) {
  const std::string t = text(s, at, "centerprint");
  if (!vm_of(s).fault.empty()) return;
  if (client_slot(s, n) < 0) {
    qvm_print(s, "tried to centerprint to a non-client");
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = 1u << (n - 1);
  ev.reliable = true;
  ev.ent = n;
  ev.text = t;
  push(s, std::move(ev));
}

void stuffcmd(QvmServer& s, int32_t n, int32_t at, int32_t flags) {
  const std::string t = text(s, at, "stuffcmd");
  if (!vm_of(s).fault.empty() || (flags & 2) != 0) return;  // flag 2: the demo's alone
  const int slot = client_slot(s, n);
  if (slot < 0) {
    fail(s, "stuffcmd: entity " + std::to_string(n) + " is not a client");
    return;
  }
  ClientInfo& c = s.clients[slot];
  if (t == "disconnect\n") {
    c.drop = true;
    NetEvent ev;
    ev.kind = EventKind::drop;
    ev.to = 1u << slot;
    ev.ent = n;
    push(s, std::move(ev));
    return;
  }
  if (c.stuff.size() + t.size() > static_cast<size_t>(kStuffBytes) * 4) {
    qvm_print(s, "stuffcmd: the client's pending command text overflowed; dropped");
    c.stuff.clear();
    return;
  }
  c.stuff += t;
  const size_t nl = c.stuff.rfind('\n');
  if (nl == std::string::npos) return;
  NetEvent ev;
  ev.kind = EventKind::stuffcmd;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.ent = n;
  ev.text = c.stuff.substr(0, nl + 1);
  c.stuff.erase(0, nl + 1);
  push(s, std::move(ev));
}

void logfrag(QvmServer& s, int32_t killer_n, int32_t killed_n) {
  const int killer = client_slot(s, killer_n), killed = client_slot(s, killed_n);
  if (killer < 0 || killed < 0) return;
  s.frag_log.push_back("\\" + info_value(s.clients[killer].userinfo, "name") + "\\" +
                       info_value(s.clients[killed].userinfo, "name") + "\\\n");
}

// ---- messages ----

void put_le(std::vector<uint8_t>& b, uint32_t v, int bytes) {
  for (int i = 0; i < bytes; i++) b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
}

std::vector<uint8_t>* dest_buffer(QvmServer& s, int32_t dest) {
  switch (dest) {
    case 0:
      return &s.out.unreliable;
    case 1: {
      const int e = sim::global_entity(s.q, G.msg_entity);
      if (client_slot(s, e) < 0) {
        fail(s, "write to one client: msg_entity is not a client (1..32)");
        return nullptr;
      }
      return &s.out.one[e - 1];
    }
    case 2:
      return &s.out.all;
    case 3:
      if (!s.loading) {
        fail(s, "write to the signon (destination 3) only while the map loads");
        return nullptr;
      }
      return &s.out.init;
    case 4:
      return &s.out.multicast;
    default:
      fail(s, "write: bad destination " + std::to_string(dest) + " (0..4)");
      return nullptr;
  }
}

void write(QvmServer& s, int number, const int32_t* a) {
  if (!has_entities(s, "write")) return;
  std::vector<uint8_t>* b = dest_buffer(s, a[1]);
  if (b == nullptr) return;
  const auto v = static_cast<uint32_t>(a[2]);
  switch (number) {
    case 37:
    case 38:
      put_le(*b, v, 1);
      break;
    case 39:
    case 44:
      put_le(*b, v, 2);
      break;
    case 40:
      put_le(*b, v, 4);
      break;
    case 41:
      encode(*b, WriteKind::angle, fa(a, 2), "");
      break;
    case 42:
      encode(*b, WriteKind::coord, fa(a, 2), "");
      break;
    case 43: {
      const std::string t = text(s, a[2], "writestring");
      if (vm_of(s).fault.empty()) encode(*b, WriteKind::string, 0, t.c_str());
      break;
    }
    default:
      break;
  }
}

void multicast(QvmServer& s, sh::Vec3f at, int32_t to) {
  if (to < 0 || to > 5) {
    fail(s, "multicast: bad destination (0..5)");
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::multicast;
  ev.reliable = to >= 3;
  const int kind = to % 3;
  ev.to = kind == 0 ? everyone(s) : reaching(s, at, kind == 2);
  ev.level = to;
  ev.origin = at;
  ev.bytes = s.out.multicast;
  s.out.multicast.clear();
  push(s, std::move(ev));
}

// ---- cvars and the console ----

void cvar_set(QvmServer& s, const std::string& name, const std::string& value) {
  if (s.cvars.find(name) == nullptr) {
    if (s.cvars.find("developer")->on()) qvm_print(s, "cvar_set: variable " + name + " not found");
    return;
  }
  s.cvars.set(name, value, ge::console::Source::code);
  settings_from_cvars(s, s.sv.set, true);
}

std::string float_text(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.8g", static_cast<double>(v));
  return buf;
}

// Runs console lines with the module's view of the world kept consistent around them.
void run_lines(QvmServer& s) {
  sim::qvm_flush(s.q);
  const Kept k = keep(s);
  qvm_run_buffer(s);
  if (s.q.data.ents != 0) restore(s, k);
  sim::qvm_refresh(s.q);
}

int32_t readcmd(QvmServer& s, int32_t line_at, int32_t buf, int32_t size) {
  const std::string line = text(s, line_at, "readcmd");
  if (!vm_of(s).fault.empty()) return 0;
  run_lines(s);
  const bool was = s.capturing;
  const std::string outer = s.capture;
  s.capturing = true;
  s.capture.clear();
  s.command_buffer += line;
  if (s.command_buffer.empty() || s.command_buffer.back() != '\n') s.command_buffer += '\n';
  run_lines(s);
  const std::string got = s.capture;
  s.capturing = was;
  s.capture = outer;
  put(s, buf, size, got, "readcmd");
  return 0;
}

void redirectcmd(QvmServer& s, int32_t client_at, int32_t line_at) {
  const int e = address(s, client_at, "redirectcmd");
  if (e < 0) return;
  const int slot = client_slot(s, e);
  if (slot < 0) {
    fail(s, "redirectcmd: not a client");
    return;
  }
  const std::string line = text(s, line_at, "redirectcmd");
  if (!vm_of(s).fault.empty()) return;
  if (s.capturing) {
    s.command_buffer += line + "\n";
    run_lines(s);
    return;
  }
  s.capturing = true;
  s.capture.clear();
  s.command_buffer += line + "\n";
  run_lines(s);
  s.capturing = false;
  for (size_t at = 0, pieces = 0; at < s.capture.size() && pieces < 128;
       at += kCaptureBytes, pieces++) {
    NetEvent ev;
    ev.kind = EventKind::print;
    ev.to = 1u << slot;
    ev.reliable = true;
    ev.level = kPrintHigh;
    ev.text = s.capture.substr(at, kCaptureBytes);
    push(s, std::move(ev));
  }
  s.capture.clear();
}

int32_t map_extension(QvmServer& s, int32_t name_at, int32_t num) {
  if (name_at == 0) return num < 94 ? -2 : -1;
  const std::string name = text(s, name_at, "map_extension");
  if (!vm_of(s).fault.empty()) return 0;
  if (num < 256 || num - 256 >= 256) return -2;
  int id = 0;
  if (name == "MapExtFieldPtr") id = kBindMapExt;
  if (name == "SetExtFieldPtr") id = kBindSetExt;
  if (name == "GetExtFieldPtr") id = kBindGetExt;
  if (id == 0) return -1;
  s.bound[num - 256] = id;
  return num;
}

int32_t ext_token(QvmServer& s, int32_t name_at) {
  const std::string name = text(s, name_at, "MapExtFieldPtr");
  if (name == "alpha") return 0x4500 | kExtAlpha;
  if (name == "colormod") return 0x4500 | kExtColormod;
  return 0;
}

int32_t ext_copy(QvmServer& s, bool set, int32_t ent_at, int32_t token, int32_t mem, int32_t size) {
  const char* what = set ? "SetExtFieldPtr" : "GetExtFieldPtr";
  const int e = address(s, ent_at, what);
  if (e < 0) return 0;
  const int id = (token & ~0xff) == 0x4500 ? token & 0xff : 0;
  const bool ok =
      (id == kExtAlpha && size == 4) || (id == kExtColormod && (size == 4 || size == 12));
  if (!ok) {
    qvm_print(s, std::string(what) + ": Corrupt field reference or size out of bounds");
    return 0;
  }
  uint8_t* p = pg::qvm_ptr(vm_of(s), mem, static_cast<uint32_t>(size), what);
  if (p == nullptr) return 0;
  pg::EntityExtras& x = s.extras[static_cast<size_t>(e)];
  float v[3] = {0, 0, 0};
  if (id == kExtAlpha) {
    if (set) {
      std::memcpy(v, p, 4);
      x.has_alpha = v[0] != 0;
      x.alpha = v[0];
    } else {
      v[0] = x.has_alpha ? x.alpha : 0.0f;
      std::memcpy(p, v, 4);
    }
  } else {
    if (set) {
      std::memcpy(v, p, static_cast<size_t>(size));
      x.has_colormod = true;
      for (int i = 0; i < size / 4; i++) x.colormod[i] = v[i];
    } else {
      for (int i = 0; i < 3; i++) v[i] = x.has_colormod ? x.colormod[i] : 0.0f;
      std::memcpy(p, v, static_cast<size_t>(size));
    }
  }
  return 1;
}

// ---- info strings ----

std::string infokey(QvmServer& s, int32_t n, const std::string& key) {
  if (n == 0) {
    if (!key.empty() && key[0] == '\\') {
      static constexpr const char* kKnown[] = {
          "date_str", "ip",     "realip",  "download", "ping",      "*userid", "login",
          "*VIP",     "*state", "netname", "mapname",  "modelname", "version", "servername"};
      const std::string k = key.substr(1);
      for (const char* x : kKnown)
        if (k == x) return "yes";
      return "no";
    }
    if (key == "date_str") return date_text(s.opt.clock(s.opt.clock_ctx, s.sv.time));
    if (key == "mapname") return s.map_name;
    if (key == "modelname") return "maps/" + s.map_name + ".bsp";
    if (key == "version") return s.opt.version;
    if (key == "servername") {
      const ge::console::Cvar* h = s.cvars.find("hostname");
      return h != nullptr ? h->value : "";
    }
    std::string v = info_value(s.serverinfo, key);
    if (v.empty()) v = info_value(s.localinfo, key);
    return v;
  }
  const int slot = client_slot(s, n);
  if (n < 1 || n > sh::kMaxClients) return {};
  if (key == "*state") return slot < 0 ? "free" : "spawned";
  if (slot < 0) return {};
  const ClientInfo& c = s.clients[slot];
  if (key == "ip" || key == "realip") {
    const size_t colon = c.address.rfind(':');
    return colon == std::string::npos ? c.address : c.address.substr(0, colon);
  }
  if (key == "download") return "-1";
  if (key == "ping") return std::to_string(c.ping_ms);
  if (key == "*userid") return std::to_string(c.userid);
  if (key == "login") return "";
  if (key == "*VIP") return "0";
  if (key == "netname") return info_value(c.userinfo, "name");
  return info_value(c.userinfo, key);
}

void setuserinfo(QvmServer& s, int32_t n, int32_t key_at, int32_t value_at, int32_t flags) {
  const std::string key = text(s, key_at, "setuserinfo");
  const std::string value = text(s, value_at, "setuserinfo");
  if (!vm_of(s).fault.empty()) return;
  if (key.find("&c") != std::string::npos || key.find("&r") != std::string::npos ||
      value.find("&c") != std::string::npos || value.find("&r") != std::string::npos)
    return;
  const int slot = client_slot(s, n);
  if (slot < 0) {
    qvm_print(s, "setuserinfo: not a client");
    return;
  }
  (void)flags;  // a star key is kept like any other
  sim::qvm_flush(s.q);
  const Kept k = keep(s);
  qvm_set_user_info(s, slot, key.substr(0, 64), value.substr(0, 64));
  restore(s, k);
  sim::qvm_refresh(s.q);
}

// ---- files ----

bool refused_name(const std::string& n) {
  return n.empty() || n[0] == '/' || n[0] == '\\' || (n.size() > 1 && n[1] == ':') ||
         n.find("..") != std::string::npos;
}

void close_file(QvmFile& f) {
  if (f.open && f.write && !f.path.empty()) {
    if (std::FILE* fp = std::fopen(f.path.c_str(), "ab")) {
      std::fwrite(f.bytes.data(), 1, f.bytes.size(), fp);
      std::fclose(fp);
    }
  }
  f = QvmFile{};
}

int32_t fs_open(QvmServer& s, int32_t name_at, int32_t handle_at, int32_t mode) {
  uint8_t* h = pg::qvm_ptr(vm_of(s), handle_at, 4, "fs_open");
  if (h == nullptr) return 0;
  std::memset(h, 0, 4);
  const std::string name = text(s, name_at, "fs_open");
  if (!vm_of(s).fault.empty() || refused_name(name) || mode < 0 || mode > 5) return -1;
  int slot = -1;
  for (int i = 0; i < kQvmMaxFiles && slot < 0; i++)
    if (!s.files[i].open) slot = i;
  if (slot < 0) return -1;
  QvmFile& f = s.files[slot];
  int32_t result = 0;
  if (mode <= 1) {
    if (s.opt.read_file == nullptr || !s.opt.read_file(s.opt.files_ctx, name, f.bytes)) return -1;
    result = static_cast<int32_t>(f.bytes.size());
  } else {
    if (s.opt.write_dir.empty()) return -1;
    const std::filesystem::path p = std::filesystem::path(s.opt.write_dir) / name;
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    const bool append = mode >= 4;
    if (!append) {
      std::FILE* fp = std::fopen(p.string().c_str(), "wb");
      if (fp == nullptr) return -1;
      std::fclose(fp);
    } else {
      result = static_cast<int32_t>(
          std::filesystem::exists(p, ec) ? std::filesystem::file_size(p, ec) : 0);
    }
    f.write = true;
    f.path = p.string();
  }
  f.open = true;
  const int32_t handle = slot + 1;
  std::memcpy(h, &handle, 4);
  return result;
}

QvmFile* file_of(QvmServer& s, int32_t handle) {
  if (handle < 1 || handle > kQvmMaxFiles || !s.files[handle - 1].open) return nullptr;
  return &s.files[handle - 1];
}

int32_t fs_read(QvmServer& s, int32_t buf, int32_t count, int32_t handle) {
  if (count < 0) return fail(s, "fs_read: a negative count");
  uint8_t* p = pg::qvm_ptr(vm_of(s), buf, static_cast<uint32_t>(count), "fs_read");
  QvmFile* f = file_of(s, handle);
  if (p == nullptr || f == nullptr || f->write) return 0;
  const size_t n = std::min(static_cast<size_t>(count), f->bytes.size() - f->pos);
  std::memcpy(p, f->bytes.data() + f->pos, n);
  f->pos += n;
  return static_cast<int32_t>(n);
}

int32_t fs_write(QvmServer& s, int32_t buf, int32_t count, int32_t handle) {
  if (count < 0) return fail(s, "fs_write: a negative count");
  const uint8_t* p = pg::qvm_ptr(vm_of(s), buf, static_cast<uint32_t>(count), "fs_write");
  QvmFile* f = file_of(s, handle);
  if (p == nullptr || f == nullptr || !f->write) return 0;
  f->bytes.insert(f->bytes.end(), p, p + count);
  return count;
}

int32_t fs_seek(QvmServer& s, int32_t handle, int32_t offset, int32_t origin) {
  QvmFile* f = file_of(s, handle);
  if (f == nullptr || f->write) return 0;
  const auto size = static_cast<int64_t>(f->bytes.size());
  int64_t to = origin == 0   ? static_cast<int64_t>(f->pos) + offset
               : origin == 1 ? size + offset
               : origin == 2 ? offset
                             : -1;
  if (to < 0) return 0;
  f->pos = static_cast<size_t>(std::min(to, size));
  return 0;
}

bool ends_with_ci(const std::string& s, const std::string& tail) {
  if (s.size() < tail.size()) return false;
  for (size_t i = 0; i < tail.size(); i++) {
    const char a = s[s.size() - tail.size() + i], b = tail[i];
    if ((a | 0x20) != (b | 0x20)) return false;
  }
  return true;
}

int32_t fs_filelist(QvmServer& s, int32_t path_at, int32_t pattern_at, int32_t buf, int32_t size,
                    int32_t flags) {
  const std::string path = text(s, path_at, "fs_filelist");
  const std::string pattern = text(s, pattern_at, "fs_filelist");
  if (!vm_of(s).fault.empty()) return 0;
  if (size < 0) return fail(s, "fs_filelist: a negative size");
  uint8_t* out = pg::qvm_ptr(vm_of(s), buf, static_cast<uint32_t>(size), "fs_filelist");
  if (out == nullptr) return 0;
  if (path.find("..") != std::string::npos || path.find("::") != std::string::npos) return 0;
  std::string ext;  // "\.bsp$" → ".bsp"; ".*" → everything
  if (pattern != ".*") {
    if (pattern.size() < 4 || pattern.compare(0, 2, "\\.") != 0 || pattern.back() != '$') {
      qvm_print(s, "fs_filelist: the pattern " + pattern +
                       " is not one this server lists (.* or \\.<ext>$)");
      return 0;
    }
    ext = "." + pattern.substr(2, pattern.size() - 3);
  }
  std::vector<std::string> names;
  const size_t dirs =
      (flags & 1) != 0 ? std::min<size_t>(1, s.opt.list_dirs.size()) : s.opt.list_dirs.size();
  for (size_t d = 0; d < dirs && names.size() < 4096; d++) {
    std::error_code ec;
    const std::filesystem::path base = std::filesystem::path(s.opt.list_dirs[d]) / path;
    for (const auto& it : std::filesystem::directory_iterator(base, ec)) {
      std::string n = it.path().filename().string();
      if (!ext.empty() && !ends_with_ci(n, ext)) continue;
      if ((flags & 4) == 0) {
        const size_t dot = n.rfind('.');
        if (dot != std::string::npos && dot > 0) n.erase(dot);
      }
      if ((flags & 2) != 0) n.insert(0, s.opt.list_dirs[d] + "/");
      names.push_back(n);
      if (names.size() >= 4096) break;
    }
  }
  std::sort(names.begin(), names.end());
  int32_t count = 0;
  size_t at = 0;
  for (size_t i = 0; i < names.size(); i++) {
    const std::string& n = names[i];
    if (n.empty() || (i > 0 && n == names[i - 1])) continue;
    if (at + n.size() + 1 > static_cast<size_t>(size)) break;
    std::memcpy(out + at, n.c_str(), n.size() + 1);
    at += n.size() + 1;
    count++;
  }
  return count;
}

// ---- the C library ----

// The length of the text at `addr` but at most `cap`, the bytes checked; -1 after a fault.
int64_t bounded_len(QvmServer& s, int32_t addr, int64_t cap, const char* what) {
  const auto a = static_cast<uint32_t>(addr);
  const auto mem = static_cast<int64_t>(vm_of(s).mem.size());
  if (a >= vm_of(s).mem.size()) {
    fail(s, std::string(what) + ": address " + std::to_string(addr) + " is outside the memory");
    return -1;
  }
  const int64_t room = std::min(cap, mem - a);
  const void* z = std::memchr(vm_of(s).mem.data() + a, 0, static_cast<size_t>(room));
  if (z == nullptr) {
    if (room < cap) {
      fail(s, std::string(what) + ": the text runs off the module's memory");
      return -1;
    }
    return cap;
  }
  return static_cast<const uint8_t*>(z) - (vm_of(s).mem.data() + a);
}

int32_t strlcpy_(QvmServer& s, int32_t dst, int32_t src, int32_t size) {
  const std::string t = text(s, src, "strlcpy");
  if (!vm_of(s).fault.empty()) return 0;
  if (size > 0) put(s, dst, size, t, "strlcpy");
  return static_cast<int32_t>(t.size());
}

int32_t strlcat_(QvmServer& s, int32_t dst, int32_t src, int32_t size) {
  const std::string t = text(s, src, "strlcat");
  if (!vm_of(s).fault.empty() || size < 0) return 0;
  uint8_t* d = pg::qvm_ptr(vm_of(s), dst, static_cast<uint32_t>(size), "strlcat");
  if (d == nullptr) return 0;
  const size_t have = strnlen(reinterpret_cast<const char*>(d), static_cast<size_t>(size));
  if (have == static_cast<size_t>(size)) return static_cast<int32_t>(have + t.size());
  const size_t room = static_cast<size_t>(size) - have - 1;
  const size_t n = std::min(room, t.size());
  std::memcpy(d + have, t.data(), n);
  d[have + n] = 0;
  return static_cast<int32_t>(have + t.size());
}

int32_t compare(QvmServer& s, int32_t a_at, int32_t b_at, int64_t n, bool fold, const char* what) {
  const int64_t la = bounded_len(s, a_at, n, what), lb = bounded_len(s, b_at, n, what);
  if (la < 0 || lb < 0) return 0;
  const uint8_t* a = vm_of(s).mem.data() + static_cast<uint32_t>(a_at);
  const uint8_t* b = vm_of(s).mem.data() + static_cast<uint32_t>(b_at);
  for (int64_t i = 0; i < n; i++) {
    int x = i < la ? a[i] : 0, y = i < lb ? b[i] : 0;
    if (fold) {
      if (x >= 'A' && x <= 'Z') x += 32;
      if (y >= 'A' && y <= 'Z') y += 32;
    }
    if (x != y) return x < y ? -1 : 1;
    if (x == 0) return 0;
  }
  return 0;
}

int day_of_year(const DateTime& d) {
  static constexpr int kBefore[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  const bool leap = (d.year % 4 == 0 && d.year % 100 != 0) || d.year % 400 == 0;
  const int m = d.month >= 1 && d.month <= 12 ? d.month - 1 : 0;
  return kBefore[m] + d.day + (leap && m >= 2 ? 1 : 0);
}

int32_t strftime_(QvmServer& s, int32_t buf, int32_t size, int32_t format_at, int32_t offset) {
  const std::string format = text(s, format_at, "strftime");
  if (!vm_of(s).fault.empty()) return 0;
  if (size <= 0) return 0;
  const DateTime d = s.opt.clock(s.opt.clock_ctx, s.sv.time + offset);
  const std::string full = date_text(d);  // "Mon Jan 02, 15:04:05 2006"
  static constexpr const char* kDays[7] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                           "Thursday", "Friday", "Saturday"};
  static constexpr const char* kMonths[12] = {"January",   "February", "March",    "April",
                                              "May",       "June",     "July",     "August",
                                              "September", "October",  "November", "December"};
  const std::string wday = full.substr(0, 3);
  int weekday = 0;
  for (int i = 0; i < 7; i++)
    if (std::string_view(kDays[i]).substr(0, 3) == wday) weekday = i;
  const int month = d.month >= 1 && d.month <= 12 ? d.month - 1 : 0;
  auto two = [](int v) {
    char b[8];
    std::snprintf(b, sizeof b, "%02d", v);
    return std::string(b);
  };
  std::string out;
  for (size_t i = 0; i < format.size(); i++) {
    if (format[i] != '%' || i + 1 >= format.size()) {
      out += format[i];
      continue;
    }
    const char c = format[++i];
    switch (c) {
      case 'Y':
        out += std::to_string(d.year);
        break;
      case 'y':
        out += two(d.year % 100);
        break;
      case 'm':
        out += two(d.month);
        break;
      case 'd':
        out += two(d.day);
        break;
      case 'e':
        out += (d.day < 10 ? " " : "") + std::to_string(d.day);
        break;
      case 'H':
        out += two(d.hour);
        break;
      case 'I':
        out += two(d.hour % 12 == 0 ? 12 : d.hour % 12);
        break;
      case 'p':
        out += d.hour < 12 ? "AM" : "PM";
        break;
      case 'M':
        out += two(d.minute);
        break;
      case 'S':
        out += two(d.second);
        break;
      case 'a':
        out += std::string(kDays[weekday]).substr(0, 3);
        break;
      case 'A':
        out += kDays[weekday];
        break;
      case 'b':
        out += std::string(kMonths[month]).substr(0, 3);
        break;
      case 'B':
        out += kMonths[month];
        break;
      case 'j': {
        char b[8];
        std::snprintf(b, sizeof b, "%03d", day_of_year(d));
        out += b;
        break;
      }
      case 'Z':
        out += "UTC";
        break;
      case '%':
        out += '%';
        break;
      default:
        out += '%';
        out += c;
        break;
    }
  }
  if (out.size() + 1 > static_cast<size_t>(size)) {
    put(s, buf, size, "", "strftime");
    return 0;
  }
  put(s, buf, size, out, "strftime");
  return static_cast<int32_t>(out.size());
}

int32_t libm_call(int number, const int32_t* a) {
  const double x = fa(a, 1);
  double r = 0;
  switch (number) {
    case 49:
      r = libm::sin(x);
      break;
    case 50:
      r = libm::cos(x);
      break;
    case 51:
      r = libm::atan2(x, static_cast<double>(fa(a, 2)));
      break;
    case 52:
      r = std::sqrt(x);
      break;
    case 53:
      r = std::floor(x);
      break;
    case 54:
      r = std::ceil(x);
      break;
    case 55:
      r = libm::acos(x);
      break;
    default:
      break;
  }
  return fr(static_cast<float>(r));
}

int32_t dispatch(QvmServer& s, int number, const int32_t* a) {
  sim::QvmProgram& q = s.q;
  switch (number) {
    case 0:
      return kQvmApiVersion;
    case 1: {
      const std::string t = text(s, a[1], "dprint");
      if (s.cvars.find("developer")->on()) qvm_print(s, t);
      return 0;
    }
    case 2: {
      const std::string t = text(s, a[1], "error");
      s.map_stopped = true;
      s.stop_reason = "Program error: " + t;
      qvm_print(s, s.stop_reason);
      return fail(s, s.stop_reason);
    }
    case 3:
      return entity_token(s, a[1], a[2]);
    case 4:
      return spawn(s);
    case 5:
      remove(s, a[1]);
      return 0;
    case 6:
      precache(s, a[1], s.sounds, kMaxSounds, "precache_sound");
      return 0;
    case 7:
      precache(s, a[1], s.models, kMaxModels, "precache_model");
      return 0;
    case 8:
      lightstyle(s, a[1], a[2]);
      return 0;
    case 9:
      if (const int e = number_arg(s, a[1], "setorigin"); e >= 0) setorigin(s, e, va(a, 2));
      return 0;
    case 10:
      if (const int e = number_arg(s, a[1], "setsize"); e >= 0) setsize(s, e, va(a, 2), va(a, 5));
      return 0;
    case 11:
      if (const int e = number_arg(s, a[1], "setmodel"); e >= 0) setmodel(s, e, a[2]);
      return 0;
    case 12:
      bprint(s, a[1], a[2], a[3]);
      return 0;
    case 13:
      sprint(s, a[1], a[2], a[3]);
      return 0;
    case 14:
      centerprint(s, a[1], a[2]);
      return 0;
    case 15:
      ambientsound(s, va(a, 1), a[4], fa(a, 5), fa(a, 6));
      return 0;
    case 16:
      if (const int e = number_arg(s, a[1], "sound"); e >= 0)
        sound(s, e, a[2], a[3], fa(a, 4), fa(a, 5));
      return 0;
    case 17:
      trace(s, va(a, 1), {}, {}, va(a, 4), a[7], a[8], true);
      return 0;
    case 18:
      return checkclient(s);
    case 19:
      stuffcmd(s, a[1], a[2], a[3]);
      return 0;
    case 20:
      s.command_buffer += text(s, a[1], "localcmd");
      return 0;
    case 21: {
      const ge::console::Cvar* c = s.cvars.find(text(s, a[1], "cvar"));
      return fr(c == nullptr ? 0.0f : static_cast<float>(c->number));
    }
    case 22:
      cvar_set(s, text(s, a[1], "cvar_set"), text(s, a[2], "cvar_set"));
      return 0;
    case 23:
      return findradius(s, a[1], a[2], fa(a, 3));
    case 24:
      if (const int e = number_arg(s, a[1], "walkmove"); e >= 0)
        return walkmove(s, e, fa(a, 2), fa(a, 3));
      return 0;
    case 25:
      if (const int e = number_arg(s, a[1], "droptofloor"); e >= 0) return droptofloor(s, e);
      return 0;
    case 26:
      if (const int e = number_arg(s, a[1], "checkbottom"); e >= 0) {
        sim::qvm_flush(q);
        return check_bottom(s.sv, e) ? 1 : 0;
      }
      return 0;
    case 27:
      return has_entities(s, "pointcontents") ? sh::world_contents(s.sv, va(a, 1)) : 0;
    case 28:
      return nextent(s, a[1]);
    case 29:
      return 0;
    case 30:
      if (const int e = number_arg(s, a[1], "makestatic"); e >= 0) makestatic(s, e);
      return 0;
    case 31:
      if (const int e = number_arg(s, a[1], "setspawnparms"); e >= 0) setspawnparms(s, e);
      return 0;
    case 32: {
      const std::string map = text(s, a[1], "changelevel");
      const std::string ent = text(s, a[2], "changelevel");
      if (!vm_of(s).fault.empty() || s.changelevel_queued) return 0;
      std::vector<uint8_t> probe;
      if (s.opt.read_file != nullptr &&
          !s.opt.read_file(s.opt.files_ctx, "maps/" + map + ".bsp", probe)) {
        qvm_print(s, "Can't find maps/" + map + ".bsp");
        return 0;
      }
      s.changelevel_queued = true;
      s.command_buffer += "map " + map + (ent.empty() ? "" : " " + ent) + "\n";
      return 0;
    }
    case 33:
      logfrag(s, a[1], a[2]);
      return 0;
    case 34: {
      const std::string key = text(s, a[2], "infokey");
      if (!vm_of(s).fault.empty()) return 0;
      put(s, a[3], a[4], infokey(s, a[1], key), "infokey");
      return 0;
    }
    case 35:
      multicast(s, va(a, 1), a[4]);
      return 0;
    case 36:
      return 0;  // disableupdates: every client gets every update here
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
      write(s, number, a);
      return 0;
    case 45:
      return 0;  // flushsignon: the signon is one stream here
    case 46: {
      if (a[3] < 0) return fail(s, "memset: a negative count");
      uint8_t* p = pg::qvm_ptr(vm_of(s), a[1], static_cast<uint32_t>(a[3]), "memset");
      if (p != nullptr) std::memset(p, a[2] & 0xff, static_cast<size_t>(a[3]));
      return a[1];
    }
    case 47: {
      if (a[3] < 0) return fail(s, "memcpy: a negative count");
      uint8_t* d = pg::qvm_ptr(vm_of(s), a[1], static_cast<uint32_t>(a[3]), "memcpy");
      const uint8_t* src = pg::qvm_ptr(vm_of(s), a[2], static_cast<uint32_t>(a[3]), "memcpy");
      if (d != nullptr && src != nullptr) std::memmove(d, src, static_cast<size_t>(a[3]));
      return a[1];
    }
    case 48: {
      if (a[3] < 0) return fail(s, "strncpy: a negative count");
      uint8_t* d = pg::qvm_ptr(vm_of(s), a[1], static_cast<uint32_t>(a[3]), "strncpy");
      const int64_t n = bounded_len(s, a[2], a[3], "strncpy");
      if (d == nullptr || n < 0) return a[1];
      std::memmove(d, vm_of(s).mem.data() + static_cast<uint32_t>(a[2]), static_cast<size_t>(n));
      std::memset(d + n, 0, static_cast<size_t>(a[3] - n));
      return a[1];
    }
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
      return libm_call(number, a);
    case 56:
      return s.words.count;
    case 57: {
      const char* w = a[1] >= 0 && a[1] < s.words.count ? pg::word(s.words, a[1]) : "";
      put(s, a[2], a[3], w, "argv");
      return 0;
    }
    case 58:
      trace(s, va(a, 1), va(a, 9), va(a, 12), va(a, 4), a[7], a[8], false);
      return 0;
    case 59:
      return fs_open(s, a[1], a[2], a[3]);
    case 60:
      if (QvmFile* f = file_of(s, a[1])) close_file(*f);
      return 0;
    case 61:
      return fs_read(s, a[1], a[2], a[3]);
    case 62:
      return fs_write(s, a[1], a[2], a[3]);
    case 63:
      return fs_seek(s, a[1], a[2], a[3]);
    case 64: {
      const QvmFile* f = file_of(s, a[1]);
      return f == nullptr ? 0 : static_cast<int32_t>(f->write ? f->bytes.size() : f->pos);
    }
    case 65:
      return fs_filelist(s, a[1], a[2], a[3], a[4], a[5]);
    case 66:
      cvar_set(s, text(s, a[1], "cvar_set_float"), float_text(fa(a, 2)));
      return 0;
    case 67: {
      const ge::console::Cvar* c = s.cvars.find(text(s, a[1], "cvar_string"));
      put(s, a[2], a[3], c == nullptr ? "" : c->value, "cvar_string");
      return 0;
    }
    case 68:
      return map_extension(s, a[1], a[2]);
    case 69:
      return compare(s, a[1], a[2], INT64_MAX / 2, false, "strcmp");
    case 70:
      return a[3] <= 0 ? 0 : compare(s, a[1], a[2], a[3], false, "strncmp");
    case 71:
      return compare(s, a[1], a[2], INT64_MAX / 2, true, "stricmp");
    case 72:
      return a[3] <= 0 ? 0 : compare(s, a[1], a[2], a[3], true, "strnicmp");
    case 73:
      return find(s, a[1], a[2], a[3]);
    case 74:
      run_lines(s);
      return 0;
    case 75:
      qvm_print(s, text(s, a[1], "conprint"));
      return 0;
    case 76:
      return readcmd(s, a[1], a[2], a[3]);
    case 77:
      redirectcmd(s, a[1], a[2]);
      return 0;
    case 78:
      return 0;  // add_bot: the module's own bots are not run here (no free slot)
    case 79:
    case 80:
    case 81:
      qvm_print(s, std::string(kNames[number]) + ": not a bot");
      return 0;
    case 82:
      return strftime_(s, a[1], a[2], a[3], a[4]);
    case 83:
      put(s, a[1], a[2], s.words.args, "args");
      return 0;
    case 84: {
      const std::string t = text(s, a[1], "tokenize");
      pg::tokenize_words(t.c_str(), s.words);
      return s.words.count;
    }
    case 85:
      return strlcpy_(s, a[1], a[2], a[3]);
    case 86:
      return strlcat_(s, a[1], a[2], a[3]);
    case 87:
      makevectors(s, a[1]);
      return 0;
    case 88:
      return nextclient(s, a[1]);
    case 89:
      return precache_vwep(s, a[1]);
    case 90: {
      const bool want = a[1] != 0;
      if (want != s.paused) qvm_pause(s, -1, want);
      return 0;
    }
    case 91:
      setuserinfo(s, a[1], a[2], a[3], a[4]);
      return 0;
    case 92:
      return 0;  // movetogoal: the module's own monsters' chase is not run here
    case 93: {
      const int viewer = number_arg(s, a[1], "visibleto");
      return viewer < 0 ? 0 : visibleto(s, viewer, a[2], a[3], a[4]);
    }
    default:
      break;
  }
  if (number >= 256 && number < 512) {
    switch (s.bound[number - 256]) {
      case kBindMapExt:
        return ext_token(s, a[1]);
      case kBindSetExt:
        return ext_copy(s, true, a[1], a[2], a[3], a[4]);
      case kBindGetExt:
        return ext_copy(s, false, a[1], a[2], a[3], a[4]);
      default:
        break;
    }
  }
  return fail(s, "bad system call " + std::to_string(number) +
                     " (not one this server answers, nor bound by map_extension)");
}

}  // namespace

int32_t qvm_syscall(void* user, pg::Qvm& /*vm*/, const int32_t* args) {
  QvmServer& s = srv(user);
  const int number = args[0];
  if (number >= 0 && number < kQvmSyscalls) s.syscall_counts[number]++;
  return dispatch(s, number, args);
}

const char* qvm_syscall_name(int number) {
  if (number >= 0 && number < 94) return kNames[number];
  if (number >= 256 && number < 512) return "extension";
  return "";
}

void qvm_close_files(QvmServer& s) {
  for (QvmFile& f : s.files) close_file(f);
}

}  // namespace shooters_edge::server
