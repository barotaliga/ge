// WHAT A SKIER THROWS INTO THE AIR, as arithmetic: each frame, the grains of snow his skis and
// his body kick up, read off his state, for a game to fly and draw (Fall Line hands them to the
// engine's particle pool, render/particles.hpp).
//
// FOUR SOURCES, each a rate a second scaled by what the state says:
// - THE SKID: skis sliding sideways across their own line scrape a sheet off the snow and throw
//   it the way they slide, the faster the slide the more and the higher. Thrown from the edge
//   leading the slide, along the length of the skis. Skis pivoted across to brake (a hockey stop)
//   scrape too, read off how fast he slows on the snow (past what the air and the base take): that
//   sheet flies on ahead of him, fanned out to the side.
// - THE CARVE: a ski on a hard edge cuts and flings a thin curl out of the turn from the outside
//   ski's tail, more the harder the turn loads him; a straight run on a flat ski throws nothing.
// - THE POWDER: in loose snow the skis churn up FINES that drift and hang (light, slow to fall),
//   more the deeper the snow and the faster he goes, and the SNOW CLOUD over them: a few big, thin
//   puffs that hang for seconds behind him and drift off on the wind (the particles' drag pulls
//   toward the air).
// - THE LANDING: back on the snow after a flight, a burst out round the skis, bigger the harder
//   he came down; and a skier DOWN sliding on his body throws snow all round him.
// The grains of one frame are spread back along the way he came that frame, so a fast skier's
// spray is a ribbon, not a clump at each frame's end. Every number is drawn from the pen's own
// generator (core/prng.hpp), so a run thrown again throws the same spray.
//
// ge:summary The spray a skier throws each frame: skid, carve, powder, landing, the body down
// ge:keywords skier spray grains skid carve powder fines landing burst emit spray-pen spray_grains
// ge:keywords hockey-stop sheet plume
// ge:skill skier render
// ge:test tests/skier_spray_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "skier/skier.hpp"

namespace ge::skier {

// One grain or fine as it leaves the snow: where, how fast, and how it flies and looks
// (render/particles.hpp's `Particle` reads each field the same).
struct SprayGrain {
  double x, y, z;     // m
  double vx, vy, vz;  // m/s
  double life;        // s
  double size;        // m
  double drag;        // 1/s
  double fall;        // gravity's share
  double streak;      // s of motion drawn as its streak
  double albedo;      // 0..1
  double opacity;     // 0..1
};

// The rates, a second, at their fullest; and the most one skier throws in a frame.
inline constexpr double kSkidRate = 900;
inline constexpr double kCarveRate = 500;
inline constexpr double kPowderRate = 140;
inline constexpr double kCloudRate = 30;
inline constexpr double kDownRate = 500;
inline constexpr int kMaxGrainsFrame = 1500;
// A flight this long before a landing throws a burst, s.
inline constexpr double kBurstFlight = 0.25;
// Slowing on the snow past this is the skis scraping, m/s² (the air and the base take less).
inline constexpr double kScrapeFrom = 1.5;

// What a skier's spray remembers between frames: his last state's flight and the fractions of
// grains owed, and his generator.
struct SprayPen {
  core::Rng rng{1};
  double owed[5] = {0, 0, 0, 0, 0};  // skid, carve, powder, down, cloud
  double air_time = 0;               // the last frame's
  double vy = 0;                     // the last frame's vertical speed, m/s
  double speed = 0;                  // the last frame's speed, m/s
  bool on_snow = false;              // the last frame's
  bool placed = false;               // a frame has been seen
};

// A pen for racer `k`: its own stream.
SprayPen spray_pen(uint32_t k);

// The grains `s` throws over the `dt` s since the last call, appended to `out`; the count.
// The first call only places the pen.
int spray_grains(const SkierState& s, double dt, SprayPen& pen, std::vector<SprayGrain>& out);

// How fast the skis slide across their own line, m/s (signed: + to the skier's right).
double ski_slide(const SkierState& s);

}  // namespace ge::skier
