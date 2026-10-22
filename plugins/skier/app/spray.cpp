// The spray's four sources and the landing's burst.
//
// ge:summary The spray's sources: the skid's and brake's sheet, carve, powder, landing, body
// ge:keywords skier spray skid brake scrape carve powder landing burst grains rates
// ge:skill skier render
// ge:test tests/skier_spray_test.cpp
#include "skier/app/spray.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::skier {

namespace {

using core::range;

constexpr double kTwoPi = 6.283185307179586;

// The skis' way and the skier's right on the plan (heading 0 = +z, clockwise from above).
struct Frame {
  double fx, fz, rx, rz;
};
Frame frame_of(const SkierState& s) {
  const double c = core::libm::cos(s.heading), n = core::libm::sin(s.heading);
  return {n, c, c, -n};
}

SprayGrain grain(core::Rng& r) {
  SprayGrain g{};
  g.life = range(r, 0.5, 1.1);
  g.size = range(r, 0.025, 0.055);
  g.drag = 0.9;
  g.fall = 1.0;
  g.streak = 0.045;
  g.albedo = 0.92;
  g.opacity = 0.9;
  return g;
}

SprayGrain fine(core::Rng& r) {
  SprayGrain g{};
  g.life = range(r, 1.4, 2.8);
  g.size = range(r, 0.25, 0.6);
  g.drag = 1.6;
  g.fall = 0.08;
  g.streak = 0.0;
  g.albedo = 0.95;
  g.opacity = range(r, 0.12, 0.25);
  return g;
}

// Where along the way he came this frame a grain leaves: `u` of the frame back.
void place(SprayGrain& g, const SkierState& s, double dt, double u, double dx, double dz,
           double dy = 0.05) {
  g.x = s.x - s.vx * dt * u + dx;
  g.y = s.y - s.vy * dt * u + dy;
  g.z = s.z - s.vz * dt * u + dz;
}

// How many of a source's grains fall due this frame, keeping the fraction owed.
int due(double& owed, double rate, double dt) {
  owed += rate * dt;
  const double n = std::floor(owed);
  owed -= n;
  return static_cast<int>(n);
}

}  // namespace

SprayPen spray_pen(uint32_t k) {
  SprayPen pen;
  pen.rng = core::make_rng(0x5eed0000u + k * 7919u);
  return pen;
}

double ski_slide(const SkierState& s) {
  const Frame f = frame_of(s);
  return s.vx * f.rx + s.vz * f.rz;
}

int spray_grains(const SkierState& s, double dt, SprayPen& pen, std::vector<SprayGrain>& out) {
  const size_t before = out.size();
  core::Rng& r = pen.rng;
  if (!pen.placed || dt <= 0) {
    pen.placed = true;
    pen.air_time = s.air_time;
    pen.vy = s.vy;
    pen.speed = s.speed;
    pen.on_snow = s.on_snow && !s.down;
    return 0;
  }
  dt = std::min(dt, 0.1);
  const Frame f = frame_of(s);
  const double way = s.vx * f.fx + s.vz * f.fz;
  const double slide = ski_slide(s);
  const double loose = std::clamp(1.0 - s.packed, 0.0, 1.0);
  const auto room = [&] { return static_cast<int>(out.size() - before) < kMaxGrainsFrame; };

  if (s.down) {
    // His body ploughing the snow: grains all round him.
    const int n = due(pen.owed[3], kDownRate * std::clamp(s.speed / 10.0, 0.0, 1.0), dt);
    for (int i = 0; i < n && room(); ++i) {
      SprayGrain g = grain(r);
      const double a = range(r, 0, kTwoPi), sp = range(r, 0.5, 2.5);
      place(g, s, dt, next(r), range(r, -0.5, 0.5), range(r, -0.5, 0.5));
      g.vx = 0.6 * s.vx + sp * core::libm::cos(a);
      g.vz = 0.6 * s.vz + sp * core::libm::sin(a);
      g.vy = range(r, 0.8, 2.8);
      out.push_back(g);
    }
  } else if (s.on_snow) {
    // THE LANDING: a burst round the skis, as hard as he came down.
    if (pen.air_time >= kBurstFlight) {
      const double hit = std::max(-pen.vy, 0.0);
      const int n = std::min(80 + static_cast<int>(40 * hit), 500);
      const double power = std::sqrt(std::max(hit, 1.0) / 4.0);
      for (int i = 0; i < n && room(); ++i) {
        SprayGrain g = i % 6 == 5 ? fine(r) : grain(r);
        const double a = range(r, 0, kTwoPi), sp = range(r, 1.0, 3.2) * power;
        const double along = range(r, -0.9, 0.9);
        place(g, s, dt, 0, f.fx * along, f.fz * along);
        g.vx = 0.5 * s.vx + sp * core::libm::cos(a);
        g.vz = 0.5 * s.vz + sp * core::libm::sin(a);
        g.vy = range(r, 1.0, 3.0) * power;
        out.push_back(g);
      }
    }
    // THE SKID: the sheet thrown the way the skis slide, off their leading edge; and the brake's,
    // thrown on ahead of him.
    const double across = std::clamp((std::fabs(slide) - 0.5) / 6.0, 0.0, 1.0);
    const double slowing = pen.on_snow ? (pen.speed - s.speed) / dt : 0.0;
    const double scrape = std::clamp((slowing - kScrapeFrom) / 4.0, 0.0, 1.0);
    const double sliding = std::max(across, scrape);
    const double side =
        std::fabs(slide) > 0.3 ? (slide >= 0 ? 1.0 : -1.0) : (s.edge >= 0 ? 1.0 : -1.0);
    int n = due(pen.owed[0], kSkidRate * sliding * (0.6 + 0.4 * loose), dt);
    for (int i = 0; i < n && room(); ++i) {
      SprayGrain g = loose > 0.5 && i % 4 == 3 ? fine(r) : grain(r);
      const double along = range(r, -0.8, 0.8);
      place(g, s, dt, next(r), f.fx * along + f.rx * side * 0.12,
            f.fz * along + f.rz * side * 0.12);
      const double out_sp = std::fabs(slide) * range(r, 0.55, 1.15) + scrape * range(r, 0.5, 2.5);
      const double fwd = way * (scrape * range(r, 0.6, 1.05) + (1 - scrape) * range(r, 0.3, 0.7));
      g.vx = f.rx * side * out_sp + f.fx * fwd;
      g.vz = f.rz * side * out_sp + f.fz * fwd;
      g.vy = range(r, 0.8, 3.5) * (0.4 + 0.6 * sliding);
      out.push_back(g);
    }
    // THE CARVE: a curl off the outside ski's tail, out of the turn.
    const double load = std::clamp((std::fabs(s.g_lat) - 0.3) / 1.5, 0.0, 1.0);
    const double pace = std::clamp(s.speed / 15.0, 0.0, 1.0);
    const double outside = s.edge > 0 ? -1.0 : 1.0;  // on the right edges the left ski is outside
    n = due(pen.owed[1], kCarveRate * load * pace * (std::fabs(s.edge) > 0.2 ? 1.0 : 0.0), dt);
    for (int i = 0; i < n && room(); ++i) {
      SprayGrain g = grain(r);
      const double back = range(r, 0.5, 0.9);
      place(g, s, dt, next(r), -f.fx * back + f.rx * outside * 0.2,
            -f.fz * back + f.rz * outside * 0.2);
      const double fling = range(r, 1.0, 3.0) * (0.5 + std::fabs(s.g_lat));
      g.vx = 0.55 * s.vx + f.rx * outside * fling;
      g.vz = 0.55 * s.vz + f.rz * outside * fling;
      g.vy = range(r, 0.6, 2.2);
      out.push_back(g);
    }
    // THE POWDER: fines churned up behind the skis, drifting.
    n = due(pen.owed[2], kPowderRate * loose * pace, dt);
    for (int i = 0; i < n && room(); ++i) {
      SprayGrain g = fine(r);
      const double back = range(r, 0.2, 1.0);
      place(g, s, dt, next(r), -f.fx * back + range(r, -0.3, 0.3),
            -f.fz * back + range(r, -0.3, 0.3), 0.15);
      g.vx = 0.35 * s.vx + range(r, -0.6, 0.6);
      g.vz = 0.35 * s.vz + range(r, -0.6, 0.6);
      g.vy = range(r, 0.6, 1.8);
      out.push_back(g);
    }
  }
  pen.air_time = s.air_time;
  pen.vy = s.vy;
  pen.speed = s.speed;
  pen.on_snow = s.on_snow && !s.down;
  return static_cast<int>(out.size() - before);
}

}  // namespace ge::skier
