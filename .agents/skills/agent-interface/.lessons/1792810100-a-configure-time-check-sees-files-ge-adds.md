---
title: A CMake EXISTS check in a game repo sees a file ge just wrote
date: 2026-10-22
scope: scaffold/CMakeLists.txt.in, engine/cli/build.cpp
concepts: [scaffold, cmake, configure, baseline, labs-digests]
---

Every `ge build`, `ge test` and `ge check` runs `cmake --preset` before it builds, so a scaffold
`if(EXISTS ...)` is evaluated again on every command. That makes an opt-in file a switch with no
cache to clear: `ge digest --write` writes `tests/labs_digests.txt`, and the next `ge test` adds
`--check-digests` to the `labs_smoke` case. Prefer that to a flag the agent has to remember, and
do not rely on it for a raw `cmake --build`, which does not reconfigure for a new file.
