// THE QUICK WEAPON: a fire key fires the best weapon carried by its list, then puts the usual one
// back. Driven against a small server model that drops a weapon's ask while the weapon in hand is
// reloading and shows the client its state a round trip late: a tap fires exactly one shot of the
// best weapon and ends on the kept one; a held key fires on, moving down the list as each runs dry;
// a key that is off fires what is in hand; the player's own weapon key wins; a dead player is let
// be; the look ghosts the weapon +attack would fire and turns it solid as it is pressed.
//
// ge:summary Tests: quick weapon taps, holds, lists, the way back and the preview, against a
// ge:summary model server
// ge:keywords test quick-weapon quick_step quick_look weapon_list best_ready
// ge:skill shooter testing
#include <array>
#include <deque>

#include "check.hpp"
#include "shooter/quick_weapon.hpp"

namespace sh = ge::shooter;

namespace {

constexpr double kDt = 1.0 / 77;

// The server's side: weapons 1 (melee) .. 8, their ammunition and refire.
struct Server {
  int in_hand = 2;
  std::array<int, 9> ammo{0, 0, 20, 20, 50, 50, 5, 5, 30};  // by weapon (1 needs none)
  std::array<double, 9> refire{0, 0.5, 0.5, 0.7, 0.1, 0.1, 0.6, 0.8, 0.1};
  uint32_t carried = 0x1FE;  // 1..8
  double free_at = 0;
  std::array<int, 9> shots{};
  // One command at `t`: a weapon's ask only while the weapon in hand may fire (else dropped), then
  // the attack.
  void run(const sh::QuickCommand& c, double t) {
    if (t < free_at) return;
    if (c.select != 0 && (carried >> c.select & 1u) != 0 && (c.select == 1 || ammo[c.select] > 0))
      in_hand = c.select;
    if (c.attack) {
      if (in_hand != 1) {
        if (ammo[in_hand] <= 0) return;
        --ammo[in_hand];
      }
      ++shots[in_hand];
      free_at = t + refire[in_hand];
    }
  }
  sh::QuickArmed armed() const {
    sh::QuickArmed a;
    for (int w = 1; w <= 8; ++w)
      if ((carried >> w & 1u) != 0 && (w == 1 || ammo[w] > 0)) a.ready |= 1u << w;
    a.in_hand = in_hand;
    for (int w = 1; w <= 8; ++w) a.ammo[static_cast<size_t>(w)] = w == 1 ? -1 : ammo[w];
    return a;
  }
};

struct Run {
  Server server;
  sh::QuickState q;
  sh::QuickSettings s;
  std::deque<sh::QuickArmed> seen;  // what the client sees: the server `lag` frames ago
  int lag = 6;
  double t = 0;
  Run() {
    s.on = {true, true};
    s.prio[0] = sh::weapon_list("7 8 5 6 3 2 4 1");
    s.prio[1] = sh::weapon_list("8 3 2");
    s.fallback = sh::weapon_list("2 1");
  }
  sh::QuickCommand frame(bool fire0, bool fire1 = false, int own = 0) {
    seen.push_back(server.armed());
    while (static_cast<int>(seen.size()) > lag + 1) seen.pop_front();
    const sh::QuickCommand c =
        sh::quick_step(q, s, seen.front(), {.fire = {fire0, fire1}, .own_select = own}, t);
    server.run(c, t);
    t += kDt;
    return c;
  }
  void idle(int frames) {
    for (int i = 0; i < frames; ++i) frame(false);
  }
};

}  // namespace

TEST(lists_read_weapon_numbers_once_each_in_order) {
  CHECK(sh::weapon_list("7 8 5 6 3 2 4 1") == (std::vector<int>{7, 8, 5, 6, 3, 2, 4, 1}));
  CHECK(sh::weapon_list("\"2,1\"") == (std::vector<int>{2, 1}));
  CHECK(sh::weapon_list("0 9 32 7 7 31") == (std::vector<int>{9, 7, 31}));
  CHECK(sh::weapon_list("").empty());
}

TEST(a_tap_fires_one_rocket_and_puts_the_shotgun_back) {
  Run r;
  r.idle(10);
  const sh::QuickCommand first = r.frame(true);
  CHECK(first.attack && first.select == 7);  // asked for with the attack, in the same command
  r.idle(200);
  CHECK(r.server.shots[7] == 1);
  CHECK(r.server.shots[2] == 0);
  CHECK(r.server.in_hand == 2);
  CHECK(!r.q.returning && r.q.key == -1);
}

TEST(a_tap_while_the_shotgun_reloads_still_fires_the_rocket) {
  Run r;
  r.server.free_at = 0.3;  // the shotgun just fired: the ask is dropped until it is free
  r.frame(true);
  r.idle(200);
  CHECK(r.server.shots[7] == 1);
  CHECK(r.server.shots[2] == 0);  // never the shotgun by mistake
  CHECK(r.server.in_hand == 2);
}

TEST(a_held_key_fires_on_down_the_list_as_each_runs_dry) {
  Run r;
  r.server.ammo[7] = 2;
  r.server.ammo[8] = 3;
  for (int i = 0; i < 77 * 4; ++i) r.frame(true);
  CHECK(r.server.shots[7] == 2);
  CHECK(r.server.shots[8] == 3);
  CHECK(r.server.shots[5] > 10);  // the nailgun next
  r.idle(200);
  CHECK(r.server.in_hand == 2);
}

TEST(the_second_key_fires_its_own_list) {
  Run r;
  r.server.carried = 0x1FE & ~(1u << 8);  // no beam gun: the double shotgun
  r.frame(false, true);
  r.idle(200);
  CHECK(r.server.shots[3] == 1);
  CHECK(r.server.shots[7] == 0);
  CHECK(r.server.in_hand == 2);
}

TEST(a_key_that_is_off_fires_what_is_in_hand) {
  Run r;
  r.s.on[0] = false;
  const sh::QuickCommand c = r.frame(true);
  CHECK(c.attack && c.select == 0);
  r.idle(100);
  CHECK(r.server.shots[2] == 1);
  r.s.on[0] = true;
  r.s.prio[0].clear();  // on, but an empty list: the same
  r.idle(50);
  CHECK(r.frame(true).select == 0);
}

TEST(the_players_own_weapon_key_wins_over_the_way_back) {
  Run r;
  r.frame(true);
  r.idle(40);  // the rocket fired; the shotgun asked for, the launcher still reloading
  CHECK(r.server.shots[7] == 1);
  CHECK(r.q.returning);
  const sh::QuickCommand own = r.frame(false, false, 5);
  CHECK(own.select == 5 && !r.q.returning);
  bool asked_back = false;
  for (int i = 0; i < 200; ++i) asked_back |= r.frame(false).select != 0;
  CHECK(!asked_back);  // the way back dropped: never the shotgun again
}

TEST(a_dead_player_is_let_be) {
  Run r;
  sh::QuickArmed dead = r.server.armed();
  dead.alive = false;
  const sh::QuickCommand c = sh::quick_step(r.q, r.s, dead, {.fire = {true, false}}, 0);
  CHECK(c.attack && c.select == 0);
  CHECK(sh::quick_look(r.q, r.s, dead, 0).ghost == false);
}

TEST(the_look_ghosts_the_weapon_attack_would_fire) {
  Run r;
  r.lag = 0;
  r.idle(2);
  sh::QuickLook look = sh::quick_look(r.q, r.s, r.server.armed(), r.t);
  CHECK(look.ghost && look.weapon == 7 && look.opacity == sh::kGhost);
  r.s.preview = false;
  CHECK(!sh::quick_look(r.q, r.s, r.server.armed(), r.t).hide);  // neither asked for
  r.s.hide = true;
  look = sh::quick_look(r.q, r.s, r.server.armed(), r.t);
  CHECK(look.hide && !look.ghost);
  r.s.preview = true;
  // Pressed while the shotgun reloads: the ghost turns solid while the rocket is not yet in hand.
  r.server.free_at = r.t + 1;
  r.frame(true);
  const double pressed = r.q.pressed_at;
  look = sh::quick_look(r.q, r.s, r.server.armed(), pressed + sh::kSolidS / 2);
  CHECK(look.ghost && look.weapon == 7 && look.opacity > sh::kGhost && look.opacity < 1);
  look = sh::quick_look(r.q, r.s, r.server.armed(), pressed + sh::kSolidS);
  CHECK(look.opacity == 1);
  // The launcher in hand: no ghost.
  r.server.in_hand = 7;
  CHECK(!sh::quick_look(r.q, r.s, r.server.armed(), r.t).ghost);
  // With cl_quickweapon off, no look at all.
  r.s.on[0] = false;
  r.server.in_hand = 2;
  r.q = {};
  CHECK(!sh::quick_look(r.q, r.s, r.server.armed(), r.t).ghost);
}
