// engine/server/vote: players' votes on a server's settings for one map. The lists (deny wins,
// an empty allow list allows the rest, `*` patterns); a vote passes with more than half the
// voters and fails once yes can no longer win; a lone player passes at once; time runs out; a
// voter leaving is weighed again; the caller's wait after a failed vote; the game's check refuses
// and rewrites; the map's end puts every change back, newest first, with the first `before`.
//
// ge:summary Test: votes on a server's settings: lists, majority, time, leavers, revert per map
// ge:keywords test vote majority sv_vote_allow sv_vote_deny vote_command vote_map_over
// ge:skill net testing
#include <cstdlib>
#include <map>
#include <string>

#include "check.hpp"
#include "server/vote.hpp"

using namespace ge::server;

namespace {

// A game with numbers in 0..100 as its settings, "fraglimit" 20 and "timelimit" 10 at the start.
struct Fake {
  std::map<std::string, std::string> values = {{"fraglimit", "20"}, {"timelimit", "10"}};
};

bool fake_check(void* ctx, std::string_view name, std::string_view value, VoteCheck& out) {
  auto& f = *static_cast<Fake*>(ctx);
  const auto it = f.values.find(std::string(name));
  if (it == f.values.end()) {
    out.error = std::string(name) + " is no setting here";
    return false;
  }
  const int v = std::atoi(std::string(value).c_str());
  if (v < 0 || v > 100) {
    out.error = std::string(name) + " " + std::string(value) + " is outside 0..100";
    return false;
  }
  out.before = it->second;
  out.value = std::to_string(v);  // the game's own form: "030" becomes "30"
  return true;
}

VoteBook seated(int people) {
  VoteBook b;
  for (int i = 0; i < people; i++) vote_seat(b, i, "p" + std::to_string(i), 0);
  return b;
}

bool said(const VoteBook& b, const std::string& part) {
  for (const std::string& line : b.said)
    if (line.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(the_lists_deny_first_then_allow) {
  CHECK(vote_pattern_match("sv_vote*", "sv_vote_time"));
  CHECK(vote_pattern_match("*password*", "sv_spectatorpassword"));
  CHECK(vote_pattern_match("a*b*c", "axxbyyc"));
  CHECK(!vote_pattern_match("a*b*c", "axxbyy"));
  CHECK(vote_pattern_match("fraglimit", "fraglimit"));
  CHECK(!vote_pattern_match("fraglimit", "fraglimits"));
  VoteSettings s;
  CHECK(vote_permitted(s, "fraglimit") == VoteWhy::ok);
  for (const char* owner : {"rcon_password", "sv_password", "hostname", "sv_maxclients", "sv_vote",
                            "sv_vote_allow", "net_port", "sv_mintic", "sv_admin"})
    CHECK(vote_permitted(s, owner) == VoteWhy::denied);
  s.allow = "fraglimit, timelimit pm_*";
  CHECK(vote_permitted(s, "timelimit") == VoteWhy::ok);
  CHECK(vote_permitted(s, "pm_speedjump") == VoteWhy::ok);
  CHECK(vote_permitted(s, "teamplay") == VoteWhy::not_allowed);
  s.deny = "pm_ktjump";
  CHECK(vote_permitted(s, "pm_ktjump") == VoteWhy::denied);  // deny wins over allow
}

TEST(more_than_half_passes_and_the_change_is_due) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(4);
  CHECK(vote_command(b, t, 1, "vote fraglimit \"030\"", 1));
  CHECK(b.open && b.asked.value == "30" && b.asked.before == "20" && b.asked.key == "fraglimit");
  CHECK(vote_needed(b) == 3);
  CHECK(said(b, "p1 calls fraglimit 30 (now 20): vote yes/no"));
  CHECK(vote_command(b, t, 2, "vote yes", 2));
  CHECK(b.open);  // 2 of 4 is half, not more
  CHECK(vote_command(b, t, 3, "yes", 3));
  CHECK(!b.open && b.passed == 1);
  CHECK(said(b, "Vote passed: fraglimit 30 for this map"));
  const auto due = vote_take(b);
  CHECK(due.size() == 1 && due[0].name == "fraglimit" && due[0].value == "30");
  CHECK(vote_take(b).empty());
}

TEST(half_saying_no_fails_and_the_caller_waits) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(4);
  vote_command(b, t, 0, "vote timelimit 20", 0);
  vote_command(b, t, 1, "vote no", 1);
  CHECK(b.open);
  vote_command(b, t, 2, "vote no", 2);  // 2 no of 4: yes can reach 2 at most, not 3
  CHECK(!b.open && b.failed == 1 && vote_take(b).empty());
  b.said.clear();
  vote_command(b, t, 0, "vote timelimit 15", 10);
  CHECK(!b.open && b.notes.back().why == VoteWhy::waiting && b.notes.back().to == 0);
  CHECK(said(b, "call another in 22 s"));
  vote_command(b, t, 0, "vote timelimit 15", 33);  // the wait is over
  CHECK(b.open);
}

TEST(a_player_alone_passes_at_once) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(1);
  vote_command(b, t, 0, "vote fraglimit 5", 0);
  CHECK(!b.open && b.passed == 1 && vote_take(b).size() == 1);
}

TEST(answers_change_and_repeat_is_refused) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(5);
  vote_command(b, t, 0, "vote fraglimit 50", 0);
  vote_command(b, t, 1, "vote no", 1);
  vote_command(b, t, 1, "vote no", 2);
  CHECK(b.notes.back().why == VoteWhy::same);
  vote_command(b, t, 1, "vote yes", 3);  // a mind changed
  CHECK(vote_count(b, Ballot::yes) == 2 && vote_count(b, Ballot::no) == 0);
  vote_command(b, t, 2, "vote yes", 4);
  CHECK(!b.open && b.passed == 1);
}

TEST(time_runs_out) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(3);
  b.settings.time_s = 30;
  vote_command(b, t, 0, "vote fraglimit 50", 10);
  CHECK(vote_seconds_left(b, 25) == 15);
  vote_tick(b, 39.9);
  CHECK(b.open);
  vote_tick(b, 40);
  CHECK(!b.open && b.failed == 1 && said(b, "Vote ran out: fraglimit 50 (1 yes, 2 needed)"));
}

TEST(a_leaver_is_weighed_again_and_bots_never_vote) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(4);
  vote_command(b, t, 0, "vote fraglimit 50", 0);
  vote_command(b, t, 1, "vote yes", 1);
  vote_command(b, t, 3, "vote no", 1);
  CHECK(b.open);
  vote_seat(b, 3, "", 2);  // the no leaves: 2 yes of 3 is more than half
  CHECK(!b.open && b.passed == 1);
  vote_seat(b, 3, "", 3);  // leaving twice is nothing
  // A bot is no voter: its call and its answer are refused.
  vote_command(b, t, 9, "vote fraglimit 1", 4);
  CHECK(!b.open && b.notes.back().why == VoteWhy::not_voter);
  // Everybody leaving drops the vote.
  vote_command(b, t, 0, "vote timelimit 1", 5);
  vote_seat(b, 1, "", 6);  // 1 yes of 2: half, not more
  CHECK(b.open);
  vote_seat(b, 0, "", 7);
  vote_seat(b, 2, "", 7);
  CHECK(!b.open && said(b, "Vote dropped: timelimit 1, nobody left"));
}

TEST(refusals_say_why_and_what_next) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(3);
  vote_command(b, t, 0, "vote rcon_password x", 0);
  CHECK(b.notes.back().why == VoteWhy::denied && said(b, "Type \"vote rules\""));
  vote_command(b, t, 0, "vote gravity 5", 0);
  CHECK(b.notes.back().why == VoteWhy::refused && said(b, "gravity is no setting here"));
  vote_command(b, t, 0, "vote fraglimit 500", 0);
  CHECK(b.notes.back().why == VoteWhy::refused && said(b, "outside 0..100"));
  vote_command(b, t, 0, "vote fraglimit", 0);
  CHECK(b.notes.back().why == VoteWhy::no_value);
  vote_command(b, t, 0, "vote yes", 0);
  CHECK(b.notes.back().why == VoteWhy::none_open);
  CHECK(!vote_command(b, t, 0, "yes", 0));  // no vote open: the mod's own word
  CHECK(!vote_command(b, t, 0, "ready", 0));
  vote_command(b, t, 0, "vote fraglimit 40", 1);
  vote_command(b, t, 1, "vote timelimit 40", 1);
  CHECK(b.notes.back().why == VoteWhy::busy);
  vote_command(b, t, 1, "vote cancel", 1);
  CHECK(b.notes.back().why == VoteWhy::not_caller);
  vote_command(b, t, 0, "vote cancel", 2);
  CHECK(!b.open && said(b, "Vote cancelled: fraglimit 40"));
  b.settings.enabled = false;
  vote_command(b, t, 0, "vote fraglimit 40", 3);
  CHECK(b.notes.back().why == VoteWhy::off);
  for (const VoteNote& n : b.notes)
    if (n.kind == VoteNoteKind::refused) CHECK(n.to == n.slot && n.to >= 0);
}

TEST(the_map_over_puts_every_change_back) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(1);
  vote_command(b, t, 0, "vote fraglimit 30", 0);
  f.values["fraglimit"] = "30";  // the game made it
  vote_command(b, t, 0, "vote timelimit 5", 1);
  f.values["timelimit"] = "5";
  vote_command(b, t, 0, "vote fraglimit 40", 2);  // the same key again: the first before stays
  CHECK(b.kept.size() == 2 && b.kept[0].value == "40" && b.kept[0].before == "20");
  vote_seat(b, 1, "late", 3);
  vote_command(b, t, 0, "vote timelimit 7", 3);  // open when the map ends: dropped
  CHECK(b.open);
  const auto back = vote_map_over(b);
  CHECK(!b.open && b.kept.empty());
  CHECK(back.size() == 2);
  CHECK(back[0].name == "timelimit" && back[0].value == "10");
  CHECK(back[1].name == "fraglimit" && back[1].value == "20");
  CHECK(said(b, "fraglimit back to 20 for the new map"));
  CHECK(said(b, "Vote dropped: timelimit 7, new map"));
  CHECK(vote_map_over(b).empty());
}

TEST(status_and_rules_go_to_the_asker_alone) {
  Fake f;
  const VoteTarget t{&f, fake_check};
  VoteBook b = seated(3);
  vote_command(b, t, 2, "vote", 0);
  CHECK(b.notes.back().kind == VoteNoteKind::told && b.notes.back().to == 2);
  vote_command(b, t, 2, "vote rules", 0);
  CHECK(said(b, "any setting may be voted on; never: *password*"));
  CHECK(said(b, "more than half of the 3 players"));
  vote_command(b, t, 0, "vote fraglimit 50", 0);
  b.said.clear();
  vote_command(b, t, 1, "vote", 5);
  CHECK(said(b, "vote: fraglimit 50 called by p0: 1 yes, 0 no, 2 needed, 25 s left"));
}
