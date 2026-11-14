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

#include <cstdint>
#include <iterator>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server {

namespace rules::campaign {

// void(entity targ, entity inflictor, entity attacker, float damage) damage
void damage_call(A& q) {
  damage(q, pg::parm_int(q.vm, 0), pg::parm_int(q.vm, 1), pg::parm_int(q.vm, 2),
         pg::parm_float(q.vm, 3));
}

// void(entity inflictor, entity attacker, float damage, entity ignore, string how) radius_damage
void radius_damage_call(A& q) {
  radius_damage(q, pg::parm_int(q.vm, 0), pg::parm_int(q.vm, 1), pg::parm_float(q.vm, 2),
                pg::parm_int(q.vm, 3), pg::parm_int(q.vm, 4));
}

// void(entity who) teleport_arrived
void teleport_arrived_call(A& q) { teleport_arrived(q, pg::parm_int(q.vm, 0)); }

}  // namespace rules::campaign

namespace {

namespace cp = rules::campaign;

// A rule as the VM runs it: the rules' view made over the VM, the rule run.
template <void (*Rule)(cp::A&)>
void run(pg::Vm& vm) {
  cp::A q{vm};
  Rule(q);
}

constexpr uint8_t kDamageParms[] = {1, 1, 1, 1};
constexpr uint8_t kRadiusParms[] = {1, 1, 1, 1, 1};
constexpr uint8_t kOneParm[] = {1};

#define SE_X(name, type) {#name, pg::type},
constexpr pg::NativeDef kGlobals[] = {SE_COMMON_GLOBALS(SE_X) SE_CAMPAIGN_GLOBALS(SE_X)};
constexpr pg::NativeDef kFields[] = {SE_COMMON_FIELDS(SE_X) SE_CAMPAIGN_FIELDS(SE_X)};
#undef SE_X
#define SE_X(name, text) text,
// Every entry is its own literal (the list is long and alike, which the comma check misreads).
// NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
constexpr const char* kStrings[] = {SE_RULES_STRINGS(SE_X)};
#undef SE_X
#define SE_X(name, qc_name) {qc_name, &run<&cp::name>},
const pg::NativeFunction kFunctions[] = {
    SE_COMMON_FUNCTIONS(SE_X)
        SE_CAMPAIGN_FUNCTIONS(SE_X){"damage", &run<&cp::damage_call>, kDamageParms},
    {"radius_damage", &run<&cp::radius_damage_call>, kRadiusParms},
    {"teleport_arrived", &run<&cp::teleport_arrived_call>, kOneParm}};
#undef SE_X

static_assert(std::size(kGlobals) == cp::kGlobalCount && std::size(kFields) == cp::kFieldCount &&
              std::size(kStrings) == cp::kStringCount &&
              std::size(kFunctions) == cp::kFunctionCount);

}  // namespace

const pg::NativeProgram& native_campaign() {
  static const pg::NativeProgram p = [] {
    pg::NativeProgram n;
    n.form = &pg::kSinglePlayerLayout;
    n.file = "campaign";
    n.globals = kGlobals;
    n.fields = kFields;
    n.strings = kStrings;
    n.functions = kFunctions;
    return n;
  }();
  return p;
}

}  // namespace shooters_edge::server
