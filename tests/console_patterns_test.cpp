// The console's commands over names picked by a regular expression (console_patterns.cpp), and
// `unset` for a setting `set` made beside a game's own value.
//
// ge:summary Tests cvarlist_re, cvar_reset_re, toggle_re, unset and unset_re, unalias_re
// ge:keywords test console cvarlist_re cmdlist_re cvar_reset_re toggle_re unset unset_re
// ge:keywords unalias_re regex
// ge:skill console testing
#include <string>

#include "check.hpp"
#include "console/console.hpp"

using namespace ge::console;

namespace {

std::string value(Console& c, const char* name) {
  const Cvar* v = c.cvars().find(name);
  return v == nullptr ? "<none>" : v->value;
}

std::string printed(Console& c) {
  std::string out;
  for (const std::string& l : c.output()) out += l + "|";
  c.clear_output();
  return out;
}

void with_settings(Console& c) {
  c.cvars().add({.name = "cl_alpha", .fallback = "0", .kind = CvarKind::boolean});
  c.cvars().add({.name = "cl_beta", .fallback = "1", .kind = CvarKind::boolean});
  c.cvars().add({.name = "hud_gamma", .fallback = "5", .kind = CvarKind::integer});
}

}  // namespace

TEST(lists_pick_names_by_a_pattern_without_case) {
  Console c;
  with_settings(c);
  c.execute("cvarlist_re ^CL_");
  const std::string got = printed(c);
  CHECK_MSG(got.find("cl_alpha") != std::string::npos && got.find("cl_beta") != std::string::npos &&
                got.find("hud_gamma") == std::string::npos &&
                got.find("|2 cvars|") != std::string::npos,
            got);
  c.execute("cmdlist_re _re$");
  const std::string cmds = printed(c);
  CHECK_MSG(
      cmds.find("toggle_re") != std::string::npos && cmds.find("cvarlist ") == std::string::npos,
      cmds);
  c.execute("cvarlist_re (");  // not a pattern: a problem, nothing listed
  CHECK(!c.problems().empty());
}

TEST(toggle_and_reset_by_pattern) {
  Console c;
  with_settings(c);
  c.execute("toggle_re ^cl_");
  CHECK(value(c, "cl_alpha") == "1" && value(c, "cl_beta") == "0");
  c.execute("toggle_re ^cl_");
  CHECK(value(c, "cl_alpha") == "0" && value(c, "cl_beta") == "1");
  c.execute("hud_gamma 9; cl_alpha 1; cvar_reset_re gamma");
  CHECK(value(c, "hud_gamma") == "5" && value(c, "cl_alpha") == "1");
}

TEST(unset_takes_away_only_what_set_made) {
  Console c;
  with_settings(c);
  c.execute("set my_one 1; set my_two 2; set other 3");
  c.execute("unset my_one");
  CHECK(value(c, "my_one") == "<none>" && value(c, "my_two") == "2");
  c.execute("unset cl_alpha");  // the game's own: kept, and a problem says so
  CHECK(value(c, "cl_alpha") == "0" && !c.problems().empty());
  c.execute("unset_re ^my_|^cl_");
  CHECK(value(c, "my_two") == "<none>" && value(c, "other") == "3" && value(c, "cl_beta") == "1");
  c.execute("set my_one again");  // made again from nothing
  CHECK(value(c, "my_one") == "again");
  // A game's own value: `unset <game>:<name>` takes it away; unset_re never touches it.
  c.cvars().set_scope("first");
  c.execute("set game:hud_gamma 7");
  CHECK(value(c, "hud_gamma") == "7");
  c.execute("unset_re gamma");
  CHECK(value(c, "hud_gamma") == "7");
  c.execute("unset first:hud_gamma");
  CHECK(value(c, "hud_gamma") == "5");
}

TEST(aliases_by_pattern) {
  Console c;
  c.execute("alias +go \"echo go\"; alias -go \"echo stop\"; alias keep \"echo kept\"");
  c.execute("unalias_re go$");
  CHECK(c.aliases().size() == 1 && c.aliases().count("keep") == 1);
}
