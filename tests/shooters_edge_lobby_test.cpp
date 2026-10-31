// Tests: the lobby before the match (server/lobby.hpp) and on the listen server (client/
// lobby_room.hpp, lobby_console.hpp): seats join the smaller team, ready and break, a countdown
// that runs only while everyone is ready and stops on a break, the leader's start, the words a
// console line or a client sends; then the listen server on the built-in arena with three bots:
// warmup with the limits held off, the bots ready as they join, the player ready, the countdown,
// the map restarted for the match with every frag gone and every seat back on its team, and a map
// change returning to warmup. The lobby panel drawn at two viewports.
//
// ge:summary Tests: the lobby: teams, ready and break, the countdown, warmup on the listen server
// ge:keywords test lobby warmup countdown ready break team leader addbot match-restart panel
// ge:skill shooter testing
#include <memory>
#include <string>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/server/lobby.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace {

namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

bool said(const sv::Lobby& l, const std::string& part) {
  for (const std::string& s : l.said)
    if (s.find(part) != std::string::npos) return true;
  return false;
}

TEST(seats_join_the_smaller_team_and_move_on_asking) {
  sv::Lobby l;
  sv::lobby_join(l, 0, "ann", false, 0);
  sv::lobby_join(l, 1, "bob", false, 0);
  sv::lobby_join(l, 2, "cy", false, 0);
  CHECK(l.seats[0].team == sv::Team::red);
  CHECK(l.seats[1].team == sv::Team::blue);
  CHECK(l.seats[2].team == sv::Team::red);
  CHECK(sv::lobby_command(l, 2, "team switch", 0));
  CHECK(l.seats[2].team == sv::Team::blue);
  CHECK(sv::lobby_command(l, 2, "team blue", 0));
  CHECK(said(l, "already so"));
  CHECK(sv::lobby_command(l, 2, "team green", 0));
  CHECK(said(l, "no such team"));
  CHECK(sv::lobby_team(l, 1, sv::Team::none, 0) == sv::LobbyWhy::ok);  // auto: to the smaller
  CHECK(l.seats[1].team == sv::Team::red);
  CHECK(!sv::lobby_command(l, 0, "say hello", 0));  // not the lobby's word
}

TEST(everyone_ready_starts_the_countdown_and_a_break_stops_it) {
  sv::Lobby l;
  l.settings.countdown_s = 5;
  sv::lobby_join(l, 0, "ann", false, 0);
  sv::lobby_join(l, 1, "bot1", true, 0);  // a bot is ready as it joins
  CHECK(l.seats[1].ready);
  CHECK(l.phase == sv::LobbyPhase::warmup);
  CHECK(sv::lobby_command(l, 0, "ready", 1));
  CHECK(l.phase == sv::LobbyPhase::countdown);
  CHECK(sv::lobby_seconds_left(l, 1) == 5);
  CHECK(sv::lobby_seconds_left(l, 3.2) == 3);
  CHECK(!sv::lobby_due(l, 4.9));
  CHECK(sv::lobby_command(l, 0, "break", 4.9));
  CHECK(l.phase == sv::LobbyPhase::warmup);
  CHECK(l.stopped == 1);
  CHECK(said(l, "took a break"));
  CHECK(sv::lobby_command(l, 0, "toggleready", 5));
  CHECK(l.phase == sv::LobbyPhase::countdown);
  // A player joining unready stops it; ready again starts it again.
  sv::lobby_join(l, 2, "cy", false, 6);
  CHECK(l.phase == sv::LobbyPhase::warmup);
  sv::lobby_ready(l, 2, true, 7);
  CHECK(l.phase == sv::LobbyPhase::countdown);
  CHECK(!sv::lobby_due(l, 11.9));
  CHECK(sv::lobby_due(l, 12));
  CHECK(!sv::lobby_due(l, 12.1));  // once
  CHECK(l.phase == sv::LobbyPhase::match);
  CHECK(l.matches == 1);
  CHECK(sv::lobby_ready(l, 0, false, 13) == sv::LobbyWhy::in_match);
  CHECK(sv::lobby_command(l, 0, "team blue", 13));
  CHECK(said(l, "the match is on"));
  sv::lobby_back(l);
  CHECK(l.phase == sv::LobbyPhase::warmup);
  CHECK(!l.seats[0].ready);
  CHECK(l.seats[1].ready);  // the bot
}

TEST(only_the_leader_starts_and_too_few_players_wait) {
  sv::Lobby l;
  l.settings.min_players = 2;
  sv::lobby_join(l, 0, "ann", false, 0);
  sv::lobby_ready(l, 0, true, 0);
  CHECK(l.phase == sv::LobbyPhase::warmup);  // one of two
  sv::lobby_join(l, 3, "dee", false, 0);
  CHECK(sv::lobby_leader(l) == 0);
  CHECK(sv::lobby_start(l, 3, 0) == sv::LobbyWhy::not_leader);
  CHECK(sv::lobby_start(l, 0, 0) == sv::LobbyWhy::ok);  // whoever is ready
  CHECK(l.phase == sv::LobbyPhase::countdown);
  sv::lobby_leave(l, 3, 1);  // under min_players
  CHECK(l.phase == sv::LobbyPhase::warmup);
  sv::lobby_leave(l, 0, 1);
  CHECK(sv::lobby_leader(l) == -1);
  CHECK(sv::lobby_ready(l, 0, true, 1) == sv::LobbyWhy::no_seat);
}

int frags_of(const cl::Listen& l, int slot) {
  return static_cast<int>(
      ge::progs::as_float(*ge::progs::entity_field(l.server->q.vm, slot + 1, ge::progs::kFFrags)));
}

TEST(the_listen_server_warms_up_then_restarts_the_map_for_the_match) {
  ge::progs::Progs progs;
  std::string error;
  CHECK_MSG(cl::baseline_progs(progs, error), error);
  cl::Listen l;
  cl::ListenOptions opt;
  opt.console = {"deathmatch 1", "fraglimit 20", "timelimit 10"};
  opt.lobby = true;
  opt.lobby_settings.countdown_s = 2;
  cl::SearchPath none;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(none, "arena", error);
  CHECK_MSG(map != nullptr, error);
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  CHECK(l.room != nullptr);
  CHECK(l.room->lobby.seats[0].used);
  // Warmup holds the limits off.
  CHECK(l.server->cvars.find("fraglimit")->number == 0);
  CHECK(l.server->cvars.find("timelimit")->number == 0);
  for (int i = 0; i < 3; i++) CHECK(cl::listen_lobby(l, "addbot"));
  CHECK(sv::lobby_seated(l.room->lobby) == 4);
  CHECK(sv::lobby_ready_count(l.room->lobby) == 3);
  CHECK(l.server->clients[3].active);
  CHECK(sv::info_value(l.server->clients[1].userinfo, "team") == "blue");
  CHECK(sv::info_value(l.server->clients[1].userinfo, "topcolor") == "13");
  // Free roam: the bots run their commands with the player's; nobody leaves the warmup alone.
  sh::MoveCmd idle;
  const sh::Vec3f bot_at = l.server->sv.edicts[2].origin;
  for (int f = 0; f < 600; f++) cl::listen_frame(l, idle);
  CHECK(l.room->lobby.phase == sv::LobbyPhase::warmup);
  const sh::Vec3f bot_now = l.server->sv.edicts[2].origin;
  CHECK(bot_now.x != bot_at.x || bot_now.y != bot_at.y);
  // A frag in the warmup does not count: give one by hand.
  *ge::progs::entity_field(l.server->q.vm, 2, ge::progs::kFFrags) = ge::progs::as_word(3);
  CHECK(frags_of(l, 1) == 3);
  CHECK(cl::listen_lobby(l, "team switch"));
  CHECK(l.room->lobby.seats[0].team == sv::Team::blue);
  CHECK(cl::listen_lobby(l, "ready"));
  CHECK(l.room->lobby.phase == sv::LobbyPhase::countdown);
  CHECK(cl::listen_lobby(l, "break"));
  CHECK(l.room->lobby.phase == sv::LobbyPhase::warmup);
  CHECK(cl::listen_lobby(l, "toggleready"));
  const sv::QcServer* warm = l.server;
  int ran = 0;
  while (!l.restarted && ran < 400) {
    cl::listen_frame(l, idle);
    ran++;
  }
  CHECK(l.restarted);
  CHECK(l.server != warm);
  CHECK(l.room->lobby.phase == sv::LobbyPhase::match);
  CHECK(ran >= 150 && ran <= 160);  // 2 s of 13 ms frames
  CHECK(frags_of(l, 1) == 0);
  for (int s = 1; s <= 4; s++) CHECK(l.server->sv.edicts[static_cast<size_t>(s)].health == 100);
  for (int s = 0; s < 4; s++) CHECK(l.server->clients[s].active);
  CHECK(sv::info_value(l.server->clients[0].userinfo, "team") == "blue");
  CHECK(l.server->cvars.find("fraglimit")->number == 20);
  CHECK(cl::listen_lobby(l, "ready"));  // refused in the match, but a lobby word
  bool refused = false;
  for (const std::string& p : l.prints)
    refused = refused || p.find("the match is on") != std::string::npos;
  CHECK(refused);
  // The next map: warmup again, the player unready, the bots still seated.
  std::unique_ptr<cl::LoadedMap> again = cl::load_named_map(none, "arena", error);
  CHECK_MSG(cl::listen_change(l, std::move(again), error), error);
  CHECK(l.room->lobby.phase == sv::LobbyPhase::warmup);
  CHECK(!l.room->lobby.seats[0].ready);
  CHECK(l.server->clients[3].active);
  CHECK(cl::listen_lobby(l, "removebot"));
  CHECK(!l.server->clients[3].active);
  CHECK(sv::lobby_seated(l.room->lobby) == 3);
}

TEST(the_lobby_words_are_console_commands) {
  ge::console::Console con;
  cl::Listen l;
  cl::add_lobby_commands(con, l);
  CHECK(l.opt.lobby);  // sv_lobby's fallback
  con.run_now("sv_countdown 7");
  CHECK(l.opt.lobby_settings.countdown_s == 7);
  con.run_now("ready");  // no lobby yet: said so, not an unknown command
  CHECK(!l.prints.empty() && l.prints.back().find("no lobby runs") != std::string::npos);
}

TEST(the_panel_draws_at_both_viewports_and_not_in_the_match) {
  shooters_edge::hud::LobbyView v;
  v.rows = {{"player", 1, false, false, true, true},
            {"bot1 (average)", 2, true, true, false, false}};
  for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
    ge::ui::DrawList dl;
    dl.begin(w, h);
    shooters_edge::hud::draw_lobby(dl, v, w, h);
    CHECK(dl.size() > 10);
  }
  v.phase = shooters_edge::hud::LobbyShown::match;
  ge::ui::DrawList dl;
  dl.begin(1280, 720);
  shooters_edge::hud::draw_lobby(dl, v, 1280, 720);
  CHECK(dl.size() == 0);
}

}  // namespace
