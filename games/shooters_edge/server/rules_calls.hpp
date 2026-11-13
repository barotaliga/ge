// THE CLASSIC SERVER'S CALLS FOR C++ RULES: what a native program (progs/native.hpp) on the mod
// server (qc_server.hpp) reads and calls, typed so a rule carried over from bytecode reads like
// the function it replaces. `Q q{vm}` at the top of a native function, then:
//
// - the FORM's globals (`q.self()`, `q.time()`, `q.v_forward()`, ...) and fields by the form's
//   offsets (`q.f(e, q.L.f.health)`, `q.set_v(e, q.L.f.origin, v)`), read afresh each time as the
//   bytecode reads them (a call in between may change `self` or `other`);
// - the program's OWN globals, fields, strings and functions by their index in its declaration
//   (`q.own_global(k)`, `q.own_field(k)`, `q.str(k)`, `q.fn(k)`);
// - the server's CALLS by name, each the catalogue's builtin at its number (catalogue.hpp), so a
//   rule reaches exactly the code bytecode reaches: the same traces, random stream and bytes.
//
// Arithmetic stays in 32-bit floats in the bytecode's order (no FMA: the engine's flags), so a
// function carried over statement for statement computes the same bits. After a fault every
// call and field access is a no-op and the function should return: `q.failed()`.
//
// ge:summary The classic server's globals, fields and builtins typed for rules written in C++
// ge:keywords native rules-calls c++-rules builtins makevectors setorigin spawn remove find
// ge:keywords findradius bprint sprint sound writebyte multicast vector-math native-program
// ge:skill progs shooter
// ge:test tests/shooters_edge_native_rules_test.cpp
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "progs/native.hpp"
#include "progs/vm.hpp"

namespace shooters_edge::server {

namespace pg = ge::progs;
using pg::Vec3;

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float f) { return {a.x * f, a.y * f, a.z * f}; }

struct Q {
  pg::Vm& vm;
  const pg::Layout& L;
  const pg::NativeOffsets& o;
  explicit Q(pg::Vm& v) : vm(v), L(pg::vm_layout(v)), o(pg::native_offsets(v)) {}

  bool failed() const { return vm.fault.set; }

  // Globals of the form.
  int32_t gi(int g) const { return pg::global_int(vm, g); }
  float gf(int g) const { return pg::global_float(vm, g); }
  Vec3 gv(int g) const { return pg::global_vec(vm, g); }
  void set_gi(int g, int32_t v) { pg::set_global_int(vm, g, v); }
  void set_gf(int g, float v) { pg::set_global_float(vm, g, v); }
  void set_gv(int g, Vec3 v) { pg::set_global_vec(vm, g, v); }
  int32_t self() const { return gi(L.g.self); }
  void set_self(int32_t e) { set_gi(L.g.self, e); }
  int32_t other() const { return gi(L.g.other); }
  int32_t world() const { return gi(L.g.world); }
  float time() const { return gf(L.g.time); }
  int32_t mapname() const { return gi(L.g.mapname); }
  Vec3 v_forward() const { return gv(L.g.v_forward); }
  void set_newmis(int32_t e) { set_gi(L.g.newmis, e); }
  int parm(int i) const { return L.g.parm1 + i - 1; }  // parm1..parm16's global

  // The program's own, by declaration index.
  int own_global(int k) const { return o.globals[static_cast<size_t>(k)]; }
  int own_field(int k) const { return o.fields[static_cast<size_t>(k)]; }
  int32_t str(int k) const { return o.strings[static_cast<size_t>(k)]; }
  int32_t fn(int k) const { return o.functions[static_cast<size_t>(k)]; }

  // Fields.
  float f(int32_t e, int field) { return pg::field_float(vm, e, field); }
  int32_t i(int32_t e, int field) { return pg::field_int(vm, e, field); }
  Vec3 v(int32_t e, int field) { return pg::field_vec(vm, e, field); }
  void set_f(int32_t e, int field, float x) { pg::set_field_float(vm, e, field, x); }
  void set_i(int32_t e, int field, int32_t x) { pg::set_field_int(vm, e, field, x); }
  void set_v(int32_t e, int field, Vec3 x) { pg::set_field_vec(vm, e, field, x); }

  // Two strings equal by their text, as EQ_S compares them.
  bool same(int32_t a, int32_t b) const {
    const char* x = pg::vm_string(vm, a);
    const char* y = pg::vm_string(vm, b);
    return std::string_view(x != nullptr ? x : "") == std::string_view(y != nullptr ? y : "");
  }

  // A function of the program (its number) run by the engine's call.
  bool call(int32_t function) { return !failed() && pg::call_function(vm, function); }

  // THE CALLS, by the catalogue's numbers.
  void makevectors(Vec3 a) { run(1, a); }
  void setorigin(int32_t e, Vec3 at) { run(2, e, at); }
  void setmodel(int32_t e, int32_t model) { run(3, e, model); }
  void setsize(int32_t e, Vec3 mins, Vec3 maxs) { run(4, e, mins, maxs); }
  void sound(int32_t e, float channel, int32_t sample, float volume, float attenuation) {
    run(8, e, channel, sample, volume, attenuation);
  }
  float vlen(Vec3 v) { return run(12, v) ? pg::result_float(vm) : 0.0f; }
  int32_t spawn() { return run(14) ? pg::result_int(vm) : 0; }
  void remove(int32_t e) { run(15, e); }
  int32_t find(int32_t from, int field, int32_t match) {
    return run(18, from, static_cast<int32_t>(field), match) ? pg::result_int(vm) : 0;
  }
  void precache_sound(int32_t s) { run(19, s); }
  void precache_model(int32_t s) { run(20, s); }
  int32_t findradius(Vec3 at, float radius) { return run(22, at, radius) ? pg::result_int(vm) : 0; }
  void bprint(float level, std::initializer_list<int32_t> text) { print(23, nullptr, level, text); }
  void sprint(int32_t e, float level, std::initializer_list<int32_t> text) {
    print(24, &e, level, text);
  }
  int32_t ftos(float v) { return run(26, v) ? pg::result_int(vm) : 0; }
  void lightstyle(float style, int32_t value) { run(35, style, value); }
  float cvar(int32_t name) { return run(45, name) ? pg::result_float(vm) : 0.0f; }
  Vec3 vectoangles(Vec3 v) { return run(51, v) ? pg::result_vec(vm) : Vec3{}; }
  void WriteByte(float to, float v) { run(52, to, v); }
  void WriteCoord(float to, float v) { run(56, to, v); }
  void changelevel(int32_t map) { run(70, map); }
  void logfrag(int32_t killer, int32_t killed) { run(79, killer, killed); }
  void multicast(Vec3 at, float to) { run(82, at, to); }
  float checkextension(int32_t name) { return run(99, name) ? pg::result_float(vm) : 0.0f; }
  void calltimeofday() { run(231); }

 private:
  void put(int k, float x) { pg::set_parm_float(vm, k, x); }
  void put(int k, int32_t x) { pg::set_parm_int(vm, k, x); }
  void put(int k, Vec3 x) { pg::set_parm_vec(vm, k, x); }
  template <typename... A>
  bool run(int32_t number, A... args) {
    if (failed()) return false;
    int k = 0;
    (put(k++, args), ...);
    return pg::call_builtin(vm, number, k);
  }
  void print(int32_t number, const int32_t* to, float level, std::initializer_list<int32_t> text) {
    if (failed()) return;
    int k = 0;
    if (to != nullptr) put(k++, *to);
    put(k++, level);
    for (int32_t s : text) put(k++, s);
    pg::call_builtin(vm, number, k);
  }
};

}  // namespace shooters_edge::server
