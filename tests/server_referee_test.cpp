// Tests: the match's referee (engine/server/referee.hpp). The password makes a player a referee
// and the same word again gives it up; a wrong one is refused and logged; a referee ends the
// match, sets the time limit, changes the map, kicks, bans and mutes, and is never kicked himself.
//
// ge:summary Tests: the referee's password and acts, mutes and their limits
// ge:keywords test referee admin endmatch changemap kickban mute
// ge:skill net testing
#include <string>

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
