// The sample program (sample.hpp), assembled function by function; the source line of each
// statement is set as if the program were written one function a block of lines, so a fault and
// a disassembly show lines.
//
// ge:summary The sample QC program's assembly and its builtins
// ge:keywords qc progs sample assemble fib vectors entities strings bits run think fault builtins
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#include "progs/sample.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "progs/assembler.hpp"

namespace ge::progs {

namespace {

constexpr int kFtos = 1, kVlen = 2, kSin = 3, kSpawn = 4, kPrint = 5;

void b_ftos(Vm& vm) {
  const std::string s = core::num::to_string(static_cast<double>(global_float(vm, parm(0))));
  set_global_int(vm, kGlobalReturn, temp_string(vm, s.c_str()));
}

void b_vlen(Vm& vm) {
  const float x = global_float(vm, parm(0)), y = global_float(vm, parm(0) + 1),
              z = global_float(vm, parm(0) + 2);
  const float sum = (x * x + y * y) + z * z;
  set_global_float(vm, kGlobalReturn, static_cast<float>(std::sqrt(static_cast<double>(sum))));
}

void b_sin(Vm& vm) {
  set_global_float(
      vm, kGlobalReturn,
      static_cast<float>(core::libm::sin(static_cast<double>(global_float(vm, parm(0))))));
}

void b_spawn(Vm& vm) {
  auto& out = *static_cast<SampleOutput*>(vm.user);
  if (out.entities >= vm.max_entities) {
    vm_error(vm, "spawn: no free entity (the sample's VM holds max_entities)");
    return;
  }
  const int e = out.entities++;
  vm.num_entities = out.entities;
  clear_entity(vm, e);
  set_global_int(vm, kGlobalReturn, entity_value(vm, e));
}

void b_print(Vm& vm) {
  auto& out = *static_cast<SampleOutput*>(vm.user);
  for (int i = 0; i < vm.argc; i++) {
    const char* s = vm_string(vm, global_int(vm, parm(i)));
    if (s == nullptr) {
      vm_error(vm, "print: an argument is not a string value");
      return;
    }
    out.text += s;
  }
}

}  // namespace

void install_sample_builtins(Vm& vm) {
  set_builtin(vm, kFtos, b_ftos, "ftos");
  set_builtin(vm, kVlen, b_vlen, "vlen");
  set_builtin(vm, kSin, b_sin, "sin");
  set_builtin(vm, kSpawn, b_spawn, "spawn");
  set_builtin(vm, kPrint, b_print, "print");
}

std::vector<uint8_t> build_sample(std::vector<int32_t>& lines) {
  Assembler a;
  const int32_t ftos = a.builtin("ftos", kFtos, {1});
  const int32_t vlen = a.builtin("vlen", kVlen, {3});
  const int32_t sin = a.builtin("sin", kSin, {1});
  const int32_t spawn = a.builtin("spawn", kSpawn);
  const int32_t print = a.builtin("print", kPrint, {1, 1});

  const int f_counter = a.field("counter", kTypeFloat);
  const int f_dir = a.field("dir", kTypeVector);
  const int f_next = a.field("next", kTypeEntity);
  const int f_fn = a.field("fn", kTypeFunction);
  const int f_which = a.field("which", kTypeField);
  const int f_label = a.field("label", kTypeString);
  const int f_origin = a.field("origin", kTypeVector);
  const int f_frame = a.field("frame", kTypeFloat);
  const int g_count = a.global("count", kTypeFloat);
  const int g_first = a.global("first", kTypeEntity);
  const int zero = a.constant_f(0), one = a.constant_f(1), two = a.constant_f(2);
  const int half = a.constant_f(0.5f), tenth = a.constant_f(0.1f);

  const int32_t think = a.declare("think");
  const int32_t fib = a.declare("fib");

  // fib(n): n < 2 ? n : fib(n - 1) + fib(n - 2)
  a.line(10);
  a.begin_function("fib", {1}, 2, "sample.qc");
  {
    const int n = a.param(0), t = a.local(0), r = a.local(1);
    const int deeper = a.label();
    a.emit(kOpLt, n, two, t);
    a.jump(kOpIfNot, deeper, t);
    a.line(11);
    a.emit(kOpReturn, n);
    a.place(deeper);
    a.line(12);
    a.emit(kOpSubF, n, one, t);
    a.call(fib, {{t, 1}});
    a.emit(kOpStoreF, kGlobalReturn, r);
    a.emit(kOpSubF, n, two, t);
    a.call(fib, {{t, 1}});
    a.line(13);
    a.emit(kOpAddF, r, kGlobalReturn, t);
    a.emit(kOpReturn, t);
  }
  a.end_function();

  // vectors(n): a loop of the vector and float arithmetic and comparisons; returns a length
  a.line(20);
  const int32_t vectors = a.begin_function("vectors", {1}, 16, "sample.qc");
  {
    const int n = a.param(0), i = a.local(0), t = a.local(1), u = a.local(2);
    const int v = a.local(3), w = a.local(6), acc = a.local(9), s = a.local(12);
    const int start = a.constant_v(1.5f, -2.25f, 3.0f), step = a.constant_v(0.25f, 0.5f, -0.75f);
    const int top = a.label(), done = a.label(), same = a.label();
    a.emit(kOpStoreV, start, v);
    a.emit(kOpStoreV, step, acc);
    a.emit(kOpStoreF, zero, i);
    a.place(top);
    a.line(21);
    a.emit(kOpLt, i, n, t);
    a.jump(kOpIfNot, done, t);
    a.emit(kOpAddV, v, acc, w);
    a.emit(kOpSubV, w, step, v);
    a.emit(kOpMulVF, acc, half, w);
    a.emit(kOpMulFV, tenth, v, s);
    a.emit(kOpAddV, w, s, acc);
    a.line(22);
    a.emit(kOpMulV, v, acc, t);
    a.emit(kOpDivF, t, two, u);
    a.emit(kOpMulF, u, u, t);
    a.emit(kOpEqV, v, acc, u);
    a.jump(kOpIf, same, u);
    a.emit(kOpNeV, v, acc, u);
    a.emit(kOpNotV, w, 0, t);
    a.place(same);
    a.emit(kOpAddF, i, one, i);
    a.jump(kOpGoto, top);
    a.place(done);
    a.line(23);
    a.call(vlen, {{acc, 3}});
    a.emit(kOpReturn, kGlobalReturn);
  }
  a.end_function();

  // think(): what STATE scheduled; counts its calls in `count`
  a.line(30);
  a.begin_function("think", {}, 0, "sample.qc");
  a.emit(kOpAddF, g_count, one, g_count);
  a.emit(kOpDone);
  a.end_function();

  // entities(): spawns three, writes and reads every field kind, compares, and STATEs each
  a.line(40);
  const int32_t entities = a.begin_function("entities", {}, 12, "sample.qc");
  {
    const int e = a.local(0), k = a.local(1), t = a.local(2), p = a.local(3), v = a.local(4);
    const int x = a.local(7), sum = a.local(8), fn = a.local(9), fld = a.local(10);
    const int str = a.local(11);
    const int three = a.constant_f(3), dir = a.constant_v(1, 2, 4), label = a.constant_s("tick");
    const int think_fn = a.constant_fn(think), frame = a.constant_f(7);
    const int top = a.label(), done = a.label(), skip = a.label();
    a.emit(kOpStoreF, zero, k);
    a.emit(kOpStoreF, zero, sum);
    a.place(top);
    a.emit(kOpLt, k, three, t);
    a.jump(kOpIfNot, done, t);
    a.line(41);
    a.call(spawn);
    a.emit(kOpStoreEnt, kGlobalReturn, e);
    a.emit(kOpNotEnt, g_first, 0, t);
    a.jump(kOpIfNot, skip, t);
    a.emit(kOpStoreEnt, e, g_first);
    a.place(skip);
    a.line(42);
    a.emit(kOpLoadF, e, f_counter, x);
    a.emit(kOpAddF, x, k, x);
    a.emit(kOpAddress, e, f_counter, p);
    a.emit(kOpStorePF, x, p);
    a.emit(kOpLoadV, e, f_dir, v);
    a.emit(kOpAddV, v, dir, v);
    a.emit(kOpAddress, e, f_dir, p);
    a.emit(kOpStorePV, v, p);
    a.emit(kOpAddress, e, f_origin, p);
    a.emit(kOpStorePV, v, p);
    a.line(43);
    a.emit(kOpAddress, e, f_next, p);
    a.emit(kOpStorePEnt, g_first, p);
    a.emit(kOpAddress, e, f_fn, p);
    a.emit(kOpStorePFnc, think_fn, p);
    a.emit(kOpAddress, e, f_which, p);
    a.emit(kOpStorePFld, f_counter, p);
    a.emit(kOpAddress, e, f_label, p);
    a.emit(kOpStorePS, label, p);
    a.line(44);
    a.emit(kOpLoadEnt, e, f_next, t);
    a.emit(kOpEqE, t, g_first, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpNeE, t, e, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpLoadFnc, e, f_fn, fn);
    a.emit(kOpEqFnc, fn, think_fn, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpNeFnc, fn, think_fn, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpNotFnc, fn, 0, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpLoadFld, e, f_which, fld);
    a.emit(kOpStoreFld, fld, t);
    a.emit(kOpLoadF, e, t, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpLoadS, e, f_label, str);
    a.emit(kOpStoreS, str, t);
    a.line(45);
    a.emit(kOpStoreEnt, e, kGSelf);
    a.emit(kOpState, frame, think_fn);
    a.emit(kOpLoadF, e, f_frame, x);
    a.emit(kOpAddF, sum, x, sum);
    a.emit(kOpStoreFnc, fn, t);
    a.emit(kOpAddF, k, one, k);
    a.jump(kOpGoto, top);
    a.place(done);
    a.line(46);
    a.emit(kOpReturn, sum);
  }
  a.end_function();

  // strings(x): a temporary from ftos, compared by content, printed
  a.line(50);
  const int32_t strings = a.begin_function("strings", {1}, 4, "sample.qc");
  {
    const int x = a.param(0), s = a.local(0), t = a.local(1), r = a.local(2);
    const int three = a.constant_s("3"), empty = a.constant_s(""), newline = a.constant_s("\n");
    a.call(ftos, {{x, 1}});
    a.emit(kOpStoreS, kGlobalReturn, s);
    a.line(51);
    a.emit(kOpEqS, s, three, r);
    a.emit(kOpNeS, s, three, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpNotS, empty, 0, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpNotS, s, 0, t);
    a.emit(kOpAddF, r, t, r);
    a.line(52);
    a.call(print, {{s, 1}, {newline, 1}});
    a.emit(kOpReturn, r);
  }
  a.end_function();

  // bits(x): the logic and bit operations and the float comparisons
  a.line(60);
  const int32_t bits = a.begin_function("bits", {1}, 4, "sample.qc");
  {
    const int x = a.param(0), r = a.local(0), t = a.local(1);
    const int mask = a.constant_f(6), flag = a.constant_f(9);
    a.emit(kOpBitAnd, x, mask, r);
    a.emit(kOpBitOr, r, flag, r);
    a.emit(kOpAnd, x, r, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpOr, zero, x, t);
    a.emit(kOpAddF, r, t, r);
    a.line(61);
    a.emit(kOpNotF, x, 0, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpLe, x, r, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpGe, x, r, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpGt, x, one, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpEqF, x, one, t);
    a.emit(kOpAddF, r, t, r);
    a.emit(kOpNeF, x, one, t);
    a.emit(kOpAddF, r, t, r);
    a.call(sin, {{x, 1}});
    a.emit(kOpAddF, r, kGlobalReturn, r);
    a.emit(kOpReturn, r);
  }
  a.end_function();

  // run(n): everything, summed into a checksum
  a.line(70);
  a.begin_function("run", {1}, 3, "sample.qc");
  {
    const int n = a.param(0), acc = a.local(0), t = a.local(1);
    a.call(fib, {{n, 1}});
    a.emit(kOpStoreF, kGlobalReturn, acc);
    a.line(71);
    a.call(vectors, {{n, 1}});
    a.emit(kOpAddF, acc, kGlobalReturn, acc);
    a.call(entities);
    a.emit(kOpAddF, acc, kGlobalReturn, acc);
    a.line(72);
    a.call(strings, {{n, 1}});
    a.emit(kOpAddF, acc, kGlobalReturn, acc);
    a.call(bits, {{n, 1}});
    a.emit(kOpAddF, acc, kGlobalReturn, acc);
    a.emit(kOpAddF, acc, g_count, t);
    a.line(73);
    a.emit(kOpReturn, t);
  }
  a.end_function();

  // fault(): calls the null function, the fault every engine reports with its stack
  a.line(80);
  a.begin_function("fault", {}, 1, "sample.qc");
  a.emit(kOpStoreFnc, zero, a.local(0));
  a.line(81);
  a.emit(kOpCall0, a.local(0));
  a.emit(kOpDone);
  a.end_function();

  lines = a.lines();
  return a.bytes();
}

bool load_sample(Progs& out, std::string& error) {
  std::vector<int32_t> lines;
  const std::vector<uint8_t> bytes = build_sample(lines);
  if (!load_progs(bytes.data(), bytes.size(), out, error)) return false;
  return set_lines(out, std::move(lines), error);
}

}  // namespace ge::progs
