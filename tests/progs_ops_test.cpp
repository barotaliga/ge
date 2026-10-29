// Tests: every opcode on the VM. The arithmetic, vector, comparison, logic and bit opcodes run
// over a matrix of awkward floats (zeros of both signs, infinities, NaN, a subnormal, values past
// the 32-bit integer range) with every result's bits held by tests/golden/progs_ops.txt; each
// opcode's stated edge (IF on the raw word, NOT_F of -0, NaN compares, the truncation of BITAND,
// STATE's rounding, the negative-field guard, strings by content) is asserted by its own case.
//
// A deliberate change to an opcode's result moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/progs_ops_test` and say why in the PR (skill: progs).
//
// ge:summary Tests: every QC opcode golden over awkward floats, and each opcode's stated edge
// ge:keywords test progs qc opcode golden ieee nan negative-zero bitand truncation state
// ge:keywords if raw-word load store address storep strings
// ge:skill progs testing
// ge:test tests/progs_ops_test.cpp
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "progs/assembler.hpp"
#include "progs/vm.hpp"

using namespace ge::progs;

namespace {

constexpr const char* kGolden = "tests/golden/progs_ops.txt";

// A program with one probe function an opcode: `op A B C` then DONE, over three fixed globals.
struct Probe {
  Progs progs;
  Vm vm;
  int A = 0, B = 0, C = 0;
  std::vector<int32_t> fn_of_op = std::vector<int32_t>(kOpCount, 0);
  std::string error;

  explicit Probe(const std::vector<uint16_t>& ops) {
    Assembler as;
    A = as.temp(3);
    B = as.temp(3);
    C = as.temp(3);
    for (uint16_t op : ops) {
      fn_of_op[op] = as.begin_function(op_name(op), {}, 0);
      as.emit(op, A, B, C);
      as.emit(kOpDone);
      as.end_function();
    }
    CHECK_MSG(as.build(progs, error), "the probe program does not load: " + error);
    create_vm(vm, progs);
  }
  void set(int at, const uint32_t (&w)[3]) {
    for (int k = 0; k < 3; k++) vm.globals[static_cast<size_t>(at) + static_cast<size_t>(k)] = w[k];
  }
  bool run(uint16_t op) { return call_function(vm, fn_of_op[op]); }
  uint32_t c(int k = 0) const {
    return vm.globals[static_cast<size_t>(C) + static_cast<size_t>(k)];
  }
};

uint32_t w(float f) { return std::bit_cast<uint32_t>(f); }
float f(uint32_t u) { return std::bit_cast<float>(u); }

const std::vector<float>& awkward() {
  static const std::vector<float> v = {0.0f,
                                       -0.0f,
                                       1.0f,
                                       -1.0f,
                                       0.1f,
                                       3.5f,
                                       -7.25f,
                                       1e30f,
                                       1e-40f,  // subnormal
                                       std::numeric_limits<float>::infinity(),
                                       -std::numeric_limits<float>::infinity(),
                                       std::numeric_limits<float>::quiet_NaN(),
                                       2147483648.0f,
                                       -2147483648.0f,
                                       16777217.0f,
                                       255.75f};
  return v;
}

const std::vector<uint16_t> kMatrixOps = {
    kOpMulF, kOpMulV,  kOpMulFV, kOpMulVF,  kOpDivF,   kOpAddF,   kOpAddV,  kOpSubF,
    kOpSubV, kOpEqF,   kOpEqV,   kOpNeF,    kOpNeV,    kOpLe,     kOpGe,    kOpLt,
    kOpGt,   kOpNotF,  kOpNotV,  kOpAnd,    kOpOr,     kOpBitAnd, kOpBitOr, kOpEqE,
    kOpNeE,  kOpEqFnc, kOpNeFnc, kOpNotEnt, kOpNotFnc, kOpStoreF, kOpStoreV};

std::string hex(uint32_t u) {
  char b[16];
  std::snprintf(b, sizeof b, "%08x", u);
  return b;
}

}  // namespace

TEST(every_matrix_opcode_keeps_its_golden_bits) {
  Probe p(kMatrixOps);
  const auto& v = awkward();
  ge::core::Rng rng = ge::core::make_rng(18);
  std::string got;
  int rows = 0;
  for (uint16_t op : kMatrixOps) {
    for (float x : v) {
      for (float y : v) {
        // Vectors take their other components from the matrix, drawn from a fixed stream.
        const uint32_t a[3] = {w(x), w(v[ge::core::pick_index(rng, v.size())]),
                               w(v[ge::core::pick_index(rng, v.size())])};
        const uint32_t b[3] = {w(y), w(v[ge::core::pick_index(rng, v.size())]),
                               w(v[ge::core::pick_index(rng, v.size())])};
        const uint32_t clear[3] = {0xdeadbeefu, 0xdeadbeefu, 0xdeadbeefu};
        p.set(p.A, a);
        p.set(p.B, b);
        p.set(p.C, clear);
        CHECK_MSG(p.run(op), std::string(op_name(op)) + " faulted: " + fault_text(p.vm));
        const uint32_t out[3] = {p.c(0), p.c(1), p.c(2)};
        // STORE writes b, not c: its result is B.
        const bool store = op == kOpStoreF || op == kOpStoreV;
        const uint32_t r[3] = {store ? p.vm.globals[static_cast<size_t>(p.B)] : out[0],
                               store ? p.vm.globals[static_cast<size_t>(p.B) + 1] : out[1],
                               store ? p.vm.globals[static_cast<size_t>(p.B) + 2] : out[2]};
        got += std::to_string(op);
        for (uint32_t u : a) got += " " + hex(u);
        for (uint32_t u : b) got += " " + hex(u);
        for (uint32_t u : r) got += " " + hex(u);
        got += "\n";
        rows++;
      }
    }
  }
  CHECK(rows == static_cast<int>(kMatrixOps.size() * v.size() * v.size()));
  if (std::getenv("GE_GOLDEN_WRITE") != nullptr) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# opcode, A (3 words), B (3 words), the result (C, or B for a STORE), hex bits; "
           "written by tests/progs_ops_test.cpp with GE_GOLDEN_WRITE=1\n"
        << got;
    return;
  }
  std::ifstream in(ge::test::root_path(kGolden));
  std::stringstream want;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] != '#') want << line << "\n";
  }
  if (got != want.str()) {
    // Name the first row that moved.
    std::istringstream g(got), wv(want.str());
    std::string gl, wl;
    int row = 0;
    while (std::getline(g, gl)) {
      std::getline(wv, wl);
      row++;
      if (gl != wl) break;
    }
    CHECK_MSG(false, "progs_ops row " + std::to_string(row) + " moved: got '" + gl + "' want '" +
                         wl + "'. If the opcode's result is meant to change, rewrite " + kGolden +
                         " with GE_GOLDEN_WRITE=1 and say why (skill: progs)");
  }
}

TEST(float_truth_reads_the_value_but_if_reads_the_raw_word) {
  Assembler as;
  const int x = as.temp(), r = as.temp();
  const int32_t fn = as.begin_function("probe", {}, 0);
  const int yes = as.label(), end = as.label();
  as.jump(kOpIf, yes, x);
  as.emit(kOpStoreF, as.constant_f(0), r);
  as.jump(kOpGoto, end);
  as.place(yes);
  as.emit(kOpStoreF, as.constant_f(1), r);
  as.place(end);
  as.emit(kOpDone);
  as.end_function();
  const int32_t not_fn = as.begin_function("not", {}, 0);
  as.emit(kOpNotF, x, 0, r);
  as.emit(kOpDone);
  as.end_function();
  Progs p;
  std::string error;
  CHECK_MSG(as.build(p, error), error);
  Vm vm;
  create_vm(vm, p);
  set_global_float(vm, x, -0.0f);
  CHECK(call_function(vm, fn));
  CHECK_MSG(global_float(vm, r) == 1.0f,
            "IF on -0: the raw word is not zero, so the jump is taken");
  CHECK(call_function(vm, not_fn));
  CHECK_MSG(global_float(vm, r) == 1.0f, "NOT_F of -0 is 1 (a float test)");
  set_global_float(vm, x, std::numeric_limits<float>::quiet_NaN());
  CHECK(call_function(vm, not_fn));
  CHECK_MSG(global_float(vm, r) == 0.0f, "NOT_F of NaN is 0: NaN is true");
  set_global_float(vm, x, 0.0f);
  CHECK(call_function(vm, fn));
  CHECK_MSG(global_float(vm, r) == 0.0f, "IF on +0 falls through");
}

TEST(comparisons_follow_ieee) {
  Probe p({kOpEqF, kOpNeF, kOpLt, kOpLe, kOpGt, kOpGe, kOpAnd, kOpOr});
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const auto both = [&](uint16_t op, float a, float b) {
    const uint32_t A[3] = {w(a), 0, 0}, B[3] = {w(b), 0, 0};
    p.set(p.A, A);
    p.set(p.B, B);
    CHECK(p.run(op));
    return f(p.c());
  };
  CHECK(both(kOpEqF, nan, nan) == 0.0f);
  CHECK(both(kOpNeF, nan, nan) == 1.0f);
  CHECK(both(kOpEqF, -0.0f, 0.0f) == 1.0f);
  CHECK(both(kOpLt, nan, 1) == 0.0f && both(kOpLe, nan, 1) == 0.0f);
  CHECK(both(kOpGt, nan, 1) == 0.0f && both(kOpGe, nan, 1) == 0.0f);
  CHECK(both(kOpAnd, nan, 1) == 1.0f);  // NaN is true
  CHECK(both(kOpOr, -0.0f, 0.0f) == 0.0f);
}

TEST(dot_product_sums_left_to_right_and_division_by_zero_is_ieee) {
  Probe p({kOpMulV, kOpDivF});
  const uint32_t a[3] = {w(1e8f), w(1.0f), w(-1e8f)}, b[3] = {w(1.0f), w(1.0f), w(1.0f)};
  p.set(p.A, a);
  p.set(p.B, b);
  CHECK(p.run(kOpMulV));
  CHECK_MSG(f(p.c()) == 0.0f, "(1e8 + 1) - 1e8 in float is 0: the sum runs left to right");
  const uint32_t one[3] = {w(1.0f), 0, 0}, zero[3] = {w(0.0f), 0, 0}, nzero[3] = {w(-0.0f), 0, 0};
  p.set(p.A, one);
  p.set(p.B, zero);
  CHECK(p.run(kOpDivF));
  CHECK(f(p.c()) == std::numeric_limits<float>::infinity());
  p.set(p.B, nzero);
  CHECK(p.run(kOpDivF));
  CHECK(f(p.c()) == -std::numeric_limits<float>::infinity());
  p.set(p.A, zero);
  CHECK(p.run(kOpDivF));
  CHECK(f(p.c()) != f(p.c()));  // 0 / 0 is NaN, not a fault
}

TEST(bit_operations_truncate_toward_zero_and_out_of_range_is_int_min) {
  Probe p({kOpBitAnd, kOpBitOr});
  const auto op = [&](uint16_t o, float a, float b) {
    const uint32_t A[3] = {w(a), 0, 0}, B[3] = {w(b), 0, 0};
    p.set(p.A, A);
    p.set(p.B, B);
    CHECK(p.run(o));
    return f(p.c());
  };
  CHECK(op(kOpBitAnd, 7.9f, 6.0f) == 6.0f);
  CHECK(op(kOpBitOr, -1.5f, 0.0f) == -1.0f);  // toward zero
  CHECK(op(kOpBitOr, std::numeric_limits<float>::quiet_NaN(), 0.0f) == -2147483648.0f);
  CHECK(op(kOpBitOr, 3e9f, 0.0f) == -2147483648.0f);
  CHECK(op(kOpBitOr, -2147483648.0f, 0.0f) == -2147483648.0f);
  CHECK(op(kOpBitAnd, 2147483520.0f, 2147483520.0f) == 2147483520.0f);
}

namespace {

// A program for the entity opcodes: probe functions over globals E (entity), F (field), P, X (3).
struct EntityProbe {
  Progs progs;
  Vm vm;
  int E = 0, F = 0, X = 0, P = 0;
  std::vector<int32_t> fn_of_op = std::vector<int32_t>(kOpCount, 0);
  int32_t think = 0;
  EntityProbe() {
    Assembler as;
    E = as.temp();
    F = as.temp();
    P = as.temp();
    X = as.temp(3);
    for (uint16_t op :
         {kOpLoadF, kOpLoadV, kOpLoadS, kOpLoadEnt, kOpLoadFld, kOpLoadFnc, kOpAddress}) {
      fn_of_op[op] = as.begin_function(op_name(op), {}, 0);
      as.emit(op, E, F, op == kOpAddress ? P : X);
      as.emit(kOpDone);
      as.end_function();
    }
    for (uint16_t op :
         {kOpStorePF, kOpStorePV, kOpStorePS, kOpStorePEnt, kOpStorePFld, kOpStorePFnc}) {
      fn_of_op[op] = as.begin_function(op_name(op), {}, 0);
      as.emit(op, X, P);
      as.emit(kOpDone);
      as.end_function();
    }
    think = as.begin_function("think", {}, 0);
    as.emit(kOpDone);
    as.end_function();
    fn_of_op[kOpState] = as.begin_function("STATE", {}, 0);
    as.emit(kOpState, X, F);
    as.emit(kOpDone);
    as.end_function();
    std::string error;
    CHECK_MSG(as.build(progs, error), error);
    create_vm(vm, progs);
    vm.num_entities = 4;
  }
  bool run(uint16_t op) { return call_function(vm, fn_of_op[op]); }
};

}  // namespace

TEST(field_loads_read_an_entitys_words_and_a_negative_field_reads_zero) {
  EntityProbe p;
  *entity_field(p.vm, 2, kFOrigin) = w(5.0f);
  *entity_field(p.vm, 2, kFOrigin + 1) = w(6.0f);
  *entity_field(p.vm, 2, kFOrigin + 2) = w(7.0f);
  set_global_int(p.vm, p.E, entity_value(p.vm, 2));
  set_global_int(p.vm, p.F, kFOrigin + 1);
  CHECK(p.run(kOpLoadF));
  CHECK(global_float(p.vm, p.X) == 6.0f);
  set_global_int(p.vm, p.F, kFOrigin);
  CHECK(p.run(kOpLoadV));
  CHECK(global_float(p.vm, p.X + 2) == 7.0f);
  set_global_int(p.vm, p.F, -3);
  set_global_float(p.vm, p.X, 9.0f);
  CHECK(p.run(kOpLoadF));
  CHECK_MSG(global_int(p.vm, p.X) == 0, "LOAD_F of a negative field reads 0");
  CHECK(p.run(kOpLoadEnt));
  CHECK(global_int(p.vm, p.X) == 0);
  CHECK_MSG(!p.run(kOpLoadV), "LOAD_V of a negative field is a fault in this VM");
  CHECK(std::strstr(p.vm.fault.message, "bad field") != nullptr);
  clear_fault(p.vm);
  set_global_int(p.vm, p.F, p.vm.entityfields);
  CHECK(!p.run(kOpLoadF));
  CHECK(std::strstr(p.vm.fault.message, "bad field") != nullptr);
  clear_fault(p.vm);
  set_global_int(p.vm, p.F, 0);
  set_global_int(p.vm, p.E, entity_value(p.vm, 2) + 4);
  CHECK_MSG(!p.run(kOpLoadF), "an entity value off the entity grid is a fault");
  clear_fault(p.vm);
  set_global_int(p.vm, p.E, entity_value(p.vm, 4));
  CHECK_MSG(!p.run(kOpLoadF), "an entity past the ones in use is a fault");
  CHECK(std::strstr(p.vm.fault.message, "bad entity value") != nullptr);
}

TEST(address_then_storep_writes_the_field_and_the_world_is_guarded_while_running) {
  EntityProbe p;
  set_global_int(p.vm, p.E, entity_value(p.vm, 3));
  set_global_int(p.vm, p.F, kFVelocity);
  CHECK(p.run(kOpAddress));
  CHECK_MSG(global_int(p.vm, p.P) == entity_value(p.vm, 3) + kFVelocity * 4,
            "a pointer is the entity value plus the field in bytes");
  set_global_float(p.vm, p.X, 1.0f);
  set_global_float(p.vm, p.X + 1, 2.0f);
  set_global_float(p.vm, p.X + 2, 3.0f);
  CHECK(p.run(kOpStorePV));
  CHECK(f(*entity_field(p.vm, 3, kFVelocity + 2)) == 3.0f);
  set_global_float(p.vm, p.X, 8.0f);
  CHECK(p.run(kOpStorePF));
  CHECK(f(*entity_field(p.vm, 3, kFVelocity)) == 8.0f);
  for (uint16_t op : {kOpStorePS, kOpStorePEnt, kOpStorePFld, kOpStorePFnc}) {
    set_global_int(p.vm, p.X, 40 + op);
    CHECK(p.run(op));
    CHECK(static_cast<int32_t>(*entity_field(p.vm, 3, kFVelocity)) == 40 + op);
  }
  // The world: writable while spawning, a fault once the map runs.
  set_global_int(p.vm, p.E, 0);
  CHECK(p.run(kOpAddress));
  p.vm.world_locked = true;
  CHECK(!p.run(kOpAddress));
  CHECK(std::strcmp(p.vm.fault.message, "assignment to world entity") == 0);
  clear_fault(p.vm);
  // A pointer outside the entities in use.
  set_global_int(p.vm, p.P, p.vm.num_entities * p.vm.entity_bytes);
  CHECK(!p.run(kOpStorePF));
  CHECK(std::strstr(p.vm.fault.message, "bad pointer") != nullptr);
  clear_fault(p.vm);
  set_global_int(p.vm, p.P, 6);
  CHECK_MSG(!p.run(kOpStorePF), "a pointer off the word grid is a fault");
}

TEST(state_sets_frame_think_and_nextthink_at_time_plus_a_tenth_in_double) {
  EntityProbe p;
  set_global_int(p.vm, kGSelf, entity_value(p.vm, 1));
  const float time = 0.013f;  // one server tic: float and double sums differ here
  set_global_float(p.vm, kGTime, time);
  set_global_float(p.vm, p.X, 12.0f);
  set_global_int(p.vm, p.F, p.think);
  CHECK(p.run(kOpState));
  const float want = static_cast<float>(static_cast<double>(time) + 0.1);
  CHECK(w(f(*entity_field(p.vm, 1, kFNextthink))) == w(want));
  CHECK_MSG(w(want) != w(time + 0.1f), "this time tells double from float addition");
  CHECK(f(*entity_field(p.vm, 1, kFFrame)) == 12.0f);
  CHECK(static_cast<int32_t>(*entity_field(p.vm, 1, kFThink)) == p.think);
  set_global_int(p.vm, kGSelf, entity_value(p.vm, 9));
  CHECK_MSG(!p.run(kOpState), "STATE on a self that is not an entity in use is a fault");
}

TEST(strings_compare_by_content_and_the_empty_string_is_string_zero) {
  Probe p({kOpEqS, kOpNeS, kOpNotS});
  const auto run = [&](uint16_t op, int32_t a, int32_t b) {
    const uint32_t A[3] = {static_cast<uint32_t>(a), 0, 0}, B[3] = {static_cast<uint32_t>(b), 0, 0};
    p.set(p.A, A);
    p.set(p.B, B);
    CHECK(p.run(op));
    return f(p.c());
  };
  const int32_t e1 = engine_string(p.vm, "door");
  const int32_t e2 = engine_string(p.vm, "door");
  const int32_t empty = engine_string(p.vm, "");
  const int32_t t = temp_string(p.vm, "door");
  CHECK(e1 != e2);
  CHECK_MSG(run(kOpEqS, e1, e2) == 1.0f, "two engine strings with one text are equal");
  CHECK(run(kOpEqS, e1, t) == 1.0f);
  CHECK(run(kOpNeS, e1, t) == 0.0f);
  CHECK_MSG(run(kOpNeS, e1, 0) == 1.0f, "NE_S is 1 when the texts differ, in this VM");
  CHECK(run(kOpEqS, 0, empty) == 1.0f);
  CHECK(run(kOpNotS, 0, 0) == 1.0f);
  CHECK(run(kOpNotS, empty, 0) == 1.0f);
  CHECK(run(kOpNotS, e1, 0) == 0.0f);
  const uint32_t bad[3] = {static_cast<uint32_t>(-5000), 0, 0};
  p.set(p.A, bad);
  CHECK_MSG(!p.run(kOpNotS), "a string value that names no string is a fault");
  CHECK(std::strstr(p.vm.fault.message, "bad string value") != nullptr);
}

TEST(a_bad_opcode_is_refused_at_load_not_run) {
  Assembler as;
  as.begin_function("bad", {}, 0);
  as.emit(66);
  as.end_function();
  Progs p;
  std::string error;
  CHECK(!as.build(p, error));
  CHECK_MSG(error.find("is not an opcode") != std::string::npos, error);
}
