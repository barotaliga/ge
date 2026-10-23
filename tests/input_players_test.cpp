// The players (engine/input/players.hpp): one player owns every device and hears its loss; two
// players join by pressing; a keyboard brings the mouse; an unplugged pad comes back to its
// player.
//
// ge:summary Tests: join on press, keyboard and mouse together, lost and returned devices
// ge:keywords test input players join lost returned hot-plug reconnect
// ge:skill input testing
#include "check.hpp"
#include "input/names.hpp"
#include "input/players.hpp"
#include "input/sim.hpp"

namespace in = ge::input;

namespace {
struct Rig {
  in::SimInput sim;
  in::InputState s;
  in::Players pl;
  void frame() {
    in::sim_frame(sim, s);
    in::players_update(pl, s);
  }
};
}  // namespace

TEST(one_player_owns_every_device_and_hears_when_his_pad_comes_out_and_back) {
  Rig r;
  const auto pad = in::sim_plug(r.sim, in::sim_pad());
  r.frame();
  CHECK(in::devices_of(r.pl, 0).empty());
  in::sim_button(r.sim, pad, 0, true);
  r.frame();
  CHECK(r.pl.events_n == 1 && r.pl.events[0].kind == in::PlayerEvent::joined);
  CHECK(in::owner_of(r.pl, pad) == 0);
  in::sim_unplug(r.sim, pad);
  r.frame();
  CHECK(r.pl.events_n == 1 && r.pl.events[0].kind == in::PlayerEvent::lost);
  CHECK(r.pl.p[0].waiting);
  const auto again = in::sim_plug(r.sim, in::sim_pad());
  r.frame();
  CHECK(r.pl.events_n == 1 && r.pl.events[0].kind == in::PlayerEvent::returned);
  CHECK(in::owner_of(r.pl, again) == 0 && !r.pl.p[0].waiting);
}

TEST(two_players_join_in_turn_and_the_keyboard_brings_the_mouse) {
  Rig r;
  r.pl.most = 2;
  const auto kb = in::sim_plug(r.sim, in::sim_keyboard());
  const auto mouse = in::sim_plug(r.sim, in::sim_mouse());
  const auto pad = in::sim_plug(r.sim, in::sim_pad());
  const auto pad2 = in::sim_plug(r.sim, in::sim_pad(in::Family::nintendo));
  r.frame();
  in::sim_button(r.sim, pad, 0, true);
  r.frame();
  in::sim_key(r.sim, kb, in::key::enter, true);
  r.frame();
  CHECK(in::owner_of(r.pl, pad) == 0);
  CHECK(in::owner_of(r.pl, kb) == 1 && in::owner_of(r.pl, mouse) == 1);
  CHECK(in::has_keyboard(r.pl, r.s, 1) && !in::has_keyboard(r.pl, r.s, 0));
  CHECK(in::devices_of(r.pl, 0).size() == 1 && in::devices_of(r.pl, 0)[0] == pad);
  in::sim_button(r.sim, pad2, 0, true);
  r.frame();
  CHECK(in::owner_of(r.pl, pad2) == -1);  // the game takes two
}

TEST(an_unplugged_pad_goes_back_to_its_own_player_before_anyone_else) {
  Rig r;
  r.pl.most = 2;
  const auto a = in::sim_plug(r.sim, in::sim_pad(in::Family::xbox));
  const auto b = in::sim_plug(r.sim, in::sim_pad(in::Family::playstation));
  r.frame();
  in::sim_button(r.sim, a, 0, true);
  in::sim_button(r.sim, b, 0, true);
  r.frame();
  CHECK(in::owner_of(r.pl, a) == 0 && in::owner_of(r.pl, b) == 1);
  in::sim_unplug(r.sim, b);
  r.frame();
  CHECK(r.pl.p[1].waiting && r.pl.events[0].player == 1);
  const auto other = in::sim_plug(r.sim, in::sim_pad(in::Family::nintendo));  // not his model
  r.frame();
  CHECK(in::owner_of(r.pl, other) == -1 && r.pl.p[1].waiting);
  const auto back = in::sim_plug(r.sim, in::sim_pad(in::Family::playstation));
  r.frame();
  CHECK(in::owner_of(r.pl, back) == 1 && !r.pl.p[1].waiting);
}
