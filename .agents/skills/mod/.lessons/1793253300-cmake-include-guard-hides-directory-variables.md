---
title: A function in an include-guarded CMake file computes its paths itself
date: 2026-10-29
scope: cmake/Mods.cmake
concepts: [cmake, include-guard, ge-add-mod, directory-scope, variables]
---

`cmake/Mods.cmake` is included from the root, from `games/shooters_edge/` and from `tests/`, and
it carries `include_guard(GLOBAL)`. Variables it set at file scope (the engine's directory, the
ABI read from `abi.hpp`, the platform) existed only in the directory that included it first, so
`ge_add_mod` called from `tests/` built with `-I/engine`. A function shared across directories
computes what it needs inside its body, from `CMAKE_CURRENT_FUNCTION_LIST_DIR`, never from
variables the guarded file set when it was first read.
