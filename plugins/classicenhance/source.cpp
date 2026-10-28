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

#include "assets/sha256.hpp"
#include "classicenhance/lha.hpp"
#include "vfs/zip.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

constexpr const char* kPackName = "pak0.pak";
constexpr const char* kPackInArchive = "id1/pak0.pak";

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

constexpr const char* kWhatToGive =
    "give the shareware release zip as published, an installed game folder, or its "
    "id1/pak0.pak, unchanged (skill: classicenhance)";

}  // namespace

const char* route_name(Route r) {
  switch (r) {
    case Route::release_zip: return "release-zip";
    case Route::pack_file: return "pack-file";
    case Route::folder: return "folder";
    case Route::installer: return "installer-archive";
  }
  return "?";
}

bool verify_pack(Source* out, std::string* error, std::span<const KnownFile> packs) {
  out->pack_sha256 = assets::sha256_hex(out->pack);
  out->known = known_in(packs, out->pack_sha256);
  if (out->known == nullptr) {
    *error = "the pack (" + std::to_string(out->pack.size()) + " bytes, SHA-256 " +
             out->pack_sha256 +
             ") is not a known unchanged release; only an unchanged shareware pack is read, so "
             "nothing was written. " + kWhatToGive;
    return false;
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
    const std::string name = lower(e.name);
    if (name == kPackInArchive || name.ends_with("/" + std::string(kPackInArchive)) ||
        name.ends_with("\\id1\\pak0.pak")) {
      if (!vfs::zip_unpack(bytes, e, &out->pack, error)) return false;
      return verify_pack(out, error, packs);
    }
  }
  const vfs::ZipEntry* res = vfs::zip_find(zip, "resource.1");
  if (res == nullptr) {
    *error = "the zip holds neither an installer archive (resource.1) nor id1/pak0.pak; " +
             std::string(kWhatToGive);
    return false;
  }
  std::vector<uint8_t> archive;
  if (!vfs::zip_unpack(bytes, *res, &archive, error)) return false;
  std::vector<LhaEntry> entries;
  if (!read_lha(archive, &entries, error)) return false;
  for (const LhaEntry& e : entries) {
    if (vfs::same_name(e.name, kPackInArchive)) {
      if (!lha_unpack(archive, e, &out->pack, error)) return false;
      return verify_pack(out, error, packs);
    }
  }
  *error = "the installer archive holds no id1\\pak0.pak; " + std::string(kWhatToGive);
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
    if (const fs::path id1 = child_any_case(file, "id1"); !id1.empty()) {
      found = child_any_case(id1, kPackName);
    }
    if (found.empty()) found = child_any_case(file, kPackName);
    if (found.empty()) {
      // A release zip dropped in the folder: the first zip that holds a known pack.
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
      *error = "no id1/pak0.pak, pak0.pak or release zip holding it in '" + path + "'; " +
               kWhatToGive;
      return false;
    }
    file = found;
  } else if (!fs::is_regular_file(file, ec)) {
    *error = "'" + path + "' is neither a file nor a folder; " + kWhatToGive;
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
  } else {
    // The installer's archive taken out of the zip by hand (`resource.1`): read as the zip's.
    std::vector<LhaEntry> entries;
    std::string why;
    if (!read_lha(bytes, &entries, &why)) {
      *error = "'" + file.string() + "' is neither a zip, an installer archive nor a pack; " +
               kWhatToGive;
      return false;
    }
    out->route = Route::installer;
    for (const LhaEntry& e : entries) {
      if (vfs::same_name(e.name, kPackInArchive)) {
        if (!lha_unpack(bytes, e, &out->pack, error)) return false;
        return verify_pack(out, error, packs);
      }
    }
    *error = "the installer archive holds no id1\\pak0.pak; " + std::string(kWhatToGive);
    return false;
  }
  return true;
}

}  // namespace ge::classicenhance
