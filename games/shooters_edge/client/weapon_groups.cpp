// Shooter's Edge's weapon groups (weapon_groups.hpp).
//
// ge:summary Shooter's Edge's weapon groups: the cvars, the number keys and the HUD's look
// ge:keywords weapon-groups group_moves group_status group_armed
// ge:skill input shooter
// ge:test tests/shooters_edge_weapon_groups_test.cpp
#include "shooters_edge/client/weapon_groups.hpp"

#include "console/cvar.hpp"
#include "shooter/quick_weapon.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/quick_fire.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

void add_group_cvars(ge::console::Console& con, WeaponGroups& g) {
  using ge::console::CvarKind;
  using ge::console::kArchive;
  con.cvars().bind(
      {.name = "cl_weapongroups",
       .fallback = "0",
       .flags = kArchive,
       .help = "Whether the number keys pick from weapon groups, cl_weapongroup1 to 9.",
       .kind = CvarKind::boolean,
       .values = "0: each key picks its own weapon (the default); 1: the best of the group you "
                 "carry, pressed again the next"},
      &g.on);
  static constexpr const char* kNames[sh::kMaxGroups] = {
      "cl_weapongroup1", "cl_weapongroup2", "cl_weapongroup3", "cl_weapongroup4", "cl_weapongroup5",
      "cl_weapongroup6", "cl_weapongroup7", "cl_weapongroup8", "cl_weapongroup9"};
  for (size_t k = 0; k < sh::kMaxGroups; ++k)
    con.cvars().bind({.name = kNames[k],
                      .fallback = "",
                      .flags = kArchive,
                      .help = "The weapons this number key picks with weapon groups on, best "
                              "first, by weapon key.",
                      .kind = CvarKind::text,
                      .values = "\"\": the game's own group (the default); 0: none; TEXT: weapon "
                                "keys with spaces between"},
                     &g.text[k]);
}

const sh::WeaponGroups& group_settings(WeaponGroups& g) {
  const void* game = g.game.data();
  if (g.parsed_once && g.parsed == g.text && g.parsed_game == game) return g.groups;
  for (size_t k = 0; k < sh::kMaxGroups; ++k) {
    const std::string_view own = k < g.game.size() ? g.game[k] : std::string_view{};
    g.groups[k] = sh::weapon_list(g.text[k].empty() ? own : std::string_view{g.text[k]});
  }
  g.parsed = g.text;
  g.parsed_game = game;
  g.parsed_once = true;
  return g.groups;
}

sh::GroupArmed group_armed(const hud::Status& s) {
  sh::GroupArmed a;
  for (int k = 0; k <= hud::kGuns; ++k)
    if (s.has(static_cast<hud::Weapon>(k))) a.carried |= 1u << (k + 1);
  a.ready = quick_armed(s).ready;
  a.in_hand = static_cast<int>(s.weapon) + 1;
  return a;
}

void group_moves(WeaponGroups& g, const hud::Status& s, double t, Moves& m) {
  if (!g.on) return;
  if (m.attack || m.attack2) sh::group_fire(g.state, t);
  const sh::WeaponGroups& groups = group_settings(g);
  if (!sh::is_group(groups, m.impulse) || s.dead) return;
  m.impulse = sh::group_press(g.state, groups, group_armed(s), m.impulse, t);
}

void group_status(WeaponGroups& g, hud::Status& s, double t) {
  s.group = {};
  if (!g.on) return;
  const sh::GroupLook l = sh::group_look(g.state, group_settings(g), group_armed(s), t);
  hud::GroupShow& show = s.group;
  show.on = true;
  for (int i = 0; i < l.count && show.count < hud::kGroupShown; ++i) {
    const int w = l.weapons[static_cast<size_t>(i)];
    if (w < 1 || w > hud::kGuns + 1) continue;  // not one of the game's weapons
    if (i == l.lit) show.lit = show.count;
    show.weapons[static_cast<size_t>(show.count)] = static_cast<hud::Weapon>(w - 1);
    show.ready[static_cast<size_t>(show.count)] = l.ready[static_cast<size_t>(i)];
    ++show.count;
  }
  show.open = l.open;
  show.more = l.more;
}

}  // namespace shooters_edge::client
