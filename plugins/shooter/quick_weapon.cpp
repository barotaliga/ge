// Quick weapon (quick_weapon.hpp).
//
// ge:summary Quick weapon for any shooter: a fire key fires the best weapon carried, then
// ge:summary switches back
// ge:keywords quick-weapon quick_step quick_look weapon_list best_ready
// ge:skill shooter input
// ge:test tests/shooter_quick_weapon_test.cpp
#include "shooter/quick_weapon.hpp"

#include <algorithm>

namespace ge::shooter {

namespace {

bool is_ready(const QuickArmed& a, int w) {
  return w >= 1 && w <= kMaxWeapon && (a.ready >> w & 1u) != 0;
}

// The ask for `w` when it is not in hand and the last ask is `kResend` old.
int ask(QuickState& q, const QuickArmed& a, int w, double t) {
  if (w == 0 || a.in_hand == w || t - q.sent_at < kResend) return 0;
  q.sent_at = t;
  return w;
}

// The weapon a fire key would fire: its list's best, else what is in hand.
int target_of(const QuickSettings& s, const QuickArmed& a, int key) {
  const int w = best_ready(a, s.prio[static_cast<size_t>(key)]);
  return w != 0 ? w : a.in_hand;
}

bool quick(const QuickSettings& s, int key) {
  const auto k = static_cast<size_t>(key);
  return s.on[k] && !s.prio[k].empty();
}

}  // namespace

std::vector<int> weapon_list(std::string_view text) {
  std::vector<int> out;
  int n = -1;
  for (size_t i = 0; i <= text.size(); ++i) {
    const char c = i < text.size() ? text[i] : ' ';
    if (c >= '0' && c <= '9') {
      n = std::min((n < 0 ? 0 : n) * 10 + (c - '0'), 1000);
      continue;
    }
    if (n >= 1 && n <= kMaxWeapon && std::find(out.begin(), out.end(), n) == out.end())
      out.push_back(n);
    n = -1;
  }
  return out;
}

int best_ready(const QuickArmed& a, const std::vector<int>& list) {
  for (int w : list)
    if (is_ready(a, w)) return w;
  return 0;
}

QuickCommand quick_step(QuickState& q, const QuickSettings& s, const QuickArmed& a,
                        const QuickKeys& keys, double t) {
  QuickCommand out;
  const std::array<bool, 2> was = q.held;
  q.held = keys.fire;
  // A key that is no quick fire is a plain attack.
  for (int k = 0; k < 2; ++k)
    if (keys.fire[static_cast<size_t>(k)] && !quick(s, k)) out.attack = true;
  if (!a.alive) {  // the attack respawns him; nothing to switch
    q.key = -1;
    q.returning = false;
    out.attack = keys.fire[0] || keys.fire[1];
    return out;
  }
  if (keys.own_select != 0) q.returning = false;
  // A new press: the last one pressed owns the attack.
  for (int k = 1; k >= 0; --k) {
    const auto i = static_cast<size_t>(k);
    if (keys.fire[i] && !was[i] && quick(s, k)) {
      q.key = k;
      q.target = 0;
      q.pressed_at = t;
      q.sent_at = -1e9;
      q.returning = false;
      break;
    }
  }
  if (q.key >= 0) {
    const int target = target_of(s, a, q.key);
    const int ammo = target >= 1 && target <= kMaxWeapon ? a.ammo[static_cast<size_t>(target)] : -1;
    if (target != q.target) {
      q.target = target;
      q.target_ammo = ammo;
      q.seen = false;
    }
    q.target_ammo = std::max(q.target_ammo, ammo);  // a pickup is no shot
    if (a.in_hand == target && !q.seen) {
      q.seen = true;
      q.seen_at = t;
    }
    // The same command switches and fires, so the first state with it in hand may show the shot.
    const bool fired = ammo >= 0 ? ammo < q.target_ammo : q.seen && t - q.seen_at >= kMeleeHold;
    const bool holding = keys.fire[static_cast<size_t>(q.key)];
    if (holding || (!fired && t - q.pressed_at < kLatchMax)) {
      out.attack = true;
      // Asked in every command until it is in hand: a server that keeps a weapon's ask only for
      // the command it came in (and lets a reloading weapon refuse it) switches, then fires, on
      // the first command the weapon is free.
      if (keys.own_select == 0 && a.in_hand != target) out.select = target;
    } else {
      q.key = -1;
      q.returning = true;
      q.return_at = t;
      q.sent_at = -1e9;
    }
  }
  if (q.key < 0 && q.returning) {
    const int home = best_ready(a, s.fallback);
    if (home == 0 || a.in_hand == home || t - q.return_at > kReturnMax)
      q.returning = false;
    else if (keys.own_select == 0)
      out.select = ask(q, a, home, t);
  }
  if (keys.own_select != 0) out.select = keys.own_select;
  return out;
}

QuickLook quick_look(const QuickState& q, const QuickSettings& s, const QuickArmed& a, double t) {
  QuickLook look;
  if (!a.alive || !quick(s, 0) || (!s.preview && !s.hide)) return look;
  int w = 0;
  float opacity = kGhost;
  if (q.key == 0) {  // pressed: the ghost turns solid until the server has it in hand
    w = q.target;
    opacity = kGhost +
              (1 - kGhost) * static_cast<float>(std::clamp((t - q.pressed_at) / kSolidS, 0.0, 1.0));
  } else if (q.key < 0) {
    w = best_ready(a, s.prio[0]);
  }
  if (w == 0 || w == a.in_hand) return look;
  if (s.preview) {
    look.ghost = true;
    look.weapon = w;
    look.opacity = opacity;
  } else {
    look.hide = true;
  }
  return look;
}

}  // namespace ge::shooter
