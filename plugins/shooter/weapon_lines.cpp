// The shooter's weapon lines hidden (weapon_lines.hpp).
//
// ge:summary The shooter's settings that hide the server's no weapon and not enough ammo lines
// ge:keywords ignore_no_weapon ignore_not_enough_ammo weapon_line_hidden
// ge:skill shooter console
// ge:test tests/shooter_weapon_lines_test.cpp
#include "shooter/weapon_lines.hpp"

namespace ge::shooter {

void add_weapon_line_cvars(console::Cvars& v, WeaponLines& w) {
  v.bind({.name = "ignore_no_weapon",
          .fallback = "0",
          .flags = console::kArchive,
          .help = "Whether the server's no weapon line is hidden.",
          .kind = console::CvarKind::boolean,
          .values = "0: shown (the default); 1: hidden"},
         &w.no_weapon);
  v.bind({.name = "ignore_not_enough_ammo",
          .fallback = "0",
          .flags = console::kArchive,
          .help = "Whether the server's not enough ammo line is hidden.",
          .kind = console::CvarKind::boolean,
          .values = "0: shown (the default); 1: hidden"},
         &w.no_ammo);
}

bool weapon_line_hidden(const WeaponLines& w, std::string_view text) {
  return (w.no_weapon && text.starts_with("no weapon")) ||
         (w.no_ammo && text.starts_with("not enough ammo"));
}

}  // namespace ge::shooter
