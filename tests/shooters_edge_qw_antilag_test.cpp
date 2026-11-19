// Tests: lag compensation's arithmetic (games/shooters_edge/net/qw_antilag.hpp): the target
// time a shot is traced at (the acknowledged frame plus the lead, a frame's time on a short ping,
// never past now), a player's position interpolated between samples, held at the oldest, and
// extrapolated past the newest by at most the lead, the ring keeping its newest samples, the cap
// and the frame back, the box kept, a teleport starting afresh, and a player's opt-out.
//
// ge:summary Tests: antilag target times, interpolation, extrapolation and the sample ring
// ge:keywords test antilag lag-compensation rewind interpolate extrapolate ring hitscan
// ge:keywords sv_antilag_max crouch teleport cl_antilag
// ge:skill net shooter testing
#include <string>

#include "check.hpp"
#include "shooters_edge/net/qw_antilag.hpp"

namespace cn = shooters_edge::net;
namespace sh = ge::shooter;

TEST(the_target_time_leads_the_acknowledged_frame) {
  CHECK(cn::antilag_target(1.0, 0.1, 77, 2.0) == 1.0 + cn::kAntilagLead);
  CHECK(cn::antilag_target(1.0, 0.01, 100, 2.0) == 1.0 + 1.0 / 100);
  CHECK(cn::antilag_target(1.0, 0.01, 5, 2.0) == 1.0 + 1.0 / 77);  // a bad maxfps
  CHECK(cn::antilag_target(1.0, 0.1, 77, 2.0, true) == 1.0);       // sv_antilagexact: no lead
  CHECK(cn::antilag_target(1.99, 0.1, 77, 2.0) == 2.0);            // never in the future
}

TEST(positions_interpolate_hold_and_extrapolate) {
  cn::AntilagHistory h;
  cn::antilag_record(h, 3, 1.0, {0, 0, 0});
  cn::antilag_record(h, 3, 1.1, {10, 0, 0});
  cn::antilag_record(h, 3, 1.2, {20, 0, 0});
  const cn::AntilagRing& r = h.players[3];
  sh::Vec3f p;
  CHECK(cn::antilag_position(r, 1.05, {}, p) && p.x > 4.99f && p.x < 5.01f);
  CHECK(cn::antilag_position(r, 0.5, {}, p) && p.x == 0);
  CHECK(cn::antilag_position(r, 1.5, {100, 0, 0}, p));
  CHECK(p.x > 21.99f && p.x < 22.01f);  // 20 + 100 x the 0.02 s lead at most
  CHECK(!cn::antilag_position(h.players[4], 1.0, {}, p));
  cn::antilag_clear(h, 3);
  CHECK(h.players[3].count == 0);
}

TEST(the_ring_keeps_the_newest_samples) {
  cn::AntilagHistory h;
  for (int i = 0; i < cn::kAntilagSamples + 10; i++)
    cn::antilag_record(h, 0, i * 0.01, {static_cast<float>(i), 0, 0});
  const cn::AntilagRing& r = h.players[0];
  CHECK(r.count == cn::kAntilagSamples);
  sh::Vec3f p;
  CHECK(cn::antilag_position(r, 0, {}, p) && p.x == 10);  // the oldest kept
}

TEST(the_target_time_is_held_to_the_cap_and_goes_back_a_frame) {
  // 300 ms of ping against a 250 ms cap: no further back than now less 0.25 s.
  CHECK(cn::antilag_target(1.0, 0.3, 77, 1.3, false, 0.25) == 1.3 - 0.25);
  CHECK(cn::antilag_target(1.0, 0.3, 77, 1.3, false, 0) == 1.0 + cn::kAntilagLead);  // no cap
  CHECK(cn::antilag_target(1.0, 0.1, 77, 2.0, false, 0, 0.1) == 1.0 + cn::kAntilagLead - 0.1);
}

TEST(a_rewound_player_keeps_the_box_he_had_and_a_teleport_starts_afresh) {
  cn::AntilagHistory h;
  cn::antilag_record(h, 1, 1.0, {0, 0, 0}, {-16, -16, -24}, {16, 16, 4});    // crouched
  cn::antilag_record(h, 1, 1.1, {10, 0, 0}, {-16, -16, -24}, {16, 16, 32});  // stood up
  cn::AntilagPlace p;
  CHECK(cn::antilag_place(h.players[1], 1.05, {}, p) && p.maxs.z == 4);
  CHECK(cn::antilag_place(h.players[1], 1.1, {}, p) && p.maxs.z == 32);
  // 2000 units in a tenth of a second is no run: the ring starts again from the new place.
  cn::antilag_record(h, 1, 1.2, {2000, 0, 0});
  CHECK(h.players[1].count == 1);
  CHECK(cn::antilag_place(h.players[1], 1.05, {}, p) && p.origin.x == 2000);
  // A fast run is kept: 150 units in 0.1 s.
  cn::antilag_record(h, 1, 1.3, {2150, 0, 0});
  CHECK(h.players[1].count == 2);
}

TEST(a_player_s_cl_antilag_opts_his_shots_out) {
  CHECK(cn::antilag_wanted(""));
  CHECK(cn::antilag_wanted("1"));
  CHECK(!cn::antilag_wanted("0"));
  CHECK(!cn::antilag_wanted("-1"));
  CHECK(std::string(cn::antilag_told(false)).find("server") != std::string::npos);
}
