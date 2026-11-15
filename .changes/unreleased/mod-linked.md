---
type: Added
title: Mods linked into the app ahead of time
---

`ge_link_mods(<app> <mods...>)` (`cmake/Mods.cmake`) compiles a mod's sources into an app and registers its entry under the SHA-256 of the mod's wasm32 module (`engine/mod/linked.hpp`). When a store build loads a package's module and that hash is registered, the loader runs the linked native code instead of the interpreter (`LoadedMod::linked`); every other check on the package still applies, and `GameOffer::linked = false` asks for the interpreter.
