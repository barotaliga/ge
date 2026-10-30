// The extension registry (extensions.hpp).
//
// ge:summary The extension registry's definitions: register, find, install
// ge:keywords qc progs extension registry register_extension find_extension install
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#include "progs/extensions.hpp"

namespace ge::progs {

bool register_extension(ExtensionRegistry& reg, Extension ext, std::string& error) {
  for (const Extension& e : reg.extensions) {
    if (e.name == ext.name) {
      error = "extension '" + ext.name + "' is registered twice; register each extension once";
      return false;
    }
    for (const ExtensionBuiltin& a : e.builtins) {
      for (const ExtensionBuiltin& b : ext.builtins) {
        if (a.number == b.number) {
          error = "builtin " + std::to_string(b.number) + " of extension '" + ext.name +
                  "' is already extension '" + e.name + "''s " + a.name + "; give it a free number";
          return false;
        }
      }
    }
  }
  reg.extensions.push_back(std::move(ext));
  return true;
}

const Extension* find_extension(const ExtensionRegistry& reg, const char* name) {
  for (const Extension& e : reg.extensions) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

void install(const ExtensionRegistry& reg, Vm& vm) {
  for (const Extension& e : reg.extensions) {
    for (const ExtensionBuiltin& b : e.builtins) set_builtin(vm, b.number, b.fn, b.name);
  }
}

}  // namespace ge::progs
