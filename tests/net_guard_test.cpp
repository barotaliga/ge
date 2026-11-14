// engine/net/guard.hpp: the server's anti-cheat checks. A field off its range is clamped and one
// off its grid snapped; a flood, a future view and a step too long are flagged; an honest
// player's score stays at nothing and a cheater's runs past the out score. The guard trial over
// the simulated network catches each of its five cheats and flags no honest player.
//
// ge:summary Test: the guard's checks one by one, then the guard trial's honest and cheating players
// ge:keywords test guard anti-cheat cheats clamp snap flood future teleport
// ge:skill net testing
#include "net/guard.hpp"

#include <string>

#include "check.hpp"
#include "net/guard_trial.hpp"

namespace gn = ge::net;

namespace {

gn::GuardRules stick_rules() {
  gn::GuardRules r;
  r.fields = {{0, 1, true, -100, 100, 10}, {1, 2, false, 0, 1000, 1}};
  r.tick_hz = 60;
  r.window_s = 1;
  r.flood_slack = 6;
  r.max_step = 0.5;
  return r;
}

}  // namespace

TEST(guard_clamps_a_field_off_its_range_and_snaps_one_off_its_grid) {
  gn::Guard g;
  gn::guard_init(g, stick_rules());
  uint8_t in[3] = {static_cast<uint8_t>(int8_t{127}), 0xe8, 0x03};  // 127, then 1000
  CHECK(gn::guard_input(g, 2, in, 5) == 1);
  CHECK(static_cast<int8_t>(in[0]) == 100);
  CHECK(in[1] == 0xe8 && in[2] == 0x03);  // in range: untouched
  uint8_t grid[3] = {static_cast<uint8_t>(int8_t{-37}), 7, 0};
  CHECK(gn::guard_input(g, 2, grid, 6) == 1);
  CHECK_MSG(static_cast<int8_t>(grid[0]) == -40, std::to_string(static_cast<int8_t>(grid[0])));
  uint8_t up[3] = {static_cast<uint8_t>(int8_t{36}), 0, 0};
  gn::guard_input(g, 2, up, 7);
  CHECK(static_cast<int8_t>(up[0]) == 40);
  uint8_t fine[3] = {static_cast<uint8_t>(int8_t{-90}), 0x10, 0};
  CHECK(gn::guard_input(g, 2, fine, 8) == 0);
  const gn::GuardClient& c = g.clients[2];
  CHECK(c.counts[static_cast<size_t>(gn::Cheat::off_range)] == 1);
  CHECK(c.counts[static_cast<size_t>(gn::Cheat::off_grid)] == 2);
  CHECK(g.flags.size() == 3 && g.flags[0].value == 127 && g.flags[1].value == -37);
  CHECK(gn::guard_summary(g, 2) == "off-range x1, off-grid x2; worst score 3.0");
}

TEST(guard_holds_a_view_from_the_future_and_flags_it) {
  gn::Guard g;
  gn::guard_init(g, stick_rules());
  CHECK(gn::guard_view(g, 1, (100u + 2u) << 8, 100) == (102u << 8));  // within the slack
  CHECK(g.raised == 0);
  CHECK(gn::guard_view(g, 1, 160u << 8, 100) == (100u << 8));
  CHECK(g.raised == 1 && g.flags[0].cheat == gn::Cheat::future && g.flags[0].value == 60);
}

TEST(guard_flags_a_flood_once_a_window_and_lets_a_burst_pass) {
  gn::Guard g;
  gn::guard_init(g, stick_rules());
  uint8_t in[3] = {0, 0, 0};
  // A burst after a lost packet: the window's 60 ticks and 6 more. Passes.
  for (uint32_t t = 1; t <= 60; t++) {
    for (int k = 0; k < (t == 30 ? 7 : 1); k++) gn::guard_input(g, 0, in, t);
    gn::guard_tick(g, t);
  }
  CHECK(g.raised == 0);
  // Three a tick: flagged at the window's end, with the count it sent.
  for (uint32_t t = 61; t <= 120; t++) {
    for (int k = 0; k < 3; k++) gn::guard_input(g, 0, in, t);
    gn::guard_tick(g, t);
  }
  CHECK(g.raised == 1 && g.flags[0].cheat == gn::Cheat::flood && g.flags[0].value == 180);
}

TEST(guard_flags_a_step_past_the_most_and_scores_decay) {
  gn::Guard g;
  gn::GuardRules r = stick_rules();
  r.out_score = 20;
  gn::guard_init(g, r);
  const double a[2] = {0, 0}, near[2] = {0.3, 0.3}, far[2] = {40, 0};
  CHECK(gn::guard_motion(g, 4, a, near, 1));
  CHECK(!gn::guard_motion(g, 4, a, far, 2));
  CHECK(g.flags.back().cheat == gn::Cheat::motion && g.flags.back().value == 800);  // 80x
  CHECK(g.clients[4].score == 8);
  for (uint32_t t = 0; t < 60 * 3; t++) gn::guard_tick(g, t);  // three seconds
  CHECK_MSG(g.clients[4].score < 5.01 && g.clients[4].score > 4.99,
            std::to_string(g.clients[4].score));
  CHECK(!gn::guard_out(g, 4));
  for (int k = 0; k < 3; k++) gn::guard_motion(g, 4, a, far, 200);
  CHECK(gn::guard_out(g, 4));
  gn::guard_reset(g, 4);
  CHECK(!gn::guard_out(g, 4) && g.clients[4].score == 0);
}

TEST(guard_trial_catches_every_cheat_and_flags_no_honest_player) {
  gn::GuardTrialConfig k;
  k.honest = 6;
  k.seconds = 15;
  const gn::GuardTrial o = gn::run_guard_trial(k);
  CHECK(o.players.size() == 11);
  for (const gn::GuardTrialPlayer& p : o.players) {
    const std::string who = gn::trial_cheat_name(p.cheat);
    if (p.cheat == gn::TrialCheat::none) {
      CHECK_MSG(p.worst == 0 && !p.out, who + " flagged");
      CHECK_MSG(p.travelled > 10, who + " never moved");
    } else {
      CHECK_MSG(p.out, who + " not caught, worst score " + std::to_string(p.worst));
    }
    // Nobody moves faster than full stick, the cheaters included.
    CHECK_MSG(p.top_speed <= o.full_stick_mps + 0.5, who + " " + std::to_string(p.top_speed));
  }
  const auto count = [&](gn::TrialCheat t, gn::Cheat c) {
    for (const gn::GuardTrialPlayer& p : o.players)
      if (p.cheat == t) return p.counts[static_cast<size_t>(c)];
    return 0u;
  };
  CHECK(count(gn::TrialCheat::speed, gn::Cheat::off_range) > 0);
  CHECK(count(gn::TrialCheat::off_grid, gn::Cheat::off_grid) > 0);
  CHECK(count(gn::TrialCheat::flood, gn::Cheat::flood) > 0);
  CHECK(count(gn::TrialCheat::future, gn::Cheat::future) > 0);
  CHECK(count(gn::TrialCheat::teleport, gn::Cheat::motion) > 0);
  // The same config, the same trial.
  const gn::GuardTrial again = gn::run_guard_trial(k);
  CHECK(again.flags == o.flags && again.players[2].travelled == o.players[2].travelled);
}
