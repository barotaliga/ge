// Sprites (engine/render/sprites.hpp), their CPU twins: each facing rule turns a sprite as it
// says (in the screen's plane, upright to the eye's point, upright with the camera, by its own
// rotation, rolled in the screen's plane), and the atlas packs frames without overlap or fails
// when they cannot fit.
//
// ge:summary Tests for sprites' CPU twins: the five facing rules and the atlas
// ge:keywords test sprites sprite_basis facing parallel upright oriented roll atlas
// ge:keywords pack_sprite_atlas
// ge:skill render testing
#include <string>
#include <vector>

#include "check.hpp"
#include "render/sprites.hpp"

namespace {

using namespace ge::render;

bool near(const double a[3], double x, double y, double z) {
  const double e = 1e-9;
  return a[0] > x - e && a[0] < x + e && a[1] > y - e && a[1] < y + e && a[2] > z - e &&
         a[2] < z + e;
}

// A camera looking down 45 degrees along +z: right +x, up and forward tilted.
const double kS = 0.7071067811865476;
const double kRight[3] = {1, 0, 0}, kUp[3] = {0, kS, kS}, kFwd[3] = {0, -kS, kS};
const double kEye[3] = {0, 10, 0};

}  // namespace

TEST(parallel_lies_in_the_screens_plane) {
  SpriteDraw d;
  d.facing = SpriteFacing::parallel;
  double r[3], u[3];
  sprite_basis(d, kEye, kRight, kUp, kFwd, r, u);
  CHECK(near(r, 1, 0, 0) && near(u, 0, kS, kS));
}

TEST(upright_stands_on_the_worlds_up_and_turns_to_the_eye) {
  SpriteDraw d;
  d.facing = SpriteFacing::upright;
  d.position[0] = 5;  // off to the right: it turns to face the eye, not the screen
  d.position[2] = 5;
  double r[3], u[3];
  sprite_basis(d, kEye, kRight, kUp, kFwd, r, u);
  CHECK(near(u, 0, 1, 0));
  CHECK(near(r, kS, 0, -kS));  // right of the direction from the eye (+x +z)
  // Its face is square to that direction, whatever the camera does.
  CHECK(r[0] * 5 + r[2] * 5 < 1e-9 && r[0] * 5 + r[2] * 5 > -1e-9);
}

TEST(parallel_upright_stands_on_the_worlds_up_and_turns_with_the_camera) {
  SpriteDraw d;
  d.facing = SpriteFacing::parallel_upright;
  d.position[0] = 5;
  double r[3], u[3];
  sprite_basis(d, kEye, kRight, kUp, kFwd, r, u);
  CHECK(near(u, 0, 1, 0) && near(r, 1, 0, 0));
}

TEST(oriented_follows_its_own_rotation) {
  SpriteDraw d;
  d.facing = SpriteFacing::oriented;
  // A quarter turn about +y: x goes to -z.
  d.rotation[1] = static_cast<float>(kS);
  d.rotation[3] = static_cast<float>(kS);
  double r[3], u[3];
  sprite_basis(d, kEye, kRight, kUp, kFwd, r, u);
  CHECK_MSG(r[0] > -1e-6 && r[0] < 1e-6 && r[2] < -0.999999,
            "right " + std::to_string(r[0]) + " " + std::to_string(r[2]));
  CHECK(u[1] > 0.999999);
}

TEST(parallel_oriented_rolls_in_the_screens_plane) {
  SpriteDraw d;
  d.facing = SpriteFacing::parallel_oriented;
  d.roll = 1.5707963267948966f;  // a quarter, right side down: the right points down the screen
  double r[3], u[3];
  sprite_basis(d, kEye, kRight, kUp, kFwd, r, u);
  CHECK(r[0] < 1e-6 && r[1] < -0.7 && r[2] < -0.7);
  CHECK(u[0] > 0.999999);
}

TEST(the_atlas_packs_without_overlap_and_refuses_what_cannot_fit) {
  std::vector<SpriteFrame> frames(60);
  for (size_t i = 0; i < frames.size(); ++i) {
    frames[i].width = 16 + static_cast<uint32_t>(i * 29 % 180);
    frames[i].height = 16 + static_cast<uint32_t>(i * 47 % 150);
  }
  std::vector<SpriteRect> r;
  CHECK(pack_sprite_atlas(frames, r) && r.size() == frames.size());
  for (size_t a = 0; a < r.size(); ++a) {
    CHECK(r[a].w == frames[a].width && r[a].h == frames[a].height);
    CHECK(r[a].x + r[a].w <= kSpriteAtlasSize && r[a].y + r[a].h <= kSpriteAtlasSize);
    for (size_t b = a + 1; b < r.size(); ++b) {
      const bool apart = r[a].x + r[a].w <= r[b].x || r[b].x + r[b].w <= r[a].x ||
                         r[a].y + r[a].h <= r[b].y || r[b].y + r[b].h <= r[a].y;
      CHECK_MSG(apart, "frames " + std::to_string(a) + " and " + std::to_string(b) + " overlap");
    }
  }
  std::vector<SpriteFrame> huge(5);
  for (SpriteFrame& f : huge) f.width = f.height = 1200;
  CHECK(!pack_sprite_atlas(huge, r));
}
