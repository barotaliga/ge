---
title: Put native hooks in front of a WebAssembly mod, so the game's seam never knows the form
date: 2026-11-15
scope: games/shooters_edge/sim/mod_host.cpp, engine/mod/wasm/mod.hpp, engine/mod/wasm/bind.hpp, cmake/Mods.cmake
concepts: [wasm, mod-host, trampoline, bind-native, state-block, snapshot, abi]
---

A game's mod host already calls a native `ge_mod_v1`, so a module is hosted by building one: a
`ge_mod_v1` and hook table in the host whose hooks are trampolines that find the module through
`host->ctx` and call its slot. Nothing else in the game changes, and the digest agrees with the
native build's on every frame when the state block has the same layout (only fixed-width ints
and floats in it). Keep the run's copy of the block the one a snapshot holds: copy it into the
module before each call and out after, or a seek restores the run but not the module. Bind
services from the same native table the native form gets (`bind_native` deduces each argument's
conversion from the function pointer's type; `float out[3]` decays to `float*`, so it copies
back). `abi.hpp` includes `<stddef.h>` in C++ as well: `<cstddef>` is not there with
`--target=wasm32 -nostdlib`. CMake must probe that clang has the wasm32 target (Apple's has
none) before adding the build, and every output of that build in a lab is measured, so a
machine without the toolchain keeps the digest.
