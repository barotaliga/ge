// engine/server/chat: who reads a say or a say_team and with which prefix: a player's say to
// everyone, team chat to the team; a spectator's team chat to the spectators, a say muted to the
// players during a match when sv_spectalk is 0; a player's team report to the spectators who follow
// the team, by sv_sayteam_to_spec and the phase; a line typed by hand stays private.
//
// ge:summary Test: the chat route: say, say_team, spectators, sv_spectalk, sv_sayteam_to_spec
// ge:keywords test chat chat_route sv_spectalk sv_sayteam_to_spec spectator-chat team-report
// ge:skill net testing
#include <array>

#include "check.hpp"
#include "console/cvar.hpp"
#include "server/chat.hpp"

using namespace ge::server;

namespace {

// Two red players (0, 1), a blue one (2), a spectator (3) following seat 0.
std::array<ChatSeat, 4> room() {
  return {{{true, false, "ann", "red", -1},
           {true, false, "bo", "red", -1},
           {true, false, "cy", "blue", -1},
           {true, true, "dee", "", 0}}};
}

}  // namespace

TEST(players_talk_to_everyone_or_their_team) {
  const auto seats = room();
  const ChatLine all = chat_route(seats, 0, false, false, true, {}, "hi");
  CHECK(all.to == 0xFu && all.text == "ann: hi\n");
  const ChatLine team = chat_route(seats, 0, true, false, true, {}, "go");
  CHECK(team.to == 0x3u && team.text == "(ann): go\n");  // typed by hand: no spectator
  CHECK(chat_route(seats, 9, false, false, true, {}, "x").to == 0);
}

TEST(spectators_are_muted_to_players_only_in_a_match_with_spectalk_off) {
  const auto seats = room();
  CHECK(chat_route(seats, 3, true, false, false, {}, "psst").text == "[SPEC] dee: psst\n");
  CHECK(chat_route(seats, 3, true, false, false, {}, "psst").to == 0x8u);
  const ChatSettings off{false, 1};
  CHECK(chat_route(seats, 3, false, false, false, off, "gl").to == 0xFu);  // before the match
  const ChatLine muted = chat_route(seats, 3, false, false, true, off, "nice");
  CHECK(muted.to == 0x8u && muted.text == "[SPEC] dee: nice\n");
  CHECK(chat_route(seats, 3, false, false, true, {}, "nice").to == 0xFu);
}

TEST(team_reports_reach_the_spectators_following_the_team) {
  auto seats = room();
  CHECK(chat_route(seats, 0, true, true, true, {}, "lost quad").to == 0xBu);
  CHECK(chat_route(seats, 2, true, true, true, {}, "took ra").to == 0x4u);    // blue: not followed
  CHECK(chat_route(seats, 0, true, true, false, {true, 1}, "r").to == 0x3u);  // 1: in a match
  CHECK(chat_route(seats, 0, true, true, false, {true, 2}, "r").to == 0xBu);  // 2: outside one
  CHECK(chat_route(seats, 0, true, true, true, {true, 2}, "r").to == 0x3u);
  CHECK(chat_route(seats, 0, true, true, true, {true, 3}, "r").to == 0xBu);  // 3: always
  CHECK(chat_route(seats, 0, true, true, true, {true, 0}, "r").to == 0x3u);  // 0: never
  seats[3].follows = -1;  // following nobody: the spectator's own team
  seats[3].team = "blue";
  CHECK(chat_route(seats, 2, true, true, true, {}, "r").to == 0xCu);
  CHECK(chat_route(seats, 0, true, true, true, {}, "r").to == 0x3u);
}

TEST(the_settings_are_read_from_the_console) {
  ge::console::Cvars cvars;
  CHECK(chat_settings(cvars).spectalk && chat_settings(cvars).sayteam_to_spec == 1);
  add_chat_settings(cvars);
  add_chat_settings(cvars);  // twice: once
  cvars.set("sv_spectalk", "0", ge::console::Source::code);
  cvars.set("sv_sayteam_to_spec", "3", ge::console::Source::code);
  CHECK(!chat_settings(cvars).spectalk && chat_settings(cvars).sayteam_to_spec == 3);
}
