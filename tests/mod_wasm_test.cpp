// THE WEBASSEMBLY INTERPRETER (engine/mod/wasm): a module clang compiled from C
// (mod_wasm_fixture.hpp) is read, instantiated over the host's imports and run: recursion, i64 and
// float arithmetic, memory, a call through the table, a br_table, conversions, the host's
// functions by import and by function pointer; the traps (fuel, division by zero, a float too big,
// a NaN to an integer) each named; the NaN an operation makes is the canonical one; a copied
// instance carries on as the original does; malformed modules are refused with the byte offset.
//
// ge:summary Tests the WebAssembly interpreter: a clang module run, traps, NaNs, copies, refusals
// ge:keywords test wasm interpreter instantiate call trap fuel canonical-nan refuse validate
// ge:skill mod testing
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "check.hpp"
#include "mod/wasm/interp.hpp"
#include "mod/wasm/module.hpp"
#include "mod_wasm_fixture.hpp"

namespace w = ge::mod::wasm;

namespace {

constexpr uint64_t kFuel = 10'000'000;

struct Host {
  std::vector<std::string> log;
};

bool host_add(w::Instance&, void*, const uint64_t* args, uint64_t* results) {
  results[0] =
      static_cast<uint32_t>(static_cast<uint32_t>(args[0]) + static_cast<uint32_t>(args[1]));
  return true;
}

bool host_log(w::Instance& in, void* user, const uint64_t* args, uint64_t*) {
  std::string s;
  if (!w::read_string(in, static_cast<uint32_t>(args[0]), 256, s)) {
    in.trap_message = "host_log: a string past the memory";
    return false;
  }
  static_cast<Host*>(user)->log.push_back(s);
  return true;
}

bool host_mul(w::Instance&, void*, const uint64_t* args, uint64_t* results) {
  results[0] =
      static_cast<uint32_t>(static_cast<uint32_t>(args[0]) * static_cast<uint32_t>(args[1]));
  return true;
}

const w::FuncType kII_I{{w::ValType::i32, w::ValType::i32}, {w::ValType::i32}};

std::vector<w::HostImport> imports(Host& h) {
  return {{"env", "host_add", kII_I, host_add, &h},
          {"env", "host_log", {{w::ValType::i32}, {}}, host_log, &h}};
}

struct Fixture {
  w::Module module;
  Host host;
  w::Instance in;
  bool ok = false;
  std::string error;
  Fixture() {
    ok = w::read_module(kWasmFixture, sizeof kWasmFixture, module, error) &&
         w::instantiate(module, imports(host), 256, kFuel, in, error);
  }
  uint32_t fn(const char* name) const {
    const w::Export* e = module.find_export(name, w::ExternKind::func);
    return e ? e->index : 0xffffffffu;
  }
};

uint64_t f64_bits(double d) { return std::bit_cast<uint64_t>(d); }
double as_f64(uint64_t b) { return std::bit_cast<double>(b); }
uint64_t f32_bits(float f) { return std::bit_cast<uint32_t>(f); }

// A module from its sections (id, payload), each length-prefixed.
std::vector<uint8_t> module_of(const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& s) {
  std::vector<uint8_t> b = {0, 'a', 's', 'm', 1, 0, 0, 0};
  for (const auto& [id, payload] : s) {
    b.push_back(id);
    b.push_back(static_cast<uint8_t>(payload.size()));  // the tests' sections are short
    b.insert(b.end(), payload.begin(), payload.end());
  }
  return b;
}

// One function of type () -> i32 with `body` (its locals' count, then its code and end).
std::vector<uint8_t> one_function(const std::vector<uint8_t>& body) {
  std::vector<uint8_t> code = {1, static_cast<uint8_t>(body.size())};
  code.insert(code.end(), body.begin(), body.end());
  return module_of({{1, {1, 0x60, 0, 1, 0x7f}}, {3, {1, 0}}, {10, code}});
}

}  // namespace

TEST(a_clang_module_reads_and_instantiates) {
  Fixture f;
  CHECK_MSG(f.ok, f.error);
  CHECK(f.module.imports.size() == 2);
  CHECK(f.module.has_memory && f.module.has_table);
  CHECK(f.fn("fib") != 0xffffffffu && f.fn("apply") != 0xffffffffu);
}

TEST(calls_arithmetic_and_memory_give_the_c_answers) {
  Fixture f;
  if (!f.ok) return;
  uint64_t r[1] = {};
  uint64_t a[2] = {20, 0};
  CHECK(w::call(f.in, f.fn("fib"), a, r, kFuel) == w::Trap::none);
  CHECK(r[0] == 6765);
  uint64_t m[2] = {static_cast<uint64_t>(-3'000'000'000ll), 7'000'000'000ull};
  CHECK(w::call(f.in, f.fn("mul64"), m, r, kFuel) == w::Trap::none);
  CHECK(r[0] ==
        static_cast<uint64_t>(-3'000'000'000ll) * 7'000'000'000ull);  // wraps, as i64.mul does
  uint64_t d[2] = {f64_bits(2.0), f64_bits(3.0)};
  CHECK(w::call(f.in, f.fn("dmix"), d, r, kFuel) == w::Trap::none);
  const double two = 2.0, three = 3.0;
  CHECK(r[0] == f64_bits(two * three + two / three - std::sqrt(two)));
  uint64_t fl[2] = {f32_bits(1.5f), f32_bits(2.25f)};
  CHECK(w::call(f.in, f.fn("fmix"), fl, r, kFuel) == w::Trap::none);
  CHECK(r[0] == f32_bits(1.5f * 2.0f + 2.0f));
  uint64_t n[1] = {10};
  CHECK(w::call(f.in, f.fn("sum_mem"), n, r, kFuel) == w::Trap::none);
  CHECK(r[0] == 88);  // fib(0) + ... + fib(9)
  for (int x = 0; x < 8; x++) {
    const int want[] = {10, 20, 30, 47, 55, 61, -1, -1};
    uint64_t s[1] = {static_cast<uint64_t>(x)};
    CHECK(w::call(f.in, f.fn("switchy"), s, r, kFuel) == w::Trap::none);
    CHECK(static_cast<int32_t>(r[0]) == want[x]);
  }
}

TEST(a_call_through_the_table_and_the_hosts_functions) {
  Fixture f;
  if (!f.ok) return;
  uint64_t r[1] = {};
  uint64_t add[3] = {0, 5, 3}, sub[3] = {1, 5, 3};
  CHECK(w::call(f.in, f.fn("apply"), add, r, kFuel) == w::Trap::none && r[0] == 8);
  CHECK(w::call(f.in, f.fn("apply"), sub, r, kFuel) == w::Trap::none && r[0] == 2);
  uint64_t h[2] = {2, 3};
  CHECK(w::call(f.in, f.fn("call_host"), h, r, kFuel) == w::Trap::none && r[0] == 10);
  CHECK(f.host.log.size() == 1 && f.host.log[0] == "hello");
  // A host function put in the table and stored where the C keeps its function pointer: the
  // module calls it as it calls its own.
  const uint32_t slot = w::add_host_function(f.in, kII_I, host_mul, nullptr);
  CHECK(slot != w::kNullFunc);
  const w::Export* ops = f.module.find_export("ops", w::ExternKind::global);
  CHECK(ops != nullptr);
  if (ops == nullptr) return;
  const uint32_t at = static_cast<uint32_t>(f.in.globals[ops->index]) + 4;  // ops[1]
  CHECK(w::write_memory(f.in, at, &slot, 4));
  CHECK(w::call(f.in, f.fn("apply"), sub, r, kFuel) == w::Trap::none);
  CHECK(r[0] == 15);
}

TEST(traps_are_named_and_stop_the_call) {
  Fixture f;
  if (!f.ok) return;
  uint64_t r[1] = {};
  uint64_t one[1] = {1};
  CHECK(w::call(f.in, f.fn("forever"), one, r, 100000) == w::Trap::fuel);
  CHECK_MSG(f.in.trap_message.find("budget") != std::string::npos, f.in.trap_message);
  uint64_t div[2] = {7, 0};
  CHECK(w::call(f.in, f.fn("divide"), div, r, kFuel) == w::Trap::divide_by_zero);
  uint64_t ovf[2] = {0x80000000u, 0xffffffffu};
  CHECK(w::call(f.in, f.fn("divide"), ovf, r, kFuel) == w::Trap::overflow);
  uint64_t big[1] = {f64_bits(1e10)};
  CHECK(w::call(f.in, f.fn("to_int"), big, r, kFuel) == w::Trap::overflow);
  uint64_t nan[1] = {f64_bits(std::numeric_limits<double>::quiet_NaN())};
  CHECK(w::call(f.in, f.fn("to_int"), nan, r, kFuel) == w::Trap::conversion);
  uint64_t ok[1] = {f64_bits(-2147483648.9)};
  CHECK(w::call(f.in, f.fn("to_int"), ok, r, kFuel) == w::Trap::none);
  CHECK(static_cast<int32_t>(r[0]) == -2147483647 - 1);
  // After a trap the instance runs on.
  uint64_t a[1] = {10};
  CHECK(w::call(f.in, f.fn("fib"), a, r, kFuel) == w::Trap::none && r[0] == 55);
}

TEST(conversions_and_nans_are_the_same_bits_everywhere) {
  Fixture f;
  if (!f.ok) return;
  uint64_t r[1] = {};
  uint64_t big[1] = {f64_bits(1e19)};
  CHECK(w::call(f.in, f.fn("to_u64"), big, r, kFuel) == w::Trap::none);
  CHECK(r[0] == 10'000'000'000'000'000'000ull);
  uint64_t max[1] = {~uint64_t{0}};
  CHECK(w::call(f.in, f.fn("from_u64"), max, r, kFuel) == w::Trap::none);
  CHECK(as_f64(r[0]) == 18446744073709551616.0);
  uint64_t odd[1] = {(uint64_t{1} << 63) + 1025};  // between two doubles, nearer the upper
  CHECK(w::call(f.in, f.fn("from_u64"), odd, r, kFuel) == w::Trap::none);
  CHECK(as_f64(r[0]) == 9223372036854777856.0);
  uint64_t inf[1] = {f32_bits(std::numeric_limits<float>::infinity())};
  CHECK(w::call(f.in, f.fn("nanf_mul"), inf, r, kFuel) == w::Trap::none);
  CHECK(r[0] == 0x7fc00000u);  // inf * 0 is the canonical NaN, whatever the CPU makes
}

TEST(a_copied_instance_carries_on_as_the_original) {
  Fixture f;
  if (!f.ok) return;
  uint64_t r1[1] = {}, r2[1] = {};
  uint64_t n[1] = {12};
  CHECK(w::call(f.in, f.fn("sum_mem"), n, r1, kFuel) == w::Trap::none);
  w::Instance copy = f.in;
  CHECK(copy.memory == f.in.memory && copy.globals == f.in.globals);
  uint64_t m[1] = {20};
  CHECK(w::call(f.in, f.fn("sum_mem"), m, r1, kFuel) == w::Trap::none);
  CHECK(w::call(copy, f.fn("sum_mem"), m, r2, kFuel) == w::Trap::none);
  CHECK(r1[0] == r2[0] && copy.memory == f.in.memory);
}

TEST(malformed_modules_are_refused_with_the_byte) {
  w::Module m;
  std::string error;
  const uint8_t not_wasm[] = {'G', 'E', 'M', 'D', 1, 0, 0, 0};
  CHECK(!w::read_module(not_wasm, sizeof not_wasm, m, error));
  CHECK_MSG(error.find("not a WebAssembly module") != std::string::npos, error);
  // Cut short: no prefix of the fixture crashes the reader (a prefix that ends on a section's
  // boundary is a smaller module, and may read); one cut inside a section is refused.
  for (size_t n = 8; n < sizeof kWasmFixture; n++) (void)w::read_module(kWasmFixture, n, m, error);
  const uint8_t cut[] = {0, 'a', 's', 'm', 1, 0, 0, 0, 1, 10, 1, 0x60, 0};
  CHECK(!w::read_module(cut, sizeof cut, m, error));
  CHECK_MSG(error.find("past the end") != std::string::npos, error);
  // i32.add over an i64: a type error, named with its byte.
  const std::vector<uint8_t> bad = one_function({0, 0x42, 1, 0x41, 2, 0x6a, 0x0b});
  CHECK(!w::read_module(bad.data(), bad.size(), m, error));
  CHECK_MSG(error.find("i64 where i32") != std::string::npos, error);
  CHECK_MSG(error.find("at byte") != std::string::npos, error);
  // An instruction the engine does not run (a SIMD prefix).
  const std::vector<uint8_t> simd = one_function({0, 0xfd, 0x0c, 0x0b});
  CHECK(!w::read_module(simd.data(), simd.size(), m, error));
  CHECK_MSG(error.find("0xfd") != std::string::npos, error);
  // A branch past the enclosing blocks.
  const std::vector<uint8_t> br = one_function({0, 0x41, 1, 0x0c, 3, 0x0b});
  CHECK(!w::read_module(br.data(), br.size(), m, error));
  CHECK_MSG(error.find("label 3") != std::string::npos, error);
  // A good one: (i32.const 40) (i32.const 2) i32.add.
  const std::vector<uint8_t> good = one_function({0, 0x41, 40, 0x41, 2, 0x6a, 0x0b});
  CHECK_MSG(w::read_module(good.data(), good.size(), m, error), error);
  w::Instance in;
  CHECK_MSG(w::instantiate(m, {}, 1, kFuel, in, error), error);
  uint64_t r[1] = {};
  CHECK(w::call(in, 0, nullptr, r, kFuel) == w::Trap::none && r[0] == 42);
}

TEST(a_missing_import_is_refused_by_name) {
  w::Module m;
  std::string error;
  CHECK(w::read_module(kWasmFixture, sizeof kWasmFixture, m, error));
  Host h;
  std::vector<w::HostImport> only_add = {imports(h)[0]};
  w::Instance in;
  CHECK(!w::instantiate(m, only_add, 256, kFuel, in, error));
  CHECK_MSG(error.find("env.host_log") != std::string::npos, error);
}

TEST(damaged_modules_never_crash_the_reader_or_the_interpreter) {
  // Every byte of the fixture changed in turn (three ways): a module that still reads is
  // instantiated and each of its functions called on zeros with a small budget. Whatever a damaged
  // module does, the validator keeps the interpreter inside its stack and memory (ASan holds it).
  int read = 0;
  for (size_t at = 8; at < sizeof kWasmFixture; at++) {
    for (uint8_t x : {uint8_t{0x01}, uint8_t{0x80}, uint8_t{0xff}}) {
      std::vector<uint8_t> b(kWasmFixture, kWasmFixture + sizeof kWasmFixture);
      b[at] ^= x;
      w::Module m;
      std::string error;
      if (!w::read_module(b.data(), b.size(), m, error)) continue;
      read++;
      Host h;
      w::Instance in;
      if (!w::instantiate(m, imports(h), 64, 10000, in, error)) continue;
      for (uint32_t fn = static_cast<uint32_t>(m.imports.size()); fn < m.func_count(); fn++) {
        uint64_t args[16] = {}, results[16] = {};
        if (m.func_type(fn).params.size() > 16 || m.func_type(fn).results.size() > 16) continue;
        (void)w::call(in, fn, args, results, 10000);
      }
    }
  }
  CHECK(read > 0);  // some changes leave a valid module (a constant, a name), and those ran
}
