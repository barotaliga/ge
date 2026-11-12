// Tests: the extended string builtins and sprintf over the VM alone (plugins/progs/qc_strings.hpp):
// stov's numbers, strstrofs, str2chr and chr2str, strconv's case and colours, strpad, the info
// string's add and get, the comparisons, and sprintf's conversions, positions, widths and stops.
//
// ge:summary Tests: the string set (stov ... strncasecmp) and sprintf, each rule by example
// ge:keywords test qc strings stov strstrofs str2chr chr2str strconv strpad infoadd infoget
// ge:keywords strncmp strcasecmp sprintf format-string
// ge:skill progs testing
#include <initializer_list>
#include <string>

#include "check.hpp"
#include "core/prng.hpp"
#include "progs/assembler.hpp"
#include "progs/stdlib.hpp"

namespace pg = ge::progs;

namespace {

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

TEST(stov_reads_up_to_three_numbers) {
  Machine m;
  m.call("stov", {"'1 2 3'"});
  CHECK(m.f(0) == 1.0f && m.f(1) == 2.0f && m.f(2) == 3.0f);
  m.call("stov", {"4\t-5"});
  CHECK(m.f(0) == 4.0f && m.f(1) == -5.0f && m.f(2) == 0.0f);
  m.call("stov", {"1 x 3"});
  CHECK(m.f(0) == 1.0f && m.f(1) == 0.0f && m.f(2) == 0.0f);
  m.call("stov", {"1 2", " 7"});  // the strings joined
  CHECK(m.f(0) == 1.0f && m.f(1) == 2.0f && m.f(2) == 7.0f);
  m.call("stov", {""});
  CHECK(m.f(0) == 0.0f && m.f(1) == 0.0f && m.f(2) == 0.0f);
}

TEST(strstrofs_str2chr_and_chr2str) {
  Machine m;
  m.call("strstrofs", {"abcabc", "bc"});
  CHECK(m.f() == 1.0f);
  m.call("strstrofs", {"abcabc", "bc", 2});
  CHECK(m.f() == 4.0f);
  m.call("strstrofs", {"abcabc", "x"});
  CHECK(m.f() == -1.0f);
  m.call("strstrofs", {"abc", "a", -1});
  CHECK(m.f() == -1.0f);
  m.call("strstrofs", {"abc", "", 3});
  CHECK(m.f() == 3.0f);
  m.call("str2chr", {"AB"});
  CHECK(m.f() == 65.0f);
  m.call("str2chr", {"AB", -1});
  CHECK(m.f() == 66.0f);
  m.call("str2chr", {"AB", 2});
  CHECK(m.f() == 0.0f);
  m.call("str2chr", {"\xc8"});
  CHECK(m.f() == 200.0f);  // unsigned
  m.call("chr2str", {72, 105});
  CHECK(m.s() == "Hi");
  m.call("chr2str", {65, 0, 66});
  CHECK(m.s() == "A");
  m.call("chr2str", {300});
  CHECK(m.s() == "?");
}

TEST(strconv_changes_case_and_colour) {
  Machine m;
  m.call("strconv", {1, 0, 0, "AbC 12"});
  CHECK(m.s() == "abc 12");
  m.call("strconv", {2, 0, 0, "abc"});
  CHECK(m.s() == "ABC");
  m.call("strconv", {0, 2, 2, "a1"});
  CHECK(m.s() == std::string("\xe1\xb1"));  // a coloured, 1 coloured (176 + 1)
  m.call("strconv", {0, 0, 3, "09"});
  CHECK(m.s() == std::string("\x12\x1b"));  // the gold digits
  m.call("strconv", {0, 1, 1, "\xe1\x12"});
  CHECK(m.s() == "a0");
  m.call("strconv", {0, 5, 0, "aaa"});
  CHECK(m.s() == std::string("\xe1"
                             "a"
                             "\xe1"));
  m.call("strconv", {0, 2, 0, "a b"});
  CHECK(m.s() == std::string("\xe1\xa0\xe2"));  // the space coloured too
}

TEST(strpad_pads_left_or_right) {
  Machine m;
  m.call("strpad", {5, "ab"});
  CHECK(m.s() == "ab   ");
  m.call("strpad", {-5, "ab"});
  CHECK(m.s() == "   ab");
  m.call("strpad", {2, "abcd"});
  CHECK(m.s() == "abcd");
}

TEST(the_info_string_add_and_get) {
  Machine m;
  m.call("infoadd", {"\\name\\bob\\team\\red", "name", "al"});
  CHECK(m.s() == "\\team\\red\\name\\al");  // the key moved to the end
  m.call("infoget", {"\\team\\red\\name\\al", "team"});
  CHECK(m.s() == "red");
  m.call("infoget", {"\\team\\red", "skin"});
  CHECK(m.s().empty());
  m.call("infoadd", {"\\a\\1\\b\\2", "a", ""});
  CHECK(m.s() == "\\b\\2");  // an empty value deletes
  m.call("infoadd", {"\\a\\1", "b", "x\"y"});
  CHECK(m.s() == "\\a\\1");  // a quote refused
}

TEST(the_comparisons) {
  Machine m;
  m.call("strncmp", {"abc", "abd"});
  CHECK(m.f() == -1.0f);
  m.call("strncmp", {"abc", "abd", 2});
  CHECK(m.f() == 0.0f);
  m.call("strncmp", {"xabc", "abz", 2, 1});
  CHECK(m.f() == 0.0f);
  m.call("strncmp", {"b", "a", 0});
  CHECK(m.f() == 0.0f);
  m.call("strncmp", {"\xe0", "a"});
  CHECK(m.f() == 1.0f);  // bytes unsigned
  m.call("strcasecmp", {"Hello", "hELLO"});
  CHECK(m.f() == 0.0f);
  m.call("strncasecmp", {"ABx", "aby", 2});
  CHECK(m.f() == 0.0f);
  m.call("strcasecmp", {"a", "B"});
  CHECK(m.f() == -1.0f);
}

TEST(sprintf_formats_its_arguments) {
  Machine m;
  m.call("sprintf", {"%d frags, %s", 12.9f, "well done"});
  CHECK(m.s() == "12 frags, well done");
  m.call("sprintf", {"[%5.1f|%-4d|%03d]", 2.25f, 7, 5});
  CHECK(m.s() == "[  2.2|7   |005]");
  m.call("sprintf", {"%2$s %1$s", "world", "hello"});
  CHECK(m.s() == "hello world");
  m.call("sprintf", {"%*d|%-*d|", 4, 1, 3, 2});
  CHECK(m.s() == "   1|2  |");
  m.call("sprintf", {"%x %X %c%c", 255, 171, 72, 105});
  CHECK(m.s() == "ff AB Hi");
  m.call("sprintf", {"%v", {1, 2.5f, -3}});
  CHECK(m.s() == "1 2.5 -3");
  m.call("sprintf", {"%.2s|%S", "abc", "say hi"});
  CHECK(m.s() == "ab|\"say hi\"");
  m.call("sprintf", {"100%% %d %s.", 1});
  CHECK(m.s() == "100% 1 .");  // past the arguments: 0 or ""
  m.call("sprintf", {"a%qyb", 1});
  CHECK(m.s() == "a");  // an unknown conversion stops the text
  m.call("sprintf", {"%d", -2.9f});
  CHECK(m.s() == "-2");
}
