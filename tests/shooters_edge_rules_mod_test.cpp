// Tests: C++ mods on the real server (games/shooters_edge/server/rules_runner.hpp § C++ mods on
// top, docs/mods.md § On the server). The sample mod `rocket-bounce`, stacked on a QC runner,
// speeds its rockets by half again and is in the runner's digest, and the same modded run replays
// to the bit; a runner whose rules keep their own entities (the sequel's) refuses a mod, saying how
// to go on; the playable server takes a package from `sv_mod`, plays and replays to the
// bit with it, and refuses a package built against another ABI with the version named.
//
// ge:summary Tests: C++ mods stacked on the server's rules runners and taken from sv_mod
// ge:keywords test mod mods rules-runner start_mods rules_hash rocket-bounce sv_mod mod-package
// ge:keywords sequel-refuses dedicated-server replay
// ge:skill mod shooter testing
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "mod/loader.hpp"
#include "server/server.hpp"
#include "shooter/server.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/test_mod.hpp"

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
Run play(const pg::Progs& progs, bool modded, int frames) {
  Run run;
  std::string error;
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> made = sv::make_qc_server(progs, o, error);
  CHECK_MSG(made != nullptr, error);
  if (!made) return run;
  const sv::RulesRunner r = sv::qc_runner(std::move(made));
  if (modded) {
    r->mods.tables.push_back(sample().table);
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

TEST(a_runner_off_the_program_seam_refuses_a_mod) {
  if (!sample().table) return;
  const sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
  r->mods.tables.push_back(sample().table);
  std::string error;
  CHECK(!sv::start_mods(*r, *r.calls, error));
  CHECK_MSG(error.find("can't run C++ mods") != std::string::npos, error);
  CHECK_MSG(error.find("sv_mod") != std::string::npos, error);
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

  gs::Server refused;
  CHECK(!open(std::string(GE_MOD_DIR) + "/abi-two", refused, error));
  CHECK_MSG(error.find("2.0") != std::string::npos, error);
}
