// THE ENGINE MOVER'S WORLD OVER CLIP HULLS: the axes' conversion keeps the view's forward, right
// and up; the player's box traced through `mover_world` lands where the classic `player_trace`
// lands on every scene, swept segment by segment; a point box reads hull 0; a box in a solid is
// reported all solid; a model's liquid leaves are laid as water volumes the swimmer's immersion
// reads as the classic water level does.
//
// ge:summary Tests: the hull world's trace against player_trace, the axes, contents, liquids
// ge:keywords test hull-world mover_world trace to_engine to_map contents_at add_liquids
// ge:keywords immersion water-level
// ge:skill shooter move testing
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "shooter/hull_world.hpp"
#include "shooter/recording.hpp"
#include "water/swim.hpp"

namespace sh = ge::shooter;
namespace mv = ge::move;

namespace {

const sh::Recording& recording() {
  static const sh::Recording rec = [] {
    std::ifstream f(ge::test::root_path("tests/golden/shooter_recording.txt"));
    std::stringstream ss;
    ss << f.rdbuf();
    sh::Recording r;
    std::string error;
    sh::parse_recording(ss.str(), r, error);
    return r;
  }();
  return rec;
}

const sh::RecordedScene& scene(const std::string& name) {
  for (const sh::RecordedScene& s : recording().scenes)
    if (s.name == name) return s;
  return recording().scenes[0];
}

// One scene as a model and as the classic move's list, side by side.
struct Both {
  sh::ModelData data;
  sh::BrushModel model;
  sh::HullWorld hulls;
  mv::World world;
  sh::SceneWorld classic;
  explicit Both(const sh::RecordedScene& s) : data(sh::build_brush_model(s.brushes)) {
    model = data.model();
    hulls = {&model, nullptr, 1};
    world = sh::mover_world(hulls);
    sh::build_scene_world(s, {}, classic);
  }
};

const mv::Box kPlayer{{-16, 0, -16}, {16, 56, 16}};

mv::Vec3 feet_of(sh::Vec3f origin) { return mv::sub(sh::to_engine(origin), {0, 24, 0}); }

}  // namespace

TEST(the_axes_keep_forward_right_and_up) {
  // The map's forward (+x), right (-y) and up (+z) are the engine's +z, +x and +y.
  CHECK(sh::to_engine({1, 0, 0}).z == 1);
  CHECK(sh::to_engine({0, -1, 0}).x == 1);
  CHECK(sh::to_engine({0, 0, 1}).y == 1);
  const sh::Vec3f p{12.5f, -3.25f, 40};
  CHECK(sh::to_map(sh::to_engine(p)) == p);
}

TEST(the_player_box_lands_where_the_classic_trace_lands) {
  ge::core::Rng rng = ge::core::make_rng(18);
  auto range = [&](double lo, double hi) {
    return static_cast<float>(ge::core::range(rng, lo, hi));
  };
  int traces = 0, hits = 0;
  for (const char* name : {"flat", "wall", "stairs", "ramp30", "ramp50", "ledge", "pool"}) {
    const Both b(scene(name));
    for (int i = 0; i < 300; i++) {
      const sh::Vec3f a{range(-200, 600), range(-150, 150), range(-100, 300)};
      const sh::Vec3f e{a.x + range(-300, 300), a.y + range(-300, 300), a.z + range(-300, 300)};
      const sh::HullTrace want = sh::player_trace(b.classic.world, a, e);
      const mv::Hit got = b.world.sweep(kPlayer, feet_of(a), feet_of(e));
      traces++;
      if (want.startsolid || want.allsolid) {
        CHECK_MSG(got.all_solid && got.start_solid && got.fraction == 0,
                  std::string(name) + ": a start in solid must be reported all solid");
        continue;
      }
      CHECK_MSG(got.fraction == want.fraction, std::string(name) + ": the fraction differs");
      const sh::Vec3f end = sh::to_map(mv::add(got.end, {0, 24, 0}));
      CHECK_MSG(end == want.endpos, std::string(name) + ": the end point differs");
      if (want.fraction < 1) {
        hits++;
        CHECK(sh::to_map(got.normal) == want.plane.normal);
        CHECK(got.solid == 1);
      }
    }
  }
  CHECK(traces == 2100);
  CHECK(hits > 300);  // the sample reaches the solids
}

TEST(a_point_reads_hull_zero) {
  const Both b(scene("flat"));
  const mv::Box point{{0, 0, 0}, {0, 0, 0}};
  // Down onto the floor from 10 over it: hull 0's floor is the plane itself.
  const mv::Hit h = b.world.sweep(point, {0, 10, 0}, {0, -10, 0});
  CHECK(h.touched() && h.normal.y == 1);
  CHECK(h.end.y > 0 && h.end.y < 0.04);  // 1/32 back from the plane
  CHECK(sh::contents_at(b.hulls, {0, 5, 0}) == sh::kContentsEmpty);
  CHECK(sh::contents_at(b.hulls, {0, -5, 0}) == sh::kContentsSolid);
}

TEST(the_pool_is_laid_as_water_the_swimmer_reads) {
  const Both b(scene("pool"));
  ge::water::World water;
  std::vector<int> kinds;
  const int laid = sh::add_liquids(b.model, {}, 32, water, kinds);
  CHECK(laid >= 1);
  CHECK(static_cast<int>(kinds.size()) == laid);
  for (int k : kinds) CHECK(k == sh::kContentsWater);
  // The swimmer's level against the classic's three samples, down a column of the pool.
  ge::water::SwimSpec spec;
  spec.scale = 32;
  spec.feet = 1.0 / 32;  // the classic's three samples: the feet + 1, the middle, the eyes
  spec.waist = 28.0 / 32;
  spec.eyes = 46.0 / 32;
  int agree = 0, n = 0;
  for (int i = 0; i <= 54; i++) {
    const float z = -150 + 3.5f * static_cast<float>(i);
    const sh::Vec3f origin{-40, 10, z};
    int level = 0;
    if (sh::water_contents(b.classic.world, {origin.x, origin.y, z - 23}) <= sh::kContentsWater) {
      level = 1;
      if (sh::water_contents(b.classic.world, {origin.x, origin.y, z + 4}) <= sh::kContentsWater) {
        level = 2;
        if (sh::water_contents(b.classic.world, {origin.x, origin.y, z + 22}) <= sh::kContentsWater)
          level = 3;
      }
    }
    const int ours = ge::water::immersion(water, spec, feet_of(origin), 0);
    n++;
    agree += ours == level;
  }
  CHECK(agree == n);
  CHECK(n > 50);
}
