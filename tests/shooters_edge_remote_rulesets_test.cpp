// games/shooters_edge/net/qw_remote_rulesets.hpp: the window's ruleset follows the loaded game's
// book, is locked while joined, and the match-only refusals hold while the player plays (joined,
// not watching, the server's status not Standby).
//
// ge:summary Test: the ruleset on a server joined: the book, the lock while joined, playing
// ge:keywords test ruleset rulesets remote playing standby f_ruleset
// ge:skill net testing
#include "shooters_edge/net/qw_remote_rulesets.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace cn = shooters_edge::net;

namespace {

bool printed(const ge::console::Console& c, const std::string& text) {
  for (const std::string& l : c.output())
    if (l.find(text) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(the_book_follows_the_loaded_game) {
  ge::console::Console con{false};
  cn::Remote r;
  bool q2 = false;
  r.q2_game = [&q2] { return q2; };
  cn::add_remote_commands(con, r);
  con.execute("ruleset smackdown");
  cn::remote_rulesets_frame(r, con);
  CHECK(r.rulesets->current()->name == "smackdown");
  CHECK(r.fair_facts.ruleset == "smackdown");
  q2 = true;  // Quake II has no smackdown: back to default
  cn::remote_rulesets_frame(r, con);
  CHECK(r.rulesets->current()->name == "default");
  con.execute("ruleset duel");
  cn::remote_rulesets_frame(r, con);
  CHECK(r.rulesets->current()->name == "duel");
}

TEST(the_ruleset_is_locked_while_joined) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  r.active = true;
  cn::remote_rulesets_frame(r, con);
  con.clear_output();
  con.execute("ruleset qcon");
  cn::remote_rulesets_frame(r, con);
  CHECK(r.rulesets->current()->name == "default");
  CHECK(printed(con, "The ruleset can only be changed while you are not connected to a server."));
  r.active = false;
  cn::remote_rulesets_frame(r, con);
  con.execute("ruleset qcon");
  cn::remote_rulesets_frame(r, con);
  CHECK(r.rulesets->current()->name == "qcon");
}

TEST(playing_is_joined_not_watching_and_not_standby) {
  cn::Remote r;
  CHECK(!cn::remote_playing(r));
  r.active = true;
  CHECK(cn::remote_playing(r));
  r.demo.state.serverinfo = {{"status", "Standby"}};
  CHECK(!cn::remote_playing(r));
  r.demo.state.serverinfo = {{"status", "Countdown"}};
  CHECK(cn::remote_playing(r));
  r.end.cfg.spectator = true;
  CHECK(!cn::remote_playing(r));
}

TEST(f_ruleset_alone_explains_the_answers) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  con.clear_output();
  con.execute("f_ruleset");
  CHECK(printed(con, "f_ruleset check: ask everyone now."));
  con.execute("f_ruleset check");
  CHECK(printed(con, "You are not connected to a server."));
}

TEST(a_change_of_cl_fakeshaft_in_a_match_is_said_to_everyone) {
  ge::console::Console con{false};
  cn::Remote r;
  r.q2_game = [] { return false; };
  cn::add_remote_commands(con, r);
  con.cvars().add({.name = "cl_fakeshaft", .fallback = "1"});
  std::vector<std::string> said;
  con.add_command("say", "", [&said](ge::console::Console&, const ge::console::Args& a) {
    said.push_back(a[1]);
  });
  cn::remote_rulesets_frame(r, con);
  con.execute("cl_fakeshaft 0.5");  // not on a server: nothing said
  cn::remote_rulesets_frame(r, con);
  con.run();
  CHECK(said.empty());
  r.active = true;
  con.execute("cl_fakeshaft 0");
  cn::remote_rulesets_frame(r, con);
  con.run();
  CHECK(said.size() == 1 && said.back() == "fakeshaft off");
  cn::remote_rulesets_frame(r, con);  // no change: said once
  con.run();
  CHECK(said.size() == 1);
}
