// Tests: the QC VM's calls and guards. The sample program runs and replays to the bit (its
// checksum, globals and entities held by tests/golden/progs_vm.txt); recursion keeps each call's
// locals; every guard faults with its message and a stack trace that names the function, the
// statement and the line (the run-away loop at its 100000th statement, the call depth, the local
// stack, the null function, a builtin the game left empty); a builtin may call back into QC;
// temporaries live in a ring of 8; the profile counts statements and calls; the extension
// registry installs and answers.
//
// A deliberate change to the VM or the sample moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/progs_vm_test` and say why in the PR (skill: progs).
//
// ge:summary Tests: QC calls, recursion, guards, faults with stack traces, replay, profile
// ge:keywords test progs qc vm sample replay golden fault stack-trace runaway stack-overflow
// ge:keywords locals builtin nested temp-string profile extension
// ge:skill progs testing
// ge:test tests/progs_vm_test.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "progs/extensions.hpp"
#include "progs/sample.hpp"
#include "progs/vm.hpp"

using namespace ge::progs;

namespace {

constexpr const char* kGolden = "tests/golden/progs_vm.txt";

uint64_t fnv(const std::vector<uint32_t>& words, uint64_t h = 0xcbf29ce484222325ull) {
  for (uint32_t w : words) {
    for (int i = 0; i < 4; i++) h = (h ^ ((w >> (8 * i)) & 0xffu)) * 0x100000001b3ull;
  }
  return h;
}

struct SampleRun {
  float checksum = 0;
  uint64_t globals = 0, entities = 0;
  int64_t statements = 0;
  std::string printed;
};

SampleRun run_sample(const Progs& p, float n) {
  Vm vm;
  create_vm(vm, p);
  SampleOutput out;
  vm.user = &out;
  install_sample_builtins(vm);
  set_global_float(vm, parm(0), n);
  SampleRun r;
  CHECK_MSG(call_function(vm, find_function(p, "run")), fault_text(vm));
  r.checksum = global_float(vm, kGlobalReturn);
  r.globals = fnv(vm.globals);
  r.entities = fnv(vm.entities);
  r.statements = vm.statements_run;
  r.printed = out.text;
  return r;
}

Progs sample() {
  Progs p;
  std::string error;
  CHECK_MSG(load_sample(p, error), error);
  return p;
}

}  // namespace

TEST(the_sample_runs_and_replays_to_the_bit) {
  const Progs p = sample();
  std::string got;
  for (float n : {1.0f, 5.0f, 10.0f, 15.0f}) {
    const SampleRun a = run_sample(p, n), b = run_sample(p, n);
    CHECK_MSG(a.globals == b.globals && a.entities == b.entities &&
                  std::bit_cast<uint32_t>(a.checksum) == std::bit_cast<uint32_t>(b.checksum),
              "two runs of the sample disagree: the VM keeps state between runs");
    char line[160];
    std::snprintf(line, sizeof line, "%08x %08x %016llx %016llx %llx\n", std::bit_cast<uint32_t>(n),
                  std::bit_cast<uint32_t>(a.checksum), static_cast<unsigned long long>(a.globals),
                  static_cast<unsigned long long>(a.entities),
                  static_cast<unsigned long long>(a.statements));
    got += line;
  }
  if (std::getenv("GE_GOLDEN_WRITE") != nullptr) {
    std::ofstream(ge::test::root_path(kGolden))
        << "# n, run(n)'s checksum, FNV-1a of the globals and of the entities after it, "
           "statements run; written by tests/progs_vm_test.cpp with GE_GOLDEN_WRITE=1\n"
        << got;
    return;
  }
  std::ifstream in(ge::test::root_path(kGolden));
  std::string want, line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] != '#') want += line + "\n";
  }
  CHECK_MSG(got == want, "the sample's run moved:\n" + got + "want\n" + want +
                             "If it is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1 and say why (skill: progs)");
}

TEST(the_sample_computes_what_its_functions_say) {
  const Progs p = sample();
  Vm vm;
  create_vm(vm, p);
  SampleOutput out;
  vm.user = &out;
  install_sample_builtins(vm);
  set_global_float(vm, parm(0), 10.0f);
  CHECK(call_function(vm, find_function(p, "fib")));
  CHECK_MSG(global_float(vm, kGlobalReturn) == 55.0f, "fib(10) is 55: each call kept its locals");
  set_global_float(vm, parm(0), 3.0f);
  CHECK(call_function(vm, find_function(p, "strings")));
  CHECK(out.text == "3\n");
  // EQ_S "3" (1) + NE_S (0) + NOT_S "" (1) + NOT_S "3" (0)
  CHECK_MSG(global_float(vm, kGlobalReturn) == 2.0f,
            std::to_string(global_float(vm, kGlobalReturn)));
  CHECK(call_function(vm, find_function(p, "entities")));
  CHECK(vm.num_entities == 4);
  CHECK(std::bit_cast<float>(*entity_field(vm, 3, kFNextthink)) == static_cast<float>(0.0 + 0.1));
  CHECK(static_cast<int32_t>(*entity_field(vm, 2, kFThink)) == find_function(p, "think"));
}

TEST(a_fault_names_the_function_the_statement_the_line_and_the_callers) {
  Assembler as;
  const int32_t inner = as.declare("inner");
  as.line(5);
  const int32_t outer = as.begin_function("outer", {}, 0, "mod.qc");
  as.line(6);
  as.call(inner);
  as.emit(kOpDone);
  as.end_function();
  as.line(10);
  as.begin_function("inner", {}, 1, "mod.qc");
  as.emit(kOpStoreFnc, as.constant_fn(0), as.local(0));
  as.line(11);
  const int bad = as.emit(kOpCall0, as.local(0));
  as.emit(kOpDone);
  as.end_function();
  Progs p;
  std::string error;
  CHECK_MSG(as.build(p, error), error);
  Vm vm;
  create_vm(vm, p);
  CHECK(!call_function(vm, outer));
  CHECK(std::strcmp(vm.fault.message, "NULL function") == 0);
  CHECK(vm.fault.depth == 2);
  CHECK(vm.fault.frames[0].function == inner && vm.fault.frames[0].statement == bad);
  CHECK(vm.fault.frames[1].function == outer);
  const std::string text = fault_text(vm);
  CHECK_MSG(text.find("at inner (mod.qc) statement " + std::to_string(bad) + ", line 11") !=
                std::string::npos,
            text);
  CHECK_MSG(text.find("from outer (mod.qc)") != std::string::npos, text);
  CHECK_MSG(text.find(", line 6") != std::string::npos, text);
  CHECK(text.find("next:") != std::string::npos);
  CHECK_MSG(vm.depth == 0 && vm.local_used == 0, "a fault unwinds to the engine's call");
  CHECK_MSG(!call_function(vm, outer), "a faulted VM refuses calls until the fault is cleared");
  clear_fault(vm);
  CHECK(!call_function(vm, outer));
}

TEST(the_runaway_guard_faults_on_the_hundred_thousandth_statement) {
  Assembler as;
  const int32_t loop = as.begin_function("spin", {}, 0);
  const int top = as.label();
  as.place(top);
  as.jump(kOpGoto, top);
  as.end_function();
  Progs p;
  std::string error;
  CHECK_MSG(as.build(p, error), error);
  Vm vm;
  create_vm(vm, p);
  CHECK(!call_function(vm, loop));
  CHECK(std::strstr(vm.fault.message, "runaway loop") != nullptr);
  CHECK_MSG(vm.profile[static_cast<size_t>(loop)].statements == kRunawayLimit - 1,
            "99999 statements run; the 100000th is the fault");
}

TEST(the_call_depth_and_the_local_stack_are_bounded) {
  Assembler as;
  const int32_t deep = as.declare("deep");
  as.begin_function("deep", {}, 0);
  as.call(deep);
  as.emit(kOpDone);
  as.end_function();
  const int32_t fat = as.declare("fat");
  as.begin_function("fat", {}, 300);
  as.call(fat);
  as.emit(kOpDone);
  as.end_function();
  Progs p;
  std::string error;
  CHECK_MSG(as.build(p, error), error);
  Vm vm;
  create_vm(vm, p);
  CHECK(!call_function(vm, deep));
  CHECK(std::strstr(vm.fault.message, "stack overflow") != nullptr);
  CHECK_MSG(vm.profile[static_cast<size_t>(deep)].calls == kMaxCallDepth,
            "32 functions run at once; the 33rd entry is the fault");
  clear_fault(vm);
  CHECK(!call_function(vm, fat));
  CHECK(std::strstr(vm.fault.message, "locals stack overflow") != nullptr);
  CHECK(vm.profile[static_cast<size_t>(fat)].calls == kLocalStackWords / 300);
}

namespace {

int32_t g_callback = 0;

// A builtin that calls back into QC: the nested engine call shares the stacks.
void b_callback(Vm& vm) {
  if (!call_function(vm, g_callback)) return;
  set_global_float(vm, kGlobalReturn, global_float(vm, kGlobalReturn) + 100.0f);
}

}  // namespace

TEST(builtins_run_from_the_table_and_may_call_back_into_qc) {
  Assembler as;
  const int32_t callback = as.builtin("callback", 7);
  const int32_t missing = as.builtin("missing", 9);
  const int32_t leaf = as.begin_function("leaf", {}, 0);
  as.emit(kOpReturn, as.constant_v(5, 0, 0));
  as.end_function();
  const int32_t top = as.begin_function("top", {}, 1);
  as.call(callback);
  as.emit(kOpReturn, kGlobalReturn);
  as.end_function();
  const int32_t broken = as.begin_function("broken", {}, 0);
  as.call(missing);
  as.emit(kOpDone);
  as.end_function();
  Progs p;
  std::string error;
  CHECK_MSG(as.build(p, error), error);
  Vm vm;
  create_vm(vm, p);
  g_callback = leaf;
  set_builtin(vm, 7, b_callback, "callback");
  CHECK_MSG(call_function(vm, top), fault_text(vm));
  CHECK(global_float(vm, kGlobalReturn) == 105.0f);
  CHECK(vm.builtin_calls[7] == 1);
  CHECK(vm.profile[static_cast<size_t>(leaf)].calls == 1);
  CHECK(!call_function(vm, broken));
  CHECK(std::strcmp(vm.fault.message, "builtin 9 is not provided by this game") == 0);
  clear_fault(vm);
  CHECK(!call_function(vm, 0));
  CHECK(std::strcmp(vm.fault.message, "NULL function") == 0);
}

TEST(temporaries_live_in_a_ring_of_eight) {
  const Progs p = sample();
  Vm vm;
  create_vm(vm, p);
  const int32_t first = temp_string(vm, "first");
  for (int i = 0; i < kTempStrings - 1; i++) temp_string(vm, "more");
  CHECK_MSG(std::strcmp(vm_string(vm, first), "first") == 0, "valid through 7 more");
  temp_string(vm, "eighth");
  CHECK_MSG(std::strcmp(vm_string(vm, first), "eighth") == 0, "reused by the 8th more");
  const int32_t e = engine_string(vm, "held");
  CHECK(e < 0 && e > -kTempBase);
  CHECK(std::strcmp(vm_string(vm, e), "held") == 0);
  CHECK(std::strcmp(vm_string(vm, 0), "") == 0);
  clear_engine_strings(vm);
  CHECK(vm_string(vm, e) == nullptr);
  CHECK(vm_string(vm, static_cast<int32_t>(p.strings.size())) == nullptr);
}

TEST(the_profile_counts_statements_and_calls_per_function) {
  const Progs p = sample();
  Vm vm;
  create_vm(vm, p);
  SampleOutput out;
  vm.user = &out;
  install_sample_builtins(vm);
  set_global_float(vm, parm(0), 6.0f);
  CHECK(call_function(vm, find_function(p, "run")));
  const FunctionProfile& fib = vm.profile[static_cast<size_t>(find_function(p, "fib"))];
  CHECK_MSG(fib.calls == 25, "fib(6) enters fib 25 times");
  int64_t total = 0;
  for (const FunctionProfile& f : vm.profile) total += f.statements;
  CHECK(total == vm.statements_run);
  CHECK(vm.builtin_calls[4] == 3);  // spawn, three times
  clear_profile(vm);
  CHECK(vm.profile[static_cast<size_t>(find_function(p, "fib"))].calls == 0);
}

namespace {
void b_nothing(Vm&) {}
}  // namespace

TEST(the_extension_registry_installs_and_answers) {
  ExtensionRegistry reg;
  std::string error;
  CHECK(register_extension(reg, {"EXT_ONE", {{60, "one", b_nothing}}, {"glow"}, {}}, error));
  CHECK(!register_extension(reg, {"EXT_ONE", {}, {}, {}}, error));
  CHECK(error.find("registered twice") != std::string::npos);
  CHECK(!register_extension(reg, {"EXT_TWO", {{60, "two", b_nothing}}, {}, {}}, error));
  CHECK(error.find("already extension 'EXT_ONE'") != std::string::npos);
  CHECK(offers(reg, "EXT_ONE") && !offers(reg, "EXT_TWO"));
  const Progs p = sample();
  Vm vm;
  create_vm(vm, p);
  install(reg, vm);
  CHECK(vm.builtins[60].fn == b_nothing && std::strcmp(vm.builtins[60].name, "one") == 0);
}
