// engine/server/aids: player aids. An aid's value from on, off or a share; an order to grant read
// from a console line (and refused in a player's words); the vote's key and its value split; the
// players list's aids column (allowed and used); the line a player who joins is told, only of
// grants.
//
// ge:summary Test: player aids: values, orders, votes' words, the players list, the join line
// ge:keywords test player-aids aid_parse aid_value users_text aids_join_line aid_vote_split
// ge:skill net testing
#include <string>
#include <vector>

#include "check.hpp"
#include "server/aids.hpp"

using namespace ge::server;

namespace {

const AidKind kKinds[] = {
    {"speedjump", "easy jump", "*speedjump", "0.8", true},
    {"aimhelp", "aim help", "*aimhelp", "1", false},
};

bool has(const std::string& text, const std::string& what) {
  return text.find(what) != std::string::npos;
}

std::string leeway(const AidKind& kind, const void*) {
  return std::string(kind.name) == "aimhelp" ? "24 across, 48 up and down" : std::string();
}

}  // namespace

TEST(an_aids_value_is_on_off_or_a_share_for_a_share_aid) {
  std::string v, error;
  CHECK(aid_value(kKinds[0], "on", v, error) && v == "0.8");
  CHECK(aid_value(kKinds[0], "off", v, error) && v.empty());
  CHECK(aid_value(kKinds[0], "0.456", v, error) && v == "0.46");
  CHECK(aid_value(kKinds[0], "0", v, error) && v.empty());  // a share of nothing is off
  CHECK(!aid_value(kKinds[0], "2", v, error));
  CHECK(has(error, "or given a share from 0 to 1"));
  CHECK(aid_value(kKinds[1], "on", v, error) && v == "1");
  error.clear();
  CHECK(!aid_value(kKinds[1], "0.5", v, error));
  CHECK(error == "aim help is turned \"on\" or \"off\", not \"0.5\".");
  CHECK(aid_share_text(1) == "1" && aid_share_text(0.5f) == "0.5");
}

TEST(an_order_names_the_aid_the_player_and_the_value) {
  AidOrder o;
  std::string error;
  CHECK(!aid_parse(kKinds, "fraglimit 10", o, error));  // not an aid's word
  CHECK(aid_parse(kKinds, "aimhelp", o, error) && o.kind == &kKinds[1] && o.who.empty());
  CHECK(aid_parse(kKinds, "aimhelp 3", o, error) && o.who == "3" && o.value == "1");
  CHECK(aid_parse(kKinds, "speedjump  ann  0.5", o, error) && o.value == "0.5");
  CHECK(error.empty());
  CHECK(aid_parse(kKinds, "aimhelp ann on now", o, error));
  CHECK(error == "Type \"aimhelp <player> on\" or \"aimhelp <player> off\".");
}

TEST(a_vote_is_keyed_by_aid_and_user_id) {
  CHECK(aid_vote_key(kKinds[1], 7) == "aimhelp:7");
  std::string who, value, error;
  CHECK(aid_vote_split(kKinds[1], "Big Ann on", who, value, error));
  CHECK(who == "Big Ann" && value == "1");
  CHECK(!aid_vote_split(kKinds[1], "ann", who, value, error));
  CHECK(has(error, "vote aimhelp <player> on"));
  CHECK(aid_vote_word(kKinds[1], "1") == "on" && aid_vote_word(kKinds[1], "") == "off");
  CHECK(aid_vote_word(kKinds[0], "0.5") == "0.5");
}

TEST(the_players_list_shows_each_seats_aids_allowed_and_used) {
  std::vector<UserRow> rows(2);
  rows[0].userid = 12;
  rows[0].frags = 3;
  rows[0].name = "ann";
  rows[0].aids.push_back(
      {&kKinds[1], "granted, 24 across, 48 up and down", "2 shots helped", true});
  rows[0].aids.push_back({&kKinds[0], "", "cl_speedjump 1", false});
  rows[1].userid = 13;
  rows[1].name = "bot1";
  rows[1].bot = true;
  const std::string t = users_text(rows);
  CHECK(has(t,
            "    12     3 ann              aim help (granted, 24 across, 48 up and down, 2 "
            "shots helped), easy jump (not allowed, cl_speedjump 1)\n"));
  CHECK(has(t, "    13     0 bot1 (bot)       -\n"));
  CHECK(has(t, "2 total users\n"));
  // Who joins is told the grants alone (the easy jump ann asks for is not one).
  CHECK(aids_join_line(rows, &leeway) ==
        "Player aids on this server: ann has aim help (24 across, 48 up and down).\n");
  rows[0].aids.clear();
  CHECK(aids_join_line(rows, &leeway).empty());
}
