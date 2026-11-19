// The console's commands over names picked by a regular expression:
//
//   cvarlist_re <re>     cmdlist_re <re>      the cvars or commands whose names hold a match
//   cvar_reset_re <re>   each matching cvar back to its default
//   toggle_re <re>       each matching cvar between 0 and 1
//   unset_re <re>        each setting `set` made that matches taken away (plain names only: a
//                        game's own value, `<game>:<name>`, is `unset`'s alone)
//   unalias_re <re>      each matching alias removed
//
// A pattern is ECMAScript, matched anywhere in the name without case (`^cl_` for a prefix). A
// pattern the console cannot read is a problem naming it, and nothing changes.
//
// ge:summary The console's regex commands: cvarlist_re, toggle_re, unset_re, unalias_re
// ge:keywords console cvarlist_re cmdlist_re cvar_reset_re toggle_re unset_re unalias_re
// ge:keywords regex pattern config-scripting
// ge:skill console
// ge:test tests/console_patterns_test.cpp
#include <optional>
#include <regex>

#include "console/cfg.hpp"
#include "console/console.hpp"

namespace ge::console {

namespace {

// The pattern `a[1]` read; none (and a problem said) when it is missing or not one.
std::optional<std::regex> pattern(Console& c, const Args& a) {
  if (a.size() < 2) {
    c.error(a[0] + " needs a pattern", "usage: " + a[0] + " <pattern>, as in " + a[0] + " ^cl_");
    return std::nullopt;
  }
  try {
    return std::regex(a[1], std::regex::ECMAScript | std::regex::icase);
  } catch (const std::regex_error&) {
    c.error("'" + a[1] + "' is not a pattern the console can read", "check its brackets");
    return std::nullopt;
  }
}

bool hits(const std::regex& re, const std::string& name) { return std::regex_search(name, re); }

void say(Console& c, const SetResult r, const std::string& name) {
  const std::string why = set_result_text(r, name);
  if (!why.empty()) c.error(why, "");
}

}  // namespace

void register_pattern_commands(Console& con) {
  con.add_command("cvarlist_re", "cvarlist_re <pattern>: the cvars whose names match",
                  [](Console& c, const Args& a) {
                    const auto re = pattern(c, a);
                    if (!re) return;
                    int n = 0;
                    for (const Cvar* v : c.cvars().sorted()) {
                      if (!hits(*re, v->name)) continue;
                      c.print(cvar_list_line(c.cvars(), *v));
                      ++n;
                    }
                    c.print(std::to_string(n) + " cvars");
                  });
  con.add_command("cmdlist_re", "cmdlist_re <pattern>: the commands whose names match",
                  [](Console& c, const Args& a) {
                    const auto re = pattern(c, a);
                    if (!re) return;
                    int n = 0;
                    for (const auto& [name, cmd] : c.commands()) {
                      if (!hits(*re, name)) continue;
                      c.print(name + "  " + cmd.help);
                      ++n;
                    }
                    c.print(std::to_string(n) + " commands");
                  });
  con.add_command("cvar_reset_re", "cvar_reset_re <pattern>: every matching cvar to its default",
                  [](Console& c, const Args& a) {
                    const auto re = pattern(c, a);
                    if (!re) return;
                    for (Cvar* v : c.cvars().sorted())
                      if (hits(*re, v->name) && (v->flags & kReadOnly) == 0)
                        say(c, c.cvars().reset(*v, a.source), v->name);
                  });
  con.add_command("toggle_re", "toggle_re <pattern>: every matching cvar between 0 and 1",
                  [](Console& c, const Args& a) {
                    const auto re = pattern(c, a);
                    if (!re) return;
                    for (Cvar* v : c.cvars().sorted()) {
                      if (!hits(*re, v->name) || (v->flags & kReadOnly) != 0) continue;
                      const bool now = !v->value.empty() && v->value != "0";
                      say(c, c.cvars().set(*v, now ? "0" : "1", a.source), v->name);
                    }
                  });
  con.add_command("unset_re",
                  "unset_re <pattern>: every matching setting made by set taken away (a game's own "
                  "values stay)",
                  [](Console& c, const Args& a) {
                    const auto re = pattern(c, a);
                    if (!re) return;
                    for (const Cvar* v : c.cvars().sorted())
                      if ((v->flags & kCreated) != 0 && hits(*re, v->name))
                        c.cvars().remove_created(v->name);
                  });
  con.add_command("unalias_re", "unalias_re <pattern>: every matching alias removed",
                  [](Console& c, const Args& a) {
                    const auto re = pattern(c, a);
                    if (!re) return;
                    std::vector<std::string> gone;
                    for (const auto& [name, al] : c.aliases())
                      if (hits(*re, name)) gone.push_back(name);
                    for (const std::string& name : gone) c.remove_alias(name);
                  });
}

}  // namespace ge::console
