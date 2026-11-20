// Regular-expression triggers (re_triggers.hpp).
//
// ge:summary Regular-expression triggers: the commands, the options, a line tried
// ge:keywords re_trigger re_trigger_options re_trigger_match re_trigger_line regex trigger
// ge:skill console
// ge:test tests/console_re_triggers_test.cpp
#include "console/re_triggers.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "console/cvar.hpp"

namespace ge::console {

namespace {

constexpr std::string_view kSpecials = "\\\"()[]{}.*+?^$|";

ReTrigger* find(ReTriggers& t, std::string_view name) {
  for (ReTrigger& r : t.list)
    if (r.name == name) return &r;
  return nullptr;
}

std::shared_ptr<const std::regex> compile(std::string_view pattern) {
  try {
    return std::make_shared<const std::regex>(std::string(pattern), std::regex::ECMAScript);
  } catch (const std::regex_error&) {
    return nullptr;
  }
}

// A word that reads as a pattern of names rather than a name.
bool is_pattern(std::string_view w) {
  return !w.empty() && w[0] != '+' && w[0] != '-' && w.find_first_of(kSpecials) != w.npos;
}

// Each trigger `word` names: the one of that name, or every one whose name the pattern finds.
std::vector<ReTrigger*> named(Console& c, ReTriggers& t, std::string_view word) {
  std::vector<ReTrigger*> out;
  if (!is_pattern(word)) {
    if (ReTrigger* r = find(t, word)) out.push_back(r);
    return out;
  }
  const std::shared_ptr<const std::regex> re = compile(word);
  if (!re) {
    c.print("The pattern " + std::string(word) + " does not read.");
    return out;
  }
  for (ReTrigger& r : t.list)
    if (std::regex_search(r.name, *re)) out.push_back(&r);
  return out;
}

std::string number_text(double v) {
  std::string s = std::to_string(v);
  s.erase(s.find_last_not_of('0') + 1);
  if (!s.empty() && s.back() == '.') s.pop_back();
  return s;
}

void show_one(Console& c, const ReTrigger& r) {
  c.print(r.name + ": \"" + r.pattern + "\"");
  std::string opts =
      "  options: mask=" + std::to_string(r.mask) + " interval=" + number_text(r.interval);
  if (r.final) opts += " final";
  if (r.remove) opts += " remove";
  if (!r.log) opts += " nolog";
  if (!r.enabled) opts += " disabled";
  if (!r.action) opts += " noaction";
  c.print(opts);
  c.print("  matched " + std::to_string(r.matched) + " times");
}

void list(Console& c, ReTriggers& t, std::string_view pattern) {
  if (t.list.empty()) {
    c.print("No re_triggers are set.");
    return;
  }
  std::shared_ptr<const std::regex> re;
  if (!pattern.empty() && !(re = compile(pattern))) {
    c.print("The pattern " + std::string(pattern) + " does not read.");
    return;
  }
  int shown = 0;
  c.print("The re_triggers:");
  for (const ReTrigger& r : t.list) {
    if (re && !std::regex_search(r.name, *re)) continue;
    ++shown;
    c.print(r.name + " : \"" + r.pattern + "\" : " + std::to_string(r.matched));
  }
  c.print(std::to_string(shown) + " of " + std::to_string(t.list.size()) + " shown");
}

void fire(Console& c, ReTrigger& r, const std::smatch& m, double now_s) {
  ++r.matched;
  r.last = now_s;
  for (size_t k = 0; k < m.size() && k < static_cast<size_t>(kReMatches); ++k)
    if (m[k].matched)
      c.cvars().set("re_trigger_match_" + std::to_string(k), m[k].str(), Source::code);
  if (!r.action) return;
  if (c.aliases().count(r.name) == 0) {
    c.print("The re_trigger " + r.name + " has no alias of its name to run.");
    return;
  }
  c.insert(r.name);
}

void options(Console& c, ReTrigger& r, const Args& a) {
  for (size_t i = 2; i < a.size(); ++i) {
    const std::string& w = a[i];
    if (w == "final" || w == "notfinal") {
      r.final = w == "final";
    } else if (w == "remove" || w == "noremove") {
      r.remove = w == "remove";
    } else if (w == "enable" || w == "disable") {
      r.enabled = w == "enable";
    } else if (w == "action" || w == "noaction") {
      r.action = w == "action";
    } else if (w == "log" || w == "nolog") {
      r.log = w == "log";
    } else if ((w == "mask" || w == "interval") && i + 1 < a.size()) {
      const double v = std::atof(a[++i].c_str());
      if (w == "mask")
        r.mask = static_cast<int>(v) & 0xff;
      else
        r.interval = std::max(0.0, v);
    } else {
      c.print(
          "re_trigger_options knows final, notfinal, remove, noremove, mask <bits>, "
          "interval <seconds>, enable, disable, action, noaction, log and nolog.");
    }
  }
}

}  // namespace

void add_re_triggers(Console& c, ReTriggers& t) {
  for (int k = 0; k < kReMatches; ++k) {
    const std::string name = "re_trigger_match_" + std::to_string(k);
    if (c.cvars().find(name) == nullptr)
      c.cvars().add({.name = name,
                     .flags = kReadOnly,
                     .help = "A part of the last line a re_trigger matched (0 the whole match).",
                     .kind = CvarKind::text});
  }
  c.add_command("re_trigger", "re_trigger [<name> [<pattern>]]: run the alias <name> on a line",
                [&t](Console& k, const Args& a) {
                  if (a.size() < 2) return list(k, t, "");
                  if (a.size() == 2) {
                    if (is_pattern(a[1])) return list(k, t, a[1]);
                    if (const ReTrigger* r = find(t, a[1])) return show_one(k, *r);
                    k.print("There is no re_trigger called " + a[1] + ".");
                    return;
                  }
                  if (a.size() > 3) {
                    k.print("Give a name and one pattern, quoted when it has spaces.");
                    return;
                  }
                  std::shared_ptr<const std::regex> re = compile(a[2]);
                  ReTrigger* r = find(t, a[1]);
                  if (!re) {
                    k.print("The pattern " + a[2] + " does not read; " + a[1] + " is gone.");
                    std::erase_if(t.list, [&](const ReTrigger& x) { return x.name == a[1]; });
                    return;
                  }
                  if (r == nullptr) {
                    ReTrigger made;
                    made.name = a[1];
                    t.list.insert(t.list.begin(), std::move(made));
                    r = &t.list.front();
                  }
                  r->pattern = a[2];
                  r->re = std::move(re);
                });
  c.add_command("re_trigger_options",
                "re_trigger_options <name> <option> ...: how a re_trigger acts on its lines",
                [&t](Console& k, const Args& a) {
                  if (a.size() < 3) {
                    k.print("Give a re_trigger's name and its options.");
                    return;
                  }
                  if (ReTrigger* r = find(t, a[1]))
                    options(k, *r, a);
                  else
                    k.print("There is no re_trigger called " + a[1] + ".");
                });
  c.add_command("re_trigger_delete", "re_trigger_delete <name or pattern> ...: re_triggers gone",
                [&t](Console& k, const Args& a) {
                  std::vector<std::string> gone;
                  for (size_t i = 1; i < a.size(); ++i)
                    for (const ReTrigger* r : named(k, t, a[i])) gone.push_back(r->name);
                  std::erase_if(t.list, [&](const ReTrigger& x) {
                    return std::find(gone.begin(), gone.end(), x.name) != gone.end();
                  });
                });
  for (const bool on : {true, false})
    c.add_command(on ? "re_trigger_enable" : "re_trigger_disable",
                  on ? "re_trigger_enable <name or pattern> ...: re_triggers on"
                     : "re_trigger_disable <name or pattern> ...: re_triggers off",
                  [&t, on](Console& k, const Args& a) {
                    for (size_t i = 1; i < a.size(); ++i)
                      for (ReTrigger* r : named(k, t, a[i])) r->enabled = on;
                  });
  c.add_command("re_trigger_match", "re_trigger_match <name> <text>: try a re_trigger on the text",
                [&t](Console& k, const Args& a) {
                  if (a.size() < 3) {
                    k.print("Give a re_trigger's name and the text to try.");
                    return;
                  }
                  ReTrigger* r = find(t, a[1]);
                  if (r == nullptr) {
                    k.print("There is no re_trigger called " + a[1] + ".");
                    return;
                  }
                  const std::string text = a.rest(1);
                  std::smatch m;
                  if (r->re && std::regex_search(text, m, *r->re)) fire(k, *r, m, t.now);
                });
}

ReLine re_trigger_line(Console& c, ReTriggers& t, std::string_view line, int kind, double now_s) {
  ReLine out;
  if (!t.on) return out;
  const std::string text(line);
  for (size_t i = 0; i < t.list.size(); ++i) {
    ReTrigger& r = t.list[i];
    if (!r.enabled || !r.re || (r.mask & kind) == 0) continue;
    if (r.interval > 0 && now_s - r.last < r.interval) continue;
    std::smatch m;
    if (!std::regex_search(text, m, *r.re)) continue;
    fire(c, r, m, now_s);
    out.remove = out.remove || r.remove;
    out.nolog = out.nolog || !r.log;
    if (r.final) break;
  }
  return out;
}

}  // namespace ge::console
