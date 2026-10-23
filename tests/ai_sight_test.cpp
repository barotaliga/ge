// Sight and memory (engine/ai/sight.hpp, memory.hpp): a ridge between the eye and the point hides
// it, a point within hearing is heard through the ridge, one past the range is never seen, the
// occluder is asked last with the distance; and a memory moves only when the quarry is seen.
//
// ge:summary Tests: the ground line over a ridge, hearing, range, the occluder; memory's age
// ge:keywords test ai sight ground-line ridge hear range occluder memory last-seen age
// ge:skill bots testing
#include <cmath>
#include <limits>

#include "ai/memory.hpp"
#include "ai/sight.hpp"
#include "check.hpp"

namespace ai = ge::ai;

namespace {

// Flat ground with a ridge 10 m high across x = 50.
double ridge(double x, double) { return std::fabs(x - 50) < 2 ? 10 : 0; }
bool nothing(double) { return false; }

}  // namespace

TEST(a_ridge_hides_the_point_and_the_line_over_it_is_clear) {
  CHECK(!ai::ground_clear({0, 2, 0}, {100, 2, 0}, 1, 0.4, ridge));
  CHECK(ai::ground_clear({0, 20, 0}, {100, 20, 0}, 1, 0.4, ridge));
  // Short of the ridge: nothing between.
  CHECK(ai::ground_clear({0, 2, 0}, {40, 2, 0}, 1, 0.4, ridge));
}

TEST(hearing_beats_the_ridge_the_range_beats_everything_and_the_occluder_is_asked_last) {
  const ai::SightSpec spec{80, 5, 1, 0.4};
  CHECK(!ai::sees({0, 2, 0}, {60, 2, 0}, spec, ridge, nothing));
  CHECK(ai::sees({48, 2, 0}, {52, 2, 0}, spec, ridge, nothing));  // within 5 m: heard
  CHECK(!ai::sees({0, 2, 0}, {90, 2, 30}, spec, [](double, double) { return 0.0; }, nothing));
  double asked = -1;
  const auto occluder = [&](double d) {
    asked = d;
    return true;
  };
  CHECK(!ai::sees({0, 2, 0}, {30, 2, 40}, spec, [](double, double) { return 0.0; }, occluder));
  CHECK(asked == 50);
  // Not asked when the ground already hides it.
  asked = -1;
  CHECK(!ai::sees({0, 2, 0}, {60, 2, 0}, spec, ridge, occluder));
  CHECK(asked == -1);
}

TEST(a_memory_moves_only_when_the_quarry_is_seen_and_ages_with_the_clock) {
  ai::Memory m;
  CHECK(!ai::known(m) && ai::age(m, 3) == std::numeric_limits<double>::infinity());
  ai::see(m, 1, 2, 3, 10);
  CHECK(ai::known(m) && m.x == 1 && m.y == 2 && m.z == 3);
  CHECK(ai::age(m, 14.5) == 4.5);
}
