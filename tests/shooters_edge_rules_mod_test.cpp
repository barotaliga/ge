// Tests: C++ mods on the real server (games/shooters_edge/server/rules_runner.hpp § C++ mods on
// top, docs/mods.md § On the server). The sample mod `rocket-bounce`, stacked on a QC runner,
// speeds its rockets by half again and is in the runner's digest, and the same modded run replays
// to the bit; the sample mod `vampire`, stacked on the sequel's rules (which keep their own
// entities), feeds a shooter by her blows, sees the kill, replays to the bit, and is refused what
// those rules keep themselves; the playable server takes a package from `sv_mod`, plays and replays
// to the bit with it, and refuses a package built against another ABI with the version named. Each
// sample loaded as its wasm32 module (as a store build loads it) plays the native library's frames.
//
// ge:summary Tests: C++ mods stacked on the server's rules runners and taken from sv_mod
// ge:keywords test mod mods rules-runner start_mods rules_hash rocket-bounce sv_mod mod-package
// ge:keywords sequel-mods vampire dedicated-server replay
// ge:skill mod shooter testing
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "classic/sequel_move.hpp"
#include "mod/loader.hpp"
#include "server/server.hpp"
#include "shooter/server.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/sequel_mod_duel.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "shooters_edge/sim/mod_host.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;
namespace gm = ge::mod;
namespace gs = ge::server;

namespace {

const gm::LoadedMod& sample() {
  static const std::unique_ptr<gm::LoadedMod> mod = [] {
    auto m = std::make_unique<gm::LoadedMod>();
    const gm::LoadResult r =
        gm::load_mod(std::string(GE_MOD_DIR) + "/rocket-bounce", {SE_MOD_GAME, SE_MOD_API}, *m);
    for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
    return m;
  }();
  return *mod;
}

// A sample package loaded as a store build loads it (its wasm32 module); null where this build
// made no module.
const gm::LoadedMod* module_of(const char* name) {
  auto m = std::make_unique<gm::LoadedMod>();
  gm::GameOffer store{SE_MOD_GAME, SE_MOD_API};
  store.form = gm::Form::wasm_only;
  if (!gm::load_mod(std::string(GE_MOD_DIR) + "/" + name, store, *m).ok()) {
    std::printf("  skipped: no wasm32 module for %s (no clang with wasm-ld)\n", name);
    return nullptr;
  }
  static std::vector<std::unique_ptr<gm::LoadedMod>> kept;
  kept.push_back(std::move(m));
  return kept.back().get();
}

sv::MapSpec arena() {
  sv::MapSpec map;
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  return map;
}

// Player 0 holding attack (the test mod fires a rocket every 0.8 s), turning slowly.
sh::Packet fire(int frame) {
  sh::Packet p;
  p.client = 0;
  p.sequence = static_cast<uint32_t>(frame + 1);
  for (sh::MoveCmd& c : p.cmds) {
    c.msec = 13;
    c.buttons = 1;
    c.angles.y = static_cast<float>(frame);
  }
  return p;
}

struct Run {
  std::vector<uint64_t> hashes;
  float rocket_speed = 0;  // the fastest fly-missile seen, units/s
  bool started = false;
};

// The test mod on the arena through a runner, with the sample mod stacked when `modded`.
Run play(const pg::Progs& progs, bool modded, int frames, const gm::LoadedMod* package = nullptr) {
  Run run;
  std::string error;
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> made = sv::make_qc_server(progs, o, error);
  CHECK_MSG(made != nullptr, error);
  if (!made) return run;
  const sv::RulesRunner r = sv::qc_runner(std::move(made));
  if (modded) {
    r->mods.tables.push_back(package ? package->table : sample().table);
    if (package) r->mods.packages.push_back(package);
    r->mods.names.push_back("rocket-bounce");
  }
  r.console("deathmatch 1");
  run.started = r.start(arena(), error);
  CHECK_MSG(run.started, error);
  if (!run.started) return run;
  CHECK(r->mods.hosts.size() == (modded ? 1u : 0u));
  r.connect(0, "gunner", false, true);
  for (int f = 0; f < frames; f++) {
    const sh::Packet p = fire(f);
    r.frame(&p, 1);
    run.hashes.push_back(sv::rules_hash(*r));
    for (int e = 1; e < r->sv.num_edicts; e++) {
      const sh::Edict& ed = r->sv.edicts[static_cast<size_t>(e)];
      if (!ed.free && ed.movetype == sh::kMovetypeFlyMissile) {
        const float v = sh::length(ed.velocity);
        if (v > run.rocket_speed) run.rocket_speed = v;
      }
    }
  }
  if (!modded) CHECK(sv::rules_hash(*r) == sh::server_hash(r->sv));
  return run;
}

const gm::LoadedMod& vampire() {
  static const std::unique_ptr<gm::LoadedMod> mod = [] {
    auto m = std::make_unique<gm::LoadedMod>();
    const gm::LoadResult r =
        gm::load_mod(std::string(GE_MOD_DIR) + "/vampire", {SE_MOD_GAME, SE_MOD_API}, *m);
    for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
    return m;
  }();
  return *mod;
}

}  // namespace

TEST(a_mod_stacked_on_a_qc_runner_changes_its_match_and_replays) {
  CHECK(sample().table != nullptr);
  if (!sample().table) return;
  pg::Progs progs;
  std::string error;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  const Run plain = play(progs, false, 240);
  const Run modded = play(progs, true, 240);
  const Run again = play(progs, true, 240);
  if (!plain.started || !modded.started) return;
  std::printf("  rocket %.1f u/s plain, %.1f u/s modded\n", plain.rocket_speed,
              modded.rocket_speed);
  CHECK(plain.rocket_speed > 900 && plain.rocket_speed < 1100);
  CHECK(modded.rocket_speed > plain.rocket_speed * 1.45f);
  CHECK(modded.hashes == again.hashes);  // the modded run replays to the bit
  CHECK(modded.hashes != plain.hashes);
}

TEST(a_modules_stack_plays_the_native_librarys_frames) {
  const gm::LoadedMod* module = module_of("rocket-bounce");
  if (!module || !sample().table) return;
  CHECK(module->is_wasm());
  pg::Progs progs;
  std::string error;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  const Run native = play(progs, true, 240);
  const Run store = play(progs, true, 240, module);
  CHECK(store.started);
  CHECK(!store.hashes.empty() && store.hashes == native.hashes);
  CHECK(store.rocket_speed == native.rocket_speed);
}

TEST(a_module_on_the_sequels_rules_plays_the_native_librarys_duel) {
  const gm::LoadedMod* module = module_of("vampire");
  if (!module || !vampire().table) return;
  const sv::SequelDuel native = sv::run_sequel_duel(vampire().table, 600);
  const sv::SequelDuel store = sv::run_sequel_duel(*module, 600);
  CHECK_MSG(store.started, store.error);
  CHECK(!store.hashes.empty() && store.hashes == native.hashes);
  CHECK(store.hook_calls == native.hook_calls);
  CHECK(store.mod_lines == native.mod_lines);
}

TEST(a_mod_stacked_on_the_sequels_rules_changes_its_match_and_replays) {
  if (!vampire().table) return;
  const sv::SequelDuel plain = sv::run_sequel_duel(nullptr, 600);
  const sv::SequelDuel modded = sv::run_sequel_duel(vampire().table, 600);
  const sv::SequelDuel again = sv::run_sequel_duel(vampire().table, 600);
  CHECK_MSG(plain.started && modded.started, plain.error + modded.error);
  if (!plain.started || !modded.started) return;
  const auto most = [](const sv::SequelDuel& d) {
    float m = 0;
    for (float h : d.health) m = h > m ? h : m;
    return m;
  };
  std::printf("  the shooter's health at most %.0f plain, %.0f with the mod; %zu deaths\n",
              most(plain), most(modded), modded.obituaries.size());
  CHECK(most(plain) <= 100);
  CHECK(most(modded) > 100);  // her blows fed her
  CHECK(!modded.obituaries.empty());
  CHECK(!modded.mod_lines.empty());  // the mod's death hook saw the kill
  CHECK(modded.hook_calls > 0);
  CHECK(modded.refusals == 0);
  CHECK(modded.hashes == again.hashes);  // the modded run replays to the bit
  CHECK(modded.hashes != plain.hashes);
}

TEST(the_sequels_rules_refuse_what_they_keep_themselves) {
  if (!vampire().table) return;
  const sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r.console("deathmatch 1");
  r->mods.tables.push_back(vampire().table);
  r->mods.names.push_back("vampire");
  std::string error;
  CHECK_MSG(r.start(sv::sequel_duel_hall(), error), error);
  CHECK(r->mods.hosts.size() == 1u);
  if (r->mods.hosts.empty()) return;
  r.connect(0, "Ana", false, false);
  const se_mod_services_v1* g = &r->mods.hosts[0]->services;
  void* ctx = r->mods.hosts[0]->host.ctx;
  CHECK(g->max_players(ctx) == sh::kMaxClients);
  CHECK(g->in_use(ctx, 1) == 1 && g->in_use(ctx, 2) == 0);
  CHECK(g->get_float(ctx, 1, SE_FIELD_HEALTH) == 100);
  float at[3];
  g->get_vec(ctx, 1, SE_FIELD_ORIGIN, at);
  CHECK(at[0] == -300);
  CHECK(g->spawn(ctx) == -1);                         // a mod's own entity: refused
  CHECK(g->get_float(ctx, 2, SE_FIELD_HEALTH) == 0);  // a slot nobody plays: refused
  CHECK(g->get_int(ctx, 1, SE_FIELD_THINK) == 0);     // a field the rules keep: refused
  CHECK(r->mods.hosts[0]->run.refusals == 3);
  CHECK(r->mods.hosts[0]->log.size() == 3u);
  if (!r->mods.hosts[0]->log.empty())
    CHECK_MSG(r->mods.hosts[0]->log[0].find("refused spawn") == 0, r->mods.hosts[0]->log[0]);
}

TEST(the_playable_server_takes_a_package_from_sv_mod) {
  if (!sample().table) return;
  auto open = [](const std::string& mod, gs::Server& s, std::string& error) {
    gs::ServerConfig c;
    c.map = "arena";
    c.maxclients = 4;
    c.bots = 1;  // a bot plays, so the mod's post-think runs
    c.mod = mod;
    return gs::server_open(s, shooters_edge::net::classic_game(), c, nullptr, nullptr, error);
  };
  const std::string pkg = std::string(GE_MOD_DIR) + "/rocket-bounce";
  gs::Server plain, modded, again;
  std::string error;
  CHECK_MSG(open("", plain, error), error);
  CHECK_MSG(open(pkg, modded, error), error);
  CHECK_MSG(open(pkg, again, error), error);
  bool same = true, differs = false;
  for (int f = 0; f < 300; f++) {
    gs::server_frame(plain);
    gs::server_frame(modded);
    gs::server_frame(again);
    same = same && gs::server_digest(modded) == gs::server_digest(again);
    differs = differs || gs::server_digest(modded) != gs::server_digest(plain);
  }
  CHECK(same);
  CHECK(differs);
  // It says which mods it runs, to a browser (serverinfo) and to a hub (the public status).
  bool listed = false;
  for (const auto& [k, v] : gs::server_public_status(modded).keys)
    listed = listed || (k == "*mods" && v == "rocket-bounce 1.0.0");
  CHECK(listed);
  for (const auto& [k, v] : gs::server_public_status(plain).keys) CHECK(k != "*mods");

  gs::Server refused;
  CHECK(!open(std::string(GE_MOD_DIR) + "/abi-two", refused, error));
  CHECK_MSG(error.find("2.0") != std::string::npos, error);
}
