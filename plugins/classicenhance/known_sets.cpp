// The known sets matched against a pack's files (known_sets.hpp).
//
// ge:summary Matches a pack's files against the known sets: every file by path, size and digest
// ge:keywords known-set match_sets sha128 manifest verify missing changed nearest
// ge:skill classicenhance
// ge:test tests/classicenhance_source_test.cpp
#include "classicenhance/known_sets.hpp"

#include <map>

#include "assets/sha256.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance {

std::string sha128_hex(std::span<const uint8_t> bytes) {
  return assets::sha256_hex(bytes).substr(0, 32);
}

SetMatch match_sets(std::span<const uint8_t> content, std::span<const KnownSet> sets) {
  SetMatch best;
  vfs::Pak pak;
  std::string why;
  if (!vfs::read_pak(content, &pak, &why)) return best;
  // Each file hashed once, however many sets ask for it.
  std::map<const vfs::PakEntry*, std::string> digests;
  auto digest = [&](const vfs::PakEntry& e) -> const std::string& {
    auto it = digests.find(&e);
    if (it == digests.end()) it = digests.emplace(&e, sha128_hex(vfs::pak_file(content, e))).first;
    return it->second;
  };
  for (const KnownSet& set : sets) {
    SetMatch m;
    m.set = &set;
    for (const KnownEntry& k : set.files) {
      if (k.path == nullptr) {
        bool held = false;
        for (const vfs::PakEntry& e : pak.entries) {
          if (e.size == k.size && digest(e) == k.sha128) {
            held = true;
            break;
          }
        }
        if (held) {
          m.held++;
        } else {
          m.missing.push_back("a file of " + std::to_string(k.size) + " bytes");
        }
        continue;
      }
      const vfs::PakEntry* e = vfs::pak_find(pak, k.path);
      if (e == nullptr) {
        m.missing.emplace_back(k.path);
      } else if (e->size != k.size || digest(*e) != k.sha128) {
        m.changed.emplace_back(k.path);
      } else {
        m.held++;
      }
    }
    if (m.whole()) return m;
    if (best.set == nullptr || m.held > best.held) best = std::move(m);
  }
  return best;
}

}  // namespace ge::classicenhance
