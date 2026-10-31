// plugins/skier/app/breath.hpp: a skier's breath shows only where the air shows one; he breathes
// out for the first part of each breath, in puffs at his mouth (lower in a tuck), blown ahead of
// his face and carried on with him; the colder the air the thicker and the longer the puffs hang;
// a racer at speed breathes faster (as long out as in: as many puffs); a pen breathes the same
// puffs again.
//
// ge:summary Tests: a skier's breath in the cold: where it shows, the mouth, the puffs, the pace
// ge:keywords test skier breath puff exhale breath_puffs mouth_of
// ge:skill skier testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "skier/app/breath.hpp"
#include "skier/skier.hpp"

namespace {

namespace sk = ge::skier;
constexpr double kDt = 1.0 / 60;

// The puffs over `seconds` of frames.
std::vector<sk::SprayGrain> breathe(const sk::SkierState& s, double show, double seconds,
                                    sk::BreathPen pen) {
  std::vector<sk::SprayGrain> out;
  const int frames = static_cast<int>(seconds / kDt);
  for (int i = 0; i < frames; ++i) sk::breath_puffs(s, show, kDt, pen, out);
  return out;
}

}  // namespace

TEST(no_breath_shows_in_warm_air) {
  sk::SkierState s;
  CHECK(breathe(s, 0, 10, sk::breath_pen(0)).empty());
}

TEST(the_puffs_leave_the_mouth_ahead_of_his_face) {
  sk::SkierState s;
  s.heading = 0.5;
  const auto out = breathe(s, 1, 10, sk::breath_pen(0));
  // About three breaths, each 0.4 of 3.2 s out at 30 a second.
  CHECK(out.size() > 90 && out.size() < 150);
  double m[3];
  sk::mouth_of(s, m);
  CHECK(std::fabs(m[1] - sk::kMouthStanding) < 1e-9);
  for (const sk::SprayGrain& g : out) {
    CHECK(std::fabs(g.y - m[1]) < 0.1);
    // Blown along his heading.
    CHECK(g.vx * std::sin(s.heading) + g.vz * std::cos(s.heading) > 0.1);
    CHECK(g.opacity > 0 && g.opacity < 0.5 && g.albedo > 0.9);
  }
}

TEST(a_tuck_lowers_the_mouth_and_a_lean_swings_it) {
  sk::SkierState s;
  s.tuck = 1;
  double m[3];
  sk::mouth_of(s, m);
  CHECK(std::fabs(m[1] - sk::kMouthTucked) < 1e-9);
  sk::SkierState l;
  l.lean = 0.5;
  sk::mouth_of(l, m);
  CHECK(m[0] > 0.5);  // heading 0 (+z): his right is +x
}

TEST(colder_air_breathes_thicker_puffs_that_hang_longer) {
  sk::SkierState s;
  const auto faint = breathe(s, 0.3, 10, sk::breath_pen(1));
  const auto thick = breathe(s, 1.5, 10, sk::breath_pen(1));
  double fo = 0, to = 0, fl = 0, tl = 0;
  for (const auto& g : faint) fo += g.opacity, fl += g.life;
  for (const auto& g : thick) to += g.opacity, tl += g.life;
  const auto nt = static_cast<double>(thick.size()), nf = static_cast<double>(faint.size());
  CHECK(to / nt > 1.5 * fo / nf);
  CHECK(tl / nt > 1.3 * fl / nf);
}

TEST(a_racer_at_speed_breathes_faster_and_the_puffs_ride_with_him) {
  sk::SkierState still, fast;
  fast.vz = fast.speed = 25;
  sk::BreathPen ps, pf;
  std::vector<sk::SprayGrain> out;
  sk::breath_puffs(still, 1, 1, ps, out);
  CHECK(std::fabs(ps.phase - 1 / sk::kBreathPeriod) < 1e-12);
  sk::breath_puffs(fast, 1, 1, pf, out);
  CHECK(std::fabs(pf.phase - 1 / sk::kBreathFastest) < 1e-12);
  for (const auto& g : breathe(fast, 1, 10, sk::breath_pen(2))) CHECK(g.vz > 24);
}

TEST(a_pen_breathes_the_same_again) {
  sk::SkierState s;
  const auto a = breathe(s, 1, 5, sk::breath_pen(3));
  const auto b = breathe(s, 1, 5, sk::breath_pen(3));
  CHECK(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) CHECK(a[i].x == b[i].x && a[i].vz == b[i].vz);
}
