// SIGHT: whether a bot's eye sees a point, over any ground and whatever stands on it.
//
// - THE GROUND LINE (`ground_clear`): the straight line from the eye to the point, read every
//   `step` m of its run over the plan, is clear where the ground at each read (the caller's height
//   sampler) stays `clear` m or more under it. The two ends are not read: the eye and the point
//   stand on the ground.
// - SEEING (`sees`): a point within `hear` m is heard, whatever is between (true); past `range`
//   it is out of sight (false); otherwise it is seen when the ground line is clear and nothing
//   else hides it, the caller's OCCLUDER asked last (woods, cover, smoke: a game's own), with
//   the distance it was asked at.
//
// Pure functions of their arguments: nothing here draws a random number or allocates.
//
// ge:summary Sight: the ground line over a height sampler, hearing, range and an occluder callback
// ge:keywords ai sight line-of-sight los ground-line occluder hear hearing range eye viewshed
// ge:keywords sees ground_clear height-sampler cover woods
// ge:skill bots
// ge:test tests/ai_sight_test.cpp
#pragma once

#include <cmath>
#include <cstdint>

namespace ge::ai {

struct Point3 {
  double x = 0, y = 0, z = 0;
};

// How far a bot sees and hears, and how its ground line is read.
struct SightSpec {
  double range = 0;  // m: nothing further is seen
  double hear = 0;   // m: anything this near is heard, seen or not
  double step = 1;   // m of the line's run over the plan between reads of the ground
  double clear = 0;  // m the ground must stay under the line
};

// Whether the line from `a` to `b` stays `clear` m over the ground (`height(x, z)`), read every
// `step` m of its run.
template <class Height>
bool ground_clear(Point3 a, Point3 b, double step, double clear, const Height& height) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double dz = b.z - a.z;
  const double run = std::sqrt(dx * dx + dz * dz);
  const auto n = static_cast<int32_t>(std::floor(run / step));
  for (int32_t i = 1; i < n; i++) {
    const double k = static_cast<double>(i) / n;
    if (height(a.x + dx * k, a.z + dz * k) + clear > a.y + dy * k) return false;
  }
  return true;
}

// WHETHER THE EYE AT `eye` SEES `at`: heard within `spec.hear`, never past `spec.range`, else the
// ground line clear and `occluded(distance)` false.
template <class Height, class Occluded>
bool sees(Point3 eye, Point3 at, const SightSpec& spec, const Height& height,
          const Occluded& occluded) {
  const double dx = at.x - eye.x;
  const double dy = at.y - eye.y;
  const double dz = at.z - eye.z;
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (d <= spec.hear) return true;
  if (d > spec.range) return false;
  if (!ground_clear(eye, at, spec.step, spec.clear, height)) return false;
  return !occluded(d);
}

}  // namespace ge::ai
