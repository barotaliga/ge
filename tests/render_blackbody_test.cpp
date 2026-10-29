// engine/render/blackbody.hpp: a blackbody's colour reddens as it cools, its luminance climbs by
// decades with its heat (a perfect radiator at platinum's freezing point is about 6e5 cd/m^2), and
// the shaders' table rises row by row to 1 at its last.
//
// ge:summary Tests for the blackbody: colour by heat, absolute luminance, the table
// ge:keywords test render blackbody planck kelvin luminance table
// ge:skill render testing
#include "check.hpp"
#include "render/blackbody.hpp"

namespace {
namespace r = ge::render;
}  // namespace

TEST(the_blackbody_reddens_as_it_cools) {
  const r::Blackbody hot = r::blackbody(3000), warm = r::blackbody(1500);
  CHECK(hot.rgb[2] > warm.rgb[2] && hot.rgb[1] > warm.rgb[1]);
  CHECK(r::blackbody(1000).luminance == 1.0);
  CHECK(hot.luminance > warm.luminance * 100);
  // A perfect radiator at 2042 K (platinum's freezing point) is about 6e5 cd/m^2.
  const double pt = r::blackbody_cd(2042);
  CHECK(pt > 5e5 && pt < 7e5);
  float table[r::kBlackbodyRows][4];
  r::blackbody_table(table);
  CHECK(table[r::kBlackbodyRows - 1][3] == 1.0f);
  for (uint32_t i = 1; i < r::kBlackbodyRows; ++i) CHECK(table[i][3] > table[i - 1][3]);
}
