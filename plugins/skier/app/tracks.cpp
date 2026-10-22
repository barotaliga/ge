// The skis' prints: where each ski touches, how much of the skier it carries, and what the edge,
// the skid and the snow make of its groove.
//
// ge:summary The prints a skier's skis lay each frame, from his state and the pair's size
// ge:keywords tracks prints ski_prints ski_load carve skid berm wall
// ge:skill skier render
// ge:test tests/skier_tracks_test.cpp
#include "skier/app/tracks.hpp"

#include <algorithm>
#include <cmath>

namespace ge::skier {

namespace {

// The edge that is a full carve for the tracks, rad (about 46 degrees).
constexpr double kFullEdge = 0.8;
// The outside ski's share of the weight past a half, on a full carve.
constexpr double kLoadShift = 0.35;
// The ski's footprint along its length that scrapes across the travel in a skid, a share of its
// length (the tip and tail ride clear of the snow's surface).
constexpr double kContact = 0.7;
// Under this speed over the snow, m/s, the travel says nothing of a skid.
constexpr double kCrawl = 0.3;
// The deepest print the map keeps, m.
constexpr double kDeepest = 1.0;

double edge_share(double edge) { return std::clamp(std::fabs(edge) / kFullEdge, 0.0, 1.0); }

}  // namespace

double ski_load(double edge, int side) {
  const int outside = edge > 0 ? 0 : 1;  // on the right edges the left ski is outside
  const double shift = kLoadShift * edge_share(edge);
  return side == outside ? 0.5 + shift : 0.5 - shift;
}

int ski_prints(const SkierState& s, const SkiSpec& spec, TrackPen& pen, SkiPrint out[2]) {
  const double loose = std::clamp(1.0 - s.packed, 0.0, 1.0);
  const double packed = 1.0 - loose;
  if (!s.on_snow) {
    pen.skis = pen.body = false;
    return 0;
  }
  if (s.down) {
    pen.skis = false;
    const double moved = std::hypot(s.x - pen.body_x, s.z - pen.body_z);
    if (pen.body && moved < kTrackStill) return 0;
    const bool drawn = pen.body && moved <= kTrackJump;
    if (drawn) {
      const double depth =
          std::min(packed * 2 * kPackedScuff + loose * std::max(kBodyGouge, s.sink), kDeepest);
      out[0] = {pen.body_x, pen.body_z, s.x, s.z, kBodyHalfWidth, depth, 0.3 * loose * depth, 3.0};
    }
    pen.body_x = s.x;
    pen.body_z = s.z;
    pen.body = true;
    return drawn ? 1 : 0;
  }
  pen.body = false;
  const double sh = std::sin(s.heading), ch = std::cos(s.heading);
  double x[2], z[2];
  for (int k = 0; k < 2; ++k) {
    const double m = k == 0 ? -1.0 : 1.0;  // the left foot is off the line to the skier's left
    x[k] = s.x + m * kTrackStance * ch;
    z[k] = s.z - m * kTrackStance * sh;
  }
  const double moved = std::max(std::hypot(x[0] - pen.x[0], z[0] - pen.z[0]),
                                std::hypot(x[1] - pen.x[1], z[1] - pen.z[1]));
  // Not moved: nothing new to lay, and the pen stays (a frame drawn twice lays its track once).
  if (pen.skis && moved < kTrackStill) return 0;
  const bool drawn = pen.skis && moved <= kTrackJump;
  if (drawn) {
    // The skid: the sine of the angle between the skis' way and the travel over the snow.
    const double v = std::hypot(s.vx, s.vz);
    const double slip = v > kCrawl ? std::fabs(sh * s.vz - ch * s.vx) / v : 0.0;
    const double carve = edge_share(s.edge) * (1.0 - slip);
    const double cross = std::sqrt(std::max(0.0, 1.0 - slip * slip));
    const double half =
        0.5 * (spec.waist * cross + kContact * spec.length * slip) * (1.0 - 0.4 * carve);
    const double base = packed * kPackedScuff + loose * std::max(kPowderFurrow, s.sink);
    for (int k = 0; k < 2; ++k) {
      const double load = 0.4 + 1.2 * ski_load(s.edge, k);
      const double depth =
          std::min(base * load * (1.0 + 0.5 * carve) * (1.0 - 0.5 * slip), kDeepest);
      out[k] = {pen.x[k],
                pen.z[k],
                x[k],
                z[k],
                half,
                depth,
                depth * (0.35 * loose + 0.15 * slip),
                2.5 + 7.5 * packed};
    }
  }
  for (int k = 0; k < 2; ++k) {
    pen.x[k] = x[k];
    pen.z[k] = z[k];
  }
  pen.skis = true;
  return drawn ? 2 : 0;
}

}  // namespace ge::skier
