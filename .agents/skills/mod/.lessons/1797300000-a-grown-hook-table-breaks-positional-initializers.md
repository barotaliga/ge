---
title: Growing a hook table breaks every table written positionally (-Wmissing-field-initializers)
date: 2026-11-14
scope: games/shooters_edge/sim/mod_api.hpp, games/shooters_edge/mods/, tests/shooters_edge_mod_test.cpp, tests/mod_abi_two_fixture.cpp
concepts: [abi-growth, hook-table, missing-field-initializers, werror, sample-mod, sequel-mods]
---

Adding `damage` and `death` at the end of `se_mod_hooks_v1` keeps old binaries loading (the
`size` rule), but every table in the tree written as a braced list (`rocket_bounce.cpp`,
`mod_abi_two_fixture.cpp`, `counting_hooks()` in `shooters_edge_mod_test.cpp`) stops compiling
under `-Werror=missing-field-initializers`. Grep `se_mod_hooks_v1` before you grow it and add the
new entries as `nullptr, // name` to each list. The template (`templates/mod-shooters_edge`)
sets members by name and needs nothing: prefer that shape for a new sample. A modder's own
positional table gets the same warning on rebuild; the old binary is unaffected.
