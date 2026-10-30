// Tests: the standard builtins over the VM alone (plugins/progs/stdlib.hpp), each by its standard
// number: the vector math, the number text in the builtins' ring, the strings and the zone, the
// words, min/max/bound, and random and randomvec drawn from the game's seeded generator, held to
// the bit by tests/golden/progs_stdlib.txt.
//
// ge:summary Tests: every stdlib builtin by number, with random's and randomvec's golden draws
// ge:keywords test qc stdlib builtins random randomvec vectoangles vectoyaw normalize vlen rint
// ge:keywords strzone strunzone substr strcat tokenize argv golden
// ge:skill progs testing
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "golden.hpp"
#include "progs/assembler.hpp"
#include "progs/stdlib.hpp"

namespace pg = ge::progs;

namespace {

constexpr const char* kGolden = "tests/golden/progs_stdlib.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

struct Machine {
  pg::Progs progs;
  pg::Vm vm;
  ge::core::Rng rng = ge::core::make_rng(7);

  Machine() {
    pg::Assembler a;
    std::string error;
    CHECK_MSG(a.build(progs, error), error);
    pg::create_vm(vm, progs);
    vm.rng = &rng;
  }

  // An argument: a float, a vector or text (an engine string).
  struct Arg {
    int kind = 0;  // 0 float, 1 vector, 2 text
    float v[3] = {0, 0, 0};
    std::string text;
    Arg(float f) { v[0] = f; }                    // NOLINT: argument lists read best
    Arg(int i) { v[0] = static_cast<float>(i); }  // NOLINT
    Arg(float x, float y, float z) : kind(1), v{x, y, z} {}
    Arg(const char* t) : kind(2), text(t) {}  // NOLINT
  };

  void call(const char* name, std::initializer_list<Arg> args) {
    const pg::StdBuiltin* b = pg::stdlib_builtin(name);
    CHECK_MSG(b != nullptr, std::string("no stdlib builtin ") + name);
    if (b == nullptr) return;
    int i = 0;
    for (const Arg& a : args) {
      if (a.kind == 2) {
        pg::set_global_int(vm, pg::parm(i), pg::engine_string(vm, a.text.c_str()));
      } else {
        for (int k = 0; k < (a.kind == 1 ? 3 : 1); k++)
          pg::set_global_float(vm, pg::parm(i) + k, a.v[k]);
      }
      i++;
    }
    vm.argc = i;
    b->fn(vm);
  }
  float f() const { return pg::global_float(vm, pg::kGlobalReturn); }
  float f(int k) const { return pg::global_float(vm, pg::kGlobalReturn + k); }
  std::string s() const {
    const char* t = pg::vm_string(vm, pg::global_int(vm, pg::kGlobalReturn));
    return t == nullptr ? "(not a string)" : t;
  }
};

}  // namespace

TEST(every_stdlib_builtin_has_its_standard_number) {
  const std::pair<int, const char*> want[] = {
      {7, "random"},     {9, "normalize"},  {12, "vlen"},          {13, "vectoyaw"},
      {26, "ftos"},      {27, "vtos"},      {36, "rint"},          {37, "floor"},
      {38, "ceil"},      {43, "fabs"},      {51, "vectoangles"},   {60, "sin"},
      {61, "cos"},       {62, "sqrt"},      {68, "precache_file"}, {77, "precache_file2"},
      {81, "stof"},      {84, "tokenize"},  {85, "argc"},          {86, "argv"},
      {91, "randomvec"}, {94, "min"},       {95, "max"},           {96, "bound"},
      {97, "pow"},       {114, "strlen"},   {115, "strcat"},       {116, "substr"},
      {118, "strzone"},  {119, "strunzone"}};
  CHECK(pg::kStdlibCount == static_cast<int>(sizeof want / sizeof want[0]));
  for (const auto& [n, name] : want) {
    const pg::StdBuiltin* b = pg::stdlib_builtin(name);
    CHECK_MSG(b != nullptr && b->number == n,
              std::string(name) + " is not builtin " + std::to_string(n));
  }
}

TEST(the_vector_builtins) {
  Machine m;
  m.call("vlen", {{3, 4, 12}});
  CHECK(m.f() == 13.0f);
  m.call("normalize", {{0, 3, 4}});
  CHECK(m.f(0) == 0.0f && m.f(1) == 0.6f && m.f(2) == 0.8f);
  m.call("normalize", {{0, 0, 0}});
  CHECK(m.f(0) == 0.0f && m.f(1) == 0.0f && m.f(2) == 0.0f);
  m.call("vectoyaw", {{0, 0, 5}});
  CHECK(m.f() == 0.0f);
  m.call("vectoyaw", {{0, -1, 0}});
  CHECK(m.f() == 270.0f);
  m.call("vectoyaw", {{1, 1, 0}});
  CHECK(m.f() == 45.0f);
  m.call("vectoangles", {{0, 0, 3}});
  CHECK(m.f(0) == 90.0f && m.f(1) == 0.0f && m.f(2) == 0.0f);
  m.call("vectoangles", {{0, 0, -3}});
  CHECK(m.f(0) == 270.0f);
  m.call("vectoangles", {{1, 0, -1}});
  CHECK(m.f(0) == 315.0f && m.f(1) == 0.0f);
  m.call("vectoangles", {{-1, 0, 0}});
  CHECK(m.f(0) == 0.0f && m.f(1) == 180.0f);
}

TEST(the_number_builtins) {
  Machine m;
  m.call("rint", {2.5f});
  CHECK(m.f() == 3.0f);
  m.call("rint", {-2.5f});
  CHECK(m.f() == -3.0f);
  m.call("rint", {1e10f});
  CHECK(m.f() == -2147483648.0f);
  m.call("floor", {-1.5f});
  CHECK(m.f() == -2.0f);
  m.call("ceil", {-1.5f});
  CHECK(m.f() == -1.0f);
  m.call("fabs", {-3.25f});
  CHECK(m.f() == 3.25f);
  m.call("sqrt", {2});
  CHECK(m.f() == 1.41421354f);
  m.call("sin", {0});
  CHECK(m.f() == 0.0f);
  m.call("cos", {0});
  CHECK(m.f() == 1.0f);
  m.call("pow", {2, 10});
  CHECK(m.f() == 1024.0f);
  m.call("min", {3, -1, 2});
  CHECK(m.f() == -1.0f);
  m.call("max", {3, -1, 7});
  CHECK(m.f() == 7.0f);
  m.call("bound", {0, 5, 3});
  CHECK(m.f() == 3.0f);
  m.call("bound", {0, -5, 3});
  CHECK(m.f() == 0.0f);
  m.call("min", {3});
  CHECK(m.vm.fault.set);
  pg::clear_fault(m.vm);
}

TEST(number_text_goes_to_the_builtins_ring) {
  Machine m;
  m.call("ftos", {12});
  CHECK(m.s() == "12");
  CHECK(pg::global_int(m.vm, pg::kGlobalReturn) <= -(pg::kBuiltinTempBase));
  m.call("vtos", {{1, 2.5f, -3}});
  CHECK(m.s() == "'  1.0   2.5  -3.0'");
  m.call("stof", {"0x20"});
  CHECK(m.f() == 32.0f);
  // The ring holds 64: the 65th overwrites the first.
  m.call("ftos", {1});
  const int32_t first = pg::global_int(m.vm, pg::kGlobalReturn);
  for (int i = 0; i < pg::kBuiltinTempStrings; i++) m.call("ftos", {static_cast<float>(i + 100)});
  CHECK(std::string(pg::vm_string(m.vm, first)) != "1");
}

TEST(the_string_builtins) {
  Machine m;
  m.call("strlen", {"hello"});
  CHECK(m.f() == 5.0f);
  m.call("strcat", {"a", "bc", "d"});
  CHECK(m.s() == "abcd");
  m.call("substr", {"hello", 1, 3});
  CHECK(m.s() == "ell");
  m.call("substr", {"hello", 3, 99});
  CHECK(m.s() == "lo");
  m.call("substr", {"hello", 9, 1});
  CHECK(m.s().empty());
  m.call("substr", {"hello", -2, -1});
  CHECK(m.s() == "hello");  // ours: a negative start is 0 and a negative length runs to the end
  m.call("substr", {"", 0, 3});
  CHECK(m.s().empty());
  m.call("precache_file", {"maps/x.bsp"});
  CHECK(m.s() == "maps/x.bsp");
}

TEST(strzone_keeps_a_string_until_strunzone) {
  Machine m;
  m.call("strzone", {"kept"});
  const int32_t z = pg::global_int(m.vm, pg::kGlobalReturn);
  CHECK(z <= -pg::kZoneBase);
  for (int i = 0; i < 100; i++) m.call("ftos", {static_cast<float>(i)});
  CHECK(std::string(pg::vm_string(m.vm, z)) == "kept");
  pg::set_global_int(m.vm, pg::parm(0), z);
  m.vm.argc = 1;
  pg::stdlib_builtin("strunzone")->fn(m.vm);
  CHECK(!m.vm.fault.set);
  CHECK(std::string(pg::vm_string(m.vm, z)).empty());
  pg::stdlib_builtin("strunzone")->fn(m.vm);  // twice is allowed
  CHECK(!m.vm.fault.set);
  m.call("strunzone", {"not a zone string"});
  CHECK(m.vm.fault.set);
  pg::clear_fault(m.vm);
}

TEST(tokenize_argc_argv) {
  Machine m;
  m.call("tokenize", {"give \"rocket launcher\" 5"});
  CHECK(m.f() == 3.0f);
  m.call("argc", {});
  CHECK(m.f() == 3.0f);
  m.call("argv", {1});
  CHECK(m.s() == "rocket launcher");
  m.call("argv", {3});
  CHECK(m.s().empty());
}

TEST(random_without_a_generator_faults) {
  Machine m;
  m.vm.rng = nullptr;
  m.call("random", {});
  CHECK(m.vm.fault.set);
  pg::clear_fault(m.vm);
}

TEST(random_and_randomvec_hold_their_golden_draws) {
  Machine m;
  std::vector<std::vector<uint64_t>> got;
  for (int i = 0; i < 16; i++) {
    m.call("random", {});
    CHECK(m.f() >= 0.0f && m.f() <= 1.0f);
    got.push_back({0, pg::as_word(m.f()), 0, 0});
  }
  for (int i = 0; i < 16; i++) {
    m.call("randomvec", {});
    const float x = m.f(0), y = m.f(1), z = m.f(2);
    CHECK((x * x + y * y) + z * z < 1.0f);
    got.push_back({1, pg::as_word(x), pg::as_word(y), pg::as_word(z)});
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/progs/stdlib.hpp's own record: random (0) and randomvec (1) from seed 7, the "
           "floats' bits (tests/progs_stdlib_test.cpp)\n";
    for (const auto& row : got) {
      char line[96];
      std::snprintf(
          line, sizeof line, "%llx %08llx %08llx %08llx\n", static_cast<unsigned long long>(row[0]),
          static_cast<unsigned long long>(row[1]), static_cast<unsigned long long>(row[2]),
          static_cast<unsigned long long>(row[3]));
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "draw " + std::to_string(i) +
                                     " moved off the golden: random's rule or core::int_in "
                                     "changed (rewrite with GE_GOLDEN_WRITE=1 only if meant)");
}
