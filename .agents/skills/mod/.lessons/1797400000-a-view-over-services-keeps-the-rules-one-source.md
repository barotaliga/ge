---
title: Put the rules' one view (Q) over the ABI's services, and the built-in copy and the package are one source
date: 2026-11-14
scope: games/shooters_edge/rules/calls.hpp, games/shooters_edge/sim/rules_api.hpp, games/shooters_edge/server/rules_package.cpp
concepts: [rules-package, native-program, services, side-by-side, abi]
---

The arena and the campaign reached the VM only through `Q` (calls.hpp), so moving them into a
package meant rewriting `Q` over `se_rules_call_v1`'s services and nothing else: the 8,000 lines of
rules changed only where they read an argument or set a result (`q.arg_i`, `q.ret_f`). Because the
game's built-in copy runs through the same table and dispatch, every existing side-by-side pin
(arena, campaign, the C++ test mod) proved the services bit-exact before any package test ran.
The package then only has to match the built-in copy. Two traps on the way: a member named `c` in
`Q` is shadowed by every `A(const se_rules_call_v1& c)` constructor and by `words(..., c, d)`
under `-Wshadow` (name it `cx`), and `cmake --build` of a package target does not rebuild the
tests that load it, so build the test target too.
