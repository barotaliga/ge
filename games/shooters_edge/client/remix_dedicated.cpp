// Classic remix on a dedicated server: the cr_ rules, cr_basedir, the remix opened (see the
// header).
//
// ge:summary Classic remix on a dedicated server: the cr_ rules, cr_basedir, the remix opened
// ge:keywords remix dedicated-server sesv cr_basedir open_server_remix
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

constexpr std::string_view kBasedir = "cr_basedir";

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
    out.push_back({kBasedir.data(),
                   "The other classic game's install the server's classic remix reads: its maps, "
                   "models and sounds.",
                   nullptr,
                   "\"\": none, so classic remix stays off (the default); FOLDER: the install of "
                   "the full game the server does not play, such as the folder holding baseq2",
                   false});
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
    if (!value.empty() && rule != kBasedir && cvars.find(rule) != nullptr)
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
  const auto at = rules.find(std::string(kBasedir));
  const std::string other_at = at != rules.end() ? at->second : std::string();
  if (other_at.empty())
    return off(
        "Classic remix stays off: the server needs the other classic game's install. Set "
        "cr_basedir to its folder.");
  std::string other_base;
  const ce::Profile* other = install_of(other_at, &other_base, &why);
  if (other == nullptr)
    return off("Classic remix stays off: cr_basedir " + other_at + " holds no classic game. " +
               why);
  if (other->id == played)
    return off("Classic remix stays off: cr_basedir holds " + whose(other, other->id) +
               ", the game this server plays. Name the other game's install.");
  if (mine == nullptr || edition_of(root, *mine) != Edition::full)
    return off("Classic remix stays off: it needs the full game, and " + whose(mine, played) +
               " here is not. Serve its full game's files.");
  if (edition_of(other_base, *other) != Edition::full)
    return off("Classic remix stays off: it needs the full game, and the " +
               whose(other, other->id) + " at cr_basedir is not.");
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
