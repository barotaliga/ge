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
