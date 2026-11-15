// MODS LINKED INTO THE APP AHEAD OF TIME (docs/mods.md § The store form): a store build that
// ships a mod inside the app compiles the mod's sources natively into the app, and registers its
// entry here under the SHA-256 of the wasm32 module built from the same sources. When the loader
// takes a package's `file.wasm32` row and the module's hash is registered, it calls the linked
// entry instead of interpreting the module: the same mod, checked the same way (the package on
// disk, its manifest, its signature, its hash, its table), at native speed. A package whose
// module is not the one linked (another build, another version) runs in the interpreter as
// before, so a linked mod can never stand in for code it was not built from.
//
// `ge_link_mods(<app> <mod targets...>)` (cmake/Mods.cmake) does the registering: it compiles each
// mod's sources with its entry renamed and generates the call to `link_mod` with the module's
// hash.
//
// ge:summary Mods linked into the app ahead of time, found by their wasm32 module's SHA-256
// ge:keywords mod linked-mod link_mod find_linked ahead-of-time aot store-build wasm32 sha256
// ge:keywords ge_link_mods registry entry
// ge:skill mod
// ge:test tests/mod_linked_test.cpp
#pragma once

#include <string_view>

#include "mod/abi.hpp"

namespace ge::mod {

struct LinkedMod {
  const char* name;    // the mod's name (the manifest's), for messages
  const char* sha256;  // the wasm32 module built from the same sources, lower-case hex
  ge_mod_entry_fn entry;
};

// Registers a linked mod (the generated code calls it from a static initializer). Returns true
// so it can initialize a constant. A second registration of the same hash is ignored.
bool link_mod(const LinkedMod& mod);

// The linked mod whose module has this SHA-256, or null.
const LinkedMod* find_linked(std::string_view sha256);

}  // namespace ge::mod
