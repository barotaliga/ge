// Rulesets and the locks and gates under them: a held setting refuses a set and keeps the
// player's value for later, a range clamps, a refused command prints the ruleset's words, the
// setting is locked while connected, an unknown name is refused, a setting registered after the
// choice is locked too, and cfg_save writes the player's own value.
//
// ge:summary Rulesets: locks, ranges, gates, the connected lock, late settings, the saved value
// ge:keywords ruleset lock unlock gate locked set_connected cfg-save wanted
// ge:skill console testing
#include "check.hpp"
#include "console/console.hpp"
#include "console/ruleset.hpp"

#include <array>
#include <string>

using namespace ge::console;

namespace {

constexpr std::array kLocks{RuleLock{.name = "cl_hud", .value = "0"},
                            RuleLock{.name = "cl_maxfps", .min = 0, .max = 77},
                            RuleLock{.name = "ce_enabled", .value = "0"}};
constexpr std::array kRefused{RuleRefusal{.command = "packet"},
                              RuleRefusal{.command = "timerefresh", .match_only = true}};
constexpr std::array kBook{
    Ruleset{.name = "default", .title = "No rules."},
    Ruleset{.name = "strict", .title = "The rules of play.", .locks = kLocks, .refused = kRefused}};

bool printed(const Console& c, const std::string& text) {
  for (const std::string& l : c.output())
    if (l.find(text) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(a_ruleset_holds_its_settings_and_gives_them_back) {
  Console c(false);
  Cvar& hud = c.cvars().add({.name = "cl_hud", .fallback = "1", .flags = kArchive});
  Cvar& fps = c.cvars().add({.name = "cl_maxfps", .fallback = "0", .flags = kArchive});
  bool in_match = false;
  Rulesets r(c, [&in_match] { return in_match; });
  c.add_command("packet", "", [](Console& k, const Args&) { k.print("sent"); });
  c.add_command("timerefresh", "", [](Console& k, const Args&) { k.print("refreshed"); });
  r.set_book(kBook);
  CHECK(r.current() != nullptr && r.current()->name == "default");
  c.execute("cl_maxfps 144");
  c.execute("ruleset strict");
  r.update();
  CHECK(r.current()->name == "strict");
  CHECK(hud.value == "0");
  CHECK(fps.value == "77");  // clamped into the range
  CHECK(c.cvars().set(hud, "1", Source::local) == SetResult::locked);
  CHECK(set_result_text(SetResult::locked, hud) == "cl_hud is locked by the strict ruleset.");
  c.execute("cl_maxfps 50");
  CHECK(fps.value == "50");  // inside the range: allowed
  c.execute("cl_maxfps 200");
  CHECK(fps.value == "77");
  // The player's own values are what cfg_save writes.
  const std::string saved = c.save_text();
  CHECK(saved.find("seta cl_hud \"1\"") != std::string::npos);
  CHECK(saved.find("seta cl_maxfps \"200\"") != std::string::npos);
  // Refused commands: always, and only during a match.
  c.clear_output();
  c.execute("packet x");
  CHECK(printed(c, "packet is not allowed under the strict ruleset."));
  CHECK(!printed(c, "sent"));
  c.execute("timerefresh");
  CHECK(printed(c, "refreshed"));
  in_match = true;
  c.execute("timerefresh");
  CHECK(printed(c, "timerefresh is not allowed during a match under the strict ruleset."));
  // Back to default: every setting the player's again, every command free.
  c.execute("ruleset default");
  r.update();
  CHECK(hud.value == "1");
  CHECK(fps.value == "200");
  c.clear_output();
  c.execute("packet x");
  CHECK(printed(c, "sent"));
}

TEST(a_ruleset_changes_only_while_not_connected) {
  Console c(false);
  Rulesets r(c);
  r.set_book(kBook);
  r.set_connected(true);
  c.clear_output();
  c.execute("ruleset strict");
  r.update();
  CHECK(r.current()->name == "default");
  CHECK(printed(c, "The ruleset can only be changed while you are not connected to a server."));
  r.set_connected(false);
  r.update();
  CHECK(r.current()->name == "default");  // the refused choice is not kept for later
  c.execute("ruleset STRICT");
  r.update();
  CHECK(r.current()->name == "strict");
  CHECK(c.cvars().find("ruleset")->value == "strict");
}

TEST(an_unknown_ruleset_is_refused_with_the_names) {
  Console c(false);
  Rulesets r(c);
  r.set_book(kBook);
  c.clear_output();
  c.execute("ruleset nonsense");
  r.update();
  CHECK(printed(c, "There is no ruleset called nonsense. The rulesets are default, strict."));
  CHECK(c.cvars().find("ruleset")->value == "default");
}

TEST(a_setting_registered_later_is_held_too) {
  Console c(false);
  Rulesets r(c);
  r.set_book(kBook);
  c.execute("ruleset strict");
  r.update();
  Cvar& ce = c.cvars().add({.name = "ce_enabled", .fallback = "1", .flags = kArchive});
  CHECK(ce.value == "1");
  r.update();
  CHECK(ce.value == "0");
  CHECK(c.cvars().set(ce, "1", Source::local) == SetResult::locked);
}

TEST(a_new_book_without_the_name_falls_back_to_default) {
  Console c(false);
  Cvar& hud = c.cvars().add({.name = "cl_hud", .fallback = "1", .flags = kArchive});
  Rulesets r(c);
  r.set_book(kBook);
  c.execute("ruleset strict");
  r.update();
  CHECK(hud.value == "0");
  constexpr std::array kOther{Ruleset{.name = "default", .title = "No rules."}};
  r.set_book(kOther);
  CHECK(r.current()->name == "default");
  CHECK(hud.value == "1");
}

TEST(rulesets_lists_the_book_and_describes_one) {
  Console c(false);
  Rulesets r(c);
  r.set_book(kBook);
  c.clear_output();
  c.execute("rulesets");
  CHECK(printed(c, "* default: No rules."));
  CHECK(printed(c, "  strict: The rules of play."));
  c.clear_output();
  c.execute("rulesets strict");
  CHECK(printed(c, "holds cl_hud 0"));
  CHECK(printed(c, "holds cl_maxfps 0 to 77"));
  CHECK(printed(c, "refuses timerefresh during a match"));
}

namespace {

constexpr std::array<std::string_view, 3> kTeamCommands{"if", "echo", "say_team"};
constexpr std::array kStrictBook{Ruleset{.name = "default", .title = "No rules."},
                                 Ruleset{.name = "tight",
                                         .title = "No scripts that act on the team's words.",
                                         .team_macro_commands = kTeamCommands,
                                         .quiet_triggers = true,
                                         .wait_limit = 3}};

}  // namespace

TEST(a_line_with_a_team_macro_runs_only_the_team_commands) {
  Console c(false);
  int health = 30;
  c.add_macro("health", [&health] { return std::to_string(health); }, MacroKind::team);
  c.add_macro("plain", [] { return std::string("7"); });
  int impulses = 0;
  std::string said;
  c.add_command("impulse", "", [&impulses](Console&, const Args&) { ++impulses; });
  c.add_command("say_team", "", [&said](Console&, const Args& a) { said = a[1]; });
  Rulesets r(c);
  r.set_book(kStrictBook);
  CHECK(c.is_team_macro("HEALTH") && !c.is_team_macro("plain"));
  // Under default a script may act on the macro.
  c.execute("impulse $health");
  CHECK(impulses == 1);
  c.execute("ruleset tight");
  r.update();
  c.execute("impulse $health");
  CHECK(impulses == 1);
  CHECK(printed(c, "impulse cannot be used with team macros under the tight ruleset."));
  c.execute("impulse $plain");  // a plain macro: no limit
  CHECK(impulses == 2);
  c.execute("say_team $health");
  CHECK(said == "30");
  // A setting named on such a line is refused the same way.
  c.cvars().add({.name = "sensitivity", .fallback = "3"});
  c.execute("sensitivity $health");
  CHECK(c.cvars().find("sensitivity")->value == "3");
  CHECK(printed(c, "sensitivity cannot be used with team macros under the tight ruleset."));
  // `if` is allowed, but what it runs is held to the limit too.
  c.execute("if $health < 50 then impulse 7");
  CHECK(impulses == 2);
  c.execute("if $health < 50 then say_team low");
  CHECK(said == "low");
  // Back to default: the limit goes.
  c.execute("ruleset default");
  r.update();
  CHECK(c.team_macro_limit() == nullptr);
  c.execute("impulse $health");
  CHECK(impulses == 3);
}

TEST(triggers_keep_quiet_during_a_match_under_a_strict_ruleset) {
  Console c(false);
  bool in_match = false;
  Rulesets r(c, [&in_match] { return in_match; });
  r.set_book(kStrictBook);
  int ran = 0;
  c.add_command("hit", "", [&ran](Console&, const Args&) { ++ran; });
  c.set_alias("ontrigger", "hit", false);
  c.execute("msg_trigger ontrigger \"quad\"");
  c.fire_triggers("Bob got the quad");
  c.run();
  CHECK(ran == 1);
  c.execute("ruleset tight");
  r.update();
  CHECK(!c.triggers_quiet() && c.sound_triggers_quiet());  // sound triggers: at all times
  c.fire_triggers("Bob got the quad");
  c.run();
  CHECK(ran == 2);  // no match yet
  in_match = true;
  CHECK(c.triggers_quiet());
  c.fire_triggers("Bob got the quad");
  c.run();
  CHECK(ran == 2);
  c.execute("ruleset default");  // changed between matches in a real game; the console allows it
  r.update();
  CHECK(!c.triggers_quiet() && !c.sound_triggers_quiet());
}

TEST(a_wait_limit_drops_a_script_that_waits_too_long) {
  Console c(false);
  Rulesets r(c);
  r.set_book(kStrictBook);
  int ran = 0;
  c.add_command("hit", "", [&ran](Console&, const Args&) { ++ran; });
  c.execute("ruleset tight");
  r.update();
  c.append("wait; wait; wait; hit");  // three waits: allowed
  for (int i = 0; i < 6; ++i) c.run();
  CHECK(ran == 1);
  c.append("wait; wait; wait; wait; hit");  // four in a row: dropped
  for (int i = 0; i < 6; ++i) c.run();
  CHECK(ran == 1);
  CHECK(printed(c,
                "The tight ruleset allows 3 waits in a row, so the rest of the script was "
                "dropped."));
}
