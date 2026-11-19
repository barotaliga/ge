// The weapon command, preselection and hiding (weapon_choice.hpp).
//
// ge:summary The weapon command, preselection on the attack press and hiding on its release
// ge:keywords weapon-choice weapon-command cl_weaponpreselect cl_weaponhide choice_moves
// ge:skill input shooter
// ge:test tests/shooters_edge_weapon_choice_test.cpp
#include "shooters_edge/client/weapon_choice.hpp"

#include <string>

#include "console/cvar.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/quick_fire.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr int kMaxWeapon = 8;  // the weapon keys a list may choose
constexpr int kNextUp = 10, kNextDown = 12;

bool ready(const hud::Status& s, int n) {
  return n >= 1 && n <= kMaxWeapon && (quick_armed(s).ready >> n & 1u) != 0;
}

int deathmatch_of(const WeaponChoice& w) { return w.deathmatch ? w.deathmatch() : 0; }

// The preselection in force: 3 and 4 are 1 and 2 in deathmatch mode 1, else 0.
int preselect_now(const WeaponChoice& w) {
  if (w.preselect <= 2) return w.preselect;
  return deathmatch_of(w) == 1 ? w.preselect - 2 : 0;
}

bool hide_now(const WeaponChoice& w) {
  return w.hide == 1 || (w.hide == 2 && deathmatch_of(w) == 1);
}

// The list's first entry stepped to the next ready weapon up or down from the one chosen now.
void step_list(WeaponChoice& w, const hud::Status& s, int dir) {
  const int from = w.last;
  int found = 1;
  for (int k = 1; k <= kMaxWeapon; ++k) {
    const int n = ((from - 1 + dir * k) % kMaxWeapon + kMaxWeapon) % kMaxWeapon + 1;
    if (ready(s, n)) {
      found = n;
      break;
    }
  }
  w.list[0] = found;
}

// A choice made (not one only shown): the list forgotten down to it with cl_weaponforgetorder.
int chosen(WeaponChoice& w, int n) {
  if (n >= 1 && n <= kMaxWeapon) w.last = n;
  if (w.forget && n != 0) {
    w.list.fill(0);
    w.list[0] = n;
    if (n == 2 || w.hide_axe)
      w.list[1] = 1;
    else
      w.list[1] = 2, w.list[2] = 1;
  }
  return n;
}

bool set_list(WeaponChoice& w, const con::Args& a) {
  if (a.size() < 2) return false;
  w.list.fill(0);
  for (size_t i = 1; i < a.size() && i <= w.list.size(); ++i) {
    double n = 0;
    w.list[i - 1] = con::parse_number(a[i], n) ? static_cast<int>(n) : 0;
  }
  return true;
}

}  // namespace

void add_choice_cvars(con::Console& c, WeaponChoice& w) {
  con::CvarLayer classic(c.cvars(), kLayerClassic);
  con::Cvars& v = c.cvars();
  v.bind({.name = "cl_weaponpreselect",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether a weapon key only picks the weapon, which the next attack brings up.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 4,
          .values = "0: the weapon at once (the default); 1: on the next attack; 2: as 1, but at "
                    "once while attacking; 3, 4: as 1 and 2 in deathmatch mode 1 only"},
         &w.preselect);
  v.bind({.name = "cl_weaponhide",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether letting go of the attack puts the weapon away for the shotgun.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: never (the default); 1: always; 2: in deathmatch mode 1 only"},
         &w.hide);
  v.bind({.name = "cl_weaponhide_axe",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether the weapon put away for is the axe rather than the shotgun.",
          .kind = con::CvarKind::boolean,
          .values = "0: the shotgun (the default); 1: the axe"},
         &w.hide_axe);
  v.bind({.name = "cl_weaponforgetorder",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether a weapon key forgets its list once it has chosen.",
          .kind = con::CvarKind::boolean,
          .values = "0: the list stays the order to choose from (the default); 1: the weapon "
                    "chosen, then the shotgun and the axe"},
         &w.forget);
  c.add_command("weapon",
                "weapon <n> [n ...]: the first weapon of the list you can fire (10 the next up, "
                "12 the next down)",
                [&w](con::Console& k, const con::Args& a) {
                  if (!set_list(w, a))
                    k.print("weapon: weapon <1..8> [more ...], the best first");
                  else
                    w.asked = true;
                });
}

void choose_list(WeaponChoice& w, const con::Args& a) {
  if (set_list(w, a)) w.asked_now = true;
}

int best_of(const WeaponChoice& w, const hud::Status& s) {
  for (const int n : w.list)
    if (ready(s, n)) return n;
  return w.list[0];
}

void choice_moves(WeaponChoice& w, const hud::Status& s, ge::shooter::Moves& m) {
  const bool attack = m.attack;
  const bool pressed = attack && !w.was_attack, released = !attack && w.was_attack;
  w.was_attack = attack;
  if (w.asked || w.asked_now) {
    if (w.list[0] == kNextUp || w.list[0] == kNextDown)
      step_list(w, s, w.list[0] == kNextUp ? 1 : -1);
    const int mode = w.asked_now ? 0 : preselect_now(w);
    const int n = best_of(w, s);
    if (mode == 0 || (mode == 2 && attack && n != 0)) m.impulse = chosen(w, n);
    w.asked = w.asked_now = false;
  }
  if (pressed && w.preselect != 0) {
    const int n = best_of(w, s);
    if (n != 0) m.impulse = chosen(w, n);
  }
  if (released && hide_now(w)) {
    const bool shotgun = s.has(hud::Weapon::shotgun) && s.ammo_for(hud::Weapon::shotgun) >= 1;
    m.impulse = !w.hide_axe && shotgun ? 2 : 1;
  }
}

int deathmatch_now(const DemoState* joined, const Listen& l) {
  std::string text;
  if (joined != nullptr) {
    for (const auto& [key, value] : joined->serverinfo)
      if (key == "deathmatch") text = value;
  } else if (l.server != nullptr) {
    if (const con::Cvar* v = l.server->cvars.find("deathmatch")) text = v->value;
  }
  double n = 0;
  return con::parse_number(text, n) ? static_cast<int>(n) : 0;
}

void reset_choice(WeaponChoice& w) {
  w.list.fill(0);
  w.list[0] = 2;
  w.list[1] = 1;
  w.last = 2;
}

}  // namespace shooters_edge::client
