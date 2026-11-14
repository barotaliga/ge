// THE ARENA'S DECLARATION: the deathmatch's own globals and functions after the shared ones
// (common_decl.hpp), the enums the rules index by, the arena's view of the server's calls (`A`),
// and the glue only the arena has. Internal to games/shooters_edge/rules.
//
// ge:summary The arena's declaration: its own lists, enums, the rules' view and glue (internal)
// ge:keywords arena native-arena declaration x-list globals functions
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#pragma once

#include <cstdint>

#include "shooters_edge/rules/common_decl.hpp"

namespace shooters_edge::server::rules::arena {

// THE ARENA'S OWN GLOBALS (the multiplayer form has no `deathmatch` or `teamplay`: the program
// declares them, and the server and the bots find them by name).
#define SE_ARENA_GLOBALS(X) \
  X(deathmatch, kTypeFloat) \
  X(teamplay, kTypeFloat)   \
  X(clock_said, kTypeFloat) \
  X(team_a, kTypeString)    \
  X(team_b, kTypeString)    \
  X(frags_a, kTypeFloat)    \
  X(frags_b, kTypeFloat)

// THE ARENA'S OWN FUNCTIONS by name (the C++ `main_hook` is the program's `main`).
#define SE_ARENA_FUNCTIONS(X)                         \
  X(main_hook, "main")                                \
  X(StartFrame, "StartFrame")                         \
  X(SetNewParms, "SetNewParms")                       \
  X(SetChangeParms, "SetChangeParms")                 \
  X(worldspawn, "worldspawn")                         \
  X(info_player_deathmatch, "info_player_deathmatch") \
  X(info_player_start, "info_player_start")           \
  X(info_intermission, "info_intermission")

#define SE_X(name, ...) g_##name,
enum Gl : uint8_t { SE_COMMON_GLOBALS(SE_X) SE_ARENA_GLOBALS(SE_X) kGlobalCount };
#undef SE_X
#define SE_X(name, ...) f_##name,
enum Fd : uint8_t { SE_COMMON_FIELDS(SE_X) kFieldCount };
#undef SE_X
#define SE_X(name, ...) s_##name,
enum St : uint16_t { SE_RULES_STRINGS(SE_X) kStringCount };
#undef SE_X
#define SE_X(name, ...) fn_##name,
enum Fn : uint8_t { SE_COMMON_FUNCTIONS(SE_X) SE_ARENA_FUNCTIONS(SE_X) kFunctionCount };
#undef SE_X

// Where an effect's bytes go: the multicast buffer, sent by `multicast`.
constexpr float kMsgEffect = 4;

// THE RULES' VIEW of the server's calls: Q's, plus the arena's own globals, fields, strings and
// functions by their enums.
struct A : Q {
  explicit A(pg::Vm& v) : Q(v) {}
  using Q::f;
  using Q::i;
  using Q::set_f;
  using Q::set_i;
  using Q::set_v;
  using Q::v;
  float f(int32_t e, Fd k) { return Q::f(e, own_field(k)); }
  int32_t i(int32_t e, Fd k) { return Q::i(e, own_field(k)); }
  Vec3 v(int32_t e, Fd k) { return Q::v(e, own_field(k)); }
  void set_f(int32_t e, Fd k, float x) { Q::set_f(e, own_field(k), x); }
  void set_i(int32_t e, Fd k, int32_t x) { Q::set_i(e, own_field(k), x); }
  void set_v(int32_t e, Fd k, Vec3 x) { Q::set_v(e, own_field(k), x); }
  float g(Gl k) const { return gf(own_global(k)); }
  int32_t ge(Gl k) const { return gi(own_global(k)); }
  void set_g(Gl k, float x) { set_gf(own_global(k), x); }
  void set_ge(Gl k, int32_t x) { set_gi(own_global(k), x); }
  int32_t s(St k) const { return str(k); }
  int32_t fn(Fn k) const { return Q::fn(k); }
  // The program's own here; the single-player form's own in the campaign.
  float deathmatch() const { return g(g_deathmatch); }
};

#include "shooters_edge/rules/common_rules.inc"

// THE ARENA'S OWN GLUE.
int32_t game_variant(A& q, int32_t stem, int32_t a, int32_t b, int32_t c, int32_t d);

#define SE_X(name, ...) void name(A& q);
SE_ARENA_FUNCTIONS(SE_X)
#undef SE_X

}  // namespace shooters_edge::server::rules::arena
