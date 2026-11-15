// The loader's steps (loader.hpp states the order).
//
// ge:summary The mod loader's steps: read, check, hash, open, resolve, check the table
// ge:keywords mod loader load_mod check_table refuse abi game hash entry
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#include "mod/loader.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

#include "mod/linked.hpp"
#include "mod/sha256.hpp"
#include "mod/wasm/mod.hpp"

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
  TableFields f;
  f.abi = t->abi;
  f.name = t->name ? t->name : "(null)";
  f.game = t->game ? t->game : "(null)";
  f.game_api = t->game_api;
  f.state_bytes = t->state_bytes;
  f.has_hooks = t->hooks != nullptr;
  return check_fields(f, m);
}

std::vector<std::string> check_fields(const TableFields& t, const Manifest& m) {
  std::vector<std::string> e;
  const uint32_t major = GE_MOD_ABI_MAJOR_OF(t.abi), minor = GE_MOD_ABI_MINOR_OF(t.abi);
  if (major != GE_MOD_ABI_MAJOR) {
    e.push_back("the library was built against mod ABI " + abi_text(major, minor) +
                ", and this engine speaks ABI " + engine_abi() +
                "; next: rebuild the mod with this engine's ge mod build");
    return e;
  }
  if (m.name != t.name)
    e.push_back("the library names itself `" + t.name + "`, the manifest `" + m.name +
                "`; next: rebuild the package (ge mod build)");
  if (m.game != t.game)
    e.push_back("the library is for game `" + t.game + "`, the manifest says `" + m.game +
                "`; next: rebuild the package");
  if (t.game_api < m.game_api_min || t.game_api > m.game_api_max)
    e.push_back("the library was built against game_api " + std::to_string(t.game_api) +
                ", outside the manifest's " + range_text(m.game_api_min, m.game_api_max) +
                "; next: rebuild the package");
  if (t.state_bytes > GE_MOD_STATE_MAX)
    e.push_back("the mod asks for a state block of " + std::to_string(t.state_bytes) +
                " bytes, over the " + std::to_string(GE_MOD_STATE_MAX) +
                " a game keeps; next: shrink the mod's state struct");
  if (!t.has_hooks)
    e.push_back(
        "the mod's table has no hooks; next: point `hooks` at the game's hook table "
        "(docs/mods.md § The ABI)");
  return e;
}

namespace {

// Step 4 for one listed file: there, and its hash the listed one. False with the error.
bool file_checks(const std::filesystem::path& file, const ManifestFile& listed,
                 std::vector<std::string>& errors) {
  const std::optional<std::string> hash = sha256_file(file);
  if (!hash) {
    errors.push_back(file.generic_string() + " (listed for " + listed.platform +
                     ") cannot be read; next: rebuild the package (ge mod build)");
    return false;
  }
  if (*hash != listed.sha256) {
    errors.push_back(file.generic_string() + " has SHA-256 " + *hash + ", the manifest lists " +
                     listed.sha256 +
                     "; next: the file is not the one packaged: rebuild (ge mod build)");
    return false;
  }
  return true;
}

// Step 6 on a native table (a library's, or a linked mod's): checked, then kept. False with the
// errors.
bool take_table(const ge_mod_v1* table, LoadedMod& out, LoadResult& r) {
  std::vector<std::string> te = check_table(table, out.manifest);
  if (!te.empty()) {
    for (std::string& s : te) r.errors.push_back(out.manifest.name + ": " + s);
    return false;
  }
  out.table = table;
  out.fields.abi = table->abi;
  out.fields.name = table->name;
  out.fields.game = table->game;
  out.fields.game_api = table->game_api;
  out.fields.state_bytes = table->state_bytes;
  out.fields.has_hooks = true;
  out.fields.hooks_bytes = *static_cast<const uint32_t*>(table->hooks);
  return true;
}

// Steps 4 to 6 for the wasm32 module: read, hashed, then either the app's linked build of it
// (linked.hpp) or the module instantiated, its entry's table checked.
void load_wasm(const std::filesystem::path& file, const ManifestFile& listed, bool use_linked,
               LoadedMod& out, LoadResult& r) {
  if (!file_checks(file, listed, r.errors)) return;
  if (const LinkedMod* linked = use_linked ? find_linked(listed.sha256) : nullptr) {
    if (!take_table(linked->entry(GE_MOD_ABI), out, r)) return;
    out.platform = listed.platform;
    out.linked = true;
    return;
  }
  std::ifstream f(file, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  wasm::WasmMod probe;
  std::string error;
  if (!wasm::load_wasm_mod(bytes.data(), bytes.size(), GE_MOD_ABI, probe, error)) {
    r.errors.push_back(file.generic_string() + ": " + error);
    return;
  }
  TableFields t;
  t.abi = probe.abi;
  t.name = probe.name;
  t.game = probe.game;
  t.game_api = probe.game_api;
  t.state_bytes = probe.state_bytes;
  t.has_hooks = probe.hooks_size != 0;
  t.hooks_bytes = probe.hooks_size;
  out.fields = t;
  for (std::string& s : check_fields(t, out.manifest))
    r.errors.push_back(out.manifest.name + ": " + s);
  if (!r.errors.empty()) return;
  out.platform = listed.platform;
  out.wasm = std::move(bytes);
}

}  // namespace

LoadResult load_mod(const std::filesystem::path& path, const GameOffer& offer, LoadedMod& out) {
  namespace fs = std::filesystem;
  LoadResult r;
  out.library.close();
  out.table = nullptr;
  out.wasm.clear();
  out.linked = false;
  out.fields = TableFields{};
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
  static const std::vector<TrustedKey> kNoKeys;
  out.signature =
      check_signature(out.dir, ss.str(), offer.trusted != nullptr ? *offer.trusted : kNoKeys);
  if (out.signature.state == Signed::broken) {
    r.errors.push_back(out.signature.error);
    return r;
  }
  if (offer.trusted_only && out.signature.state != Signed::trusted) {
    r.errors.push_back(signature_words(out.signature, m.name, m.version) +
                       ", and this game loads only packages signed by a key it trusts; next: "
                       "add the author's key to the trusted keys, or get a signed package");
    return r;
  }

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
  const ManifestFile* file = offer.form == Form::wasm_only ? nullptr : file_for(m, platform);
  const ManifestFile* module =
      offer.form == Form::native_only ? nullptr : file_for(m, kWasmPlatform);
  if (!file && module) {
    load_wasm(out.dir / fs::path(module->path), *module, offer.linked, out, r);
    return r;
  }
  if (!file) {
    r.errors.push_back(
        offer.form == Form::wasm_only
            ? m.name +
                  " lists no wasm32 module, and this build loads only that form; next: "
                  "build the package with ge mod build --wasm"
            : m.name + " lists no library for " + std::string(platform) +
                  " and no wasm32 module; next: build it on this platform (ge mod build), or "
                  "with ge mod build --wasm");
    return r;
  }
  out.platform = file->platform;
  const fs::path lib = out.dir / fs::path(file->path);
  if (!file_checks(lib, *file, r.errors)) return r;
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
  if (!take_table(entry(GE_MOD_ABI), out, r)) out.library.close();
  return r;
}

}  // namespace ge::mod
