// THE THROWN BODY, HELD: the bones keep their lengths, the joints and the muscles give the body no
// energy and no push of its own, a fall at speed tumbles and comes to lie still, and the body is
// a pure function of its throw.
//
// ge:summary Test: the thrown body's bones, energy, momentum, tumble, rest and determinism
// ge:keywords test ragdoll thrown throw tumble lie still bones energy momentum determinism
// ge:skill skier testing
#include <string>

#include "check.hpp"
#include "core/math.hpp"
#include "skier/ragdoll.hpp"
#include "skier/scenarios.hpp"
#include "skier/skis.hpp"
#include "skier/trees.hpp"

namespace {

namespace sk = ge::skier;
namespace mg = ge::mountain::mapgen;
constexpr double kDt = 1.0 / 120;

double between(const sk::Ragdoll& b, int i, int j) {
  const auto& p = b.p;
  const size_t a = 3 * static_cast<size_t>(i);
  const size_t c = 3 * static_cast<size_t>(j);
  return ge::core::hypot3(p[a] - p[c], p[a + 1] - p[c + 1], p[a + 2] - p[c + 2]);
}

// The flat strip a drop scenario stands on.
mg::Level strip() { return sk::scenario_level(*sk::find_scenario("drop-high")); }

// The bones the body holds and their lengths, m.
struct Bone {
  int a, b;
  double len;
};
constexpr Bone kBones[] = {
    {sk::kHipL, sk::kKneeL, 0.44},       {sk::kHipR, sk::kKneeR, 0.44},
    {sk::kKneeL, sk::kFootL, 0.46},      {sk::kKneeR, sk::kFootR, 0.46},
    {sk::kShoulderL, sk::kElbowL, 0.31}, {sk::kShoulderR, sk::kElbowR, 0.31},
    {sk::kElbowL, sk::kHandL, 0.34},     {sk::kElbowR, sk::kHandR, 0.34},
    {sk::kHipL, sk::kHipR, 0.24},        {sk::kShoulderL, sk::kShoulderR, 0.4},
};

void check_bones(const sk::Ragdoll& b, const std::string& when) {
  for (const Bone& bone : kBones) {
    const double d = between(b, bone.a, bone.b);
    CHECK_MSG(d > bone.len - 0.01 && d < bone.len + 0.01,
              when + ": " + sk::point_name(bone.a) + " to " + sk::point_name(bone.b) + " is " +
                  std::to_string(d) + " m, its bone " + std::to_string(bone.len));
  }
}

}  // namespace

TEST(the_bones_keep_their_lengths_through_a_fall) {
  const mg::Level level = strip();
  const double x = level.size / 2;
  const double y = level.ground_at(x, x);
  sk::Ragdoll b = sk::throw_of(x, y, x, 0, 0.2, 0, 0, 14, 0, 1, sk::Over{1, 0.25, 1.8}, kDt);
  check_bones(b, "thrown");
  for (int i = 0; i < 4 * 120; i++) {
    sk::step_ragdoll(b, level, nullptr, kDt);
    if (i % 60 == 59) check_bones(b, "after " + std::to_string(i + 1) + " steps");
  }
}

TEST(a_body_let_fall_from_standing_never_rises_above_where_it_stood) {
  const mg::Level level = strip();
  const double x = level.size / 2;
  const double y = level.ground_at(x, x);
  sk::Ragdoll b = sk::throw_body(x, y, x, 0, 0, 0, 0, 0, 0, 0, 0, kDt);
  const double start = b.y;
  double top = start;
  for (int i = 0; i < 4 * 120; i++) {
    sk::step_ragdoll(b, level, nullptr, kDt);
    top = ge::core::num::max(top, b.y);
  }
  // Its feet, stood at the skis' base, are put back up onto the snow by their radius at once;
  // nothing else may lift it.
  CHECK_MSG(top < start + sk::point_radius(sk::kFootL),
            "the body rose " + std::to_string(top - start) + " m");
  CHECK_MSG(b.down >= 0, "the trunk never met the snow");
  CHECK_MSG(b.still > 0.5, "it never lay still: " + std::to_string(b.still) + " s");
  CHECK(b.y - level.ground_at(b.x, b.z) < 0.4);
}

TEST(in_the_air_the_muscles_never_push_the_body_as_a_whole) {
  const mg::Level level = strip();
  const double x = level.size / 2;
  const double y = level.ground_at(x, x) + 40;
  sk::Ragdoll b = sk::throw_of(x, y, x, 0.3, 0, 3, 0, 9, 0.5, -1, sk::Over{1, 0.6, 0}, kDt);
  const double vx = b.vx;
  const double vz = b.vz;
  for (int i = 0; i < 60; i++) sk::step_ragdoll(b, level, nullptr, kDt);
  CHECK(!b.touching);
  // Only the air slows him across the plan (a few per cent in half a second), the same share of
  // each axis; the limbs flung about add nothing.
  const double kx = b.vx / vx;
  const double kz = b.vz / vz;
  CHECK_MSG(kx > 0.95 && kx <= 1 && kz > 0.95 && kz <= 1,
            "the way across the plan moved: " + std::to_string(kx) + ", " + std::to_string(kz));
  CHECK_MSG(kx - kz < 1e-6 && kz - kx < 1e-6, "the air turned the way");
}

TEST(a_fall_at_speed_tumbles_and_comes_to_lie_still) {
  for (const char* id : {"drop-high", "drop-harsh", "tree-fast", "drop-across"}) {
    const sk::Scenario* sc = sk::find_scenario(id);
    CHECK(sc != nullptr);
    if (!sc) continue;
    const sk::ScenarioRun r = sk::run_scenario(*sc, sk::kReferenceSkis, 120);
    const std::string k = id;
    CHECK_MSG(r.cause != sk::Wipeout::none, k + ": no wipeout");
    CHECK_MSG(r.end.thrown.active, k + ": not thrown");
    CHECK_MSG(r.tumble > 1.5, k + ": tumbled only " + std::to_string(r.tumble) + " rad");
    CHECK_MSG(r.lie_time > 0 && r.lie_time < 5,
              k + ": lay still after " + std::to_string(r.lie_time) + " s");
    CHECK_MSG(r.thrown_top < 2.5, k + ": thrown " + std::to_string(r.thrown_top) + " m high");
    CHECK_MSG(r.thrown < 20, k + ": thrown " + std::to_string(r.thrown) + " m");
  }
}

TEST(a_body_is_a_pure_function_of_its_throw) {
  const mg::Level level = sk::scenario_level(*sk::find_scenario("tree-fast"));
  const sk::TreeIndex trees = sk::build_tree_index(level);
  auto run = [&] {
    const double x = level.size / 2;
    sk::Ragdoll b = sk::throw_of(x, level.ground_at(x, 250), 250, 0, -0.1, 0, -1, 16, 0.4, 1,
                                 sk::Over{-0.45, 0.8, 0.2}, kDt);
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < 3 * 120; i++) {
      sk::step_ragdoll(b, level, &trees, kDt);
      h = sk::ragdoll_hash(b, h);
    }
    return h;
  };
  CHECK(run() == run());
}
