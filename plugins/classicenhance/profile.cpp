// The content profiles' lookups (profile.hpp).
//
// ge:summary The content profiles' lookups: by id, pack names, asset names and paths
// ge:keywords profile find_profile pack_name asset_name asset_path first_pack_path assets_of
// ge:keywords read_palette zip_name content-zip pk3
// ge:skill classicenhance
// ge:test tests/classicenhance_profile_test.cpp
#include "classicenhance/profile.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace ge::classicenhance {

const Profile& q1_profile() { return kProfiles[0]; }

const MaterialWords& classic_words() { return kProfiles[0].words; }

const Profile* find_profile(std::string_view id) {
  for (const Profile& p : kProfiles) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const MissionPack* find_pack(const Profile& p, std::string_view folder) {
  for (const MissionPack& m : p.packs) {
    if (m.folder.size() != folder.size()) continue;
    bool same = true;
    for (size_t i = 0; i < folder.size() && same; i++)
      same = std::tolower(static_cast<unsigned char>(folder[i])) == m.folder[i];
    if (same) return &m;
  }
  return nullptr;
}

const MissionPack* known_pack(uint64_t size, std::string_view sha256, const Profile** game) {
  for (const Profile& p : kProfiles) {
    for (const MissionPack& m : p.packs) {
      if (m.size != size || m.sha256 != sha256) continue;
      if (game != nullptr) *game = &p;
      return &m;
    }
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

std::string zip_name(const Profile& p, int n) {
  return std::string(p.pack_stem) + std::to_string(n) + std::string(p.zip_ext);
}

std::string first_pack_path(const Profile& p) {
  return std::string(p.base_dir) + "/" + pack_name(p, 0);
}

namespace {
// Whether `a` and `b` are the same letters, ignoring ASCII case.
bool same_letters(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    const char x = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] + 32) : a[i];
    const char y = b[i] >= 'A' && b[i] <= 'Z' ? char(b[i] + 32) : b[i];
    if (x != y) return false;
  }
  return true;
}
}  // namespace

std::string_view asset_name(const AssetKind& kind, std::string_view path) {
  if (path.size() <= kind.dir.size() + kind.ext.size() ||
      !same_letters(path.substr(0, kind.dir.size()), kind.dir) ||
      !same_letters(path.substr(path.size() - kind.ext.size()), kind.ext))
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

std::span<const uint8_t> palette_of(const Profile& p, std::span<const uint8_t> file) {
  const std::string_view name = p.palette;
  const bool pcx = name.size() >= 4 && name.substr(name.size() - 4) == ".pcx";
  if (!pcx) return file.size() >= 768 ? file.first(768) : std::span<const uint8_t>{};
  if (file.size() < 128 + 769 || file[0] != 10 || file[file.size() - 769] != 0x0C) return {};
  return file.last(768);
}

bool read_palette(const Profile& p, const vfs::Pak& pak, std::span<const uint8_t> bytes,
                  Palette* out, std::string* error) {
  const vfs::PakEntry* pal = vfs::pak_find(pak, p.palette);
  const std::span<const uint8_t> rgb =
      pal == nullptr ? std::span<const uint8_t>{} : palette_of(p, vfs::pak_file(bytes, *pal));
  if (rgb.size() < out->rgb.size()) {
    *error = "the pack has no palette in " + std::string(p.palette) + "; it is not " +
             std::string(p.what) + "'s pack (profile " + std::string(p.id) + ")";
    return false;
  }
  std::memcpy(out->rgb.data(), rgb.data(), out->rgb.size());
  return true;
}

}  // namespace ge::classicenhance
