// Tests: a native program (progs/native.hpp). Its declaration is laid out as a progs the checker
// takes: its globals, fields, strings and functions found by name; a function called by the
// engine, by another native function and by bytecode's CALL runs its C++ with the arguments in the
// parameter slots and its result in the return slot; its state lives in the VM's globals and
// fields; a field of no entity and a write to the locked world fault the call; a declaration that
// cannot be laid out is refused with what to fix.
//
// ge:summary Tests: native programs: the layout, calls three ways, state in the VM, the faults
// ge:keywords test progs native native-program build_native_progs bind_natives call_builtin
// ge:skill progs testing
#include <string>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "progs/native.hpp"
#include "progs/opcodes.hpp"
#include "progs/vm.hpp"

using namespace ge::progs;

namespace {

// The declaration's indices.
enum { kCount };           // globals
enum { kScore, kSpot };    // fields
enum { kHello };           // strings
enum { kAdd, kAddTwice };  // functions

// void(float n) add: count += n; entity 1's score += n; its spot = '1 2 3' * n.
void add(Vm& vm) {
  const NativeOffsets& o = native_offsets(vm);
  const float n = parm_float(vm, 0);
  set_global_float(vm, o.globals[kCount], global_float(vm, o.globals[kCount]) + n);
  const int32_t e = entity_value(vm, 1);
  set_field_float(vm, e, o.fields[kScore], field_float(vm, e, o.fields[kScore]) + n);
  set_field_vec(vm, e, o.fields[kSpot], {1 * n, 2 * n, 3 * n});
  set_return_float(vm, global_float(vm, o.globals[kCount]));
}

// float(float n) add_twice: add(n); add(n); returns the count.
void add_twice(Vm& vm) {
  const NativeOffsets& o = native_offsets(vm);
  const float n = parm_float(vm, 0);
  for (int i = 0; i < 2; i++) {
    set_parm_float(vm, 0, n);
    if (!call_function(vm, o.functions[kAdd])) return;
  }
  set_return_float(vm, global_float(vm, o.globals[kCount]));
}

constexpr NativeDef kGlobals[] = {{"count", kTypeFloat}};
constexpr NativeDef kFields[] = {{"score", kTypeFloat}, {"spot", kTypeVector}};
constexpr const char* kStrings[] = {"hello"};
constexpr uint8_t kOneFloat[] = {1};
void nothing(Vm&) {}
const NativeFunction kFunctions[] = {
    {"add", &add, kOneFloat}, {"add_twice", &add_twice, kOneFloat}, {"StartFrame", &nothing}};

NativeProgram program() {
  NativeProgram p;
  p.file = "test_native";
  p.globals = kGlobals;
  p.fields = kFields;
  p.strings = kStrings;
  p.functions = kFunctions;
  return p;
}

}  // namespace

TEST(a_native_program_is_laid_out_and_found_by_name) {
  Progs progs;
  NativeOffsets o;
  std::string error;
  CHECK_MSG(build_native_progs(program(), progs, o, error), error);
  CHECK(progs.crc == kClassicCrc);
  CHECK(find_function(progs, "add") == o.functions[kAdd]);
  CHECK(find_function(progs, "add_twice") == o.functions[kAddTwice]);
  CHECK(find_global(progs, "count")->offset == o.globals[kCount]);
  CHECK(find_field(progs, "spot")->offset == o.fields[kSpot]);
  CHECK(o.fields[kSpot] >= kEngineFields && progs.entityfields == o.fields[kSpot] + 3);
  CHECK(std::string(progs_string(progs, o.strings[kHello])) == "hello");
  CHECK(progs.functions[static_cast<size_t>(o.functions[kAddTwice])].first_statement ==
        -(kNativeBase + kAddTwice));
  // A hook the form names is in its global, as a compiler leaves it.
  CHECK(static_cast<int32_t>(progs.globals[kGStartFrame]) == o.functions[2]);
}

TEST(a_native_function_runs_from_the_engine_and_from_another) {
  Progs progs;
  NativeOffsets o;
  std::string error;
  CHECK_MSG(build_native_progs(program(), progs, o, error), error);
  Vm vm;
  create_vm(vm, progs);
  bind_natives(vm, o);
  vm.num_entities = 2;
  set_parm_float(vm, 0, 1.5f);
  CHECK_MSG(call_function(vm, o.functions[kAdd]), fault_text(vm));
  CHECK(result_float(vm) == 1.5f);
  set_parm_float(vm, 0, 2);
  CHECK_MSG(call_function(vm, o.functions[kAddTwice]), fault_text(vm));
  CHECK(result_float(vm) == 5.5f);
  const int32_t e = entity_value(vm, 1);
  CHECK(field_float(vm, e, o.fields[kScore]) == 5.5f);
  CHECK(field_vec(vm, e, o.fields[kSpot]).z == 6.0f);
  CHECK(global_float(vm, o.globals[kCount]) == 5.5f);  // the state is the VM's
}

TEST(bytecode_calls_a_native_function_by_its_record) {
  // A program of bytecode whose `go` calls a function record numbered as native 0.
  Assembler a;
  const int32_t twice = a.builtin("twice", kNativeBase + 0, {1});
  const int seven = a.constant_f(7);
  const int out = a.global("out", kTypeFloat);
  a.begin_function("go", {}, 0);
  a.call(twice, {{seven, 1}});
  a.emit(kOpStoreF, kGlobalReturn, out);
  a.emit(kOpDone);
  a.end_function();
  Progs progs;
  std::string error;
  CHECK_MSG(a.build(progs, error), error);
  Vm vm;
  create_vm(vm, progs);
  static const Builtin kTwice[] = {[](Vm& v) { set_return_float(v, 2 * parm_float(v, 0)); }};
  vm.natives = kTwice;
  CHECK_MSG(call_function(vm, find_function(progs, "go")), fault_text(vm));
  CHECK(global_float(vm, find_global(progs, "out")->offset) == 14.0f);
  vm.natives = {};  // none bound: the call faults and says so
  CHECK(!call_function(vm, find_function(progs, "go")));
  CHECK(std::string(vm.fault.message).find("native function 0") != std::string::npos);
}

TEST(a_native_function_faults_as_bytecode_does) {
  Progs progs;
  NativeOffsets o;
  std::string error;
  CHECK_MSG(build_native_progs(program(), progs, o, error), error);
  Vm vm;
  create_vm(vm, progs);
  bind_natives(vm, o);
  vm.num_entities = 1;  // entity 1 is not in use
  set_parm_float(vm, 0, 1);
  CHECK(!call_function(vm, o.functions[kAdd]));
  CHECK_MSG(std::string(vm.fault.message).find("names no entity") != std::string::npos,
            vm.fault.message);
  clear_fault(vm);
  vm.world_locked = true;
  set_field_float(vm, entity_value(vm, 0), o.fields[kScore], 1);
  CHECK(vm.fault.set && std::string(vm.fault.message).find("the world") != std::string::npos);
}

TEST(a_declaration_that_cannot_be_laid_out_is_refused) {
  Progs progs;
  NativeOffsets o;
  std::string error;
  constexpr NativeDef twice[] = {{"count", kTypeFloat}, {"count", kTypeFloat}};
  NativeProgram p = program();
  p.globals = twice;
  CHECK(!build_native_progs(p, progs, o, error) &&
        error.find("declared twice") != std::string::npos);
  constexpr NativeDef form[] = {{"time", kTypeFloat}};
  p.globals = form;
  CHECK(!build_native_progs(p, progs, o, error) &&
        error.find("the form's own") != std::string::npos);
  constexpr NativeDef clash[] = {{"health", kTypeVector}};
  p = program();
  p.fields = clash;
  CHECK(!build_native_progs(p, progs, o, error) && error.find("another type") != std::string::npos);
}
