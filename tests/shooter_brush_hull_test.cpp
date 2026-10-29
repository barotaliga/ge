// CLIP HULLS FROM BRUSHES: the chain's layout (a node a plane, the front child the next brush,
// the back child the next plane or the contents), the player's hull moving each plane out by the
// box and keeping only solid brushes, and the first brush holding a point deciding its contents.
//
// ge:summary Tests: hull 0 and the player's hull built from convex brushes as a plane chain
// ge:keywords test brush brush-hull chain hull0 hull1 expand player-box
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/brush_hull.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(int contents, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

}  // namespace

TEST(point_hull_is_a_chain_of_every_brush) {
  const std::vector<sh::Brush> brushes = {box(sh::kContentsSolid, {-64, -64, -64}, {64, 64, 0}),
                                          box(sh::kContentsWater, {-64, -64, 0}, {64, 64, 32})};
  const sh::HullData h = sh::build_point_hull(brushes);
  CHECK(h.nodes.size() == 12);
  CHECK(h.nodes[0].children[0] == 6);  // outside the first: the next brush
  CHECK(h.nodes[0].children[1] == 1);  // inside so far: the next plane
  CHECK(h.nodes[5].children[1] == sh::kContentsSolid);
  CHECK(h.nodes[11].children[0] == sh::kContentsEmpty);
  CHECK(h.nodes[11].children[1] == sh::kContentsWater);
  CHECK(h.planes[1].type == 3 && h.planes[0].type == 0 && h.planes[4].type == 2);
  const sh::Hull v = h.hull();
  CHECK(sh::point_contents(v, {0, 0, -1}) == sh::kContentsSolid);
  CHECK(sh::point_contents(v, {0, 0, 1}) == sh::kContentsWater);
  CHECK(sh::point_contents(v, {0, 0, 40}) == sh::kContentsEmpty);
  // On the plane both brushes share, a point is in front of each: in neither.
  CHECK(sh::point_contents(v, {0, 0, 0}) == sh::kContentsEmpty);
}

TEST(player_hull_moves_the_planes_out_and_keeps_solids_only) {
  const std::vector<sh::Brush> brushes = {box(sh::kContentsSolid, {-64, -64, -64}, {64, 64, 0}),
                                          box(sh::kContentsWater, {-64, -64, 0}, {64, 64, 32})};
  const sh::HullData h = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
  CHECK(h.nodes.size() == 6);
  CHECK(h.planes[0].dist == 80);  // +x: 64 + 16
  CHECK(h.planes[1].dist == 80);  // -x: 64 + 16
  CHECK(h.planes[4].dist == 24);  // the floor's top: 0 + 24
  CHECK(h.planes[5].dist == 96);  // the bottom: 64 + 32
  // A player standing on the floor has its origin 24 above it, and is free there.
  CHECK(sh::point_contents(h.hull(), {0, 0, 24}) == sh::kContentsEmpty);
  CHECK(sh::point_contents(h.hull(), {0, 0, 23.9f}) == sh::kContentsSolid);
  // A slanted plane moves by its own components: -0.5 x + 0.866 z.
  sh::Brush ramp = box(sh::kContentsSolid, {0, -64, 0}, {100, 64, 50});
  ramp.planes.insert(ramp.planes.begin(), sh::Plane{{-0.5f, 0, 0.866025448f}, -32});
  const sh::HullData r = sh::build_box_hull({ramp}, sh::kPlayerMins, sh::kPlayerMaxs);
  CHECK(r.planes[0].dist == -32 - (-0.5f * 16 + 0.866025448f * -24));
  CHECK(r.planes[0].type == 3);
}

TEST(no_brush_is_all_open) {
  const sh::HullData h = sh::build_point_hull({});
  CHECK(sh::point_contents(h.hull(), {1, 2, 3}) == sh::kContentsEmpty);
  const sh::HullTrace t = sh::trace_hull(h.hull(), {0, 0, 0}, {100, 0, 0});
  CHECK(t.fraction == 1 && !t.startsolid);
}
