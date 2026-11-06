// Tests: co-op as the window's mode (client/coop_mode.hpp): `sv_coop 1` starts a coop game on the
// start map with `sv_coopbots` companions, only when the game has a campaign; the window's
// friendly-fire settings are written to the campaign's server; the episodes done are merged both
// ways, so one finished in a game is kept for the next.
//
// ge:summary Tests: sv_coop starts a coop game; friendly fire and episodes kept in step
// ge:keywords test coop sv_coop sv_coopbots sv_friendlyfire sv_friendlypush sv_episodes_done
// ge:skill shooter testing
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;

std::unique_ptr<cl::LoadedMap> stub_map(const std::string& name) {
  auto m = std::make_unique<cl::LoadedMap>();
  m->name = name;
  m->spec.name = name;
  m->spec.models = sv::arena_models();
  m->spec.entities = sv::kStubEntities;
  return m;
}

bool has_line(const cl::ListenOptions& opt, const std::string& line) {
  return std::find(opt.console.begin(), opt.console.end(), line) != opt.console.end();
}

}  // namespace

TEST(sv_coop_starts_a_coop_game_on_the_start_map_with_its_bots) {
  ge::console::Console con(false);
  cl::CoopMode m;
  cl::add_coop_cvars(con, m);
  cl::Listen l;
  std::string next;
  cl::coop_frame(m, l, true, "start", next);
  CHECK(next.empty());  // off: nothing
  con.execute("sv_coopbots 3");
  con.execute("sv_coop 1");
  cl::coop_frame(m, l, true, "start", next);
  CHECK(next == "start");
  CHECK(has_line(l.opt, "deathmatch 0") && has_line(l.opt, "coop 1"));
  CHECK(has_line(l.opt, "sv_friendlyfire 0") && has_line(l.opt, "sv_friendlypush 0"));
  CHECK(l.opt.companions == 3);
  CHECK(cl::wants_campaign(l.opt));
  // Once acted on, not again until it changes.
  next.clear();
  cl::coop_frame(m, l, true, "start", next);
  CHECK(next.empty());
}

TEST(sv_coop_without_a_campaign_starts_nothing) {
  ge::console::Console con(false);
  cl::CoopMode m;
  cl::add_coop_cvars(con, m);
  cl::Listen l;
  std::string next;
  con.execute("sv_coop 1");
  cl::coop_frame(m, l, false, "start", next);
  CHECK(next.empty() && l.opt.companions == 0);
}

TEST(the_bots_are_seven_at_most_for_eight_players) {
  ge::console::Console con(false);
  cl::CoopMode m;
  cl::add_coop_cvars(con, m);
  con.execute("sv_coopbots 12");
  CHECK(m.bots->integer() == 7);
  CHECK(cl::kCoopPlayers == 8);
}

TEST(the_windows_friendly_fire_and_the_episodes_reach_the_coop_game) {
  ge::progs::Progs stub;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(stub, error), error);
  ge::console::Console con(false);
  cl::CoopMode m;
  cl::add_coop_cvars(con, m);
  con.execute("sv_friendlypush 1");
  con.execute("sv_episodes_done 1");
  cl::Listen l;
  cl::ListenOptions opt;
  opt.console = {"deathmatch 0", "coop 1"};
  CHECK_MSG(cl::listen_start(l, stub, stub_map("start"), opt, error), error);
  std::string next;
  cl::coop_frame(m, l, true, "start", next);
  CHECK(l.server->cvars.find("sv_friendlypush")->value == "1");
  CHECK(l.server->cvars.find("sv_friendlyfire")->value == "0");
  CHECK(l.server->cvars.find("sv_episodes_done")->integer() == 1);
  // An episode the game finished (its rune's bit set by the program) is the window's to keep.
  l.server->cvars.set("sv_episodes_done", "3", ge::console::Source::local);
  cl::coop_frame(m, l, true, "start", next);
  CHECK(m.episodes->integer() == 3);
}
