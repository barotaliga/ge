---
title: Templates pass the game's format because ge add formats them, and a test checks named placeholders
date: 2026-10-22
scope: templates/, engine/cli/add.cpp, tests/cli_add_test.cpp
concepts: [templates, ge-add, clang-format, placeholders]
---

`templates/**` are `.in` files like the scaffold, so nothing in this tree formats them, and a
placeholder like `{{Type}}State` reflows differently once filled. `run_add` runs `clang-format -i`
on what it wrote when clang-format is on the PATH, and the sources were formatted once by
filling them, formatting, and restoring the placeholders. A test that looks for any `{{` fails
on legitimate brace-initialisers: check each named placeholder (`{{snake}}`, `{{Type}}`, ...).
The real proof is packaging's `scaffold` job, which grows a repo with every template and runs
`ge check`; reproduce it locally before pushing a template change.
