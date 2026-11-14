// THE ARENA AS A PACKAGE: the same table the game builds in (arena_program.cpp), exported as a
// rules package (sim/rules_api.hpp) by `ge_add_mod` (games/shooters_edge/CMakeLists.txt), so the
// server can load the arena as it loads any other program of rules.
//
// ge:summary The arena's rules exported as a rules package
// ge:keywords rules-package arena-package ge_mod_entry package
// ge:skill mod progs shooter
// ge:test tests/shooters_edge_rules_package_test.cpp
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/sim/rules_api.hpp"

namespace {

const ge_mod_v1* table() {
  static const ge_mod_v1 mod = {sizeof(ge_mod_v1),
                                GE_MOD_ABI,
                                "arena",
                                "1.0.0",
                                SE_RULES_GAME,
                                SE_RULES_API,
                                0,
                                nullptr,
                                &shooters_edge::server::arena_rules()};
  return &mod;
}

}  // namespace

GE_MOD_EXPORT const ge_mod_v1* ge_mod_entry(uint32_t host_abi) {
  return GE_MOD_ABI_MAJOR_OF(host_abi) == GE_MOD_ABI_MAJOR ? table() : nullptr;
}
