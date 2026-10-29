// THE SLIDE AND THE STEP: the velocity clip and its 0.1 dead band, a slide along a wall keeping
// the speed along it, a corner's crease, a stop against a face met head on, and a step climbed
// that a slide alone would not.
//
// ge:summary Tests: clip_velocity, the slide along walls and corners, the step-slide up a stair
// ge:keywords test slide clip_velocity crease corner step-slide stair
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/slide.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// A floor, a wall at x = 64, a wall at y = 64, a 16-unit stair along -x at x = -64.
struct Room {
  sh::HullData point, clip;
  sh::MoveWorld world;
  Room() {
    const std::vector<sh::Brush> brushes = {
        box({-1024, -1024, -64}, {1024, 1024, 0}), box({64, -1024, 0}, {128, 1024, 256}),
        box({-1024, 64, 0}, {1024, 128, 256}), box({-1024, -1024, 0}, {-64, -100, 16})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.count = 1;
  }
};

}  // namespace

TEST(clip_removes_the_normal_part_and_snaps_small_ones) {
  CHECK(sh::clip_velocity({10, 5, -3}, {0, 0, 1}, 1) == (sh::Vec3f{10, 5, 0}));
  CHECK(sh::clip_velocity({0.05f, 5, 0}, {0, 1, 0}, 1) == (sh::Vec3f{0, 0, 0}));
  const sh::Vec3f v = sh::clip_velocity({0.2f, -10, 0}, {0, 1, 0}, 1);
  CHECK(v.x == 0.2f && v.y == 0);
}

TEST(slide_along_a_wall_and_into_a_corner) {
  const Room r;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  s.velocity = {300, 100, 0};
  sh::TouchList touches;
  sh::Mover m{s, sh::MoveSettings{}, r.world, touches};
  m.dt = 0.2f;
  const int blocked = sh::slide_move(m);
  CHECK(blocked == sh::kBlockedStep);
  CHECK(s.velocity.x == 0 && s.velocity.y == 100);
  CHECK(s.origin.x == 48 - 0.03125f);
  CHECK(touches.count == 1 && touches.ents[0] == 0);
  // Into the corner: both walls, the crease runs along z, which is flat here: it stops.
  s.velocity = {300, 300, 0};
  const int b2 = sh::slide_move(m);
  CHECK((b2 & sh::kBlockedStep) != 0);
  CHECK(s.velocity == (sh::Vec3f{0, 0, 0}));
}

TEST(step_slide_climbs_a_stair_a_slide_cannot) {
  const Room r;
  sh::MoveState s;
  s.origin = {-40, -200, 24};
  s.ground = 0;
  s.velocity = {-200, 0, 0};
  sh::TouchList touches;
  sh::Mover m{s, sh::MoveSettings{}, r.world, touches};
  m.dt = 0.1f;
  const int blocked = sh::step_slide_move(m, false);
  CHECK(blocked == sh::kBlockedStep);
  CHECK(s.origin.z == 40.03125f);  // on the stair's top, 1/32 above it
  CHECK(s.origin.x == -60);        // the whole 20 units across
  CHECK(s.velocity.x == -200);
}
