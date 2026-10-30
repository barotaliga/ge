// THE EXTENSION REGISTRY: what a game offers a program beyond the numbered builtins, grouped by
// name. An extension is a named group of builtins (number and name), fields and hooks; a game
// registers each one it provides, installs its builtins into a VM with `install`, and answers a
// program's question "is extension X offered?" with `offers`. The registry holds no behaviour:
// each builtin is the game's function (vm.hpp `Builtin`), and which extensions exist and what
// they do are the game's (the classic multiplayer catalogue is M18p's).
//
// Registration is a call in the game's own setup, in one order; nothing registers itself.
//
// ge:summary The extension registry: named groups of builtins, fields and hooks a game offers
// ge:keywords qc progs extension extensions registry builtin builtins offers install
// ge:keywords extension-query check-extension register_extension
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#pragma once

#include <string>
#include <vector>

#include "progs/vm.hpp"

namespace ge::progs {

struct ExtensionBuiltin {
  int number;
  const char* name;
  Builtin fn;
};

struct Extension {
  std::string name;
  std::vector<ExtensionBuiltin> builtins;
  std::vector<std::string> fields;  // fields a program may declare to use it
  std::vector<std::string> hooks;   // functions the game calls by name when present
};

struct ExtensionRegistry {
  std::vector<Extension> extensions;  // in registration order
};

// Adds `ext`. False (and nothing added) when its name is taken or one of its builtin numbers is
// another extension's: `error` says which.
bool register_extension(ExtensionRegistry& reg, Extension ext, std::string& error);
// The extension called `name` (case-sensitive), or nullptr.
const Extension* find_extension(const ExtensionRegistry& reg, const char* name);
inline bool offers(const ExtensionRegistry& reg, const char* name) {
  return find_extension(reg, name) != nullptr;
}
// Every registered extension's builtins put into the VM's table.
void install(const ExtensionRegistry& reg, Vm& vm);

}  // namespace ge::progs
