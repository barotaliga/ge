// THE MOD LOADER on real shared libraries built by this tree (cmake/Mods.cmake, under GE_MOD_DIR):
// the sample mod `rocket-bounce` loads for Shooter's Edge and resolves its table; the fixture
// built against mod ABI 2.0 is refused with a message naming 2.0 and this engine's 1.0; a manifest
// that asks for another ABI, another game, a game_api the game does not offer, a library whose
// bytes are not the listed hash, or no file for this platform is refused naming what failed and the
// next step. The host table carries the engine's own libm, bit for bit.
//
// ge:summary Tests: the mod loader on built libraries: loads, refusals by ABI, game, hash, platform
// ge:keywords test mod loader load_mod refuse abi-version game-api sha256 platform fill_host
// ge:skill mod testing
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "mod/host.hpp"
#include "mod/loader.hpp"
#include "shooters_edge/sim/mod_api.hpp"

namespace fs = std::filesystem;
namespace gm = ge::mod;

namespace {

const gm::GameOffer kShootersEdge = {SE_MOD_GAME, SE_MOD_API};

fs::path mod_dir(const char* name) { return fs::path(GE_MOD_DIR) / name; }

bool mentions(const gm::LoadResult& r, const std::string& a, const std::string& b = "") {
  for (const std::string& e : r.errors)
    if (e.find(a) != std::string::npos && (b.empty() || e.find(b) != std::string::npos) &&
        e.find("next:") != std::string::npos)
      return true;
  return false;
}

std::string read(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

// A copy of the sample package in a scratch directory, its manifest edited by `edit`.
fs::path edited_copy(const std::string& tag, std::string (*edit)(const std::string&)) {
  const fs::path src = mod_dir("rocket-bounce");
  const fs::path dst = fs::temp_directory_path() / ("ge_mod_loader_test_" + tag);
  fs::remove_all(dst);
  fs::create_directories(dst);
  for (const auto& e : fs::directory_iterator(src))
    fs::copy_file(e.path(), dst / e.path().filename(), fs::copy_options::overwrite_existing);
  const std::string text = edit(read(dst / "mod.manifest"));  // read before the write truncates
  std::ofstream(dst / "mod.manifest", std::ios::binary) << text;
  return dst;
}

std::string swap(std::string s, const std::string& from, const std::string& to) {
  const size_t at = s.find(from);
  if (at != std::string::npos) s.replace(at, from.size(), to);
  return s;
}

}  // namespace

TEST(the_sample_mod_loads_for_its_game) {
  gm::LoadedMod m;
  const gm::LoadResult r = gm::load_mod(mod_dir("rocket-bounce"), kShootersEdge, m);
  for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
  CHECK(r.ok());
  CHECK(m.table != nullptr);
  CHECK(m.library.is_open());
  CHECK(m.platform == gm::this_platform());
  CHECK(m.manifest.name == "rocket-bounce");
  if (m.table) {
    CHECK(std::string(m.table->name) == "rocket-bounce");
    CHECK(std::string(m.table->game) == SE_MOD_GAME);
    CHECK(GE_MOD_ABI_MAJOR_OF(m.table->abi) == GE_MOD_ABI_MAJOR);
    CHECK(m.table->state_bytes > 0);
  }
  // The manifest file itself is a path too, and a package checks without a game.
  gm::LoadedMod again;
  CHECK(gm::load_mod(mod_dir("rocket-bounce") / "mod.manifest", {}, again).ok());
}

TEST(a_library_built_against_another_abi_is_refused_naming_both_versions) {
  gm::LoadedMod m;
  const gm::LoadResult r = gm::load_mod(mod_dir("abi-two"), kShootersEdge, m);
  CHECK(!r.ok());
  CHECK(mentions(r, "mod ABI 2.0", "ABI 1.0"));
  CHECK(m.table == nullptr);
  CHECK(!m.library.is_open());
}

TEST(a_manifest_asking_for_another_abi_is_refused_before_the_library_opens) {
  const fs::path dir =
      edited_copy("abi", [](const std::string& s) { return swap(s, "abi = 1.0", "abi = 2.0"); });
  gm::LoadedMod m;
  const gm::LoadResult r = gm::load_mod(dir, kShootersEdge, m);
  CHECK(mentions(r, "needs mod ABI 2.0", "speaks ABI 1.0"));
  CHECK(!m.library.is_open());
  const fs::path newer =
      edited_copy("minor", [](const std::string& s) { return swap(s, "abi = 1.0", "abi = 1.9"); });
  CHECK(mentions(gm::load_mod(newer, kShootersEdge, m), "needs mod ABI 1.9"));
}

TEST(another_game_or_game_api_is_refused) {
  gm::LoadedMod m;
  CHECK(mentions(gm::load_mod(mod_dir("rocket-bounce"), {"fall_line", 1}, m),
                 "a mod for `shooters_edge`, not for `fall_line`"));
  CHECK(mentions(gm::load_mod(mod_dir("rocket-bounce"), {SE_MOD_GAME, 2}, m), "game_api 1",
                 "offers 2"));
}

TEST(a_library_that_is_not_the_one_packaged_is_refused) {
  const fs::path dir = edited_copy("hash", [](const std::string& s) {
    const size_t at = s.rfind(' ');
    return s.substr(0, at + 1) + std::string(64, '0') + "\n";
  });
  gm::LoadedMod m;
  CHECK(
      mentions(gm::load_mod(dir, kShootersEdge, m), "the manifest lists " + std::string(64, '0')));
  CHECK(!m.library.is_open());
}

TEST(no_file_for_this_platform_is_refused) {
  const fs::path dir = edited_copy("platform", [](const std::string& s) {
    return swap(s, "file." + std::string(gm::this_platform()),
                gm::this_platform() == "windows-arm64" ? "file.linux-x64" : "file.windows-arm64");
  });
  gm::LoadedMod m;
  CHECK(mentions(gm::load_mod(dir, kShootersEdge, m), "lists no library for"));
}

TEST(no_manifest_is_refused) {
  gm::LoadedMod m;
  CHECK(mentions(gm::load_mod(fs::temp_directory_path() / "ge_mod_loader_test_none", {}, m),
                 "no manifest there"));
}

TEST(a_table_is_checked_field_by_field) {
  gm::Manifest man;
  man.name = "x";
  man.game = SE_MOD_GAME;
  man.game_api_min = man.game_api_max = 1;
  CHECK(!gm::check_table(nullptr, man).empty());
  ge_mod_v1 t{};
  t.size = 8;
  CHECK(gm::check_table(&t, man).front().find("shorter") != std::string::npos);
  static const int kHooks = 0;
  t = ge_mod_v1{sizeof(ge_mod_v1), GE_MOD_ABI, "x", "1.0.0", SE_MOD_GAME, 1, 0, nullptr, &kHooks};
  CHECK(gm::check_table(&t, man).empty());
  t.state_bytes = GE_MOD_STATE_MAX + 1;
  t.game_api = 2;
  CHECK(gm::check_table(&t, man).size() == 2);
}

TEST(the_host_table_carries_the_engines_libm) {
  ge_mod_host_v1 h;
  gm::fill_host(h);
  CHECK(h.size == sizeof(ge_mod_host_v1));
  CHECK(h.abi == GE_MOD_ABI);
  CHECK_BITS(h.sin(0.7), ge::core::libm::sin(0.7));
  CHECK_BITS(h.atan2(0.3, -2.0), ge::core::libm::atan2(0.3, -2.0));
  CHECK_BITS(h.pow(1.7, 3.3), ge::core::libm::pow(1.7, 3.3));
  CHECK(GE_MOD_HAS(ge_mod_host_v1, &h, game));
  ge_mod_host_v1 older = h;
  older.size = static_cast<uint32_t>(offsetof(ge_mod_host_v1, log_line));
  CHECK(!GE_MOD_HAS(ge_mod_host_v1, &older, log_line));
  CHECK(GE_MOD_HAS(ge_mod_host_v1, &older, random));
  ge::core::Rng a = ge::core::make_rng(9), b = a;
  CHECK(gm::random_u32(a) == static_cast<uint32_t>(ge::core::next(b) * 4294967296.0));
}
