---
title: A builtin table narrower than the catalogue drops the high numbers without a word
date: 2026-10-29
scope: games/shooters_edge/server/catalogue.cpp, games/shooters_edge/sim/qc_program.cpp, plugins/progs/vm.cpp
concepts: [builtin-table, catalogue, max-builtins, extensions, install]
---

The VM's builtin table was 512 entries wide. The extension rows at 531 (`setpause`) and 532
(`precache_vwep_model`) were installed into it, and the install skipped them without an error.
`checkextension` still answered yes for their extensions, so a mod asked, was told yes, and then
faulted with "builtin 531 is not provided" on its first call. The game now sizes the table to
1024 (`sim::kQcBuiltins`), and `install_catalogue` fails when a row's number is past the table.
When you add a row, check its number against the table's width. A test that calls the builtin
by its number catches a dropped row; a test that only asks `checkextension` does not.
