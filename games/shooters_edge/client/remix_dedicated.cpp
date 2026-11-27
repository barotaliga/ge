// Classic remix on a dedicated server: the cr_ rules, the installs, the remix opened (see the
// header).
//
// ge:summary Classic remix on a dedicated server: the cr_ rules, the games' installs, the remix
// ge:keywords remix dedicated-server sesv fs_basedir fs_basegame open_server_remix
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_remix_dedicated_test.cpp
#include "shooters_edge/client/remix_dedicated.hpp"

#include <map>
#include <string>
#include <vector>

#include "classicenhance/profile.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/installs.hpp"

namespace shooters_edge::client {

namespace {

namespace ce = ge::classicenhance;
namespace gs = ge::server;

// The window's cr_ settings, bound once: their words and defaults are the rules'.
struct Bound {
  ge::console::Cvars cvars{true};
  RemixSettings s;
  Bound() { bind_remix(cvars, &s); }
};

const Bound& bound() {
  static const Bound b;
  return b;
}

std::string whose(const ce::Profile* p, std::string_view id) {
  return p != nullptr ? std::string(p->what) : std::string(id);
}

}  // namespace

std::span<const gs::GameRule> remix_rules() {
  static const std::vector<gs::GameRule> rules = [] {
    std::vector<gs::GameRule> out;
    for (const ge::console::Cvar* v : bound().cvars.sorted()) {
      if (v->name == "cr_menu") continue;  // the player's own: the menu is drawn before a server
      out.push_back({v->name.c_str(), v->help.c_str(),
                     v->fallback.empty() ? nullptr : v->fallback.c_str(), v->values.c_str(),
                     false});  // told by `remix_info_lines`, only while the remix is on
    }
    // The game served and each game's install (client/installs.hpp), the other's read by remix.
    static const BaseGame base;
    static std::vector<std::string> names, helps;
    names.emplace_back(kBaseGame);
    helps.emplace_back("The game the server serves, given on its command line.");
    for (const auto& [id, dir] : base.dirs) {
      names.push_back(basedir_setting(id));
      const ce::Profile* p = ce::find_profile(id);
      helps.push_back("The folder " + whose(p, id) +
                      " is installed in: served from there when the command line names no root, "
                      "and read by classic remix while the other game is served.");
    }
    out.push_back({names[0].c_str(), helps[0].c_str(), nullptr,
                   "\"\" or q1: Quake (the default); q2: Quake II", false});
    for (size_t i = 1; i < names.size(); i++)
      out.push_back({names[i].c_str(), helps[i].c_str(), nullptr,
                     "\"\": none (the default); FOLDER: that install", false});
    return out;
  }();
  return rules;
}

bool is_remix_rule(std::string_view rule) {
  for (const gs::GameRule& r : remix_rules())
    if (rule == r.name) return true;
  return false;
}

RemixSettings remix_of_rules(const std::map<std::string, std::string>& rules) {
  ge::console::Cvars cvars{true};
  RemixSettings s;
  bind_remix(cvars, &s);
  for (const auto& [rule, value] : rules)
    if (!value.empty() && cvars.find(rule) != nullptr)
      cvars.set(rule, value, ge::console::Source::local);
  s.menu.clear();
  return s;
}

std::string open_server_remix(RemixFiles& r, const std::map<std::string, std::string>& rules,
                              std::string_view played, const std::string& root,
                              const std::string& game, bool mod_rules) {
  const RemixSettings s = remix_of_rules(rules);
  r.balance = s.balance;
  std::string why;
  const auto off = [&](std::string line) {
    open_remix(r, RemixState{}, played, root, game, why);
    return line;
  };
  if (!s.enabled) return off("");
  const ce::Profile* mine = ce::find_profile(played);
  // The other game: the first whose install is set and is not the one served.
  const ce::Profile* other = nullptr;
  std::string other_at, other_base;
  for (const ce::Profile& p : ce::kProfiles) {
    const auto at = rules.find(basedir_setting(p.id));
    if (p.id == played || at == rules.end() || at->second.empty()) continue;
    other_at = at->second;
    other = &p;
    break;
  }
  if (other == nullptr)
    return off(
        "Classic remix stays off: the server needs the other classic game's install. Set its "
        "fs_basedir setting to its folder.");
  const std::string setting = basedir_setting(other->id);
  if (!check_game_folder(other_at, &other_base, &why, *other))
    return off("Classic remix stays off: " + setting + " " + other_at + " holds no " +
               whose(other, other->id) + ". " + why);
  if (mine == nullptr || edition_of(root, *mine) != Edition::full)
    return off("Classic remix stays off: it needs the full game, and " + whose(mine, played) +
               " here is not. Serve its full game's files.");
  if (edition_of(other_base, *other) != Edition::full)
    return off("Classic remix stays off: it needs the full game, and the " +
               whose(other, other->id) + " at " + setting + " is not.");
  RemixState st;
  st.on = true;
  st.other = std::string(other->id);
  st.other_basedir = other_base;
  st.expansions = s.expansions;
  if (!open_remix(r, st, played, root, game, why)) return off(why);
  r.mod_rules = mod_rules;
  remix_dress(r, s, played);
  return "Classic remix is on: " + whose(other, other->id) + "'s maps can be played too.";
}

std::vector<std::string> server_remix_lines(const RemixFiles& r) {
  std::vector<std::string> out;
  if (!r.state.on) return out;
  if (!r.mod_rules)
    out = worn_lines(r, {});
  else if (r.physics != nullptr)  // `cr_over_mods` (remix_dress): the physics over the mod's own
    for (const std::string_view line : r.physics->lines) out.emplace_back(line);
  for (std::string& line : remix_info_lines(r)) out.push_back(std::move(line));
  return out;
}

}  // namespace shooters_edge::client
