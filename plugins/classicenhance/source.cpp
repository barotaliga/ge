// The source search and the identity check (source.hpp).
//
// ge:summary Opens and verifies the classic pack from a zip, a pack file or a folder
// ge:keywords source open verify zip lha folder search pak0 sha256 refuse
// ge:skill classicenhance
// ge:test tests/classicenhance_source_test.cpp
#include "classicenhance/source.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "assets/sha256.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/lha.hpp"
#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

bool read_file(const fs::path& p, std::vector<uint8_t>* out, std::string* error) {
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    *error = "cannot open '" + p.string() + "' for reading";
    return false;
  }
  out->assign(std::istreambuf_iterator<char>(f), {});
  return true;
}

bool is_zip(std::span<const uint8_t> b) {
  return b.size() >= 4 && b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4;
}
bool is_pack(std::span<const uint8_t> b) {
  return b.size() >= 12 && b[0] == 'P' && b[1] == 'A' && b[2] == 'C' && b[3] == 'K';
}

std::string lower(std::string s) {
  for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  return s;
}

// The entry of `dir` named `name` in any case, or an empty path.
fs::path child_any_case(const fs::path& dir, const std::string& name) {
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(dir, ec)) {
    if (lower(e.path().filename().string()) == name) return e.path();
  }
  return {};
}

const KnownFile* known_in(std::span<const KnownFile> list, const std::string& sha) {
  for (const KnownFile& k : list) {
    if (sha == k.sha256) return &k;
  }
  return nullptr;
}

// What to give instead, naming every profile's first pack ("id1/pak0.pak").
std::string what_to_give() {
  std::string packs;
  for (const Profile& p : kProfiles) packs += (packs.empty() ? "" : " or ") + first_pack_path(p);
  return "give a release zip as published, an installed game folder, or its " + packs +
         " (skill: classicenhance)";
}

// Whether the archive path `name` (any case, '/' or '\\') is a profile's first pack in an install;
// sets `*profile` to it.
bool first_pack_in(std::string_view name, const Profile** profile) {
  for (const Profile& p : kProfiles) {
    const std::string at = first_pack_path(p);
    if (vfs::same_name(name, at) ||
        (name.size() > at.size() && vfs::same_name(name.substr(name.size() - at.size()), at) &&
         (name[name.size() - at.size() - 1] == '/' || name[name.size() - at.size() - 1] == '\\'))) {
      *profile = &p;
      return true;
    }
  }
  return false;
}

}  // namespace

const char* route_name(Route r) {
  switch (r) {
    case Route::release_zip:
      return "release-zip";
    case Route::pack_file:
      return "pack-file";
    case Route::folder:
      return "folder";
    case Route::installer:
      return "installer-archive";
  }
  return "?";
}

bool add_later_packs(const std::string& dir, Source* out, std::string* error,
                     std::span<const KnownFile> packs) {
  out->later.clear();
  out->merged.clear();
  out->set_sha256 = out->pack_sha256;
  for (int n = 1;; n++) {
    const fs::path at = child_any_case(dir, lower(pack_name(*out->profile, n)));
    if (at.empty()) break;
    LaterPack p;
    p.name = at.filename().string();
    p.path = at.string();
    if (!read_file(at, &p.bytes, error)) return false;
    vfs::Pak check;
    if (!is_pack(p.bytes) || !vfs::read_pak(p.bytes, &check, error)) {
      *error = "'" + p.path + "' does not read as a pack (" +
               (is_pack(p.bytes) ? *error : std::string("no PACK magic")) +
               "); move it out of the folder or fetch it again from the game's install";
      return false;
    }
    p.sha256 = assets::sha256_hex(p.bytes);
    p.known = known_in(packs, p.sha256);
    out->later.push_back(std::move(p));
  }
  if (out->later.empty()) return true;
  // Every path once, at its first place, holding its latest copy.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::map<std::string, size_t> at;
  auto take = [&](std::span<const uint8_t> bytes) {
    vfs::Pak pak;
    std::string why;
    if (!vfs::read_pak(bytes, &pak, &why)) return;
    for (const vfs::PakEntry& e : pak.entries) {
      const std::span<const uint8_t> b = vfs::pak_file(bytes, e);
      const auto [it, fresh] = at.emplace(e.path, files.size());
      if (fresh) {
        files.emplace_back(e.path, std::vector<uint8_t>(b.begin(), b.end()));
      } else {
        files[it->second].second.assign(b.begin(), b.end());
      }
    }
  };
  take(out->pack);
  std::vector<std::string> digests = {out->pack_sha256};
  for (const LaterPack& p : out->later) {
    take(p.bytes);
    digests.push_back(p.sha256);
  }
  if (!vfs::write_pak(files, &out->merged, error)) return false;
  out->set_sha256 = set_key(digests);
  return true;
}

bool verify_pack(Source* out, std::string* error, std::span<const KnownFile> packs) {
  out->pack_sha256 = assets::sha256_hex(out->pack);
  out->set_sha256 = out->pack_sha256;
  out->known = known_in(packs, out->pack_sha256);
  // Any pack that reads is taken: a known release is reported as one, any other (the full game,
  // another release) as the player's own copy. A pack that does not read is refused.
  vfs::Pak pak;
  std::string why;
  if (!vfs::read_pak(out->pack, &pak, &why)) {
    *error = "the pack (" + std::to_string(out->pack.size()) + " bytes, SHA-256 " +
             out->pack_sha256 + ") does not read as a pack (" + why +
             "), so nothing was written. " + what_to_give();
    out->pack_sha256.clear();
    out->set_sha256.clear();
    return false;
  }
  // A pack found by its install's or its archive's path keeps that profile; a pack alone is the
  // first row whose palette it holds.
  if (vfs::pak_find(pak, out->profile->palette) == nullptr) {
    for (const Profile& p : kProfiles) {
      if (vfs::pak_find(pak, p.palette) != nullptr) {
        out->profile = &p;
        break;
      }
    }
  }
  return true;
}

bool pack_from_zip(std::span<const uint8_t> bytes, Source* out, std::string* error,
                   std::span<const KnownFile> packs) {
  out->zip_sha256 = assets::sha256_hex(bytes);
  out->zip = known_in(kKnownZips, out->zip_sha256);
  vfs::Zip zip;
  if (!vfs::read_zip(bytes, &zip, error)) return false;
  // The pack stored in the zip itself, as some repackaged downloads carry it.
  for (const vfs::ZipEntry& e : zip.entries) {
    if (first_pack_in(e.name, &out->profile)) {
      if (!vfs::zip_unpack(bytes, e, &out->pack, error)) return false;
      return verify_pack(out, error, packs);
    }
  }
  const vfs::ZipEntry* res = vfs::zip_find(zip, "resource.1");
  if (res == nullptr) {
    *error = "the zip holds neither an installer archive (resource.1) nor a game's first pack; " +
             what_to_give();
    return false;
  }
  std::vector<uint8_t> archive;
  if (!vfs::zip_unpack(bytes, *res, &archive, error)) return false;
  std::vector<LhaEntry> entries;
  if (!read_lha(archive, &entries, error)) return false;
  for (const LhaEntry& e : entries) {
    if (first_pack_in(e.name, &out->profile)) {
      if (!lha_unpack(archive, e, &out->pack, error)) return false;
      return verify_pack(out, error, packs);
    }
  }
  *error = "the installer archive holds no game's first pack; " + what_to_give();
  return false;
}

bool open_source(const std::string& path, Source* out, std::string* error,
                 std::span<const KnownFile> packs) {
  *out = Source{};
  std::error_code ec;
  fs::path file = path;
  if (fs::is_directory(file, ec)) {
    out->route = Route::folder;
    fs::path found;
    // An install: a profile's base folder holding its first pack.
    for (const Profile& p : kProfiles) {
      if (const fs::path base = child_any_case(file, lower(std::string(p.base_dir)));
          !base.empty()) {
        found = child_any_case(base, lower(pack_name(p, 0)));
      }
      if (!found.empty()) {
        out->profile = &p;
        break;
      }
    }
    // The first pack itself, of any profile.
    for (size_t i = 0; found.empty() && i < std::size(kProfiles); i++) {
      found = child_any_case(file, lower(pack_name(kProfiles[i], 0)));
    }
    if (found.empty()) {
      // A release zip dropped in the folder: the first zip that holds a pack.
      for (const auto& e : fs::directory_iterator(file, ec)) {
        if (lower(e.path().extension().string()) != ".zip") continue;
        Source s;
        std::string why;
        if (open_source(e.path().string(), &s, &why, packs)) {
          s.route = Route::folder;
          *out = std::move(s);
          return true;
        }
      }
      *error = "no game's first pack, nor a release zip holding one, in '" + path + "'; " +
               what_to_give();
      return false;
    }
    file = found;
  } else if (!fs::is_regular_file(file, ec)) {
    *error = "'" + path + "' is neither a file nor a folder; " + what_to_give();
    return false;
  }
  std::vector<uint8_t> bytes;
  if (!read_file(file, &bytes, error)) return false;
  const Route via = out->route;
  out->path = file.string();
  if (is_zip(bytes)) {
    out->route = Route::release_zip;
    if (!pack_from_zip(bytes, out, error, packs)) return false;
  } else if (is_pack(bytes)) {
    out->route = via == Route::folder ? Route::folder : Route::pack_file;
    out->pack = std::move(bytes);
    if (!verify_pack(out, error, packs)) return false;
    // The full game's later packs sit beside the first, in an installed folder or not.
    if (!add_later_packs(file.parent_path().string(), out, error, packs)) return false;
  } else {
    // The installer's archive taken out of the zip by hand (`resource.1`): read as the zip's.
    std::vector<LhaEntry> entries;
    std::string why;
    if (!read_lha(bytes, &entries, &why)) {
      *error = "'" + file.string() + "' is neither a zip, an installer archive nor a pack; " +
               what_to_give();
      return false;
    }
    out->route = Route::installer;
    for (const LhaEntry& e : entries) {
      if (first_pack_in(e.name, &out->profile)) {
        if (!lha_unpack(bytes, e, &out->pack, error)) return false;
        return verify_pack(out, error, packs);
      }
    }
    *error = "the installer archive holds no game's first pack; " + what_to_give();
    return false;
  }
  return true;
}

}  // namespace ge::classicenhance
