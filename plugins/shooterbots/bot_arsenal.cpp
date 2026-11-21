// A game's weapons as a bot sees them (bot_arsenal.hpp): a weapon's part and row, its ammunition,
// its worth at a range, the launcher.
//
// ge:summary A game's weapons as a bot sees them: roles, rows, ammunition, worth, the launcher
// ge:keywords bot-arsenal weapon-score role_of weapon_row ammo_of loaded launcher_rounds
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_q2_nav_test.cpp
#include "shooterbots/bot_arsenal.hpp"

namespace ge::shooterbots {

WeaponRole role_of(const BotArsenal& a, int32_t bit) {
  if (bit == 0) return WeaponRole::none;
  if (bit == a.launcher) return WeaponRole::launcher;
  if (bit == a.lob) return WeaponRole::lob;
  if (bit == a.beam) return WeaponRole::beam;
  if (bit == a.heavy) return WeaponRole::heavy;
  return WeaponRole::none;
}

const BotWeapon* weapon_row(const BotArsenal& a, int32_t bit) {
  for (const BotWeapon& w : a.weapons)
    if (w.bit == bit) return &w;
  return nullptr;
}

float ammo_of(const Stats& st, int ammo) {
  return ammo >= 0 && ammo < kMaxAmmo ? st.ammo[ammo] : 1.f;
}

bool loaded(const Stats& st, const BotWeapon& w) {
  return (st.items & w.bit) != 0 && ammo_of(st, w.ammo) >= w.cost;
}

double weapon_score(const BotWeapon& w, float d) {
  switch (w.curve) {
    case WeaponCurve::blade:
      return d < 72 ? 30 : 0.1;
    case WeaponCurve::scatter:
      return 20;
    case WeaponCurve::double_:  // its spread is wider than a man past a few hundred units
      return d < 250 ? 60 : d < 350 ? 30 : 3;
    case WeaponCurve::spiker:
      return 35;
    case WeaponCurve::heavy:
      return d < 600 ? 65 : 40;
    case WeaponCurve::lob:  // indoors at a room's range the blast beats the spikes
      return d > 120 && d < 450 ? 70 : d > 100 && d < 550 ? 40 : 5;
    case WeaponCurve::rocket:
      return d > 130 ? 90 : 15;
    case WeaponCurve::beam:
      return d < 560 ? 85 : 5;
    case WeaponCurve::hitscan:
      return 45;
    case WeaponCurve::rail:
      return d > 300 ? 95 : 60;
    case WeaponCurve::big_blast:
      return d > 400 ? 100 : 2;
  }
  return 0;
}

float launcher_rounds(const Stats& st) {
  return st.arsenal->launcher != 0 ? ammo_of(st, st.arsenal->launcher_ammo) : 0.f;
}

bool holds_launcher(const Stats& st) {
  return st.arsenal->launcher != 0 && st.weapon == st.arsenal->launcher;
}

bool has_launcher(const Stats& st) { return (st.items & st.arsenal->launcher) != 0; }

int launcher_number(const BotArsenal& a) {
  const BotWeapon* w = weapon_row(a, a.launcher);
  return w != nullptr ? w->number : 0;
}

}  // namespace ge::shooterbots
