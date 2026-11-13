// tests/brush_sky_box_test.cpp: a map's own sky box: every texel of the atlas looks along the
// same direction as the picture texel it was read from; the up picture's corner lands where the
// table says; the box's size is the rt picture's smaller side; an empty picture refuses the box.
//
// ge:summary Tests for the sky box built from a map's six pictures
// ge:keywords test brush sky-box skybox build_sky_box orientation rt bk lf ft up dn
// ge:skill brush testing
#include <array>
#include <cstdint>
#include <string>

#include "brush/app/sky_box.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;
namespace as = ge::assets;

// A picture of n x n whose texel (x, y) is coloured (x, y, picture): each texel says where it was.
as::Image coded(int n, int picture) {
  as::Image p;
  p.width = p.height = n;
  p.rgba.resize(size_t(n) * size_t(n) * 4);
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      uint8_t* t = &p.rgba[(size_t(y) * size_t(n) + size_t(x)) * 4];
      t[0] = uint8_t(x);
      t[1] = uint8_t(y);
      t[2] = uint8_t(picture * 40);
      t[3] = 255;
    }
  }
  return p;
}

std::array<as::Image, 6> coded_set(int n) {
  std::array<as::Image, 6> set;
  for (int i = 0; i < 6; ++i) set[size_t(i)] = coded(n, i);
  return set;
}

// The direction a face's texel (i, j) of n looks along: look + s right + t up.
void direction(const int look[3], const int up[3], const int right[3], int i, int j, int n,
               double d[3]) {
  const double s = (i + 0.5) * 2.0 / n - 1.0, t = 1.0 - (j + 0.5) * 2.0 / n;
  for (int k = 0; k < 3; ++k) d[k] = look[k] + s * right[k] + t * up[k];
}

}  // namespace

TEST(every_texel_of_the_box_looks_where_its_picture_texel_looks) {
  const int n = 16;
  ba::Scene scene;
  CHECK(ba::build_sky_box(coded_set(n), &scene));
  CHECK(scene.sky_box_face == uint32_t(n));
  CHECK(scene.sky_box.width == uint32_t(3 * n) && scene.sky_box.height == uint32_t(2 * n));
  int checked = 0;
  for (int f = 0; f < 6; ++f) {
    const int* look = ba::kSkyBoxLook[f];
    const int* up = ba::kSkyBoxUp[f];
    const int right[3] = {up[1] * look[2] - up[2] * look[1], up[2] * look[0] - up[0] * look[2],
                          up[0] * look[1] - up[1] * look[0]};
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        const size_t at = ((size_t(f / 3) * n + size_t(j)) * scene.sky_box.width +
                           size_t(f % 3) * n + size_t(i)) *
                          4;
        const uint8_t* t = &scene.sky_box.levels[0][at];
        const int picture = t[2] / 40;
        CHECK(picture >= 0 && picture < 6);
        const ba::SkyBoxPicture& q = ba::kSkyBoxPictures[picture];
        double from_box[3], from_picture[3];
        direction(look, up, right, i, j, n, from_box);
        direction(q.look, q.top, q.right, t[0], t[1], n, from_picture);
        CHECK(from_box[0] == from_picture[0] && from_box[1] == from_picture[1] &&
              from_box[2] == from_picture[2]);
        ++checked;
      }
    }
  }
  CHECK(checked == 6 * n * n);
}

TEST(the_up_pictures_bottom_left_is_the_top_left_of_the_boxs_up_face) {
  // The up picture's top edge points to the map's -x (the engine's -x): seen in the box's +y face
  // (its top edge toward +z, its right toward -x), the picture's bottom-left corner is the face's
  // top-left.
  const int n = 8;
  ba::Scene scene;
  CHECK(ba::build_sky_box(coded_set(n), &scene));
  const size_t at = (size_t(0) * scene.sky_box.width + size_t(2 * n)) * 4;  // face 2, (0, 0)
  const uint8_t* t = &scene.sky_box.levels[0][at];
  CHECK(t[2] == 4 * 40);  // the up picture
  CHECK(t[0] == 0 && t[1] == n - 1);
}

TEST(the_box_is_the_rt_pictures_smaller_side_and_every_picture_is_rescaled_to_it) {
  std::array<as::Image, 6> set = coded_set(32);
  set[0] = coded(24, 0);
  ba::Scene scene;
  CHECK(ba::build_sky_box(set, &scene));
  CHECK(scene.sky_box_face == 24u);
  CHECK(scene.sky_box.width == 72u && scene.sky_box.height == 48u);
}

TEST(an_empty_picture_refuses_the_whole_box) {
  std::array<as::Image, 6> set = coded_set(8);
  set[4] = {};
  ba::Scene scene;
  std::string why;
  CHECK(!ba::build_sky_box(set, &scene, &why));
  CHECK(scene.sky_box.levels.empty() && scene.sky_box_face == 0);
  CHECK(why.find("up") != std::string::npos);
}
