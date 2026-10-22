// plugins/skier/app/spray.hpp: what a skier throws. The first frame throws nothing (the pen is
// placed); a straight run on a flat ski on the piste throws nothing; a skid throws a sheet the way
// the skis slide and a brake one on ahead of him; a carve throws a curl out of the turn off the
// outside ski's tail; loose snow throws fines that drift; a landing after a flight bursts, bigger
// the harder he came down; a skier down throws snow all round him; and the same frames thrown again
// throw the same spray.
//
// ge:summary Tests for the spray: nothing straight, the skid's sheet, the carve's curl, powder
// fines, the landing's burst, the body down, the pen's determinism
// ge:keywords test spray spray_grains skid carve powder landing burst determinism
// ge:skill skier testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "skier/app/spray.hpp"
#include "skier/skier.hpp"

namespace {

using ge::skier::SkierState;
using ge::skier::SprayGrain;
using ge::skier::SprayPen;

constexpr double kDt = 1.0 / 60;

// A skier at the origin heading +z (his right is +x) at `v` m/s, his skis on `edge` rad.
SkierState at(double v, double edge = 0, double packed = 1) {
  SkierState s;
  s.vz = v;
  s.speed = v;
  s.edge = edge;
  s.packed = packed;
  return s;
}

// The grains of `frames` frames of `s` (the pen placed on a first frame of its own).
std::vector<SprayGrain> throw_for(const SkierState& s, int frames, SprayPen pen) {
  std::vector<SprayGrain> out;
  ge::skier::spray_grains(s, kDt, pen, out);
  for (int i = 0; i < frames; ++i) ge::skier::spray_grains(s, kDt, pen, out);
  return out;
}

double mean_vx(const std::vector<SprayGrain>& g) {
  double sum = 0;
  for (const SprayGrain& x : g) sum += x.vx;
  return g.empty() ? 0 : sum / static_cast<double>(g.size());
}

}  // namespace

TEST(the_first_frame_only_places_the_pen) {
  SprayPen pen = ge::skier::spray_pen(0);
  std::vector<SprayGrain> out;
  SkierState s = at(12);
  s.vx = 8;  // a hard skid
  CHECK(ge::skier::spray_grains(s, kDt, pen, out) == 0);
  CHECK(out.empty());
  CHECK(ge::skier::spray_grains(s, kDt, pen, out) > 0);
}

TEST(a_straight_run_on_a_flat_ski_throws_nothing) {
  CHECK(throw_for(at(15), 60, ge::skier::spray_pen(1)).empty());
}

TEST(a_skid_throws_a_sheet_the_way_the_skis_slide) {
  SkierState s = at(10);
  s.vx = 6;  // sliding to his right
  CHECK(ge::skier::ski_slide(s) > 5.9);
  const std::vector<SprayGrain> right = throw_for(s, 30, ge::skier::spray_pen(2));
  CHECK(right.size() > 100);
  CHECK(mean_vx(right) > 2);
  for (const SprayGrain& g : right) CHECK(g.vy > 0);
  s.vx = -6;
  const std::vector<SprayGrain> left = throw_for(s, 30, ge::skier::spray_pen(2));
  CHECK(mean_vx(left) < -2);
  // A harder slide throws more.
  s.vx = 2;
  CHECK(throw_for(s, 30, ge::skier::spray_pen(2)).size() < right.size());
}

TEST(a_carve_throws_a_curl_out_of_the_turn_off_the_tail) {
  // On his right edges, turning right: the left ski is outside, the curl flies left (-x).
  SkierState s = at(18, 0.8);
  s.g_lat = 1.4;
  const std::vector<SprayGrain> g = throw_for(s, 60, ge::skier::spray_pen(3));
  CHECK(g.size() > 50);
  CHECK(mean_vx(g) < -1);
  double z = 0;
  for (const SprayGrain& x : g) z += x.z;
  CHECK(z / static_cast<double>(g.size()) < 0);  // behind him: off the tails
  // A flat ski with the same load throws no curl.
  s.edge = 0.1;
  CHECK(throw_for(s, 60, ge::skier::spray_pen(3)).empty());
}

TEST(loose_snow_throws_fines_that_drift) {
  const std::vector<SprayGrain> g = throw_for(at(12, 0, 0), 60, ge::skier::spray_pen(4));
  CHECK(g.size() > 60);
  for (const SprayGrain& x : g) {
    CHECK(x.fall < 0.2);
    CHECK(x.life > 1);
  }
}

TEST(a_landing_bursts_harder_the_harder_he_came_down) {
  auto land = [](double vy) {
    SprayPen pen = ge::skier::spray_pen(5);
    std::vector<SprayGrain> out;
    SkierState air = at(12);
    air.on_snow = false;
    air.air_time = 0.6;
    air.vy = vy;
    ge::skier::spray_grains(air, kDt, pen, out);
    ge::skier::spray_grains(air, kDt, pen, out);
    CHECK(out.empty());  // nothing thrown in the air
    ge::skier::spray_grains(at(12), kDt, pen, out);
    return out.size();
  };
  const size_t soft = land(-2), hard = land(-8);
  CHECK(soft >= 80);
  CHECK(hard > soft);
  // A hop too short to count throws no burst.
  SprayPen pen = ge::skier::spray_pen(5);
  std::vector<SprayGrain> out;
  SkierState hop = at(12);
  hop.on_snow = false;
  hop.air_time = 0.1;
  hop.vy = -3;
  ge::skier::spray_grains(hop, kDt, pen, out);
  ge::skier::spray_grains(hop, kDt, pen, out);
  ge::skier::spray_grains(at(12), kDt, pen, out);
  CHECK(out.empty());
}

TEST(a_skier_down_throws_snow_all_round_him) {
  SkierState s = at(10);
  s.down = true;
  const std::vector<SprayGrain> g = throw_for(s, 30, ge::skier::spray_pen(6));
  CHECK(g.size() > 100);
  bool left = false, right = false;
  for (const SprayGrain& x : g) {
    left = left || x.vx < -1;
    right = right || x.vx > 1;
  }
  CHECK(left && right);
}

TEST(the_same_frames_throw_the_same_spray) {
  SkierState s = at(14, -0.6, 0.4);
  s.vx = -3;
  s.g_lat = -1.0;
  const std::vector<SprayGrain> a = throw_for(s, 40, ge::skier::spray_pen(7));
  const std::vector<SprayGrain> b = throw_for(s, 40, ge::skier::spray_pen(7));
  CHECK(!a.empty() && a.size() == b.size());
  bool same = true;
  for (size_t i = 0; i < a.size(); ++i) {
    same = same && a[i].x == b[i].x && a[i].vy == b[i].vy && a[i].size == b[i].size;
  }
  CHECK(same);
  // Another racer's pen throws another spray.
  const std::vector<SprayGrain> c = throw_for(s, 40, ge::skier::spray_pen(8));
  CHECK(c.empty() || c[0].x != a[0].x || c[0].vy != a[0].vy);
}

TEST(a_brake_scrapes_a_sheet_on_ahead_of_him) {
  SprayPen pen = ge::skier::spray_pen(9);
  std::vector<SprayGrain> out;
  SkierState s = at(15);
  ge::skier::spray_grains(s, kDt, pen, out);
  // Slowing 6 m/s² on the snow with the skis straight: a hockey stop's scrape.
  for (int i = 0; i < 30; ++i) {
    s.speed -= 6 * kDt;
    s.vz = s.speed;
    ge::skier::spray_grains(s, kDt, pen, out);
  }
  CHECK(out.size() > 100);
  double vz = 0;
  for (const SprayGrain& g : out) vz += g.vz;
  CHECK(vz / static_cast<double>(out.size()) > 0.6 * s.speed);  // on ahead
  CHECK(mean_vx(out) > 0.5);                                    // fanned out to a side
  // Slowing gently (the air and the base) throws nothing.
  out.clear();
  pen = ge::skier::spray_pen(9);
  s = at(15);
  ge::skier::spray_grains(s, kDt, pen, out);
  for (int i = 0; i < 30; ++i) {
    s.speed -= 1.0 * kDt;
    s.vz = s.speed;
    ge::skier::spray_grains(s, kDt, pen, out);
  }
  CHECK(out.empty());
}
