// Tests: the QC compiler. Programs compiled from source load through the checker and compute on
// the VM what their source says (arithmetic, recursion, loops with break and continue, vectors and
// their members, strings, entities and their fields, compound assignment, calls inside calls);
// every construct's bytecode is held by tests/golden/progs_qc_compile.txt (the disassembly of one
// program that uses them all); a build is the same bytes twice; a mistake stops the build with its
// file, its line and what to do.
//
// A deliberate change to the code the compiler emits moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/progs_qc_compile_test` and say why in the PR (skill:
// progs).
//
// ge:summary Tests: QC source compiled, run on the VM, its bytecode held golden, errors with lines
// ge:keywords test qc compiler compile golden disassembly recursion loops vectors fields
// ge:keywords diagnostics determinism compile_qc
// ge:skill progs testing
// ge:test tests/progs_qc_compile_test.cpp
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "progs/disasm.hpp"
#include "progs/qc_compile.hpp"
#include "progs/sample.hpp"
#include "progs/vm.hpp"

using namespace ge::progs;

namespace {

constexpr const char* kGolden = "tests/golden/progs_qc_compile.txt";

// The sample program's builtins, as a QC source declares them.
constexpr const char* kBuiltins = R"(
string(float f) ftos = #1;
float(vector v) vlen = #2;
entity() spawn = #4;
void(string s, ...) print = #5;
)";

QcResult build(const std::string& text) {
  const QcSource src[] = {{"builtins.qc", kBuiltins}, {"test.qc", text}};
  return compile_qc(src);
}

std::string diagnostics(const QcResult& r) {
  std::string out;
  for (const QcDiagnostic& d : r.diagnostics) out += diagnostic_text(d) + "\n";
  return out;
}

// Compiles `text`, loads it through the checker, runs `fn` with float arguments and returns the
// float in the return slot (the printed text in `printed`).
float run(const std::string& text, const char* fn, std::initializer_list<float> args = {},
          std::string* printed = nullptr) {
  const QcResult r = build(text);
  CHECK_MSG(r.ok, diagnostics(r));
  if (!r.ok) return -1;
  const std::vector<uint8_t> bytes = write_progs(r.progs);
  Progs p;
  std::string error;
  LoadOptions o;
  o.crc = 0;
  o.check_layout = false;
  CHECK_MSG(load_progs(bytes.data(), bytes.size(), p, error, o), error);
  Vm vm;
  create_vm(vm, p);
  SampleOutput out;
  vm.user = &out;
  install_sample_builtins(vm);
  int i = 0;
  for (float a : args) set_global_float(vm, parm(i++), a);
  CHECK_MSG(call_function(vm, find_function(p, fn)), fault_text(vm));
  if (printed) *printed = out.text;
  return global_float(vm, kGlobalReturn);
}

// One program that uses every construct, for the golden disassembly.
constexpr const char* kConstructs = R"(
float LIMIT = 3;
vector UP = '0 0 1';
string HELLO = "hello";
.float health;
.vector org;
float counter;

float(float a, float b) add = { return a + b; };
float sub(float a, float b) { return a - b; }
float(float n) fact = { if (n <= 1) return 1; return n * fact(n - 1); };

float(float n) loops = {
  local float i, sum;
  sum = 0;
  for (i = 0; i < n; i += 1) {
    if (i == 2) continue;
    if (i > 6) break;
    sum = sum + i;
  }
  while (sum > 10) sum -= 10;
  do { sum = sum * 2; } while (sum < 5);
  return sum;
};

float(vector v) vectors = {
  local vector w = v * 2 + UP;
  w.y = w_x - v.z;
  w = w / 4;
  return w * v + vlen(-w);
};

float(float x) logic = {
  if (!x || (x & 2) && x != LIMIT) return 1;
  else if (x >= 4 | 1) return 2;
  return -x;
};

void(entity e) fields = {
  e.health = e.health + 1;
  e.org = e.org + UP;
  e.org_x *= 2;
  e.org.y = add(e.org.x, sub(3, e.health));
  counter += 1;
};

void() strings = {
  if (HELLO == "hello") print(HELLO, " ", ftos(fact(LIMIT)), "\n");
};
)";

}  // namespace

TEST(arithmetic_and_recursion_compute) {
  const char* src = R"(
    float(float a, float b) add = { return a + b * 2; };
    float(float n) fact = { if (n <= 1) return 1; return n * fact(n - 1); };
    float(float n) fib = { if (n < 2) return n; return fib(n - 1) + fib(n - 2); };
    float(float x) nested = { return add(add(x, 1), add(2, x)); };
  )";
  CHECK(run(src, "add", {1, 3}) == 7);
  CHECK(run(src, "fact", {5}) == 120);
  CHECK(run(src, "fib", {10}) == 55);
  CHECK(run(src, "nested", {1}) == 11);  // add(add(1, 1), add(2, 1)) = add(3, 4) = 3 + 8
}

TEST(loops_break_and_continue) {
  const char* src = R"(
    float(float n) sum_to = { local float i, s; for (i = 1; i <= n; i += 1) s += i; return s; };
    float() skip = {
      float i = 0, s = 0;
      while (1) { i += 1; if (i > 10) break; if (i & 1) continue; s += i; }
      return s;
    };
    float(float n) halve = { local float k; do { n = n / 2; k += 1; } while (n >= 1); return k; };
  )";
  CHECK(run(src, "sum_to", {100}) == 5050);
  CHECK(run(src, "skip") == 30);  // 2 + 4 + 6 + 8 + 10
  CHECK(run(src, "halve", {8}) == 4);
  CHECK(run(src, "halve", {0.5f}) == 1);
}

TEST(vectors_members_and_products) {
  const char* src = R"(
    vector ONE = '1 2 3';
    float() dot = { return '1 1 1' * ONE; };
    float() members = { local vector v = ONE * 2; v.y = 10; v_z += 1; return v.x + v.y + v.z; };
    float() divided = { local vector v = '4 8 12' / 4; return v_x * 100 + v_y * 10 + v_z; };
    float() negated = { local vector v = -ONE; return v.x + v.y + v.z; };
    float() length = { return vlen('3 4 0'); };
    float() test = { if ('0 0 0') return 1; if (ONE) return 2; return 3; };
  )";
  CHECK(run(src, "dot") == 6);
  CHECK(run(src, "members") == 19);  // 2 + 10 + 7
  CHECK(run(src, "divided") == 123);
  CHECK(run(src, "negated") == -6);
  CHECK(run(src, "length") == 5);
  CHECK(run(src, "test") == 2);
}

TEST(entities_and_fields) {
  const char* src = R"(
    .float health;
    .vector org;
    .entity friend;
    .float(float x) twice;
    float(float x) double_it = { return x * 2; };
    float() go = {
      local entity a = spawn(), b = spawn();
      a.health = 5;
      a.health += 2;
      a.org = '1 2 3';
      a.org_y = 20;
      a.org.z += a.health;
      a.friend = b;
      a.friend.health = 100;
      a.twice = double_it;
      return a.health + a.org.x + a.org_y + a.org_z + b.health + a.twice(4) + (a.friend == b);
    };
  )";
  // 7 + 1 + 20 + 10 + 100 + 8 + 1
  CHECK(run(src, "go") == 147);
}

TEST(strings_compare_and_print) {
  const char* src = R"(
    string NAME = "edge";
    float(string s) same = { return s == NAME; };
    float() go = {
      print("frags: ", ftos(12), "\n");
      if (same("edge") && !same("other")) return 1;
      return 0;
    };
  )";
  std::string printed;
  CHECK(run(src, "go", {}, &printed) == 1);
  CHECK_MSG(printed == "frags: 12\n", printed);
}

TEST(the_constructs_compile_to_the_golden_bytecode) {
  const QcResult r = build(kConstructs);
  CHECK_MSG(r.ok, diagnostics(r));
  if (!r.ok) return;
  std::string got;
  for (size_t f = 1; f < r.progs.functions.size(); ++f) {
    got += function_text(r.progs, static_cast<int>(f));
  }
  if (std::getenv("GE_GOLDEN_WRITE") != nullptr) {
    std::ofstream(ge::test::root_path(kGolden))
        << "# The disassembly of tests/progs_qc_compile_test.cpp's kConstructs; written by that "
           "test with GE_GOLDEN_WRITE=1\n"
        << got;
    return;
  }
  std::ifstream in(ge::test::root_path(kGolden));
  std::string want, line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] == '#') continue;
    want += line + "\n";
  }
  CHECK_MSG(got == want, "the compiler's code moved:\n" + got + "\nIf it is meant, rewrite " +
                             kGolden + " with GE_GOLDEN_WRITE=1 and say why (skill: progs)");
}

TEST(a_build_is_the_same_bytes_twice) {
  const QcResult a = build(kConstructs), b = build(kConstructs);
  CHECK(a.ok && b.ok);
  CHECK(write_progs(a.progs) == write_progs(b.progs));
  CHECK(a.progs.lines.size() == a.progs.statements.size());
}

TEST(mistakes_name_their_line) {
  const struct {
    const char* src;
    int line;
    const char* says;
  } cases[] = {
      {"float x;\nfloat() f = { return y; };", 2, "unknown name 'y'"},
      {"float x\nfloat y;", 2, "expected ';'"},
      {"\n\nfloat() f = { local string s = 1; return 0; };", 3, "wants string"},
      {"float() f;", 1, "never defined"},
      {"vector v;\nfloat() f = { return v; };", 2, "wants float"},
      {"float K = 1;\nvoid() f = { K = 2; };", 2, "cannot be assigned"},
      {"void() f = {\n  break;\n};", 2, "outside a loop"},
      {"float() f = { return 1 + \"a\"; };", 1, "does not take float and string"},
      {"void() f = { /* open", 1, "never closed"},
  };
  for (const auto& c : cases) {
    const QcResult r = build(c.src);
    CHECK_MSG(!r.ok, std::string("compiled: ") + c.src);
    if (r.ok || r.diagnostics.empty()) continue;
    const QcDiagnostic& d = r.diagnostics.back();
    CHECK_MSG(
        d.file == "test.qc" && d.line == c.line && d.message.find(c.says) != std::string::npos,
        diagnostic_text(d) + " (want line " + std::to_string(c.line) + ": " + c.says + ")");
  }
}
