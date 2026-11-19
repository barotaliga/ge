---
title: A module loaded from a package has no native table, so read its fields, never its table
date: 2026-11-15
scope: engine/mod/loader.cpp, engine/mod/loader.hpp, engine/toolkit/mod.cpp, cmake/Mods.cmake
concepts: [wasm, loader, form, file-wasm32, loaded-mod, table-fields, store-build, ge-mod-build]
---

When the loader takes a package's `file.wasm32` row, `LoadedMod::table` stays null (the table
lives in the module's memory, read per instance) and the bytes go in `LoadedMod::wasm`. Code that
reports on a loaded mod (`ge mod check`'s JSON, the `mod` lab's verdict) must read
`LoadedMod::fields`, which both paths fill; reading `table->` segfaults on a module. Hosts that
run a package take `host_package`, which picks the form. A seam that can only call native code
(Shooter's Edge's rules packages) offers `Form::native_only`, so a module is refused with words
instead of crashing. In `cmake/Mods.cmake`, the wasm32 compile is a custom command that runs in
the build directory: make every source and include directory absolute, or a mod repo with
`SOURCES src/mod.cpp` builds natively and fails for wasm32. Do not include `mod/loader.hpp` from
`sim/mod_host.hpp`: its `ge::mod` names reach `qw_server.cpp` by ADL and clash with
`std::quoted`; forward-declare `LoadedMod` instead.
