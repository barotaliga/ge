// plugins/skier/app/figure.hpp: the placeholder figure stands on its skis (the skis' pieces lie
// at the snow, the head well over them), a tuck lowers the hip from 0.99 m to 0.65 m and the
// head with it, a right edge drops the skis' right side, a lean into the turn tips the head to
// its side, a wiped-out figure lies on the snow, and the same state poses the same bits.
//
// ge:summary Test: the placeholder figure's pose from a SkierState: stance, tuck, edge, lean, down
// ge:keywords test skier figure pose tuck edge lean down placeholder pieces
// ge:skill skier testing
#include <array>
#include <cmath>
#include <cstring>

#include "check.hpp"
#include "skier/app/figure.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace sk = ge::skier;
using Pose = std::array<sk::PiecePose, sk::kFigurePieces>;

// Pieces by their place in figure_pieces(): the skis, the pelvis, the head.
constexpr size_t kLeftSki = 0, kRightSki = 1, kPelvis = 8, kHead = 14;

struct Strip {
  ge::mountain::mapgen::Level level = sk::flat_strip(0.0, 1.0);
  sk::World world{&level, nullptr};
  sk::SkierState stand(double heading = 0) {
    return sk::stand_skier(world, sk::kStripSize / 2, 1000, heading, 10);
  }
};

Pose pose(const sk::SkierState& s, const sk::World& w) {
  Pose p{};
  sk::pose_figure(s, w, p);
  return p;
}

// How far a piece's centre stands over the skier's base, m.
double over(const sk::PiecePose& p, const sk::SkierState& s) { return p.centre.y - s.y; }

// The world x of a point on the piece's own +x side, relative to its centre.
ge::core::Vec3 right_of(const sk::PiecePose& p) { return ge::core::rotate(p.rotation, {1, 0, 0}); }

}  // namespace

TEST(figure_stands_on_its_skis) {
  Strip st;
  const sk::SkierState s = st.stand();
  const Pose p = pose(s, st.world);
  CHECK(sk::figure_pieces().size() == size_t{sk::kFigurePieces});
  CHECK(sk::figure_pieces()[kLeftSki].part == sk::FigurePart::ski);
  CHECK(sk::figure_pieces()[kHead].part == sk::FigurePart::helmet);
  for (size_t k : {kLeftSki, kRightSki}) {
    CHECK(std::fabs(over(p[k], s)) < 0.03);
    CHECK(p[k].size[2] > 1.5);  // a ski is long along the way
  }
  CHECK(p[kLeftSki].centre.x < p[kRightSki].centre.x);  // heading +z: the right ski is at +x
  CHECK(over(p[kHead], s) > 1.5 && over(p[kHead], s) < 1.9);
  for (const sk::PiecePose& piece : p) {
    CHECK(over(piece, s) > -0.05);  // nothing under the snow
  }
}

TEST(tuck_lowers_the_hip) {
  CHECK(std::fabs(sk::figure_hip_height(0) - 0.99) < 0.01);
  CHECK(std::fabs(sk::figure_hip_height(1) - 0.65) < 0.01);
  CHECK(sk::figure_hip_height(0.5) < sk::figure_hip_height(0));
  Strip st;
  sk::SkierState s = st.stand();
  const Pose up = pose(s, st.world);
  s.tuck = 1;
  const Pose down = pose(s, st.world);
  CHECK(over(down[kPelvis], s) < over(up[kPelvis], s) - 0.25);
  CHECK(over(down[kHead], s) < over(up[kHead], s) - 0.4);
}

TEST(edge_and_lean_tip_to_their_side) {
  Strip st;
  sk::SkierState s = st.stand();
  s.edge = 0.6;  // on the right edges: the skis' right side down
  Pose p = pose(s, st.world);
  CHECK(right_of(p[kLeftSki]).y < -0.5);
  s.edge = -0.6;
  p = pose(s, st.world);
  CHECK(right_of(p[kLeftSki]).y > 0.5);
  s.edge = 0;
  const double flat_x = pose(s, st.world)[kHead].centre.x;
  s.lean = 0.5;  // into a right turn: the head goes right, +x
  CHECK(pose(s, st.world)[kHead].centre.x > flat_x + 0.5);
}

TEST(down_lies_on_the_snow) {
  Strip st;
  sk::SkierState s = st.stand();
  s.down = true;
  const Pose p = pose(s, st.world);
  CHECK(over(p[kHead], s) < 0.4);
}

TEST(same_state_same_pose) {
  Strip st;
  sk::SkierState s = st.stand(0.7);
  s.tuck = 0.4;
  s.edge = 0.3;
  s.lean = 0.2;
  const Pose a = pose(s, st.world), b = pose(s, st.world);
  CHECK(std::memcmp(a.data(), b.data(), sizeof(sk::PiecePose) * a.size()) == 0);
}
