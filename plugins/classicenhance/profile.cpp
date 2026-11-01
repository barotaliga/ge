// The content profiles' lookups (profile.hpp).
//
// ge:summary The content profiles' lookups: by id, pack names, asset names and paths
// ge:keywords profile find_profile pack_name asset_name asset_path first_pack_path assets_of
// ge:keywords read_palette
// ge:skill classicenhance
// ge:test tests/classicenhance_profile_test.cpp
#include "classicenhance/profile.hpp"

#include <algorithm>
#include <cstring>

namespace ge::classicenhance {

const Profile& classic_profile() { return kProfiles[0]; }

const Profile* find_profile(std::string_view id) {
  for (const Profile& p : kProfiles) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

std::string profile_ids() {
  std::string out;
  for (const Profile& p : kProfiles) out += (out.empty() ? "" : ", ") + std::string(p.id);
  return out;
}

std::string pack_name(const Profile& p, int n) {
  return std::string(p.pack_stem) + std::to_string(n) + std::string(p.pack_ext);
}

std::string first_pack_path(const Profile& p) {
  return std::string(p.base_dir) + "/" + pack_name(p, 0);
}

std::string_view asset_name(const AssetKind& kind, std::string_view path) {
  if (path.size() <= kind.dir.size() + kind.ext.size() || !path.starts_with(kind.dir) ||
      !path.ends_with(kind.ext))
    return {};
  return path.substr(kind.dir.size(), path.size() - kind.dir.size() - kind.ext.size());
}

std::string asset_path(const AssetKind& kind, std::string_view name) {
  return std::string(kind.dir) + std::string(name) + std::string(kind.ext);
}

std::vector<const vfs::PakEntry*> assets_of(const vfs::Pak& pak, const AssetKind& kind) {
  std::vector<const vfs::PakEntry*> out;
  for (const vfs::PakEntry& e : pak.entries) {
    if (!asset_name(kind, e.path).empty()) out.push_back(&e);
  }
  std::sort(out.begin(), out.end(), [](auto* a, auto* b) { return a->path < b->path; });
  return out;
}

bool read_palette(const Profile& p, const vfs::Pak& pak, std::span<const uint8_t> bytes,
                  Palette* out, std::string* error) {
  const vfs::PakEntry* pal = vfs::pak_find(pak, p.palette);
  if (pal == nullptr || pal->size < out->rgb.size()) {
    *error = "the pack has no " + std::string(p.palette) + " of 768 bytes; it is not " +
             std::string(p.what) + "'s pack (profile " + std::string(p.id) + ")";
    return false;
  }
  std::memcpy(out->rgb.data(), vfs::pak_file(bytes, *pal).data(), out->rgb.size());
  return true;
}

}  // namespace ge::classicenhance
