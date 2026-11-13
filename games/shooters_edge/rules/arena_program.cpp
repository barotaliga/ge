// The arena's declaration put together: the X-lists of arena_decl.hpp as the native program's
// globals, fields, strings and functions, each function a C++ rule run by the VM.
//
// ge:summary The arena's native program declaration assembled from its lists
// ge:keywords arena native-arena native_arena declaration native-program
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include "shooters_edge/rules/arena.hpp"

#include <iterator>

#include "shooters_edge/rules/arena_decl.hpp"

namespace shooters_edge::server {

namespace {

namespace ar = arena;

// A rule as the VM runs it: the rules' view made over the VM, the rule run.
template <void (*Rule)(ar::A&)>
void run(pg::Vm& vm) {
  ar::A q{vm};
  Rule(q);
}

#define SE_X(name, type) {#name, pg::type},
constexpr pg::NativeDef kGlobals[] = {SE_ARENA_GLOBALS(SE_X)};
constexpr pg::NativeDef kFields[] = {SE_ARENA_FIELDS(SE_X)};
#undef SE_X
#define SE_X(name, text) text,
constexpr const char* kStrings[] = {SE_ARENA_STRINGS(SE_X)};
#undef SE_X
#define SE_X(name, qc_name) {qc_name, &run<&ar::name>},
constexpr pg::NativeFunction kFunctions[] = {SE_ARENA_FUNCTIONS(SE_X)};
#undef SE_X

static_assert(std::size(kGlobals) == ar::kGlobalCount && std::size(kFields) == ar::kFieldCount &&
              std::size(kStrings) == ar::kStringCount &&
              std::size(kFunctions) == ar::kFunctionCount);

}  // namespace

const pg::NativeProgram& native_arena() {
  static const pg::NativeProgram p = [] {
    pg::NativeProgram n;
    n.file = "arena";
    n.globals = kGlobals;
    n.fields = kFields;
    n.strings = kStrings;
    n.functions = kFunctions;
    return n;
  }();
  return p;
}

}  // namespace shooters_edge::server
