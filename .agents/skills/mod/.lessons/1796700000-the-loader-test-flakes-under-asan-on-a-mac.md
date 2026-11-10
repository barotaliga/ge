---
title: mod_loader_test flakes under ASan on macOS (a global-buffer-overflow in check_table)
date: 2026-11-10
scope: tests/mod_loader_test.cpp, engine/mod/loader.cpp
concepts: [asan, macos, dlopen, flaky-test, global-buffer-overflow, precheck]
---

On the Mac's debug preset (Apple Clang + ASan) `mod_loader_test` fails about one run in five:
AddressSanitizer reports a global-buffer-overflow at `check_table` (loader.cpp, reading the
table's `size`) under `the_sample_mod_loads_for_its_game`, then passes on the next run with
nothing rebuilt. The sample mod's table and entry are right (`rocket_bounce.cpp`: `kMod`, sized by
`sizeof(ge_mod_v1)`), and the sample mod is built without the sanitizer, so the report is ASan's
shadow for an uninstrumented library, not a read past the table. A precheck red on this test
alone, on a change that does not reach `engine/mod`, is this: run `build/debug/tests/
mod_loader_test` a few times to confirm, say so in the PR, and do not change the test. Not seen
on Linux (GCC + ASan) or in CI's macOS row (release, no sanitizers). Open: why the shadow is
poisoned there (a library mapped where an earlier one's globals were?).
