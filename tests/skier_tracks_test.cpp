// plugins/skier/app/tracks.hpp: the prints a skier's skis lay. The first frame lays nothing (the
// pen goes down), the next two capsules a stance apart from where the skis were to where they
// are; the outside ski of a turn the deeper, a carve narrower and a skid wider and shallower than
// a flat ski running straight; a scuff on the piste and a furrow in loose snow; nothing in the
// air, nothing for a move too short to see or too long to be a track, and one wide print for a
// body sliding on the snow.
//
// ge:summary Tests for the skis' prints: stance, outside ski, carve, skid, snow, air, reset, body
// ge:keywords test tracks ski_prints ski_load prints outside-ski carve skid furrow scuff
// ge:skill skier testing
#include <cmath>

#include "check.hpp"
#include "skier/app/tracks.hpp"
#include "skier/skier.hpp"
#include "skier/skis.hpp"

namespace {

using ge::skier::SkierState;
using ge::skier::SkiPrint;
using ge::skier::TrackPen;

bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

// A skier on the snow at (0, z) heading +z at `v` m/s, his skis on `edge` rad, on snow `packed`.
SkierState at(double z, double edge = 0, double packed = 1, double v = 10) {
  SkierState s;
  s.x = 0;
  s.z = z;
  s.vz = v;
  s.speed = v;
  s.edge = edge;
  s.packed = packed;
  return s;
}

// The prints of moving from `a` to `b`, the pen put down at `a` first.
int lay(const SkierState& a, const SkierState& b, SkiPrint out[2]) {
  TrackPen pen;
  SkiPrint first[2]{};
  if (ge::skier::ski_prints(a, ge::skier::kReferenceSkis, pen, first) != 0) return -1;
  return ge::skier::ski_prints(b, ge::skier::kReferenceSkis, pen, out);
}

}  // namespace

TEST(two_lines_a_stance_apart_from_the_second_frame) {
  TrackPen pen;
  SkiPrint p[2]{};
  CHECK(ge::skier::ski_prints(at(0), ge::skier::kReferenceSkis, pen, p) == 0);
  CHECK(ge::skier::ski_prints(at(0.2), ge::skier::kReferenceSkis, pen, p) == 2);
  // Heading +z, the skier's right is +x: the left ski at -x.
  CHECK(near(p[0].ax, -ge::skier::kTrackStance, 1e-12));
  CHECK(near(p[1].ax, ge::skier::kTrackStance, 1e-12));
  CHECK(near(p[0].az, 0, 1e-12));
  CHECK(near(p[0].bz, 0.2, 1e-12));
  CHECK(p[0].depth == p[1].depth);
}

TEST(the_outside_ski_carries_more_of_him) {
  CHECK(near(ge::skier::ski_load(0.5, 0) + ge::skier::ski_load(0.5, 1), 1, 1e-12));
  CHECK(ge::skier::ski_load(0, 0) == 0.5);
  // On the right edges the left ski is outside.
  CHECK(ge::skier::ski_load(0.6, 0) > ge::skier::ski_load(0.6, 1));
  CHECK(ge::skier::ski_load(-0.6, 1) > ge::skier::ski_load(-0.6, 0));
  SkiPrint p[2]{};
  CHECK(lay(at(0, 0.6), at(0.2, 0.6), p) == 2);
  CHECK(p[0].depth > p[1].depth);
}

TEST(a_carve_cuts_narrower_and_a_skid_wider_and_shallower) {
  SkiPrint flat[2]{}, carve[2]{}, skid[2]{};
  CHECK(lay(at(0), at(0.2), flat) == 2);
  CHECK(lay(at(0, 0.8), at(0.2, 0.8), carve) == 2);
  CHECK(carve[0].half_width < flat[0].half_width);
  // Sliding at 45 degrees across his skis' way.
  SkierState a = at(0), b = at(0.2);
  b.vx = 7.07;
  b.vz = 7.07;
  CHECK(lay(a, b, skid) == 2);
  CHECK(skid[0].half_width > 2 * flat[0].half_width);
  CHECK(skid[0].depth < flat[0].depth);
  CHECK(skid[0].berm > flat[0].berm);
}

TEST(a_scuff_on_the_piste_and_a_furrow_in_loose_snow) {
  SkiPrint piste[2]{}, powder[2]{};
  CHECK(lay(at(0), at(0.2), piste) == 2);
  CHECK(lay(at(0, 0, 0), at(0.2, 0, 0), powder) == 2);
  CHECK(near(piste[0].depth, ge::skier::kPackedScuff, 1e-12));
  CHECK(near(powder[0].depth, ge::skier::kPowderFurrow, 1e-12));
  CHECK(piste[0].berm == 0);
  CHECK(powder[0].berm > 0);
  // Round walls in powder, square on the piste.
  CHECK(powder[0].wall < piste[0].wall);
}

TEST(nothing_in_the_air_nor_for_a_reset_nor_standing_still) {
  TrackPen pen;
  SkiPrint p[2]{};
  const auto& skis = ge::skier::kReferenceSkis;
  CHECK(ge::skier::ski_prints(at(0), skis, pen, p) == 0);
  SkierState air = at(0.2);
  air.on_snow = false;
  CHECK(ge::skier::ski_prints(air, skis, pen, p) == 0);
  // The landing starts a new line: its first frame lays nothing.
  CHECK(ge::skier::ski_prints(at(3), skis, pen, p) == 0);
  CHECK(ge::skier::ski_prints(at(3.2), skis, pen, p) == 2);
  // Too short a move: nothing, and the pen waits where it was.
  CHECK(ge::skier::ski_prints(at(3.201), skis, pen, p) == 0);
  CHECK(ge::skier::ski_prints(at(3.4), skis, pen, p) == 2);
  CHECK(near(p[0].az, 3.2, 1e-12));
  // A reset: no line across the mountain.
  CHECK(ge::skier::ski_prints(at(50), skis, pen, p) == 0);
  CHECK(ge::skier::ski_prints(at(50.2), skis, pen, p) == 2);
}

TEST(a_body_down_gouges_one_wide_print) {
  TrackPen pen;
  SkiPrint p[2]{};
  const auto& skis = ge::skier::kReferenceSkis;
  SkierState down = at(0, 0, 0);
  down.down = true;
  CHECK(ge::skier::ski_prints(down, skis, pen, p) == 0);
  down.z = 0.5;
  CHECK(ge::skier::ski_prints(down, skis, pen, p) == 1);
  CHECK(p[0].half_width == ge::skier::kBodyHalfWidth);
  CHECK(near(p[0].depth, ge::skier::kBodyGouge, 1e-12));
}
