---
type: Added
title: Mods run as WebAssembly modules through the same ABI
---

A mod built for wasm32 now loads through the mod ABI: `engine/mod/wasm/mod.hpp` calls the module's `ge_mod_entry`, reads its tables in wasm32's layout and lays the host table, the game's services and the state block out in the module's memory; `engine/mod/wasm/bind.hpp` binds a native function into the module by its C++ type. Shooter's Edge hosts such a module behind its native seam (`create_wasm_mod_host`), and `ge_add_mod(... WASM)` builds a mod for wasm32 where clang and wasm-ld are found. The sample mod's module plays the same match to the same hashes as its native library. `engine/mod/abi.hpp` now includes `<stddef.h>` in C++ too.
