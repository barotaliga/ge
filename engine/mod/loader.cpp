// The loader's steps (loader.hpp states the order).
//
// ge:summary The mod loader's steps: read, check, hash, open, resolve, check the table
// ge:keywords mod loader load_mod check_table refuse abi game hash entry
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#include "mod/loader.hpp"

#include <cstring>
#include <fstream>
#include <sstream>

#include "mod/sha256.hpp"

namespace ge::mod {

namespace {

std::string abi_text(uint32_t major, uint32_t minor) {
  return std::to_string(major) + "." + std::to_string(minor);
}

std::string engine_abi() { return abi_text(GE_MOD_ABI_MAJOR, GE_MOD_ABI_MINOR); }

std::string range_text(uint32_t lo, uint32_t hi) {
  return lo == hi ? std::to_string(lo) : std::to_string(lo) + ".." + std::to_string(hi);
}

}  // namespace

std::vector<std::string> check_table(const ge_mod_v1* t, const Manifest& m) {
  std::vector<std::string> e;
  if (!t) {
    e.push_back("ge_mod_entry returned no table: the mod refuses this engine's ABI " +
                engine_abi() + "; next: rebuild the mod against this engine (ge mod build)");
    return e;
  }
  if (t->size < offsetof(ge_mod_v1, hooks) + sizeof(t->hooks)) {
    e.push_back("the mod's table is " + std::to_string(t->size) + " bytes, shorter than ABI " +
                engine_abi() + "'s first version; next: rebuild it against engine/mod/abi.hpp");
    return e;
  }
  const uint32_t major = GE_MOD_ABI_MAJOR_OF(t->abi), minor = GE_MOD_ABI_MINOR_OF(t->abi);
  if (major != GE_MOD_ABI_MAJOR) {
    e.push_back("the library was built against mod ABI " + abi_text(major, minor) +
                ", and this engine speaks ABI " + engine_abi() +
                "; next: rebuild the mod with this engine's ge mod build");
    return e;
  }
  if (!t->name || m.name != t->name)
    e.push_back(std::string("the library names itself `") + (t->name ? t->name : "(null)") +
                "`, the manifest `" + m.name + "`; next: rebuild the package (ge mod build)");
  if (!t->game || m.game != t->game)
    e.push_back(std::string("the library is for game `") + (t->game ? t->game : "(null)") +
                "`, the manifest says `" + m.game + "`; next: rebuild the package");
  if (t->game_api < m.game_api_min || t->game_api > m.game_api_max)
    e.push_back("the library was built against game_api " + std::to_string(t->game_api) +
                ", outside the manifest's " + range_text(m.game_api_min, m.game_api_max) +
                "; next: rebuild the package");
  if (t->state_bytes > GE_MOD_STATE_MAX)
    e.push_back("the mod asks for a state block of " + std::to_string(t->state_bytes) +
                " bytes, over the " + std::to_string(GE_MOD_STATE_MAX) +
                " a game keeps; next: shrink the mod's state struct");
  if (!t->hooks)
    e.push_back(
        "the mod's table has no hooks; next: point `hooks` at the game's hook table "
        "(docs/mods.md § The ABI)");
  return e;
}

LoadResult load_mod(const std::filesystem::path& path, const GameOffer& offer, LoadedMod& out) {
  namespace fs = std::filesystem;
  LoadResult r;
  out.library.close();
  out.table = nullptr;
  std::error_code ec;
  const bool is_dir = fs::is_directory(path, ec);
  const fs::path manifest_path = is_dir ? path / kManifestName : path;
  out.dir = manifest_path.parent_path();
  std::ifstream f(manifest_path, std::ios::binary);
  if (!f) {
    r.errors.push_back(manifest_path.generic_string() +
                       ": no manifest there; next: point the game at a package directory with a "
                       "mod.manifest (ge mod build writes one)");
    return r;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  const ManifestCheck mc = parse_manifest(ss.str(), manifest_path.generic_string(), out.manifest);
  r.warnings = mc.warnings;
  if (!mc.ok()) {
    r.errors = mc.errors;
    return r;
  }
  const Manifest& m = out.manifest;

  if (m.abi_major != GE_MOD_ABI_MAJOR || m.abi_minor > GE_MOD_ABI_MINOR) {
    r.errors.push_back(m.name + " " + m.version + " needs mod ABI " +
                       abi_text(m.abi_major, m.abi_minor) + ", and this engine speaks ABI " +
                       engine_abi() + "; next: rebuild the mod against this engine (ge mod build)");
    return r;
  }
  if (!offer.game.empty()) {
    if (m.game != offer.game) {
      r.errors.push_back(m.name + " is a mod for `" + m.game + "`, not for `" +
                         std::string(offer.game) + "`; next: load it in its own game");
      return r;
    }
    if (offer.api < m.game_api_min || offer.api > m.game_api_max) {
      r.errors.push_back(m.name + " runs on " + m.game + "'s game_api " +
                         range_text(m.game_api_min, m.game_api_max) + ", and this game offers " +
                         std::to_string(offer.api) +
                         "; next: rebuild the mod against this game's headers (ge mod build)");
      return r;
    }
  }

  const std::string_view platform = this_platform();
  const ManifestFile* file = file_for(m, platform);
  if (!file) {
    r.errors.push_back(m.name + " lists no library for " + std::string(platform) +
                       "; next: build it on this platform (ge mod build) or get a package that "
                       "has one");
    return r;
  }
  out.platform = file->platform;
  const fs::path lib = out.dir / fs::path(file->path);
  const std::optional<std::string> hash = sha256_file(lib);
  if (!hash) {
    r.errors.push_back(lib.generic_string() + " (listed for " + file->platform +
                       ") cannot be read; next: rebuild the package (ge mod build)");
    return r;
  }
  if (*hash != file->sha256) {
    r.errors.push_back(lib.generic_string() + " has SHA-256 " + *hash + ", the manifest lists " +
                       file->sha256 +
                       "; next: the library is not the one packaged: rebuild (ge mod build)");
    return r;
  }
  std::string why;
  if (!open_library(lib, out.library, why)) {
    r.errors.push_back(lib.generic_string() + " does not open: " + why +
                       "; next: build it for this platform (ge mod build)");
    return r;
  }
  void* sym = out.library.symbol(GE_MOD_ENTRY_NAME);
  if (!sym) {
    out.library.close();
    r.errors.push_back(lib.generic_string() +
                       " exports no ge_mod_entry; next: define it with GE_MOD_EXPORT "
                       "(engine/mod/abi.hpp)");
    return r;
  }
  const auto entry = reinterpret_cast<ge_mod_entry_fn>(sym);
  const ge_mod_v1* table = entry(GE_MOD_ABI);
  std::vector<std::string> te = check_table(table, m);
  if (!te.empty()) {
    out.library.close();
    for (std::string& s : te) r.errors.push_back(m.name + ": " + s);
    return r;
  }
  out.table = table;
  return r;
}

}  // namespace ge::mod
