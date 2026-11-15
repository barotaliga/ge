// MODS LINKED INTO THE APP AHEAD OF TIME (engine/mod/linked.hpp, cmake/Mods.cmake's
// ge_link_mods): this test links the sample mod `rocket-bounce` into itself, as a store build
// would, registered under its wasm32 module's SHA-256. Loading the package as a store build does
// (the module's row only) then runs the linked code, not the interpreter, and the match it plays
// is the native library's and the interpreted module's to the bit on every frame. A package whose
// module is not the one linked still runs in the interpreter, and `GameOffer::linked = false`
// asks for the interpreter outright. Where the build has no wasm32 toolchain nothing is linked
// and the match tests skip.
//
// ge:summary Tests: a mod linked into the app ahead of time runs a store build's module natively
// ge:keywords test mod linked-mod ahead-of-time ge_link_mods find_linked store-build wasm32
// ge:skill mod testing
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "check.hpp"
#include "mod/linked.hpp"
#include "mod/loader.hpp"
#include "mod/sha256.hpp"
#include "mod/wasm/mod.hpp"
#include "shooter/frame.hpp"
#include "shooters_edge/sim/match.hpp"
#include "shooters_edge/sim/mod_host.hpp"

namespace se = shooters_edge::sim;
namespace sh = ge::shooter;
namespace gm = ge::mod;
namespace fs = std::filesystem;

namespace {

const fs::path kSample = fs::path(GE_MOD_DIR) / "rocket-bounce";

// The sample's module hash when this test linked it (ge_link_mods), else empty.
std::string linked_hash() {
  const std::optional<std::string> h = gm::sha256_file(kSample / "rocket_bounce.wasm");
  if (!h || !gm::find_linked(*h)) return {};
  return *h;
}

bool linked_here() {
  if (!linked_hash().empty()) return true;
  std::printf(
      "  skipped: the sample is not linked here (no wasm32 toolchain, no module to link)\n");
  return false;
}

const ge_mod_v1* no_table(uint32_t) { return nullptr; }

std::unique_ptr<gm::LoadedMod> load(gm::Form form, bool linked) {
  auto m = std::make_unique<gm::LoadedMod>();
  gm::GameOffer offer{SE_MOD_GAME, SE_MOD_API};
  offer.form = form;
  offer.linked = linked;
  const gm::LoadResult r = gm::load_mod(kSample, offer, *m);
  for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
  CHECK(r.ok());
  return m;
}

// The scripted duel with `package` on the seam, both players holding attack (player 1 rockets
// ahead, player 2 grenades up the room): the hash of every frame.
std::vector<uint64_t> play(const gm::LoadedMod& package, int frames) {
  se::Arena arena;
  se::build_arena(arena);
  const std::vector<sh::BrushModel> models = arena.models();
  sh::ServerSettings set;
  set.maxclients = 2;
  set.max_edicts = 64;
  se::Rules rules;
  se::create_rules(rules, arena, set.max_edicts);
  std::unique_ptr<gm::wasm::WasmMod> instance;
  se::ModHost mod;
  CHECK(se::host_package(mod, package, instance, nullptr, nullptr, se::rules_program(rules), 1)
            .empty());
  sh::Server sv;
  sh::create_server(sv, set, models, se::mod_program(mod));
  se::spawn_arena_entities(sv, rules);
  se::mod_match_start(mod, sv);
  sh::connect_client(sv, 0, false, true);
  sh::connect_client(sv, 1, false, true);
  std::vector<uint64_t> hashes;
  for (uint32_t seq = 1; seq <= static_cast<uint32_t>(frames); ++seq) {
    sh::Packet p[2];
    const float pitch[2] = {0, -30}, yaw[2] = {arena.spawn_yaw[0], arena.spawn_yaw[1]};
    for (int c = 0; c < 2; c++) {
      p[c].client = c;
      p[c].sequence = seq;
      sh::MoveCmd cmd;
      cmd.msec = static_cast<uint8_t>(se::kServerFrameMs);
      cmd.angles = {pitch[c], yaw[c], 0};
      cmd.buttons = sh::kButtonAttack;
      for (sh::MoveCmd& m : p[c].cmds) m = cmd;
    }
    sh::server_frame(sv, p, 2);
    hashes.push_back(se::mod_hash(mod.run, sh::server_hash(sv)));
  }
  return hashes;
}

}  // namespace

TEST(the_registry_finds_a_mod_by_its_module_hash_once) {
  const char* hash = "00000000000000000000000000000000000000000000000000000000000000aa";
  CHECK(gm::find_linked(hash) == nullptr);
  CHECK(gm::link_mod({"fake", hash, &no_table}));
  const gm::LinkedMod* m = gm::find_linked(hash);
  CHECK(m != nullptr && std::string(m->name) == "fake");
  CHECK(gm::link_mod({"other", hash, &no_table}));  // a second registration is ignored
  CHECK(std::string(gm::find_linked(hash)->name) == "fake");
}

TEST(a_store_build_runs_the_linked_mod_for_its_module) {
  if (!linked_here()) return;
  const auto m = load(gm::Form::wasm_only, true);
  CHECK(m->linked);
  CHECK(!m->is_wasm());
  CHECK(m->table != nullptr);
  CHECK(m->platform == "wasm32");
  CHECK(m->fields.name == "rocket-bounce");
  const auto interpreted = load(gm::Form::wasm_only, false);
  CHECK(!interpreted->linked);
  CHECK(interpreted->is_wasm());
  CHECK(interpreted->table == nullptr);
}

TEST(the_linked_mod_plays_the_native_and_interpreted_match_to_the_bit) {
  if (!linked_here()) return;
  constexpr int kFrames = 700;
  const auto native = load(gm::Form::native_only, true);
  const auto linked = load(gm::Form::wasm_only, true);
  const auto interpreted = load(gm::Form::wasm_only, false);
  const std::vector<uint64_t> a = play(*native, kFrames);
  const std::vector<uint64_t> b = play(*linked, kFrames);
  const std::vector<uint64_t> c = play(*interpreted, kFrames);
  CHECK(a.size() == static_cast<size_t>(kFrames));
  int first_apart = -1;
  for (size_t i = 0; i < a.size() && first_apart < 0; ++i)
    if (a[i] != b[i] || a[i] != c[i]) first_apart = static_cast<int>(i);
  if (first_apart >= 0) std::printf("  the forms part at frame %d\n", first_apart);
  CHECK(first_apart < 0);
}

TEST(a_module_that_is_not_the_one_linked_runs_in_the_interpreter) {
  if (!linked_here()) return;
  // A copy of the package with another module: the vampire's, so its hash is not the one linked.
  const fs::path dir = fs::temp_directory_path() / "ge_mod_linked_test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path vampire = fs::path(GE_MOD_DIR) / "vampire";
  fs::copy_file(vampire / "vampire.wasm", dir / "vampire.wasm");
  std::ifstream in(vampire / "mod.manifest");
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  {
    std::ofstream out(dir / "mod.manifest");
    out << text;
  }
  gm::LoadedMod m;
  gm::GameOffer offer{SE_MOD_GAME, SE_MOD_API};
  offer.form = gm::Form::wasm_only;
  const gm::LoadResult r = gm::load_mod(dir, offer, m);
  for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
  CHECK(r.ok());
  CHECK(!m.linked);
  CHECK(m.is_wasm());
  fs::remove_all(dir);
}
