// The registry of mods linked into the app (linked.hpp).
//
// ge:summary The linked mods' registry: a fixed table filled by static initializers
// ge:keywords mod linked-mod link_mod find_linked registry
// ge:skill mod
// ge:test tests/mod_linked_test.cpp
#include "mod/linked.hpp"

#include <cstddef>

namespace ge::mod {

namespace {

constexpr size_t kMaxLinked = 64;  // far more mods than an app ships

struct Registry {
  LinkedMod mods[kMaxLinked];
  size_t count = 0;
};

// A function-local static: the registry exists before any other file's initializer calls in.
Registry& registry() {
  static Registry r;
  return r;
}

}  // namespace

bool link_mod(const LinkedMod& mod) {
  Registry& r = registry();
  if (find_linked(mod.sha256) || r.count == kMaxLinked) return true;
  r.mods[r.count++] = mod;
  return true;
}

const LinkedMod* find_linked(std::string_view sha256) {
  const Registry& r = registry();
  for (size_t i = 0; i < r.count; ++i)
    if (sha256 == r.mods[i].sha256) return &r.mods[i];
  return nullptr;
}

}  // namespace ge::mod
