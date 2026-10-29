// engine/render/wet_lens.hpp's state, on the CPU: a dry lens stays dry; going under clears it;
// coming out lays the sheet, whose edge runs down in the drain time, and the drops it leaves; the
// big drops slide and leave beads, the small cling; everything dries in time; a splash spatters
// only a lens in the air; and two runs are the same to the bit.
//
// ge:summary Test: the wet lens: sheet, drops, sliding, beads, drying, spatter, determinism
// ge:keywords test render wet-lens drops sheet drain slide bead dry spatter
// ge:skill render water testing
#include <cmath>

#include "check.hpp"
#include "render/wet_lens.hpp"

namespace {

namespace gr = ge::render;
constexpr double kDt = 1.0 / 120;

// Under for a second, then out for `seconds`.
gr::WetLensState surfaced(double seconds) {
  gr::WetLensState lens;
  for (int i = 0; i < 120; ++i) gr::step_wet_lens(lens, true, kDt);
  for (int i = 0; i < static_cast<int>(seconds / kDt); ++i) gr::step_wet_lens(lens, false, kDt);
  return lens;
}

}  // namespace

TEST(a_dry_lens_stays_dry_and_going_under_clears_it) {
  gr::WetLensState lens;
  CHECK(!gr::wet(lens));
  for (int i = 0; i < 100; ++i) gr::step_wet_lens(lens, false, kDt);
  CHECK(!gr::wet(lens));
  gr::spatter_wet_lens(lens, 10, 0.02);
  CHECK(gr::wet(lens));
  gr::step_wet_lens(lens, true, kDt);
  CHECK(lens.drops.empty());
  CHECK(!gr::wet(lens));  // under the water there is nothing on the lens to see
  gr::spatter_wet_lens(lens, 10, 0.02);
  CHECK(lens.drops.empty());
}

TEST(coming_out_lays_the_sheet_and_its_drops) {
  const gr::WetLensState lens = surfaced(kDt);
  CHECK(gr::wet(lens));
  CHECK(static_cast<int>(lens.drops.size()) == lens.look.drops);
  CHECK(gr::sheet_edge(lens.look, 0) == 0);
  CHECK(std::fabs(gr::sheet_edge(lens.look, 0.5 * lens.look.drain) - 0.25) < 1e-12);
  CHECK(gr::sheet_edge(lens.look, lens.look.drain) == 1);
  for (const gr::LensDrop& d : lens.drops) {
    CHECK(d.r >= lens.look.small && d.r <= lens.look.big);
    CHECK(d.born >= 0 && d.born <= lens.look.drain);
  }
}

TEST(big_drops_slide_and_bead_and_everything_dries) {
  const gr::WetLensState early = surfaced(0.1);
  gr::WetLensState later = surfaced(1.5);
  CHECK(later.drops.size() > early.drops.size());  // the beads
  bool slid = false;
  for (const gr::LensDrop& d : later.drops) slid = slid || d.speed > 0;
  CHECK(slid);
  for (int i = 0; i < static_cast<int>(10 / kDt); ++i) gr::step_wet_lens(later, false, kDt);
  CHECK(later.drops.empty());
  CHECK(!gr::wet(later));
}

TEST(two_runs_are_the_same_to_the_bit) {
  const gr::WetLensState a = surfaced(2.0), b = surfaced(2.0);
  CHECK(a.drops.size() == b.drops.size());
  for (size_t i = 0; i < a.drops.size() && i < b.drops.size(); ++i) {
    const gr::LensDrop &x = a.drops[i], &y = b.drops[i];
    CHECK(x.x == y.x && x.y == y.y && x.r == y.r && x.born == y.born && x.life == y.life &&
          x.age == y.age && x.speed == y.speed && x.ran == y.ran);
  }
}
