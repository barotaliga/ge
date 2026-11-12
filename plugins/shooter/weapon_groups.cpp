// Weapon groups for any shooter (weapon_groups.hpp).
//
// ge:summary Weapon groups for any shooter: the press, the fire and the look
// ge:keywords weapon-groups group_press group_fire group_look
// ge:skill shooter input
// ge:test tests/shooter_weapon_groups_test.cpp
#include "shooter/weapon_groups.hpp"

#include <algorithm>

namespace ge::shooter {

namespace {

bool bit(uint32_t bits, int w) { return w > 0 && w < 32 && (bits >> w & 1u) != 0; }

// The group is open at `t` (pressed under kOpenS ago, not fired since).
bool is_open(const GroupState& s, double t) {
  return s.group >= 0 && s.closed_at < s.pressed_at && t - s.pressed_at < kOpenS;
}

// When the group began (or begins) to close.
double close_start(const GroupState& s) {
  return s.closed_at >= s.pressed_at ? s.closed_at : s.pressed_at + kOpenS;
}

}  // namespace

bool is_group(const WeaponGroups& g, int key) {
  return key >= 1 && key <= kMaxGroups && !g[static_cast<size_t>(key - 1)].empty();
}

int group_of(const WeaponGroups& g, int weapon) {
  if (weapon <= 0) return -1;
  for (size_t k = 0; k < g.size(); ++k)
    if (std::find(g[k].begin(), g[k].end(), weapon) != g[k].end()) return static_cast<int>(k);
  return -1;
}

int group_press(GroupState& s, const WeaponGroups& g, const GroupArmed& a, int key, double t) {
  if (!is_group(g, key)) return 0;
  const std::vector<int>& list = g[static_cast<size_t>(key - 1)];
  const auto n = list.size();
  // Again while open: start after the weapon last asked for; else from the best.
  size_t from = 0;
  if (is_open(s, t) && s.group == key - 1) {
    const auto at = std::find(list.begin(), list.end(), s.asked);
    if (at != list.end()) from = static_cast<size_t>(at - list.begin()) + 1;
  }
  s.group = key - 1;
  s.pressed_at = t;
  int pick = 0;
  for (size_t i = 0; i < n && pick == 0; ++i) {
    const int w = list[(from + i) % n];
    if (bit(a.ready, w)) pick = w;
  }
  if (pick != 0) s.asked = pick;
  return pick;
}

void group_fire(GroupState& s, double t) {
  if (is_open(s, t)) s.closed_at = t;
}

GroupLook group_look(const GroupState& s, const WeaponGroups& g, const GroupArmed& a, double t) {
  GroupLook l;
  auto fill = [&](int group) {
    for (const int w : g[static_cast<size_t>(group)])
      if (bit(a.carried, w) && l.count < kGroupMost) {
        l.weapons[static_cast<size_t>(l.count)] = w;
        l.ready[static_cast<size_t>(l.count)] = bit(a.ready, w);
        ++l.count;
      }
  };
  auto index_of = [&](int w) {
    for (int i = 0; i < l.count; ++i)
      if (l.weapons[static_cast<size_t>(i)] == w) return i;
    return -1;
  };
  const double fold = t - close_start(s);
  if (s.group >= 0 && fold < kCloseS) {  // open, or folding away
    l.group = s.group;
    fill(s.group);
    l.lit = index_of(s.asked);
    if (l.lit < 0) l.lit = index_of(a.in_hand);
    l.open = fold <= 0 ? 1.0f : static_cast<float>(1 - fold / kCloseS);
  } else {  // closed: the weapon in hand alone
    l.group = group_of(g, a.in_hand);
    if (a.in_hand > 0) {
      l.weapons[0] = a.in_hand;
      l.ready[0] = bit(a.ready, a.in_hand);
      l.count = 1;
      l.lit = 0;
    }
  }
  if (l.lit >= 0) {
    const int w = l.weapons[static_cast<size_t>(l.lit)];
    const int home = group_of(g, w);
    if (home >= 0)
      for (const int o : g[static_cast<size_t>(home)])
        if (o != w && bit(a.carried, o)) l.more = true;
  }
  return l;
}

}  // namespace ge::shooter
