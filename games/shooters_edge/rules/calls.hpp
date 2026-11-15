// THE CLASSIC SERVER'S CALLS FOR C++ RULES: what a native program (progs/native.hpp) on the mod
// server (qc_server.hpp) reads and calls, typed so a rule carried over from bytecode reads like
// the function it replaces. `Q q{*call}` at the top of a function (sim/rules_api.hpp), then:
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
// ge:keywords native rules-calls native-rules builtins makevectors setorigin spawn remove find
// ge:keywords findradius bprint sprint sound writebyte multicast vector-math native-program
// ge:keywords movetogoal walkmove checkclient changeyaw particle ambientsound single-player-form
// ge:skill progs shooter
// ge:test tests/shooters_edge_native_rules_test.cpp
#pragma once

#include <bit>
#include <climits>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "progs/layout.hpp"
#include "shooters_edge/sim/rules_api.hpp"

namespace shooters_edge::server {

namespace pg = ge::progs;

struct Vec3 {
  float x = 0, y = 0, z = 0;
};
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float f) { return {a.x * f, a.y * f, a.z * f}; }

// `&` and `|` on floats as the bytecode does them: each side truncated toward zero to a 32-bit
// integer (out of range or NaN: -2147483648), the result back to a float.
inline int32_t word_int(float f) {
  if (!(f > -2147483904.0f && f < 2147483648.0f)) return INT32_MIN;
  return static_cast<int32_t>(f);
}
inline float bit_and(float a, float b) { return static_cast<float>(word_int(a) & word_int(b)); }
inline float bit_or(float a, float b) { return static_cast<float>(word_int(a) | word_int(b)); }

// The form's offsets as the call hands them: plugins/progs/layout.hpp's structs, whose members
// are the ints of its lists in order.
static_assert(sizeof(pg::LayoutGlobals) % sizeof(int32_t) == 0 &&
              sizeof(pg::LayoutFields) % sizeof(int32_t) == 0 && sizeof(int) == sizeof(int32_t));
constexpr uint32_t kFormGlobalCount = sizeof(pg::LayoutGlobals) / sizeof(int32_t);
constexpr uint32_t kFormFieldCount = sizeof(pg::LayoutFields) / sizeof(int32_t);

struct Q {
  struct Form {
    const pg::LayoutGlobals& g;
    const pg::LayoutFields& f;
  };
  const se_rules_call_v1& cx;
  const Form L;
  // A call whose form tables are not this build's size is refused by the server before any
  // function runs (rules_package.cpp), so the casts below read the tables the host laid out.
  explicit Q(const se_rules_call_v1& call)
      : cx(call),
        L{*reinterpret_cast<const pg::LayoutGlobals*>(call.form_globals),
          *reinterpret_cast<const pg::LayoutFields*>(call.form_fields)} {}

  bool failed() const { return cx.services->failed(cx.ctx) != 0; }

  // Words.
  uint32_t word(int g) const { return cx.services->global(cx.ctx, g); }
  void set_word(int g, uint32_t w) { cx.services->set_global(cx.ctx, g, w); }

  // Globals of the form.
  int32_t gi(int g) const { return std::bit_cast<int32_t>(word(g)); }
  float gf(int g) const { return std::bit_cast<float>(word(g)); }
  Vec3 gv(int g) const { return {gf(g), gf(g + 1), gf(g + 2)}; }
  void set_gi(int g, int32_t v) { set_word(g, std::bit_cast<uint32_t>(v)); }
  void set_gf(int g, float v) { set_word(g, std::bit_cast<uint32_t>(v)); }
  void set_gv(int g, Vec3 v) {
    set_gf(g, v.x);
    set_gf(g + 1, v.y);
    set_gf(g + 2, v.z);
  }
  int32_t self() const { return gi(L.g.self); }
  void set_self(int32_t e) { set_gi(L.g.self, e); }
  int32_t other() const { return gi(L.g.other); }
  void set_other(int32_t e) { set_gi(L.g.other, e); }
  int32_t world() const { return gi(L.g.world); }
  float time() const { return gf(L.g.time); }
  int32_t mapname() const { return gi(L.g.mapname); }
  Vec3 v_forward() const { return gv(L.g.v_forward); }
  Vec3 v_right() const { return gv(L.g.v_right); }
  Vec3 v_up() const { return gv(L.g.v_up); }
  void set_newmis(int32_t e) { set_gi(L.g.newmis, e); }
  int parm(int i) const { return L.g.parm1 + i - 1; }  // parm1..parm16's global

  // The call's arguments (parameter i, from 0) and its result, and a called builtin's result.
  int32_t arg_i(int i) const { return gi(pg::kGlobalParm0 + 3 * i); }
  float arg_f(int i) const { return gf(pg::kGlobalParm0 + 3 * i); }
  Vec3 arg_v(int i) const { return gv(pg::kGlobalParm0 + 3 * i); }
  void ret_f(float v) { set_gf(pg::kGlobalReturn, v); }
  void ret_i(int32_t v) { set_gi(pg::kGlobalReturn, v); }
  void ret_v(Vec3 v) { set_gv(pg::kGlobalReturn, v); }
  float result_f() const { return gf(pg::kGlobalReturn); }
  int32_t result_i() const { return gi(pg::kGlobalReturn); }
  Vec3 result_v() const { return gv(pg::kGlobalReturn); }

  // The program's own, by declaration index.
  int own_global(int k) const { return cx.globals[k]; }
  int own_field(int k) const { return cx.fields[k]; }
  int32_t str(int k) const { return cx.strings[k]; }
  int32_t fn(int k) const { return cx.functions[k]; }

  // Fields.
  float f(int32_t e, int field) {
    uint32_t w = 0;
    cx.services->field(cx.ctx, e, field, 1, &w);
    return std::bit_cast<float>(w);
  }
  int32_t i(int32_t e, int field) {
    uint32_t w = 0;
    cx.services->field(cx.ctx, e, field, 1, &w);
    return std::bit_cast<int32_t>(w);
  }
  Vec3 v(int32_t e, int field) {
    uint32_t w[3] = {};
    cx.services->field(cx.ctx, e, field, 3, w);
    return {std::bit_cast<float>(w[0]), std::bit_cast<float>(w[1]), std::bit_cast<float>(w[2])};
  }
  void set_f(int32_t e, int field, float x) {
    const uint32_t w = std::bit_cast<uint32_t>(x);
    cx.services->set_field(cx.ctx, e, field, 1, &w);
  }
  void set_i(int32_t e, int field, int32_t x) {
    const uint32_t w = std::bit_cast<uint32_t>(x);
    cx.services->set_field(cx.ctx, e, field, 1, &w);
  }
  void set_v(int32_t e, int field, Vec3 x) {
    const uint32_t w[3] = {std::bit_cast<uint32_t>(x.x), std::bit_cast<uint32_t>(x.y),
                           std::bit_cast<uint32_t>(x.z)};
    cx.services->set_field(cx.ctx, e, field, 3, w);
  }

  // Two strings equal by their text, as EQ_S compares them.
  bool same(int32_t a, int32_t b) const {
    const char* x = cx.services->string(cx.ctx, a);
    const char* y = cx.services->string(cx.ctx, b);
    return std::string_view(x != nullptr ? x : "") == std::string_view(y != nullptr ? y : "");
  }

  // A string value's text ("" for none), and whether it is `text`.
  const char* text(int32_t s) const {
    const char* x = cx.services->string(cx.ctx, s);
    return x != nullptr ? x : "";
  }
  bool is(int32_t s, std::string_view t) const { return std::string_view(text(s)) == t; }

  // A function of the program (its number) run by the engine's call.
  bool call(int32_t function) {
    return !failed() && cx.services->call_function(cx.ctx, function) != 0;
  }

  // THE CALLS, by the catalogue's numbers. A text call takes its strings as a list (the
  // bytecode's variadic arguments).
  void makevectors(Vec3 a) { run(1, a); }
  void setorigin(int32_t e, Vec3 at) { run(2, e, at); }
  void setmodel(int32_t e, int32_t model) { run(3, e, model); }
  void setsize(int32_t e, Vec3 mins, Vec3 maxs) { run(4, e, mins, maxs); }
  float random() { return run(7) ? result_f() : 0.0f; }
  void sound(int32_t e, float channel, int32_t sample, float volume, float attenuation) {
    run(8, e, channel, sample, volume, attenuation);
  }
  Vec3 normalize(Vec3 v) { return run(9, v) ? result_v() : Vec3{}; }
  void error(std::initializer_list<int32_t> text) { print(10, nullptr, nullptr, text); }
  void objerror(std::initializer_list<int32_t> text) { print(11, nullptr, nullptr, text); }
  float vlen(Vec3 v) { return run(12, v) ? result_f() : 0.0f; }
  float vectoyaw(Vec3 v) { return run(13, v) ? result_f() : 0.0f; }
  int32_t spawn() { return run(14) ? result_i() : 0; }
  void remove(int32_t e) { run(15, e); }
  void traceline(Vec3 from, Vec3 to, float nomonsters, int32_t ignore) {
    run(16, from, to, nomonsters, ignore);
  }
  int32_t checkclient() { return run(17) ? result_i() : 0; }
  int32_t find(int32_t from, int field, int32_t match) {
    return run(18, from, static_cast<int32_t>(field), match) ? result_i() : 0;
  }
  void precache_sound(int32_t s) { run(19, s); }
  void precache_model(int32_t s) { run(20, s); }
  void stuffcmd(int32_t e, int32_t s) { run(21, e, s); }
  int32_t findradius(Vec3 at, float radius) { return run(22, at, radius) ? result_i() : 0; }
  void bprint(float level, std::initializer_list<int32_t> text) {
    print(23, nullptr, &level, text);
  }
  void sprint(int32_t e, float level, std::initializer_list<int32_t> text) {
    print(24, &e, &level, text);
  }
  // The single-player form's prints, with no level.
  void bprint(std::initializer_list<int32_t> text) { print(23, nullptr, nullptr, text); }
  void sprint(int32_t e, std::initializer_list<int32_t> text) { print(24, &e, nullptr, text); }
  void dprint(std::initializer_list<int32_t> text) { print(25, nullptr, nullptr, text); }
  int32_t ftos(float v) { return run(26, v) ? result_i() : 0; }
  int32_t vtos(Vec3 v) { return run(27, v) ? result_i() : 0; }
  float walkmove(float yaw, float dist) { return run(32, yaw, dist) ? result_f() : 0.0f; }
  float droptofloor() { return run(34) ? result_f() : 0.0f; }
  void lightstyle(float style, int32_t value) { run(35, style, value); }
  float rint(float v) { return run(36, v) ? result_f() : 0.0f; }
  float floor(float v) { return run(37, v) ? result_f() : 0.0f; }
  float ceil(float v) { return run(38, v) ? result_f() : 0.0f; }
  float checkbottom(int32_t e) { return run(40, e) ? result_f() : 0.0f; }
  float pointcontents(Vec3 at) { return run(41, at) ? result_f() : 0.0f; }
  float fabs(float v) { return run(43, v) ? result_f() : 0.0f; }
  Vec3 aim(int32_t e, float speed) { return run(44, e, speed) ? result_v() : Vec3{}; }
  float cvar(int32_t name) { return run(45, name) ? result_f() : 0.0f; }
  void localcmd(int32_t s) { run(46, s); }
  int32_t nextent(int32_t e) { return run(47, e) ? result_i() : 0; }
  void particle(Vec3 at, Vec3 dir, float colour, float count) { run(48, at, dir, colour, count); }
  void changeyaw() { run(49); }
  Vec3 vectoangles(Vec3 v) { return run(51, v) ? result_v() : Vec3{}; }
  void WriteByte(float to, float v) { run(52, to, v); }
  void WriteShort(float to, float v) { run(54, to, v); }
  void WriteCoord(float to, float v) { run(56, to, v); }
  void WriteString(float to, int32_t s) { run(58, to, s); }
  void WriteEntity(float to, int32_t e) { run(59, to, e); }
  void movetogoal(float dist) { run(67, dist); }
  void makestatic(int32_t e) { run(69, e); }
  void changelevel(int32_t map) { run(70, map); }
  void cvar_set(int32_t name, int32_t value) { run(72, name, value); }
  void centerprint(int32_t e, std::initializer_list<int32_t> text) { print(73, &e, nullptr, text); }
  void ambientsound(Vec3 at, int32_t sample, float volume, float attenuation) {
    run(74, at, sample, volume, attenuation);
  }
  void setspawnparms(int32_t e) { run(78, e); }
  void logfrag(int32_t killer, int32_t killed) { run(79, killer, killed); }
  int32_t infokey(int32_t e, int32_t key) { return run(80, e, key) ? result_i() : 0; }
  void multicast(Vec3 at, float to) { run(82, at, to); }
  float tokenize(int32_t s) { return run(84, s) ? result_f() : 0.0f; }
  int32_t argv(float n) { return run(86, n) ? result_i() : 0; }
  float checkextension(int32_t name) { return run(99, name) ? result_f() : 0.0f; }
  int32_t cvar_string(int32_t name) { return run(103, name) ? result_i() : 0; }
  int32_t strcat(std::initializer_list<int32_t> text) {
    print(115, nullptr, nullptr, text);
    return failed() ? 0 : result_i();
  }
  int32_t strzone(int32_t s) { return run(118, s) ? result_i() : 0; }
  void calltimeofday() { run(231); }
  // A loaded game's own words (GE_QC_WORDS): the template under `key` filled with four values.
  int32_t words(int32_t key, int32_t a, int32_t b, int32_t c, int32_t d) {
    return run(640, key, a, b, c, d) ? result_i() : 0;
  }

 private:
  void put(int k, float x) { set_gf(pg::kGlobalParm0 + 3 * k, x); }
  void put(int k, int32_t x) { set_gi(pg::kGlobalParm0 + 3 * k, x); }
  void put(int k, Vec3 x) { set_gv(pg::kGlobalParm0 + 3 * k, x); }
  template <typename... A>
  bool run(int32_t number, A... args) {
    if (failed()) return false;
    int k = 0;
    (put(k++, args), ...);
    return cx.services->call_builtin(cx.ctx, number, k) != 0;
  }
  // A call whose arguments end in a list of strings: an entity and a level before them when the
  // call takes them.
  void print(int32_t number, const int32_t* to, const float* level,
             std::initializer_list<int32_t> text) {
    if (failed()) return;
    int k = 0;
    if (to != nullptr) put(k++, *to);
    if (level != nullptr) put(k++, *level);
    for (int32_t s : text) put(k++, s);
    cx.services->call_builtin(cx.ctx, number, k);
  }
};

}  // namespace shooters_edge::server
