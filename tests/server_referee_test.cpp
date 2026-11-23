// Tests: the match's referee (engine/server/referee.hpp). The password makes a player a referee
// and the same word again gives it up; a wrong one is refused and logged; a referee ends the
// match, sets the time limit, changes the map, kicks, bans and mutes, and is never kicked himself.
//
// ge:summary Tests: the referee's password and acts, mutes and their limits
// ge:keywords test referee admin endmatch changemap kickban mute
// ge:skill net testing
#include <array>
#include <string>
#include <string_view>

#include "check.hpp"
#include "server/referee.hpp"

namespace sv = ge::server;

namespace {

std::array<std::string, sv::kLobbySeats> names() {
  std::array<std::string, sv::kLobbySeats> n{};
  n[0] = "ana";
  n[1] = "ben";
  n[2] = "cid";
  return n;
}

}  // namespace

TEST(the_password_makes_a_referee_and_a_wrong_one_is_logged) {
  const auto n = names();
  sv::Referee r;
  sv::RefereeCaller ana{0, "", false, &n};
  CHECK(sv::referee_command(r, ana, "admin secret", 1));
  CHECK(r.said.back() == "There is no referee on this server.");
  ana.password = "secret";
  CHECK(sv::referee_command(r, ana, "referee wrong", 2));
  CHECK(r.said.back() == "ana gave a wrong referee password." && r.to.back() == -2);
  CHECK(sv::referee_command(r, ana, "admin secret", 2.5));
  CHECK(r.said.back() == "Wait a second before trying again.");
  CHECK(sv::referee_command(r, ana, "admin secret", 4));
  CHECK(sv::referee_is(r, 0) && r.said.back() == "ana became a referee." && r.to.back() == -1);
  sv::RefereeCaller ben{1, "secret", false, &n};
  CHECK(sv::referee_command(r, ben, "kick ana", 5));
  CHECK(r.said.back() ==
        "Only a referee can do that. Type referee and the password to become one.");
  CHECK(!sv::referee_command(r, ben, "break", 5));  // the lobby's not-ready word outside a match
  CHECK(sv::referee_command(r, ana, "admin", 6));
  CHECK(!sv::referee_is(r, 0));
}

TEST(a_referee_ends_the_match_changes_the_map_and_removes_players) {
  const auto n = names();
  sv::Referee r;
  r.referees = 1;
  sv::RefereeCaller ana{0, "secret", true, &n};
  CHECK(sv::referee_command(r, ana, "break", 1));
  CHECK(r.acts.back().kind == sv::RefereeActKind::end_match);
  CHECK(r.said.back() == "ana has ended the match.");
  CHECK(sv::referee_command(r, ana, "tl 15", 1));
  CHECK(r.acts.back().kind == sv::RefereeActKind::time_limit && r.acts.back().value == 15);
  CHECK(sv::referee_command(r, ana, "changemap q2dm1", 1));
  CHECK(r.acts.back().text == "q2dm1");
  CHECK(sv::referee_command(r, ana, "ban ben 9999", 1));
  CHECK(r.acts.back().kind == sv::RefereeActKind::ban && r.acts.back().slot == 1 &&
        r.acts.back().value == 720);
  CHECK(sv::referee_command(r, ana, "kickban 2", 1));
  CHECK(r.acts.back().slot == 2 && r.acts.back().value == 60);
  CHECK(sv::referee_command(r, ana, "kick ana", 1));
  CHECK(r.said.back() == "A referee cannot be kicked, banned or muted.");
}

TEST(a_muted_player_is_held_back_until_it_runs_out) {
  const auto n = names();
  sv::Referee r;
  r.referees = 1;
  sv::RefereeCaller ana{0, "secret", false, &n};
  CHECK(sv::referee_command(r, ana, "mute ben 90", 10));
  CHECK(r.said.back() == "ben is muted for 60 minutes.");
  CHECK(sv::referee_muted(r, 1, 100));
  CHECK(r.said.back() == "You are muted for 59 minutes more." && r.to.back() == 1);
  CHECK(!sv::referee_muted(r, 1, 10 + 3600));
  CHECK(sv::referee_command(r, ana, "mute ben", 10));
  CHECK(sv::referee_command(r, ana, "unmute ben", 11));
  CHECK(!sv::referee_muted(r, 1, 12));
}

// A game's own words: its login word and spellings, a wait after a wrong try with the seconds
// left, a player named by user id or his exact name, a kick's reason and ban, a map the server
// lacks, and words it does not have left alone.
TEST(a_game_speaks_its_own_words) {
  auto n = names();
  sv::RefereeWords w;
  w.login = {"admin"};
  w.words = {{"forcemap", "changemap"}, {"kick", "kick"}};
  w.break_ends = false;
  w.wait_s = 5;
  w.wait_after_wrong = true;
  w.no_password_is_wrong = true;
  w.exact_names = true;
  w.kick_reason = true;
  w.kick_ban_minutes = 0.5;
  w.became = "{name} gains admins status!";
  w.became_me = "first\nsecond";
  w.wrong = "Access denied...";
  w.wait = "Wait {n} {seconds}!";
  w.none = "NO admins on this server!";
  w.not_one = "You are not an admin";
  w.map_unknown = "Map {map} not available on this server.";
  w.new_map = "{name} forces a map change to {map}!";
  w.no_player = "kick: client {player} not found";
  w.kicked = "{player} was kicked by {name}";
  w.kicked_you = "You were kicked from the server";
  w.kick_said_reason = "[{reason}]";
  const std::array<int, sv::kLobbySeats> ids = {5, 6, 7};
  const auto map_known = [](const void*, std::string_view m) { return m == "dm2"; };
  sv::Referee r;
  sv::RefereeCaller ana{0, "", false, &n, &w, true, &ids, map_known, nullptr};
  CHECK(sv::referee_command(r, ana, "admin 1", 10));
  CHECK(r.said.back() == "Access denied...");
  ana.password = "4321";
  CHECK(sv::referee_command(r, ana, "admin 4321", 11.2));
  CHECK(r.said.back() == "Wait 4 seconds!");
  CHECK(sv::referee_command(r, ana, "admin 4321", 14.6));
  CHECK(r.said.back() == "Wait 1 second!");
  CHECK(sv::referee_command(r, ana, "admin 4321", 16));
  CHECK(sv::referee_is(r, 0) && r.said[r.said.size() - 3] == "ana gains admins status!");
  CHECK(r.said[r.said.size() - 2] == "first" && r.said.back() == "second" && r.to.back() == 0);
  CHECK(!sv::referee_command(r, ana, "tl 20", 16));  // not this game's word
  CHECK(!sv::referee_command(r, ana, "break", 16));
  CHECK(sv::referee_command(r, ana, "forcemap dm9", 16));
  CHECK(r.said.back() == "Map dm9 not available on this server." && r.acts.empty());
  CHECK(sv::referee_command(r, ana, "forcemap dm2", 16));
  CHECK(r.said.back() == "ana forces a map change to dm2!" && r.acts.back().text == "dm2");
  CHECK(sv::referee_command(r, ana, "kick Ben", 16));  // the exact name only
  CHECK(r.said.back() == "kick: client Ben not found");
  CHECK(sv::referee_command(r, ana, "kick 6 too much camping", 16));  // user id 6: ben
  CHECK(r.acts.back().kind == sv::RefereeActKind::kick && r.acts.back().slot == 1);
  CHECK(r.acts.back().value == 0.5 && r.acts.back().text == "too much camping");
  CHECK(r.said[r.said.size() - 3] == "ben was kicked by ana");
  CHECK(r.said[r.said.size() - 2] == "[too much camping]");
  CHECK(r.said.back() == "You were kicked from the server" && r.to.back() == 1);
  sv::RefereeCaller cid{2, "4321", false, &n, &w, false, &ids, nullptr, nullptr};
  CHECK(sv::referee_command(r, cid, "admin 4321", 20));
  CHECK(r.said.back() == "NO admins on this server!");
  CHECK(sv::referee_command(r, cid, "kick ana", 20));
  CHECK(r.said.back() == "You are not an admin");
}
