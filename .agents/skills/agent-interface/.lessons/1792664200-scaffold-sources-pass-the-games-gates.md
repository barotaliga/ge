---
title: The scaffold's sources are formatted in a scaffolded repo, never in this tree
date: 2026-10-21
scope: scaffold, engine/cli/scaffold.cpp
concepts: [scaffold, clang-format, ge-new]
---

`scaffold/**` files end in `.in` so this tree's `make fmt`, `make lint` and CMake globs never
touch them, which also means nothing here formats them: the first scaffolded repo failed its own
`make fmt-check` on aligned comments and macro backslashes. After editing a scaffold source, run
`make ge ARGS="new <scratch>/snow-racer"`, `make fmt` in that repo, and copy each file back with
`sed -e 's/snow_racer/{{ident}}/g; s/snow-racer/{{name}}/g'`. CMake's `file(GLOB_RECURSE)` does
pick up dotfiles (`.gitignore.in`, `.github/workflows/ci.yml.in`), so they need no special case.
