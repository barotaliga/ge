// The map drawn from above (plugins/mountain/picture): the whole map is the window over the whole
// map pixel for pixel, a window round a point puts that point at the picture's centre, and a
// window off the map's edge draws without reading past it.
//
// ge:summary Tests: the map from above, whole and windowed, and a window over the map's edge
// ge:keywords test mountain map picture draw_map window
// ge:skill mountain testing
// ge:test tests/mountain_map_picture_test.cpp
#include <cstdint>

#include "check.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/picture/map_picture.hpp"

namespace mg = ge::mountain::mapgen;
namespace pic = ge::mountain::picture;

namespace {

const mg::Level& level() {
  static const mg::Level l = mg::generate_level(38);
  return l;
}

ge::lab::Rgba at(const ge::lab::Picture& p, int x, int y) {
  const auto* c =
      &p.rgba[(static_cast<size_t>(y) * static_cast<size_t>(p.width) + static_cast<size_t>(x)) * 4];
  return {c[0], c[1], c[2], c[3]};
}

}  // namespace

TEST(the_whole_map_is_the_window_over_the_whole_map) {
  const mg::Level& l = level();
  const auto whole = pic::draw_map(l, 96);
  const auto window = pic::draw_map(l, 96, {l.size / 2, l.size / 2, l.size});
  CHECK(whole.width == 96 && whole.height == 96);
  CHECK(whole.rgba == window.rgba);
}

TEST(a_window_round_the_spawn_puts_it_at_the_centre) {
  const mg::Level& l = level();
  const auto p = pic::draw_map(l, 64, {l.spawn.x, l.spawn.z, 40});
  // The spawn's disc is the start's green.
  const ge::lab::Rgba c = at(p, 32, 32);
  CHECK_MSG(c.g > c.r && c.g > c.b,
            std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b));
}

TEST(a_window_past_the_edge_still_draws) {
  const mg::Level& l = level();
  const auto p = pic::draw_map(l, 32, {0, 0, 200});
  CHECK(p.width == 32 && p.rgba.size() == size_t{32} * 32 * 4);
}
