// The arena's declaration put together: the X-lists of common_decl.hpp and arena_decl.hpp as the
// native program's globals, fields, strings and functions, each function a C++ rule run by the VM.
//
// ge:summary The arena's native program declaration assembled from its lists
// ge:keywords arena native-arena native_arena declaration native-program
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include "shooters_edge/rules/arena.hpp"

#include <array>
#include <cstddef>
#include <iterator>

#include "shooters_edge/rules/arena_decl.hpp"
#include "shooters_edge/rules/q2_arm_strings.hpp"

namespace shooters_edge::server {

namespace {

namespace ar = rules::arena;

// A rule as the VM runs it: the rules' view made over the VM, the rule run.
template <void (*Rule)(ar::A&)>
void run(const se_rules_call_v1* call) {
  ar::A q{*call};
  Rule(q);
}

#define SE_X(name, type) {#name, static_cast<uint32_t>(pg::type)},
constexpr se_rules_def_v1 kGlobals[] = {SE_COMMON_GLOBALS(SE_X) SE_ARENA_GLOBALS(SE_X)};
constexpr se_rules_def_v1 kFields[] = {SE_COMMON_FIELDS(SE_X) SE_ARENA_FIELDS(SE_X)};
#undef SE_X
#define SE_X(name, text) text,
// Every entry is its own literal (the list is long and alike, which the comma check misreads).
// NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
constexpr const char* kOwnStrings[] = {SE_RULES_STRINGS(SE_X)};
#undef SE_X
// The rules' strings, then Quake II's weapons' (q2_arm_strings.hpp).
constexpr auto kStrings = [] {
  std::array<const char*, ar::kStringCount + rules::kQ2ArmStringCount> a{};
  size_t n = 0;
  for (const char* s : kOwnStrings) a[n++] = s;
  for (const char* s : rules::kQ2ArmStrings) a[n++] = s;
  return a;
}();
#define SE_X(name, qc_name) {qc_name, &run<&ar::name>, 0, nullptr},
constexpr se_rules_function_v1 kFunctions[] = {SE_COMMON_FUNCTIONS(SE_X) SE_ARENA_FUNCTIONS(SE_X)};
#undef SE_X

static_assert(std::size(kGlobals) == ar::kGlobalCount && std::size(kFields) == ar::kFieldCount &&
              std::size(kOwnStrings) == ar::kStringCount &&
              std::size(kFunctions) == ar::kFunctionCount);

}  // namespace

const se_rules_v1& arena_rules() {
  static const se_rules_v1 p = {sizeof(se_rules_v1),
                                SE_RULES_API,
                                SE_RULES_FORM_MULTIPLAYER,
                                "arena",
                                kGlobals,
                                static_cast<uint32_t>(std::size(kGlobals)),
                                kFields,
                                static_cast<uint32_t>(std::size(kFields)),
                                kStrings.data(),
                                static_cast<uint32_t>(std::size(kStrings)),
                                kFunctions,
                                static_cast<uint32_t>(std::size(kFunctions))};
  return p;
}

}  // namespace shooters_edge::server
