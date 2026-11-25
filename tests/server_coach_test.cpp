// Tests: a team's coach (engine/server/coach.hpp). A spectator asks in the warmup, the players
// elect him by their share, one coach a team; the refusals say why; the candidate withdraws, the
// election times out and the candidate waits; a coach steps down, leaves or takes a seat; an
// administrator names and removes one; a coach's camera stays on his team.
//
// ge:summary Tests: coaches elected per team, refused, withdrawn, timed out, removed, followed
// ge:keywords test coach coaches uncoach coach-election coach_next_follow
// ge:skill net testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "server/coach.hpp"

namespace sv = ge::server;

namespace {

struct Room {
  sv::Lobby l;
  sv::Teams t;
  sv::Coaches c;
  std::array<std::string, sv::kLobbySeats> names{};
  double now = 1;
  Room() {
    t.settings.names = {"Hometeam", "Visitors"};
    sit(0, "ann", sv::Team::red);
    sit(1, "bo", sv::Team::red);
    sit(2, "cy", sv::Team::blue);
    sit(3, "di", sv::Team::blue);
    names[8] = "eve";  // spectators
    names[9] = "fay";
  }
  void sit(int slot, const std::string& name, sv::Team team) {
    names[static_cast<size_t>(slot)] = name;
    sv::lobby_join(l, slot, name, false, 0);
    l.seats[static_cast<size_t>(slot)].team = team;
    sv::teams_tick(t, l);
  }
  bool say(int slot, const std::string& line, bool admin = false, bool vote_open = false) {
    sv::CoachCaller who;
    who.slot = slot;
    who.admin = admin;
    who.spectator = !l.seats[static_cast<size_t>(slot)].used;
    who.vote_open = vote_open;
    who.names = &names;
    return sv::coach_command(c, l, t, who, line, true, now);
  }
  void tick() { sv::coach_tick(c, l, t, names, true, now); }
  bool heard(const std::string& text) const {
    return std::any_of(c.said.begin(), c.said.end(),
                       [&](const std::string& s) { return s.find(text) != std::string::npos; });
  }
};

}  // namespace

TEST(the_players_elect_a_spectator_to_coach_a_team) {
  Room r;
  CHECK(r.say(8, "coach hometeam"));
  CHECK(r.c.candidate == 8 && r.heard("eve asks to coach 'Hometeam'. Players, type yes to agree "
                                      "(3 needed)."));
  CHECK(sv::coach_needed(r.c, r.l) == 3);  // 51 % of 4, rounded up
  CHECK(r.say(0, "yes") && r.say(2, "yes"));
  CHECK(r.say(2, "no") && r.heard("cy takes back the yes."));
  CHECK(r.say(2, "yes") && sv::coach_team(r.c, 8) == sv::Team::none);
  CHECK(r.say(3, "yes"));
  CHECK(sv::coach_team(r.c, 8) == sv::Team::red && r.heard("eve now coaches 'Hometeam'."));
  CHECK(r.c.candidate == -1 && !r.say(0, "yes"));  // no election: not the coaches' word
}

TEST(a_coach_is_refused_with_the_reason) {
  Room r;
  CHECK(r.say(0, "coach red") && r.heard("Only a spectator can coach a team."));
  CHECK(r.say(8, "coach") && r.heard("Say which team you want to coach: coach Hometeam or coach "
                                     "Visitors."));
  CHECK(r.say(8, "coach red", false, true) && r.heard("Another vote is under way."));
  r.c.coach[0] = 9;
  CHECK(r.say(8, "coach a") && r.heard("'Hometeam' already has a coach, fay."));
  r.l.phase = sv::LobbyPhase::match;
  CHECK(r.say(8, "coach blue") && r.heard("A coach is chosen in the warmup"));
  r.l.phase = sv::LobbyPhase::warmup;
  sv::lobby_leave(r.l, 3, 0);
  sv::lobby_leave(r.l, 2, 0);
  CHECK(r.say(8, "coach blue") && r.heard("at least 3 players"));
  sv::Coaches off;
  sv::CoachCaller who{8, false, true, false, &r.names};
  CHECK(sv::coach_command(off, r.l, r.t, who, "coach red", false, 1));
  CHECK(off.said.back() == "Coaches are for team matches only.");
}

TEST(an_election_is_withdrawn_or_times_out_and_its_candidate_waits) {
  Room r;
  r.say(8, "coach red");
  CHECK(r.say(9, "coach blue") && r.heard("Another vote is under way."));
  CHECK(r.say(8, "coach") && r.heard("eve withdrew from coaching 'Hometeam'."));
  r.now = 11;
  CHECK(r.say(8, "coach red") && r.heard("Wait 20 seconds."));
  r.say(9, "coach blue");
  r.now = 71;
  r.tick();
  CHECK(r.heard("The coach election for fay timed out.") && r.c.candidate == -1);
  r.say(8, "coach red");  // eve's wait is over
  CHECK(r.c.candidate == 8);
  sv::coach_match_began(r.c, r.t);
  CHECK(r.c.candidate == -1 && r.heard("the match has started"));
}

TEST(a_coach_steps_down_leaves_or_takes_a_seat) {
  Room r;
  r.c.coach = {8, 9};
  CHECK(r.say(8, "coach") && r.heard("eve no longer coaches 'Hometeam'."));
  r.names[9] = "";
  r.tick();
  CHECK(r.c.coach[1] == -1 && r.heard("The coach of 'Visitors' left."));
  r.c.coach[0] = 8;
  r.sit(8, "eve", sv::Team::red);
  r.tick();
  CHECK(r.c.coach[0] == -1 && r.heard("eve plays now and no longer coaches."));
  r.c.coach[0] = 9;
  sv::coach_tick(r.c, r.l, r.t, r.names, false, r.now);  // not a team match: none
  CHECK(r.c.coach[0] == -1);
}

TEST(an_administrator_names_and_removes_a_coach) {
  Room r;
  CHECK(r.say(0, "coach blue fay", true) && sv::coach_team(r.c, 9) == sv::Team::blue);
  CHECK(r.say(0, "uncoach blue") && r.heard("Only the referee can take a coach off."));
  CHECK(r.say(0, "uncoach blue", true) && r.c.coach[1] == -1);
  CHECK(r.say(9, "coaches") && r.heard("'Visitors': no coach"));
}

TEST(a_coach_follows_only_his_teams_players) {
  Room r;
  r.c.coach[1] = 8;  // eve coaches blue: cy (2) and di (3)
  CHECK(sv::coach_may_follow(r.c, r.l, 8, 2) && !sv::coach_may_follow(r.c, r.l, 8, 0));
  CHECK(sv::coach_may_follow(r.c, r.l, 9, 0));  // a plain spectator follows anyone
  CHECK(sv::coach_next_follow(r.c, r.l, 8, 0) == 2);
  CHECK(sv::coach_next_follow(r.c, r.l, 8, 2) == 3);
  CHECK(sv::coach_next_follow(r.c, r.l, 8, 3) == 2);  // wraps
  sv::lobby_leave(r.l, 2, 0);
  sv::lobby_leave(r.l, 3, 0);
  CHECK(sv::coach_next_follow(r.c, r.l, 8, 0) == -1);
}
