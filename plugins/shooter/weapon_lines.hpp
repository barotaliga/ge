// A SHOOTER'S WEAPON LINES HIDDEN: the server's word that a weapon key found no weapon, or not
// enough ammunition, which a player pressing weapon keys often reads again and again.
//
//   ignore_no_weapon 0|1          1: a server line beginning "no weapon" hidden
//   ignore_not_enough_ammo 0|1    1: a server line beginning "not enough ammo" hidden
//
// ge:summary The shooter's settings that hide the server's no weapon and not enough ammo lines
// ge:keywords ignore_no_weapon ignore_not_enough_ammo weapon-lines no-weapon not-enough-ammo
// ge:skill shooter console
// ge:test tests/shooter_weapon_lines_test.cpp
#pragma once

#include <string_view>

#include "console/cvar.hpp"

namespace ge::shooter {

struct WeaponLines {
  bool no_weapon = false;  // ignore_no_weapon
  bool no_ammo = false;    // ignore_not_enough_ammo
};

// The settings over `w` (`w` must outlive `v`).
void add_weapon_line_cvars(console::Cvars& v, WeaponLines& w);
// Whether the server's line `text` is hidden.
bool weapon_line_hidden(const WeaponLines& w, std::string_view text);

}  // namespace ge::shooter
