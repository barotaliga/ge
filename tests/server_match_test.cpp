// engine/server/match: the lobby before the match on its own, no game under it. Seats take the
// smaller team; everyone ready starts the countdown and a break stops it; only the leader starts
// it early; the countdown runs out once into the match and the map change brings warmup back;
// every refusal names what to do instead; a session of acts on the server's clock replays to the
// same phases and lines; with `humans` bots alone wait, and a player leads before a bot.
//
// ge:summary Test: the engine's lobby before the match: teams, ready, countdown, match, warmup
// ge:keywords test lobby match-phases warmup countdown ready break leader lobby_due lobby_back
// ge:keywords humans bots-alone
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "server/match.hpp"

using namespace ge::server;

namespace {

bool said(const Lobby& l, const std::string& part) {
  for (const std::string& line : l.said)
    if (line.find(part) != std::string::npos) return true;
  return false;
}

// Four seats, two of them bots, through every phase; the phases and lines folded into a digest.
uint64_t session() {
  Lobby l;
  l.settings.min_players = 2;
  l.settings.countdown_s = 3;
  uint64_t h = 1469598103934665603ull;
  const auto fold = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  double now = 0;
  lobby_join(l, 0, "host", false, now);
  lobby_join(l, 1, "bot1", true, now);
  lobby_join(l, 2, "bot2", true, now);
  lobby_join(l, 3, "guest", false, now);
  for (int step = 0; step < 400; step++) {
    now = step * 0.05;
    if (step == 20) lobby_command(l, 3, "ready", now);
    if (step == 30) lobby_command(l, 0, "ready", now);
    if (step == 40) lobby_command(l, 3, "break", now);
    if (step == 50) lobby_command(l, 3, "team switch", now);
    if (step == 60) lobby_command(l, 3, "ready", now);
    if (step == 200) lobby_back(l);
    if (step == 210) lobby_command(l, 0, "start", now);
    lobby_due(l, now);
    fold(static_cast<uint64_t>(l.phase));
    fold(static_cast<uint64_t>(lobby_seconds_left(l, now)));
  }
  for (const std::string& line : l.said)
    for (char ch : line) fold(static_cast<uint8_t>(ch));
  fold(static_cast<uint64_t>(l.matches));
  return h;
}

}  // namespace

TEST(server_match_seats_take_the_smaller_team) {
  Lobby l;
  lobby_join(l, 0, "host", false, 0);
  lobby_join(l, 1, "bot", true, 0);
  lobby_join(l, 2, "guest", false, 0);
  CHECK(l.seats[0].team == Team::red);
  CHECK(l.seats[1].team == Team::blue);
  CHECK(l.seats[2].team == Team::red);
  CHECK(l.seats[1].ready);  // a bot is ready as it joins
  CHECK(lobby_smaller_team(l) == Team::blue);
  CHECK(lobby_team(l, 2, Team::blue, 0) == LobbyWhy::ok);
  CHECK(lobby_team(l, 2, Team::blue, 0) == LobbyWhy::same);
  CHECK(lobby_seated(l) == 3);
  CHECK(lobby_leader(l) == 0);
}

TEST(server_match_ready_counts_down_into_the_match_and_back) {
  Lobby l;
  l.settings.countdown_s = 2;
  lobby_join(l, 0, "host", false, 0);
  lobby_join(l, 1, "bot", true, 0);
  CHECK(l.phase == LobbyPhase::warmup);
  CHECK(lobby_ready(l, 0, true, 1) == LobbyWhy::ok);
  CHECK(l.phase == LobbyPhase::countdown);
  CHECK(lobby_seconds_left(l, 1.5) == 2);
  CHECK(lobby_command(l, 0, "break", 1.5));  // a break stops it
  CHECK(l.phase == LobbyPhase::warmup);
  CHECK(l.stopped == 1);
  CHECK(lobby_command(l, 0, "ready", 2));
  CHECK(!lobby_due(l, 3.9));
  CHECK(lobby_due(l, 4.0));
  CHECK(!lobby_due(l, 4.1));  // once
  CHECK(l.phase == LobbyPhase::match);
  CHECK(l.matches == 1);
  CHECK(lobby_ready(l, 0, false, 5) == LobbyWhy::in_match);
  CHECK(said(l, "next: play the match"));
  lobby_back(l);
  CHECK(l.phase == LobbyPhase::warmup);
  CHECK(!l.seats[0].ready);
  CHECK(l.seats[1].ready);
}

TEST(server_match_only_the_leader_starts_and_too_few_wait) {
  Lobby l;
  l.settings.min_players = 3;
  lobby_join(l, 0, "host", false, 0);
  lobby_join(l, 1, "guest", false, 0);
  CHECK(lobby_ready(l, 0, true, 0) == LobbyWhy::ok);
  CHECK(lobby_ready(l, 1, true, 0) == LobbyWhy::ok);
  CHECK(l.phase == LobbyPhase::warmup);  // two of the three it waits for
  CHECK(lobby_start(l, 1, 0) == LobbyWhy::not_leader);
  CHECK(said(l, "next: `ready`"));
  CHECK(lobby_start(l, 0, 0) == LobbyWhy::ok);
  CHECK(l.phase == LobbyPhase::countdown);
  CHECK(!lobby_command(l, 0, "fraglimit 20", 0));  // not the lobby's
  CHECK(lobby_command(l, 0, "team purple", 0));
  CHECK(said(l, "next: `team red`"));
}

TEST(server_match_bots_alone_wait_for_a_player_who_leads) {
  Lobby l;
  l.settings.humans = 1;
  lobby_join(l, 1, "bot1", true, 0);
  lobby_join(l, 2, "bot2", true, 0);
  CHECK(l.phase == LobbyPhase::warmup);  // every seat ready, but nobody to play against
  CHECK(lobby_humans(l) == 0);
  lobby_join(l, 3, "guest", false, 1);
  CHECK(lobby_leader(l) == 3);  // the player, not the lowest bot
  CHECK(lobby_ready(l, 3, true, 2) == LobbyWhy::ok);
  CHECK(l.phase == LobbyPhase::countdown);
  lobby_leave(l, 3, 3);
  CHECK(l.phase == LobbyPhase::warmup);
  CHECK(said(l, "no player left"));
  CHECK(lobby_leader(l) == 1);  // bots alone: the lowest seat
}

TEST(server_match_a_session_replays) { CHECK(session() == session()); }
