// The campaign's declaration put together: the X-lists of common_decl.hpp and campaign_decl.hpp
// as the native program's globals, fields, strings and functions on the single-player form, each
// function a C++ rule run by the VM; and the rules called by name with arguments, reading them as
// the bytecode passes them.
//
// ge:summary The campaign's native program declaration assembled from its lists
// ge:keywords campaign native-campaign native_campaign declaration native-program
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include "shooters_edge/rules/campaign.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "shooters_edge/rules/campaign_decl.hpp"
#include "shooters_edge/rules/campaign_q2.hpp"
#include "shooters_edge/rules/q2_arm_strings.hpp"

namespace shooters_edge::server {

namespace rules::campaign {

// void(entity targ, entity inflictor, entity attacker, float damage) damage
void damage_call(A& q) { damage(q, q.arg_i(0), q.arg_i(1), q.arg_i(2), q.arg_f(3)); }

// void(entity inflictor, entity attacker, float damage, entity ignore, string how) radius_damage
void radius_damage_call(A& q) {
  radius_damage(q, q.arg_i(0), q.arg_i(1), q.arg_f(2), q.arg_i(3), q.arg_i(4));
}

// void(entity who) teleport_arrived
void teleport_arrived_call(A& q) { teleport_arrived(q, q.arg_i(0)); }

}  // namespace rules::campaign

namespace {

namespace cp = rules::campaign;

// A rule as the VM runs it: the rules' view made over the VM, the rule run.
template <void (*Rule)(cp::A&)>
void run(const se_rules_call_v1* call) {
  cp::A q{*call};
  Rule(q);
}

constexpr uint8_t kDamageParms[] = {1, 1, 1, 1};
constexpr uint8_t kRadiusParms[] = {1, 1, 1, 1, 1};
constexpr uint8_t kOneParm[] = {1};

#define SE_X(name, type) {#name, static_cast<uint32_t>(pg::type)},
constexpr se_rules_def_v1 kGlobals[] = {SE_COMMON_GLOBALS(SE_X) SE_CAMPAIGN_GLOBALS(SE_X)};
constexpr se_rules_def_v1 kFields[] = {SE_COMMON_FIELDS(SE_X) SE_CAMPAIGN_FIELDS(SE_X)};
#undef SE_X
#define SE_X(name, text) text,
// Every entry is its own literal (the list is long and alike, which the comma check misreads).
// NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
constexpr const char* kOwnStrings[] = {SE_RULES_STRINGS(SE_X)};
#undef SE_X
// The rules' strings, then Quake II's monsters' (campaign_q2.hpp), then Quake II's weapons'
// (q2_arm_strings.hpp).
constexpr auto kStrings = [] {
  std::array<const char*, cp::kStringCount + cp::kSqStringCount + rules::kQ2ArmStringCount> a{};
  size_t n = 0;
  for (const char* s : kOwnStrings) a[n++] = s;
  for (int k = 0; k < cp::kSqRunCount; k++)
    for (int w = 0; w < cp::kSqWords; w++) a[n++] = cp::sq_word(k, w);
  for (const char* s : cp::kSqShared) a[n++] = s;
  for (const char* s : rules::kQ2ArmStrings) a[n++] = s;
  return a;
}();
#define SE_X(name, qc_name) {qc_name, &run<&cp::name>, 0, nullptr},
const se_rules_function_v1 kFunctions[] = {
    SE_COMMON_FUNCTIONS(SE_X)
        SE_CAMPAIGN_FUNCTIONS(SE_X){"damage", &run<&cp::damage_call>, 4, kDamageParms},
    {"radius_damage", &run<&cp::radius_damage_call>, 5, kRadiusParms},
    {"teleport_arrived", &run<&cp::teleport_arrived_call>, 1, kOneParm}};
#undef SE_X

static_assert(std::size(kGlobals) == cp::kGlobalCount && std::size(kFields) == cp::kFieldCount &&
              std::size(kOwnStrings) == cp::kStringCount &&
              std::size(kFunctions) == cp::kFunctionCount);

}  // namespace

const se_rules_v1& campaign_rules() {
  static const se_rules_v1 p = {sizeof(se_rules_v1),
                                SE_RULES_API,
                                SE_RULES_FORM_SINGLE_PLAYER,
                                "campaign",
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
