// Tests: a loaded progs drives the shooter loop through the QC adapter. Assembled bytecode runs a
// think chain under the loop's physics frames (StartFrame each frame, a think that counts in a
// field of its own and reschedules itself from `time`, then removes its entity, which gets the
// progs' free rule), a trigger's touch when a box is linked into it (`other` hurt, an entity
// spawned and set at an origin, all read back by the loop), the hooks taken from the hook globals
// and by name, and a fault in a think counted with its QC stack while the match goes on.
//
// ge:summary Tests: the QC adapter running a think chain and a touch through the shooter loop
// ge:keywords test qc progs adapter think touch start_frame hooks spawn remove setorigin fault
// ge:skill progs shooter testing
// ge:test tests/shooters_edge_qc_program_test.cpp
#include <string>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "shooter/physics.hpp"
#include "shooter_loop_bench.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace sh = ge::shooter;
namespace pg = ge::progs;
using ge::test::loop::Bench;
using shooters_edge::sim::QcProgram;

namespace {

constexpr int kSpawn = 14, kRemove = 15, kSetorigin = 2;  // the standard numbers

struct Mod {
  pg::Progs progs;
  int32_t tick = 0, bang = 0, crash = 0, start_frame = 0, spectator = 0;
  int frames = 0, touched = 0, spawned = 0;  // globals
  int count = 0;                             // the program's own field's offset
};

// The test mod, assembled.
void build_mod(Mod& m) {
  pg::Assembler a;
  m.frames = a.global("frames", pg::kTypeFloat);
  m.touched = a.global("touched", pg::kTypeEntity);
  m.spawned = a.global("spawned", pg::kTypeEntity);
  const int f_count = a.field("count", pg::kTypeFloat);
  const int f_nextthink = a.field("nextthink", pg::kTypeFloat);
  const int f_health = a.field("health", pg::kTypeFloat);
  const int one = a.constant_f(1), three = a.constant_f(3), tenth = a.constant_f(0.1f);
  const int ten = a.constant_f(10), where = a.constant_v(1, 2, 3), none = a.constant_i(0);
  const int32_t spawn = a.builtin("spawn", kSpawn);
  const int32_t remove = a.builtin("remove", kRemove, {1});
  const int32_t setorigin = a.builtin("setorigin", kSetorigin, {1, 3});

  m.start_frame = a.begin_function("StartFrame", {}, 0, "mod.qc");
  a.emit(pg::kOpAddF, m.frames, one, m.frames);
  a.emit(pg::kOpDone);
  a.end_function();

  // tick(): self.count += 1; below 3 think again in 0.1 s, else remove(self)
  m.tick = a.begin_function("tick", {}, 3, "mod.qc");
  {
    const int t = a.local(0), p = a.local(1), c = a.local(2);
    const int gone = a.label();
    a.emit(pg::kOpLoadF, pg::kGSelf, f_count, t);
    a.emit(pg::kOpAddF, t, one, t);
    a.emit(pg::kOpAddress, pg::kGSelf, f_count, p);
    a.emit(pg::kOpStorePF, t, p);
    a.emit(pg::kOpLt, t, three, c);
    a.jump(pg::kOpIfNot, gone, c);
    a.emit(pg::kOpAddF, pg::kGTime, tenth, t);
    a.emit(pg::kOpAddress, pg::kGSelf, f_nextthink, p);
    a.emit(pg::kOpStorePF, t, p);
    a.emit(pg::kOpDone);
    a.place(gone);
    a.call(remove, {{pg::kGSelf, 1}});
    a.emit(pg::kOpDone);
  }
  a.end_function();

  // bang(): other.health -= 10; touched = other; spawned = spawn(); setorigin(spawned, '1 2 3')
  m.bang = a.begin_function("bang", {}, 2, "mod.qc");
  {
    const int h = a.local(0), p = a.local(1);
    a.emit(pg::kOpLoadF, pg::kGOther, f_health, h);
    a.emit(pg::kOpSubF, h, ten, h);
    a.emit(pg::kOpAddress, pg::kGOther, f_health, p);
    a.emit(pg::kOpStorePF, h, p);
    a.emit(pg::kOpStoreEnt, pg::kGOther, m.touched);
    a.call(spawn);
    a.emit(pg::kOpStoreEnt, pg::kGlobalReturn, m.spawned);
    a.call(setorigin, {{m.spawned, 1}, {where, 3}});
    a.emit(pg::kOpDone);
  }
  a.end_function();

  // crash(): calls function 0
  a.line(7);
  m.crash = a.begin_function("crash", {}, 0, "mod.qc");
  a.emit(pg::kOpCall0, none);
  a.emit(pg::kOpDone);
  a.end_function();

  m.spectator = a.begin_function("SpectatorConnect", {}, 0, "mod.qc");
  a.emit(pg::kOpDone);
  a.end_function();

  a.globals[static_cast<size_t>(pg::kGStartFrame)] = static_cast<uint32_t>(m.start_frame);
  m.count = static_cast<int>(a.globals[static_cast<size_t>(f_count)]);
  std::string error;
  CHECK_MSG(a.build(m.progs, error), error);
}

struct Match {
  Mod mod;
  Bench b;
  QcProgram q;

  Match() {
    build_mod(mod);
    b.start();
    std::string error;
    CHECK_MSG(shooters_edge::sim::create_qc_program(q, mod.progs, b.sv.set.max_edicts, error),
              error);
    pg::set_builtin(q.vm, kSpawn, shooters_edge::sim::qc_spawn, "spawn");
    pg::set_builtin(q.vm, kRemove, shooters_edge::sim::qc_remove, "remove");
    pg::set_builtin(q.vm, kSetorigin, shooters_edge::sim::qc_setorigin, "setorigin");
    b.sv.program = shooters_edge::sim::qc_program(q);
  }

  void frame(double dt) {
    b.sv.time += dt;
    sh::physics_frame(b.sv, dt);
  }

  float vm_float(int e, int field) { return pg::as_float(*pg::entity_field(q.vm, e, field)); }
};

}  // namespace

TEST(the_hooks_come_from_the_hook_globals_and_by_name) {
  Match m;
  CHECK(m.b.sv.program.start_frame == m.mod.start_frame);
  CHECK(m.b.sv.program.spectator_connect == m.mod.spectator);
  CHECK(m.b.sv.program.player_pre_think == sh::kNoFunc);
  CHECK(m.b.sv.program.spectator_think == sh::kNoFunc);
}

TEST(a_think_chain_runs_under_the_physics_frames_then_removes_itself) {
  Match m;
  const int e = sh::spawn_edict(m.b.sv);
  m.b.ent(e).think = m.mod.tick;
  m.b.ent(e).nextthink = static_cast<float>(m.b.sv.time + 0.05);
  int frames = 0;
  float last_next = 0;
  for (int i = 0; i < 10; i++) {
    m.frame(0.1);
    frames++;
    if (!m.b.ent(e).free) last_next = m.b.ent(e).nextthink;
  }
  CHECK(m.q.faults == 0);
  CHECK(pg::global_float(m.q.vm, m.mod.frames) == static_cast<float>(frames));
  CHECK(m.b.ent(e).free);
  // The program's own field survived the free; the free rule set nextthink to -1.
  CHECK(m.vm_float(e, m.mod.count) == 3);
  CHECK(m.vm_float(e, pg::kFNextthink) == -1);
  CHECK(last_next > 0);
  // Each think rescheduled from the program's time: three thinks, StartFrame every frame.
  CHECK(m.q.calls == frames + 3);
}

TEST(a_trigger_touch_hurts_the_toucher_and_spawns_an_entity_the_loop_sees) {
  Match m;
  const int trig = m.b.spawn_box({0, 0, 32}, {-16, -16, -16}, {16, 16, 16}, sh::kSolidTrigger,
                                 sh::kMovetypeNone);
  m.b.ent(trig).touch = m.mod.bang;
  const int box =
      m.b.spawn_box({200, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  m.b.ent(box).health = 100;
  const int before = m.b.sv.num_edicts;
  sh::set_origin(m.b.sv, box, {4, 0, 32});
  sh::link_edict(m.b.sv, box, true);
  CHECK(m.q.faults == 0);
  CHECK(m.b.ent(box).health == 90);
  CHECK(pg::global_int(m.q.vm, m.mod.touched) == pg::entity_value(m.q.vm, box));
  const int made = pg::entity_number(m.q.vm, pg::global_int(m.q.vm, m.mod.spawned));
  CHECK(made == before);
  CHECK(m.b.sv.num_edicts == before + 1);
  CHECK(!m.b.ent(made).free);
  CHECK(m.b.ent(made).origin.x == 1 && m.b.ent(made).origin.y == 2 && m.b.ent(made).origin.z == 3);
  // The loop's globals are restored by the loop; the touch saw self = the trigger.
  CHECK(m.b.ent(trig).touch == m.mod.bang);
}

TEST(a_fault_in_a_think_is_counted_with_its_stack_and_the_match_goes_on) {
  Match m;
  const int e = sh::spawn_edict(m.b.sv);
  m.b.ent(e).think = m.mod.crash;
  m.b.ent(e).nextthink = static_cast<float>(m.b.sv.time + 0.05);
  m.frame(0.1);
  m.frame(0.1);
  CHECK(m.q.faults == 1);
  CHECK(m.q.last_fault.find("NULL function") != std::string::npos);
  CHECK(m.q.last_fault.find("crash") != std::string::npos);
  CHECK(pg::global_float(m.q.vm, m.mod.frames) == 2);
  CHECK(!m.q.vm.fault.set);
}
