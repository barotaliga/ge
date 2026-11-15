---
title: Link a mod ahead of time under its module's hash, so the linked code stands in only for that build
date: 2026-11-15
scope: engine/mod/linked.hpp, engine/mod/loader.cpp, cmake/Mods.cmake, tests/mod_linked_test.cpp
concepts: [linked-mod, ahead-of-time, ge-link-mods, wasm32, sha256, store-build, static-initializer]
---

`ge_link_mods` compiles a mod's sources into the app as an OBJECT library with
`-Dge_mod_entry=ge_mod_entry_<name>`, and a cmake -P step writes a registration file holding the
wasm32 module's SHA-256 (read at build time, so it depends on the `<mod>_wasm` target and the
file). Register from a static initializer in that generated .cpp added to the app's own sources,
never from a static library (the linker drops an unreferenced object, and its initializer with
it); the registry is a function-local static so it exists before any initializer calls in. Write
the generated file every time the step runs: writing it only when it changed leaves it older than
its input, and Ninja rebuilds it forever. Build the C++ text with `string(CONCAT ...)`, not a list:
the code's semicolons split a CMake list. A test that only partly skips (no toolchain) prints
`skipped:` instead of calling `skip()`, which makes the whole executable exit 77, a failure where
`SKIP_RETURN_CODE` is not set.
