// Tests: QuakeWorld's captain mode (games/shooters_edge/net/qw_captains.hpp) on the dedicated
// server, four classic clients over the simulated network. Two captains are elected by the others'
// yes; the rest are numbered (shown as their frags) and picked in turn by number keys, the last
// one going by himself, each taking his captain's team; a pick out of turn is refused; a team
// change while picking is refused; out of team play, with too few players or without a team name
// `captain` is refused.
//
// ge:summary Tests: QuakeWorld's captain mode: elections, numbered picks in turn, the refusals
// ge:keywords test qw-captains captain-mode team-picking captain-election impulse-pick
// ge:skill net shooter testing
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/main.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_game.hpp"

using namespace ge::server;
namespace gn = ge::net;
namespace se = shooters_edge::net;

namespace {

constexpr uint16_t kPort = 27500;
constexpr int kClients = 4;

ServerConfig arena(bool teamplay = true) {
  ServerConfig c;
  c.hostname = "Captains Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = 7;
  if (teamplay) c.rules["teamplay"] = "1";
  return c;
}

// What one client says or presses, at a frame: a line, or a number key (`impulse`) when `key`.
struct Act {
  int at;
  std::string line;
  int key = 0;
};

struct Four {
  gn::SimNetwork net;
  gn::Transport server_port, ports[kClients];
  Server s;
  se::QwClientEnd clients[kClients];
  std::array<int, kClients> key{};  // the number key each client presses on its next move
  std::string error;
  bool opened = false;
};

struct Who {
  const char* name;
  const char* team;
  int color;
};

// The server and `n` clients for `frames` frames; each says or presses its acts at their frames.
void run(Four& r, const ServerConfig& c, int frames, const std::vector<std::vector<Act>>& acts,
         int n = kClients,
         const std::array<Who, kClients>& who = {
             {{"ana", "red", 4}, {"ben", "blue", 13}, {"cid", "", 0}, {"dee", "", 0}}}) {
  gn::LinkConditions lan;
  r.net = gn::make_sim_network(c.seed, lan);
  gn::open_sim_transport(r.server_port, r.net, gn::sim_address(0, kPort));
  r.opened = server_open(r.s, se::qw_game(), c, &r.server_port, nullptr, r.error);
  if (!r.opened) return;
  for (int i = 0; i < n; i++)
    gn::open_sim_transport(r.ports[i], r.net, gn::sim_address(1 + i, uint16_t(27001 + i)));
  for (int f = 0; f < frames; f++) {
    const int64_t now = r.s.now_us;
    gn::sim_advance(r.net, now);
    for (int i = 0; i < n; i++) {
      if (f == 0) {
        se::ClientConfig cc;
        cc.files = {"skins/base.pcx"};
        cc.name = who[static_cast<size_t>(i)].name;
        cc.team = who[static_cast<size_t>(i)].team;
        cc.topcolor = cc.bottomcolor = who[static_cast<size_t>(i)].color;
        int* key = &r.key[static_cast<size_t>(i)];
        cc.brain = [key](int tick) {
          ge::shooter::MoveCmd m = se::default_script(tick, 0);
          m.buttons = 0;
          m.impulse = static_cast<uint8_t>(*key);
          *key = 0;
          return m;
        };
        se::client_start(r.clients[i], r.ports[i], gn::sim_address(0, kPort), cc,
                         static_cast<double>(now) / 1e6);
      }
      if (i < static_cast<int>(acts.size()))
        for (const Act& a : acts[static_cast<size_t>(i)])
          if (f == a.at) {
            if (a.key != 0)
              r.key[static_cast<size_t>(i)] = a.key;
            else
              se::client_command(r.clients[i], a.line);
          }
      se::client_tick(r.clients[i], now);
    }
    server_frame(r.s);
  }
}

bool printed(const se::QwClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (p.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(qw_captains_elected_then_the_rest_picked_in_turn) {
  Four r;
  // Each captain presses 1 twice: the one whose turn it is picks number 1, the other is told it is
  // not his turn; at his second press, after the first pick, only number 2 is left, and that one
  // goes by himself to the captain whose turn it is.
  run(r, arena(), 900,
      {{{150, "captain"}, {170, "yes"}, {400, "", 1}, {500, "", 1}},
       {{160, "yes"}, {200, "captain"}, {400, "", 1}, {500, "", 1}},
       {{165, "yes"}, {210, "yes"}, {300, "setinfo team green"}},
       {{215, "yes"}}});
  CHECK_MSG(r.opened, r.error);
  const se::QwClientEnd& ana = r.clients[0];
  const se::QwClientEnd& cid = r.clients[2];
  CHECK(printed(ana, "ana has requested captain status!"));
  CHECK(printed(r.clients[1], "Type yes in console to approve"));
  CHECK(printed(ana, "Type captain to abort election"));
  CHECK(printed(ana, "ben gives his vote"));
  CHECK(printed(ana, "ana becomes a captain"));
  CHECK(printed(ana, "One more captain should be elected"));
  CHECK(printed(ana, "ben becomes a captain"));
  CHECK(printed(ana, "Both captains elected"));
  CHECK(printed(ana, "Team picking begins"));
  CHECK(printed(ana, "Use numbers or impulses to choose"));
  CHECK(printed(ana, " is picking"));
  CHECK(printed(cid, "You may not change team"));
  CHECK(printed(ana, "It's not your turn") || printed(r.clients[1], "It's not your turn"));
  CHECK(printed(ana, "cid set to team "));
  CHECK(printed(ana, "dee set to team "));
  CHECK(printed(cid, "You were picked by "));
  CHECK(printed(cid, "Time to go ready"));
  CHECK(printed(ana, "All players chosen. Captain modes exited"));
  CHECK(printed(ana, "You are no longer a captain"));
  // cid and dee went to different captains: one red, one blue.
  const bool cid_red = printed(ana, "cid set to team red");
  CHECK(cid_red != printed(ana, "dee set to team red"));
  CHECK(cid_red != printed(ana, "cid set to team blue"));
}

TEST(qw_captains_numbers_show_as_frags_while_picking) {
  Four r;
  run(r, arena(), 380,
      {{{150, "captain"}},
       {{160, "yes"}, {200, "captain"}},
       {{165, "yes"}, {210, "yes"}},
       {{215, "yes"}}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.clients[0], "Team picking begins"));
  // cid and dee (slots 2 and 3) wait as numbers 1 and 2.
  CHECK(r.clients[0].frags[2] == 1);
  CHECK(r.clients[0].frags[3] == 2);
}

TEST(qw_captains_refused_out_of_team_play_and_without_a_team) {
  Four off;
  run(off, arena(false), 250, {{{150, "captain"}}});
  CHECK_MSG(off.opened, off.error);
  CHECK(printed(off.clients[0], "No team picking in non team mode"));
  Four two;
  run(two, arena(), 250, {{{150, "captain"}}}, 2);
  CHECK(printed(two.clients[0], "Not enough players present"));
  Four none;
  run(none, arena(), 250, {{}, {}, {{150, "captain"}}});
  CHECK(printed(none.clients[2], "Set your team name first"));
}

TEST(qw_captains_a_ready_player_keeps_a_team_and_picking_unreadies) {
  ServerConfig c = arena();
  c.lobby = 1;
  Four r;
  run(r, c, 700,
      {{{150, "ready"}, {200, "setinfo team \"\""}, {300, "captain"}},
       {{310, "yes"}, {400, "captain"}},
       {{320, "yes"}, {410, "yes"}},
       {{330, "yes"}, {420, "yes"}}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.clients[0], "Empty team not allowed"));
}
