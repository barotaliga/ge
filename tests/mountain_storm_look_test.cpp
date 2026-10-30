// plugins/mountain/app/storm_look.hpp: a strike's first stroke lights a lamp and its channel with
// its branches, the candela shared out along the segments; the channel is built once and kept
// while its flash lasts, then let go; without channels only the lamps; a flash inside the cloud
// draws no channel; and a frame with no flash is dark.
//
// ge:summary Test: a storm's flashes as a frame's lamps and channels
// ge:keywords test storm lightning lamps bolts storm_lights
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
#include "mountain/app/storm_look.hpp"

namespace {

namespace gw = ge::wind;
namespace ma = ge::mountain::app;

gw::Storm one_cell() {
  gw::Storm s;
  s.cells = 1;
  s.cell[0].peak = 0.9;
  s.cell[0].core = 3000;
  s.cell[0].seed = 1234567;
  return s;
}

// The first flash of `kind` in the cell's mature life.
bool first_of(const gw::Storm& s, gw::FlashKind kind, gw::Flash& out) {
  for (int64_t n = static_cast<int64_t>(gw::kTowering) + 300; n < 4000; ++n) {
    if (gw::flash_in_slot(s, 0, n, out) && out.kind == kind) return true;
  }
  return false;
}

}  // namespace

TEST(a_strike_lights_a_lamp_and_its_channel) {
  const gw::Storm s = one_cell();
  gw::Flash f;
  CHECK(first_of(s, gw::FlashKind::ground, f));
  const double eye[3] = {f.ex + 3000, f.ey + 2, f.ez};
  const double a = f.t0 + f.stroke[0].t - 0.002, b = a + 1.0 / 60;
  ma::StormLights lights;
  ma::storm_lights(lights, s, eye, a, b);
  CHECK(lights.lamps.count >= 1);
  CHECK(lights.sky > 0);
  CHECK(!lights.bolts.empty());
  CHECK(lights.kept.size() == 1);
  double sum = 0;
  for (const ge::render::BoltSegment& g : lights.bolts) {
    const double len = std::hypot(std::hypot(g.b[0] - g.a[0], g.b[1] - g.a[1]), g.b[2] - g.a[2]);
    sum += g.candela * len;
  }
  CHECK(std::fabs(sum / (gw::light_over(f, a, b) * gw::kUnitCandela) - 1) < 1e-3);
  // The next frame keeps the channel it built.
  const gw::Channel* built = lights.kept[0].channel.get();
  ma::storm_lights(lights, s, eye, b, b + 1.0 / 60);
  CHECK(lights.kept.empty() || lights.kept[0].channel.get() == built);
  // Without channels, the lamps alone.
  ma::storm_lights(lights, s, eye, a, b, false);
  CHECK(lights.lamps.count >= 1 && lights.bolts.empty());
  // Once the flash is over its channel is let go, and a frame with no flash is dark.
  const double late = f.t0 + gw::kMaxFlash + 1;
  gw::Flash none[4];
  if (gw::flashes_between(s, late, late + 1.0 / 60, none, 4) == 0) {
    ma::storm_lights(lights, s, eye, late, late + 1.0 / 60);
    CHECK(lights.kept.empty());
    CHECK(lights.lamps.count == 0 && lights.sky == 0 && lights.bolts.empty());
  }
}

TEST(a_flash_inside_the_cloud_draws_no_channel) {
  const gw::Storm s = one_cell();
  gw::Flash f;
  CHECK(first_of(s, gw::FlashKind::cloud, f));
  std::vector<ge::render::BoltSegment> out;
  gw::Channel ch;
  gw::build_channel(f, ch);
  ma::bolts_of(f, ch, f.t0, f.t0 + f.length, 1e9, out);
  CHECK(out.empty());
  const ge::render::Flashes one = ma::lamps_of(&f, f.t0, f.t0 + f.length);
  CHECK(one.count == 1 && one.lamp[0].out_tau > 3);
  CHECK(ma::lamps_of(nullptr, 0, 1).count == 0);
}
