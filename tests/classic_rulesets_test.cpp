// plugins/classic/rulesets.hpp: every book starts with `default` and holds nothing there; every
// other ruleset turns off what this engine adds and refuses the rocket jump helpers; smackdown
// holds the league's settings; the f_ruleset question is the word alone or with digits; the
// answer reads server, version, ruleset and the aids; one answer every 20 seconds.
//
// ge:summary Test: the classic rulesets' books, f_ruleset's question, answer and pace
// ge:keywords test ruleset smackdown duel f_ruleset fair_query ruleset_reply rsn
// ge:skill classic testing
#include "classic/rulesets.hpp"

#include <span>
#include <string>
#include <string_view>

#include "check.hpp"
#include "console/console.hpp"
#include "console/ruleset.hpp"

namespace cl = ge::classic;
namespace gc = ge::console;

namespace {

const gc::RuleLock* lock_of(const gc::Ruleset& r, std::string_view name) {
  for (const gc::RuleLock& l : r.locks)
    if (l.name == name) return &l;
  return nullptr;
}

bool refuses(const gc::Ruleset& r, std::string_view cmd) {
  for (const gc::RuleRefusal& f : r.refused)
    if (f.command == cmd) return true;
  return false;
}

void check_book(std::span<const gc::Ruleset> book) {
  CHECK(!book.empty());
  CHECK(book.front().name == "default");
  CHECK(book.front().locks.empty() && book.front().refused.empty());
  for (const gc::Ruleset& r : book.subspan(1)) {
    for (const char* ours : {"ce_enabled", "cr_enabled", "cl_speedjump"}) {
      const gc::RuleLock* l = lock_of(r, ours);
      CHECK(l != nullptr && l->value == "0");
    }
    CHECK(refuses(r, "+rjump") && refuses(r, "+rrjump"));
    CHECK(!r.title.empty() && r.title.back() == '.');
    for (char c : r.name) CHECK(c >= 'a' && c <= 'z');
  }
}

}  // namespace

TEST(every_book_starts_free_and_turns_ours_off_elsewhere) {
  check_book(cl::qw_rulesets());
  check_book(cl::q2_rulesets());
  CHECK(cl::qw_rulesets().size() == 6);
  CHECK(cl::q2_rulesets().size() == 2 && cl::q2_rulesets()[1].name == "duel");
}

TEST(smackdown_holds_the_league_settings) {
  const gc::Ruleset& s = cl::qw_rulesets()[1];
  CHECK(s.name == "smackdown");
  CHECK(lock_of(s, "allow_scripts")->value == "0");
  CHECK(lock_of(s, "cl_hud")->value == "0");
  CHECK(lock_of(s, "scr_allowsnap")->value == "1");
  CHECK(lock_of(s, "cl_physfps")->max == 77);
  CHECK(lock_of(s, "cl_rollangle")->max == 5);
  CHECK(refuses(s, "timerefresh"));
}

TEST(the_ruleset_rides_in_the_user_info) {
  CHECK(cl::ruleset_info_key(false) == "*rsn");
  CHECK(cl::ruleset_info_key(true) == "rsn");
}

TEST(f_ruleset_is_the_word_alone_or_with_digits) {
  CHECK(cl::fair_query("bob: f_ruleset\n", "f_ruleset"));
  CHECK(cl::fair_query("bob: f_ruleset 4812  \n", "f_ruleset"));
  CHECK(cl::fair_query("a: b: f_ruleset", "f_ruleset"));  // a name holding ": "
  std::string red = "bob: f_ruleset";
  for (size_t i = 5; i < red.size(); ++i) red[i] = static_cast<char>(red[i] | 0x80);
  CHECK(cl::fair_query(red, "f_ruleset"));  // coloured letters count as plain
  CHECK(!cl::fair_query("bob: f_rulesets", "f_ruleset"));
  CHECK(!cl::fair_query("bob: f_ruleset please", "f_ruleset"));
  CHECK(!cl::fair_query("f_ruleset", "f_ruleset"));  // no speaker
}

TEST(the_answer_names_server_version_ruleset_and_aids) {
  cl::FairFacts f{
      .server = "10.0.0.1:27500", .version = "ShootersEdge 0.1", .ruleset = "smackdown"};
  CHECK(cl::ruleset_reply(f) == "10.0.0.1:27500 ShootersEdge 0.1 smackdown-msfi");
  f.ruleset = "default";
  f.modified = true;
  f.side_step = true;
  CHECK(cl::ruleset_reply(f) == "10.0.0.1:27500 ShootersEdge 0.1 default+mi-sf");
  CHECK(cl::fair_help().find("f_ruleset check") != std::string::npos);
}

TEST(one_answer_every_twenty_seconds) {
  cl::FairReplies r;
  CHECK(r.may_answer(100));
  CHECK(!r.may_answer(110));
  CHECK(r.may_answer(120));
}

TEST(a_classic_book_holds_the_window_settings) {
  gc::Console con{false};
  gc::Cvar& ce = con.cvars().add({.name = "ce_enabled", .fallback = "1", .flags = gc::kArchive});
  gc::Rulesets r(con);
  r.set_book(cl::q2_rulesets());
  con.execute("ruleset duel");
  r.update();
  CHECK(ce.value == "0");
  con.clear_output();
  con.execute("ce_enabled 1");
  CHECK(ce.value == "0");
  bool said = false;
  for (const std::string& l : con.output())
    said = said || l.find("ce_enabled is locked by the duel ruleset.") != std::string::npos;
  CHECK(said);
}

TEST(the_strict_sets_keep_scripts_off_the_team_macros) {
  const auto allows = [](const gc::Ruleset& r, std::string_view cmd) {
    for (std::string_view c : r.team_macro_commands)
      if (c == cmd) return true;
    return false;
  };
  for (const gc::Ruleset& r : cl::qw_rulesets()) {
    const bool strict = r.name == "smackdown" || r.name == "smackdrive" || r.name == "qcon" ||
                        r.name == "thunderdome";
    CHECK(r.quiet_triggers == strict);
    CHECK(r.team_macro_commands.empty() == !strict);
    if (strict) {
      CHECK(allows(r, "say_team") && allows(r, "if") && allows(r, "tp_msgreport"));
      CHECK(!allows(r, "impulse") && !allows(r, "weapon"));
    }
    CHECK(r.wait_limit == (r.name == "qcon" ? 10 : 0));
  }
  // Quake II's duel keeps scripts off them too: the team macros are every classic game's.
  for (const gc::Ruleset& r : cl::q2_rulesets())
    CHECK(r.team_macro_commands.empty() == (r.name == "default"));
}
