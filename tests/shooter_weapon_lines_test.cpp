// THE SHOOTER'S WEAPON LINES: the server's no weapon and not enough ammo lines hidden only when
// their setting is on, by the line's beginning.
//
// ge:summary Tests: the shooter's no weapon and not enough ammo lines hidden
// ge:keywords test ignore_no_weapon ignore_not_enough_ammo weapon-lines
// ge:skill shooter testing
#include "check.hpp"
#include "console/console.hpp"
#include "shooter/weapon_lines.hpp"

namespace sh = ge::shooter;

TEST(the_weapon_lines_hide_only_when_asked) {
  ge::console::Console con;
  sh::WeaponLines w;
  sh::add_weapon_line_cvars(con.cvars(), w);
  CHECK(!sh::weapon_line_hidden(w, "no weapon.\n"));
  con.execute("ignore_no_weapon 1");
  con.run();
  CHECK(sh::weapon_line_hidden(w, "no weapon.\n"));
  CHECK(!sh::weapon_line_hidden(w, "not enough ammo.\n"));
  CHECK(!sh::weapon_line_hidden(w, "Player: no weapon here\n"));  // only the line's beginning
  con.execute("ignore_not_enough_ammo 1");
  con.run();
  CHECK(sh::weapon_line_hidden(w, "not enough ammo.\n"));
}
