// Tests: the game's own rules shipped as rules packages (sim/rules_api.hpp,
// server/rules_package.hpp). The `arena` and `campaign` packages the build makes from the same
// sources as the copy built into the game load through the mod loader and play the same matches
// to the bit: a deathmatch on the baseline arena to its time limit, and the campaign's coop hall.
// A rules package is refused where a mod is asked for, and a table of another API is refused.
//
// ge:summary Tests: the arena and campaign as rules packages play the built-in rules' matches
// ge:keywords test rules-package arena-package campaign-package load_rules_package rules_progs
// ge:keywords side-by-side mod-loader refusal
// ge:skill mod progs shooter testing
#include <cstdint>
#include <memory>
#include <string>

#include "check.hpp"
#include "mod/loader.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/rules_package.hpp"
#include "shooters_edge/sim/mod_api.hpp"

namespace sb = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;

namespace sb = ge::shooterbots;

namespace {

const std::string kMods = GE_MOD_DIR;

uint64_t mix_in(uint64_t h, uint64_t w) {
  for (int i = 0; i < 8; i++) {
    h ^= (w >> (8 * i)) & 0xffu;
    h *= 1099511628211ull;
  }
  return h;
}

uint64_t fold(const sv::ModMatchReport& r) {
  uint64_t h = 1469598103934665603ull;
  for (uint64_t d : r.loop_digests) h = mix_in(h, d);
  for (const std::string& t : r.frag_log)
    for (char c : t) h = mix_in(h, static_cast<uint8_t>(c));
  for (const std::string& t : r.prints)
    for (char c : t) h = mix_in(h, static_cast<uint8_t>(c));
  return h;
}

uint64_t fold(const sb::CoopReport& r) {
  uint64_t h = 1469598103934665603ull;
  for (uint64_t d : r.digests) h = mix_in(h, d);
  for (const sb::CoopKill& k : r.kills) {
    h = mix_in(h, static_cast<uint32_t>(k.ent));
    h = mix_in(h, static_cast<uint32_t>(k.by));
  }
  return h;
}

sv::ModMatchReport deathmatch(const pg::Progs* package) {
  sv::ModMatchOptions o;
  if (package != nullptr)
    o.compiled = package;
  else
    o.native = &sv::arena_rules();
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = 5100;
  o.console = {"deathmatch 1", "timelimit 1"};
  return sv::run_mod_match(o);
}

}  // namespace

TEST(the_arena_package_plays_the_built_in_arenas_deathmatch) {
  pg::Progs package;
  std::string error;
  CHECK_MSG(sv::load_rules_package(kMods + "/arena", package, error), error);
  const sv::ModMatchReport a = deathmatch(nullptr);
  const sv::ModMatchReport b = deathmatch(&package);
  CHECK_MSG(a.ok && b.ok, a.error + b.error);
  CHECK(a.faults == 0 && b.faults == 0);
  CHECK(a.ended && b.ended);
  CHECK(a.frames == b.frames);
  CHECK(!a.frag_log.empty());
  CHECK_MSG(fold(a) == fold(b), "the package parts from the built-in arena at frames " +
                                    std::to_string(a.frames) + " and " + std::to_string(b.frames));
}

TEST(the_campaign_package_plays_the_built_in_campaigns_coop_hall) {
  pg::Progs package, built_in;
  std::string error;
  CHECK_MSG(sv::load_rules_package(kMods + "/campaign", package, error), error);
  CHECK_MSG(sv::campaign_mod(built_in, error), error);
  sb::CoopOptions o;
  o.root = GE_SOURCE_DIR;
  o.seconds = 45;
  o.progs = &built_in;
  const sb::CoopReport a = sb::run_coop(o);
  o.progs = &package;
  const sb::CoopReport b = sb::run_coop(o);
  CHECK_MSG(a.ok && b.ok, a.error + b.error);
  CHECK(a.faults == 0 && b.faults == 0);
  CHECK(a.digests.size() == b.digests.size() && !a.digests.empty());
  CHECK(a.kills.size() == b.kills.size());
  CHECK_MSG(fold(a) == fold(b), "the package parts from the built-in campaign");
}

TEST(a_rules_package_is_refused_where_a_mod_is_asked_for) {
  auto m = std::make_unique<ge::mod::LoadedMod>();
  const ge::mod::LoadResult r = ge::mod::load_mod(kMods + "/arena", {SE_MOD_GAME, SE_MOD_API}, *m);
  CHECK(!r.ok());
  CHECK_MSG(!r.errors.empty() && r.errors.front().find(SE_RULES_GAME) != std::string::npos,
            r.errors.empty() ? "no error" : r.errors.front());
}

TEST(a_rules_table_of_another_api_is_refused) {
  se_rules_v1 t = sv::arena_rules();
  t.api = SE_RULES_API + 1;
  pg::Progs p;
  std::string error;
  CHECK(!sv::rules_progs(t, p, error));
  CHECK_MSG(error.find("rules API 2") != std::string::npos, error);
}
