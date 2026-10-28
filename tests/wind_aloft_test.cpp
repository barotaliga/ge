// engine/wind/aloft.hpp: the wind grows with height, is floored at a kind's least and held under
// a jet stream's; it veers clockwise in the north and the other way in the south; dusk falls from
// 0 with the sun 6 degrees up to 1 with it 6 degrees down; and the evening's jet winds the wind up
// to 1.7 times at 400 m and leaves the air kilometres up as it was.
//
// ge:summary Test: the wind aloft's growth and veer, dusk, the evening's low-level jet
// ge:keywords test wind aloft wind_aloft veer_aloft dusk_of jet_at low-level-jet
// ge:skill water testing
#include "check.hpp"
#include "wind/aloft.hpp"

namespace {
namespace gw = ge::wind;
constexpr double kDeg = 3.14159265358979323846 / 180;
}  // namespace

TEST(the_wind_grows_with_height_within_its_floor_and_its_ceiling) {
  CHECK(gw::wind_aloft(4, 2000, 0) > gw::wind_aloft(4, 500, 0));
  CHECK(gw::wind_aloft(4, 0, 0) == 4);
  CHECK(gw::wind_aloft(0, 2000, 6) == 6);
  CHECK(gw::wind_aloft(40, 9000, 0) == 45);
}

TEST(the_wind_veers_clockwise_in_the_north) {
  CHECK(gw::veer_aloft(2000, 45) > 0);
  CHECK(gw::veer_aloft(2000, -45) < 0);
  CHECK(gw::veer_aloft(0, 45) == 0);
}

TEST(dusk_falls_as_the_sun_sinks_and_winds_up_the_jet_low_down) {
  CHECK(gw::dusk_of(10 * kDeg) == 0 && gw::dusk_of(-10 * kDeg) == 1);
  CHECK(gw::dusk_of(0) > 0.4 && gw::dusk_of(0) < 0.6);
  CHECK(gw::jet_at(400, 0) == 1);
  CHECK(gw::jet_at(400, 1) > 1.65 && gw::jet_at(400, 1) <= 1.7);
  CHECK(gw::jet_at(3000, 1) < 1.01);
}
