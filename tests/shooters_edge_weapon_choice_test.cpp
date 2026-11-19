// WEAPON CHOICE: `weapon <n> [n ...]` asks for the first weapon of its list the player can fire
// (or the list's first when none); preselection holds the choice for the next attack press (2:
// at once while attacking; 3 and 4 only in deathmatch mode 1); hiding asks for the shotgun (or the
// axe) when the attack is let go; forgetting the order rewrites the list; a list `impulse` chooses
// at once; `weapon 10` steps to the next weapon up.
//
// ge:summary Tests: the weapon command, preselection, hiding and the list impulse
// ge:keywords test weapon-choice weapon-command cl_weaponpreselect cl_weaponhide
// ge:keywords cl_weaponforgetorder impulse-list
// ge:skill shooter testing
#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/weapon_choice.hpp"

namespace cl = shooters_edge::client;
namespace hud = shooters_edge::hud;
namespace sh = ge::shooter;

namespace {

// A player carrying `guns` (bit k - 1 for weapon k + 1: 1 the shotgun) with `ammo` of each kind.
hud::Status carrying(uint8_t guns, int ammo) {
  hud::Status s;
  s.owned = guns;
  s.ammo.fill(ammo);
  return s;
}

constexpr uint8_t kShotgun = 1, kNailgun = 4, kRockets = 32, kBeam = 64;

struct Window {
  ge::console::Console con;
  cl::ControlSettings c;
  int dm = 1;
  Window() {
    cl::add_cvars(con, c);
    c.choice.deathmatch = [this] { return dm; };
  }
  // One frame: the console's lines run, then the moves made from them with `attack` held.
  int frame(const hud::Status& s, bool attack = false, const char* line = nullptr) {
    if (line != nullptr) con.execute(line);
    con.run();
    sh::Moves m;
    m.attack = attack;
    cl::choice_moves(c.choice, s, m);
    return m.impulse;
  }
};

}  // namespace

TEST(the_weapon_command_asks_for_the_best_weapon_it_can_fire) {
  Window w;
  const hud::Status s = carrying(kShotgun | kRockets | kNailgun, 20);
  CHECK(w.frame(s, false, "weapon 8 7 4") == 7);  // no beam gun: the launcher
  CHECK(w.frame(s) == 0);                         // asked once
  const hud::Status empty = carrying(kShotgun | kRockets, 0);
  CHECK(w.frame(empty, false, "weapon 7 2 1") == 1);  // no rockets, no shells: the axe
  CHECK(w.frame(carrying(kShotgun, 0), false, "weapon 7 8") == 7);  // none: the first as typed
  const hud::Status one_shell = carrying(kShotgun | 2, 1);
  CHECK(w.frame(one_shell, false, "weapon 3 2") == 2);  // the double shotgun takes two shells
}

TEST(preselection_waits_for_the_attack_and_hiding_puts_the_weapon_away) {
  Window w;
  const hud::Status s = carrying(kShotgun | kRockets | kBeam, 20);
  w.con.execute("cl_weaponpreselect 1");
  CHECK(w.frame(s, false, "weapon 7") == 0);  // only chosen
  CHECK(w.frame(s, true) == 7);               // asked with the attack, in the same command
  CHECK(w.frame(s, true) == 0);               // held: asked once
  w.con.execute("cl_weaponpreselect 2");
  CHECK(w.frame(s, true, "weapon 8") == 8);   // attacking: at once
  CHECK(w.frame(s, false, "weapon 7") == 0);  // the release, and the choice only kept
  w.con.execute("cl_weaponpreselect 3");
  w.dm = 3;
  CHECK(w.frame(s, false, "weapon 8") == 8);  // mode 1 only in deathmatch 1: here at once
  w.dm = 1;
  CHECK(w.frame(s, false, "weapon 7") == 0);
  CHECK(w.frame(s, true) == 7);
  // Hiding: the release asks for the shotgun, the axe without shells or with cl_weaponhide_axe.
  w.con.execute("cl_weaponpreselect 0");
  w.con.execute("cl_weaponhide 1");
  CHECK(w.frame(s, false) == 2);
  CHECK(w.frame(carrying(kShotgun, 0), true) == 0);
  CHECK(w.frame(carrying(kShotgun, 0), false) == 1);
  w.con.execute("cl_weaponhide 2");
  w.dm = 4;
  w.frame(s, true);
  CHECK(w.frame(s, false) == 0);  // deathmatch 1 only
}

TEST(forgetting_the_order_and_the_list_impulse) {
  Window w;
  const hud::Status s = carrying(kShotgun | kRockets | kNailgun, 20);
  w.con.execute("cl_weaponforgetorder 1");
  CHECK(w.frame(s, false, "weapon 8 7") == 7);
  CHECK(w.c.choice.list[0] == 7 && w.c.choice.list[1] == 2 && w.c.choice.list[2] == 1);
  w.con.execute("cl_weaponforgetorder 0");
  w.con.execute("cl_weaponpreselect 1");
  CHECK(w.frame(s, false, "impulse 8 4") == 4);  // a list impulse: at once, whatever the mode
  CHECK(w.frame(s, false, "impulse 7") == 0);    // one number: the button's own (button_moves)
  w.con.execute("cl_weaponpreselect 0");
  CHECK(w.frame(s, false, "weapon 2") == 2);
  CHECK(w.frame(s, false, "weapon 10") == 4);  // the next up from the shotgun it can fire
}
