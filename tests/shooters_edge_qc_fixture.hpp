// The mod server's tests' fixture: the test mod on the arena (or a map's entity text of the test's
// own), with clients, and a way to call one builtin by its number as QC would, inside a call's
// bracket (the loop's state mirrored in, then out), reading its result back.
//
// ge:summary Tests' fixture: the test mod's server on the arena, and builtins called by number
// ge:keywords test fixture qc-server builtin-call test-mod arena
// ge:skill progs shooter testing
#pragma once

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace qcfix {

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

// An argument as QC passes it: a float, a vector, text (an engine string) or an entity.
struct Arg {
  int kind = 0;  // 0 float, 1 vector, 2 text, 3 entity, 4 a raw word (a field's offset)
  float v[3] = {0, 0, 0};
  std::string text;
  int ent = 0;
  Arg(float f) { v[0] = f; }                       // NOLINT: argument lists read best
  Arg(int i) { v[0] = static_cast<float>(i); }     // NOLINT
  Arg(sh::Vec3f x) : kind(1), v{x.x, x.y, x.z} {}  // NOLINT
  Arg(const char* t) : kind(2), text(t) {}         // NOLINT
  Arg(const std::string& t) : kind(2), text(t) {}  // NOLINT
};
inline Arg ent(int e) {
  Arg a(0);
  a.kind = 3;
  a.ent = e;
  return a;
}
inline Arg raw(int word) {
  Arg a(0);
  a.kind = 4;
  a.ent = word;
  return a;
}

struct Fixture {
  pg::Progs progs;
  std::unique_ptr<sv::QcServer> s;
  std::string error;

  // The test mod (or `mod`, when given) on the arena with `entities`, the console lines run first.
  explicit Fixture(const char* entities = sv::kArenaEntities,
                   const std::vector<std::string>& console = {}, const pg::Progs* mod = nullptr) {
    if (mod != nullptr)
      progs = *mod;
    else
      CHECK_MSG(sv::build_test_mod(progs, error), error);
    sv::QcServerOptions o;
    o.loop.max_edicts = 1024;
    s = sv::make_qc_server(progs, o, error);
    CHECK_MSG(s != nullptr, error);
    if (!s) return;
    for (const std::string& l : console) sv::server_command(*s, l);
    sv::MapSpec map;
    map.models = sv::arena_models();
    map.entities = entities;
    loaded = sv::load_map(*s, map, error);
  }
  bool loaded = false;

  pg::Vm& vm() { return s->q.vm; }

  // A client in slot `slot` (entity slot + 1) through the server's connect, then mirrored into
  // the VM (a mod without connect hooks makes no call that would), so a test may set its fields.
  int join(int slot, const char* name, bool spectator = false) {
    sv::connect(*s, slot, name, spectator, true);
    shooters_edge::sim::enter_qc(s->q, s->sv);
    shooters_edge::sim::leave_qc(s->q);
    return slot + 1;
  }

  // Builtin `number` with `args`, `self` as QC's self, as one engine call would run it. The
  // result is read into `rf`, `rv`, `rs` and `re`; a fault leaves `fault` set (and cleared in the
  // VM).
  void call(int number, std::initializer_list<Arg> args, int self = 0) {
    sv::QcServer& q = *s;
    q.sv.g.self = self;
    q.sv.g.other = 0;
    q.sv.g.time = static_cast<float>(q.sv.time);
    shooters_edge::sim::enter_qc(q.q, q.sv);
    pg::Vm& m = q.q.vm;
    int i = 0;
    for (const Arg& a : args) {
      if (a.kind == 2)
        pg::set_global_int(m, pg::parm(i), pg::engine_string(m, a.text.c_str()));
      else if (a.kind == 3)
        pg::set_global_int(m, pg::parm(i), pg::entity_value(m, a.ent));
      else if (a.kind == 4)
        pg::set_global_int(m, pg::parm(i), a.ent);
      else
        for (int k = 0; k < (a.kind == 1 ? 3 : 1); k++)
          pg::set_global_float(m, pg::parm(i) + k, a.v[k]);
      i++;
    }
    m.argc = i;
    const pg::Builtin fn = number < static_cast<int>(m.builtins.size())
                               ? m.builtins[static_cast<size_t>(number)].fn
                               : nullptr;
    CHECK_MSG(fn != nullptr, "builtin " + std::to_string(number) + " is not installed");
    if (fn != nullptr) fn(m);
    fault = m.fault.set;
    fault_message = fault ? m.fault.message : "";
    pg::clear_fault(m);
    rf = pg::global_float(m, pg::kGlobalReturn);
    rv = {pg::global_float(m, pg::kGlobalReturn), pg::global_float(m, pg::kGlobalReturn + 1),
          pg::global_float(m, pg::kGlobalReturn + 2)};
    const char* t = pg::vm_string(m, pg::global_int(m, pg::kGlobalReturn));
    rs = t == nullptr ? "" : t;
    re = pg::entity_number(m, pg::global_int(m, pg::kGlobalReturn));
    shooters_edge::sim::leave_qc(q.q);
  }
  // Runs `body` inside a call's bracket, as QC code would run: what it writes to a mirrored field
  // (origin, maxspeed, ...) reaches the loop when the call ends.
  template <typename F>
  void in_call(F body) {
    shooters_edge::sim::enter_qc(s->q, s->sv);
    body();
    shooters_edge::sim::leave_qc(s->q);
  }

  float rf = 0;
  sh::Vec3f rv;
  std::string rs;
  int re = 0;
  bool fault = false;
  std::string fault_message;

  float f(int e, int field, int k = 0) { return pg::as_float(pg::entity_field(vm(), e, field)[k]); }
  void set_f(int e, int field, float v, int k = 0) {
    pg::entity_field(vm(), e, field)[k] = pg::as_word(v);
  }
  float g(int ofs) { return pg::global_float(vm(), ofs); }
  std::string str_field(int e, int field) {
    const char* t = pg::vm_string(vm(), static_cast<int32_t>(*pg::entity_field(vm(), e, field)));
    return t == nullptr ? "" : t;
  }
  int field(const char* name) {
    const pg::Def* d = pg::find_field(progs, name);
    return d == nullptr ? -1 : d->offset;
  }
  // A new entity (spawn, builtin 14), its number.
  int spawn() {
    call(14, {});
    return re;
  }
  // The events of one kind this frame.
  std::vector<const sv::NetEvent*> events(sv::EventKind k) const {
    std::vector<const sv::NetEvent*> out;
    for (const sv::NetEvent& e : s->out.events)
      if (e.kind == k) out.push_back(&e);
    return out;
  }
};

}  // namespace qcfix
