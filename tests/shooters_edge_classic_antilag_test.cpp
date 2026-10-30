// Tests: lag compensation's arithmetic (games/shooters_edge/net/classic_antilag.hpp): the target
// time a shot is traced at (the acknowledged frame plus the lead, a frame's time on a short ping,
// never past now), a player's position interpolated between samples, held at the oldest, and
// extrapolated past the newest by at most the lead, and the ring keeping its newest samples.
//
// ge:summary Tests: antilag target times, interpolation, extrapolation and the sample ring
// ge:keywords test antilag lag-compensation rewind interpolate extrapolate ring hitscan
// ge:skill net shooter testing
#include "check.hpp"
#include "shooters_edge/net/classic_antilag.hpp"

namespace cn = shooters_edge::net;
namespace sh = ge::shooter;

TEST(the_target_time_leads_the_acknowledged_frame) {
  CHECK(cn::antilag_target(1.0, 0.1, 77, 2.0) == 1.0 + cn::kAntilagLead);
  CHECK(cn::antilag_target(1.0, 0.01, 100, 2.0) == 1.0 + 1.0 / 100);
  CHECK(cn::antilag_target(1.0, 0.01, 5, 2.0) == 1.0 + 1.0 / 77);  // a bad maxfps
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
