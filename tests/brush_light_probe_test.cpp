// THE BAKED LIGHT UNDER A POINT on the brush plugin's fixture map: over every room's floor a point
// finds its floor's light, the same as the face's own luxels say; outside the map, nothing; a map
// with no lightmaps gives an empty probe.
//
// ge:summary Test: the light under a point read off the fixture's lightmaps, and nothing outside
// ge:keywords test brush light-probe light_below make_light_probe lightmap floor fixture lightmap_at
// ge:skill brush testing
#include <string>
#include <vector>

#include "brush/light_probe.hpp"
#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

br::Map fixture_map() {
  std::vector<uint8_t> bytes;
  std::string error;
  br::Map map;
  CHECK(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error));
  CHECK(br::read_bsp(bytes, &map, &error));
  return map;
}

}  // namespace

TEST(every_rooms_floor_is_found_under_a_point_over_it) {
  const br::Map map = fixture_map();
  const br::LightProbe probe = br::make_light_probe(map);
  CHECK(probe.nx > 0 && probe.ny > 0);
  CHECK(!probe.tris.empty());
  const br::Fixture fx = br::sample_fixture();
  for (const br::FixtureRoom& r : fx.rooms) {
    const float p[3] = {0.5f * float(r.min[0] + r.max[0]), 0.5f * float(r.min[1] + r.max[1]),
                        float(r.min[2]) + 40};
    float rgb[3] = {-1, -1, -1};
    CHECK(br::light_below(probe, map, p, rgb));
    for (float c : rgb) CHECK(c >= 0 && c <= 1);
    CHECK(rgb[0] + rgb[1] + rgb[2] > 0);  // every fixture room is lit
  }
}

TEST(nothing_is_found_outside_the_map_or_above_its_reach) {
  const br::Map map = fixture_map();
  const br::LightProbe probe = br::make_light_probe(map);
  float rgb[3];
  const float far[3] = {1e5f, 1e5f, 0};
  CHECK(!br::light_below(probe, map, far, rgb));
  const br::Fixture fx = br::sample_fixture();
  const br::FixtureRoom& r = fx.rooms[0];
  const float high[3] = {0.5f * float(r.min[0] + r.max[0]), 0.5f * float(r.min[1] + r.max[1]),
                         float(r.min[2]) + 40};
  CHECK(!br::light_below(probe, map, high, rgb, 10));  // the floor is 40 units down
}

TEST(a_map_with_no_lightmaps_gives_an_empty_probe) {
  br::Map map = fixture_map();
  map.lightmaps.clear();
  const br::LightProbe probe = br::make_light_probe(map);
  CHECK(probe.tris.empty());
  float rgb[3];
  const float p[3] = {0, 0, 0};
  CHECK(!br::light_below(probe, map, p, rgb));
}

TEST(a_faces_own_lightmap_is_read_where_it_is_asked) {
  const br::Map map = fixture_map();
  int lit = 0;
  for (const br::Surface& s : map.surfaces) {
    if (s.lightmap < 0 || s.count < 3) continue;
    // The middle of its first triangle, in its lightmap's coordinates.
    float uv[2] = {0, 0};
    for (uint32_t i = 0; i < 3; ++i)
      for (int k = 0; k < 2; ++k) uv[k] += map.vertices[map.indices[s.first + i]].lm[k] / 3;
    float rgb[3] = {-1, -1, -1};
    CHECK(br::lightmap_at(map, s.lightmap, uv, rgb));
    for (float c : rgb) CHECK(c >= 0 && c <= 1);
    lit += rgb[0] + rgb[1] + rgb[2] > 0 ? 1 : 0;
  }
  CHECK(lit > 0);
  const float uv[2] = {0.5f, 0.5f};
  float rgb[3];
  CHECK(!br::lightmap_at(map, -1, uv, rgb));
  CHECK(!br::lightmap_at(map, static_cast<int32_t>(map.lightmaps.size()), uv, rgb));
}
