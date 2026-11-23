// Tests: the two teams and their captains (engine/server/captains.hpp). The first on a team is its
// captain and the next one takes over when he leaves; a captain locks, renames, invites, picks and
// removes; joining a locked or full team is refused; an administrator acts on either team; the
// shuffle deals the seats again.
//
// ge:summary Tests: captains kept, teams locked, renamed, invited to, picked and shuffled
// ge:keywords test captain lockteam teamname invite accept pick kickplayer shuffle
// ge:skill net testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "server/captains.hpp"

namespace sv = ge::server;

namespace {

struct Room {
  sv::Lobby l;
  sv::Teams t;
  std::array<std::string, sv::kLobbySeats> names{};
  Room() { t.settings.names = {"Hometeam", "Visitors"}; }
  void sit(int slot, const std::string& name, sv::Team team) {
    names[static_cast<size_t>(slot)] = name;
    sv::lobby_join(l, slot, name, false, 0);
    l.seats[static_cast<size_t>(slot)].team = team;
    sv::teams_tick(t, l);
  }
  bool say(int slot, const std::string& line, bool admin = false) {
    const sv::TeamsCaller who{slot, admin, &names};
    return sv::teams_command(t, l, who, line, 1);
  }
  bool heard(const std::string& text) const {
    return std::any_of(t.said.begin(), t.said.end(),
                       [&](const std::string& s) { return s.find(text) != std::string::npos; });
  }
};

}  // namespace

TEST(the_first_on_a_team_is_its_captain_and_the_next_takes_over) {
  Room r;
  r.sit(0, "ana", sv::Team::red);
  r.sit(1, "ben", sv::Team::red);
  r.sit(2, "cid", sv::Team::blue);
  CHECK(r.t.side[0].captain == 0 && r.t.side[1].captain == 2);
  CHECK(r.heard("ana became captain of 'Hometeam'"));
  sv::lobby_leave(r.l, 0, 1);
  sv::teams_tick(r.t, r.l);
  CHECK(r.t.side[0].captain == 1);
  CHECK(r.heard("ben became captain of 'Hometeam'"));
  CHECK(r.say(1, "captain"));
  CHECK(r.t.said.back() == "You are the captain of team 'Hometeam'.");
  CHECK(r.say(2, "captains"));
  CHECK(r.t.said.back() == "Team 'Visitors' captain: cid");
}

TEST(a_locked_or_full_team_takes_nobody_by_team) {
  Room r;
  r.t.settings.max_per_team = 2;
  r.sit(0, "ana", sv::Team::red);
  r.sit(1, "ben", sv::Team::blue);
  r.sit(2, "cid", sv::Team::blue);
  CHECK(r.say(1, "lock"));
  CHECK(r.t.side[1].locked);
  CHECK(r.say(0, "team 2"));
  CHECK(r.t.said.back() == "Team 'Visitors' is locked. Ask its captain for an invite.");
  CHECK(r.l.seats[0].team == sv::Team::red);
  CHECK(r.say(2, "lock"));  // not the captain
  CHECK(r.t.said.back() == "Only the captain of 'Visitors' can do that.");
  CHECK(r.say(1, "unlock"));
  CHECK(r.say(0, "join b"));
  CHECK(r.t.said.back() == "Team 'Visitors' is full.");
  CHECK(r.say(2, "team hometeam"));  // by its name
  CHECK(r.l.seats[2].team == sv::Team::red);
  CHECK(r.heard("cid joined team 'Hometeam'"));
}

TEST(a_captain_renames_invites_picks_and_removes) {
  Room r;
  r.sit(0, "ana", sv::Team::red);
  r.sit(1, "ben", sv::Team::red);
  r.names[5] = "dee";  // watching, not seated
  CHECK(r.say(0, "teamname Wolves"));
  CHECK(r.t.said.back() == "Team 'Hometeam' renamed to 'Wolves'.");
  CHECK(r.say(0, "teamname \"x\""));
  CHECK(r.t.settings.names[0] == "Wolves");
  CHECK(r.say(0, "invite de"));
  CHECK(r.t.invited[5] == static_cast<int8_t>(sv::Team::red));
  CHECK(r.say(5, "accept"));
  CHECK(r.t.moves.size() == 1 && r.t.moves[0].slot == 5 && r.t.moves[0].team == sv::Team::red);
  r.t.moves.clear();
  r.t.settings.allow_pick = true;
  r.names[6] = "eve";
  CHECK(r.say(0, "pick eve"));
  CHECK(r.t.moves.size() == 1 && r.t.moves[0].slot == 6);
  CHECK(r.heard("ana picked eve for team 'Wolves'."));
  r.t.moves.clear();
  CHECK(r.say(0, "remove ben"));
  CHECK(r.t.moves.size() == 1 && r.t.moves[0].slot == 1 && r.t.moves[0].team == sv::Team::none);
  CHECK(!r.say(0, "say hello"));
}

TEST(a_referee_acts_on_either_team_and_the_shuffle_deals_again) {
  Room r;
  for (int i = 0; i < 4; i++) r.sit(i, std::string(1, char('a' + i)), sv::Team::red);
  r.l.seats[0].ready = true;
  CHECK(r.say(7, "lockteam blue", true));
  CHECK(r.t.said.back() == "An empty team cannot be locked.");
  CHECK(r.say(7, "captain red c", true));
  CHECK(r.t.side[0].captain == 2);
  sv::teams_shuffle(r.t, r.l, 7);
  int red = 0, blue = 0;
  for (int i = 0; i < 4; i++)
    (r.l.seats[static_cast<size_t>(i)].team == sv::Team::red ? red : blue)++;
  CHECK(red == 2 && blue == 2);
  CHECK(!r.l.seats[0].ready);
  CHECK(r.t.side[0].captain >= 0 && r.t.side[1].captain >= 0);
}
