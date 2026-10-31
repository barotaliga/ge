// Tests: what the scores key shows, read off the listen server (games/shooters_edge/client/
// scores.hpp): a match's board of the local player and a second client off their user info and
// their entities' frags, teamplay off the server's cvar, the minutes on the server; which screen
// the key and the player's state pick in a match.
//
// ge:summary Tests: the scoreboard's rows off the listen server and which screen the key shows
// ge:keywords test scores scores_of scores_screen listen-server frags userinfo teamplay
// ge:skill ui shooter testing
#include <bit>
#include <cstdint>
#include <memory>
#include <string>

#include "check.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/scores.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;

bool start(cl::Listen& l, const pg::Progs& progs) {
  cl::SearchPath files;
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "arena", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  const bool ok = cl::listen_start(l, progs, std::move(map), {}, error);
  CHECK_MSG(ok, error);
  return ok;
}

void set_frags(cl::Listen& l, int entity, float frags) {
  const pg::Def* d = pg::find_field(*l.progs, "frags");
  CHECK(d != nullptr);
  if (d != nullptr)
    *pg::entity_field(l.server->q.vm, entity, d->offset) = std::bit_cast<uint32_t>(frags);
}

}  // namespace

TEST(a_matchs_board_lists_its_clients_off_their_user_info_and_frags) {
  pg::Progs progs;
  std::string error;
  CHECK_MSG(cl::baseline_progs(progs, error), error);
  cl::Listen l;
  if (!start(l, progs)) return;
  sv::connect(*l.server, 1, "rival", false, true);
  std::string old;
  sv::set_info_value(l.server->clients[1].userinfo, "topcolor", "4", old);
  sv::set_info_value(l.server->clients[1].userinfo, "bottomcolor", "12", old);
  sv::set_info_value(l.server->clients[1].userinfo, "team", "red", old);
  l.server->clients[1].ping_ms = 33;
  for (int i = 0; i < 10; i++) cl::listen_frame(l, {});
  set_frags(l, 1, 3);
  set_frags(l, 2, 7);
  const shooters_edge::menu::Scoreboard b = cl::scores_of(l);
  CHECK(b.rows.size() == 2);
  if (b.rows.size() != 2) return;
  CHECK(b.rows[0].name == "player" && b.rows[0].self && b.rows[0].frags == 3);
  CHECK(b.rows[1].name == "rival" && !b.rows[1].self && b.rows[1].frags == 7);
  CHECK(b.rows[1].top == 4 && b.rows[1].bottom == 12 && b.rows[1].team == "red");
  CHECK(b.rows[1].ping == 33 && b.rows[1].minutes == 0);
  CHECK(!b.teamplay);
  l.server->cvars.set("teamplay", "1", ge::console::Source::code);
  CHECK(cl::scores_of(l).teamplay);
  // The key held shows the players' board; nothing without it while the player lives.
  CHECK(cl::scores_screen(l, true) == cl::ScoresScreen::players);
  CHECK(cl::scores_screen(l, false) == cl::ScoresScreen::none);
}
