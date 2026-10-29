// The storm's definitions: each cell's life read off its age as smooth stages (smoothsteps, no
// state), the gust front's ring and its profiles, the rain cores, and the multicell layout.
//
// ge:summary Thunderstorm cells' stages, the gust front's ring, the rain cores, lay_storm
// ge:keywords storm cell life-cycle smoothstep outflow gust-front rain-core lay_storm
// ge:skill water
// ge:test tests/wind_storm_test.cpp
#include "wind/storm.hpp"

#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {

constexpr double kFrontWidth = 600;   // m: the head, from its strongest air to none past the edge
constexpr double kNewCellTurn = 0.7;  // rad: a new cell's bearing off the drift, to its right

double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// 0 at or before `a`, 1 at or after `b`, smooth between.
double smooth(double a, double b, double x) {
  const double u = clamp01((x - a) / (b - a));
  return u * u * (3 - 2 * u);
}

double tower_of(double peak) { return 7000 + 7000 * peak; }

}  // namespace

const char* stage_name(Stage stage) {
  switch (stage) {
    case Stage::none:
      return "none";
    case Stage::towering:
      return "towering";
    case Stage::mature:
      return "mature";
    case Stage::dissipating:
      return "dissipating";
  }
  return "none";
}

double storm_hash(uint32_t seed, uint32_t salt, uint32_t index) {
  uint32_t h = (seed ^ (salt * 0x9e3779b1u)) * 0x85ebca77u;
  h ^= index * 0xc2b2ae3du;
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  h ^= h >> 16;
  return static_cast<double>(h) / 4294967296.0;
}

double mature_of(double peak) { return 900 + 900 * clamp01(peak); }
double life_of(double peak) { return kTowering + mature_of(peak) + kDissipating; }

CellAt cell_at(const Storm& storm, int k, double t) {
  CellAt at;
  const Cell& c = storm.cell[k];
  const double age = t - c.born;
  const double peak = clamp01(c.peak);
  const double t1 = kTowering, t2 = t1 + mature_of(peak), t3 = t2 + kDissipating;
  at.age = age;
  at.x = c.x + storm.vx * age;
  at.z = c.z + storm.vz * age;
  if (age < 0 || age >= t3) return at;
  at.stage = age < t1 ? Stage::towering : (age < t2 ? Stage::mature : Stage::dissipating);

  // The updraft builds through the cumulus stage, holds into the mature one and is cut off by the
  // rain-cooled downdraft before the cell dies.
  const double w = 8 + 32 * peak;
  at.updraft = w * smooth(0, t1, age) * (1 - smooth(t1 + 0.5 * (t2 - t1), t2, age));
  // The tower climbs to its top through the cumulus stage and early maturity; the anvil it spreads
  // there sinks a little as the cell dies.
  const double top = tower_of(peak);
  at.top =
      storm.base + (top - storm.base) * smooth(0, 1.1 * t1, age) * (1 - 0.2 * smooth(t2, t3, age));
  // The downdraft: dragged down by the rain from its first echo, strongest through maturity.
  const double d = 4 + 16 * peak;
  at.downdraft = d * smooth(0.7 * t1, t1 + 0.3 * (t2 - t1), age) * (1 - smooth(t2, t3, age));
  // The rain: the first echo at 0.7 of the cumulus stage, the heaviest through maturity, then
  // the anvil's light fall lingering after the core has rained out.
  const double r = 0.45 + 0.55 * peak;
  at.rain = r * smooth(0.7 * t1, t1 + 0.25 * (t2 - t1), age) *
            (0.35 * (1 - smooth(t2, t3, age)) + 0.65 * (1 - smooth(t2, t2 + 600, age)));

  // Lightning once the tower passes the glaciation level, lagging the updraft on the way down.
  if (top > kGlaciation) {
    const double share = (kGlaciation - storm.base) / (top - storm.base);
    const double first = 1.1 * t1 * share;  // when the climb passes it, read as linear
    const double rate = 1 + 40 * peak * peak * peak;
    at.flashes =
        rate * smooth(first, first + 300, age) * (1 - smooth(t2 - 0.2 * (t2 - t1), t2 + 900, age));
    const double usual = 0.7 + 0.2 * peak;
    const double early = 1 - smooth(first, first + 300, age);
    at.in_cloud = usual + (0.97 - usual) * early - (usual - 0.55) * smooth(t2, t2 + 900, age);
  }

  // The gust front: the downdraft touches down as the cell matures and its cold air runs out.
  if (age > t1) {
    at.front = (4 + 8 * peak) * (age - t1);
    // The cold pool keeps spreading as the cell dies, but its wind is spent within a quarter hour.
    at.outflow = (8 + 22 * peak) * smooth(t1, t1 + 300, age) * (1 - smooth(t2, t2 + 900, age));
  }
  return at;
}

Air outflow_at(const Storm& storm, const Where& where, double t, double roughness) {
  Air air;
  double head = 0;
  const double above = where.above;
  // The outflow's strongest air runs some tens of metres up, under the log law nearer the ground.
  const double low = profile_at(roughness, above < 60 ? above : 60) / profile_at(roughness, 60);
  for (int k = 0; k < storm.cells; ++k) {
    const CellAt c = cell_at(storm, k, t);
    if (c.front <= 0 || c.outflow <= 0) continue;
    const double depth = 800 + 700 * clamp01(storm.cell[k].peak);
    const double high = 1 - smooth(0.5 * depth, depth, above);
    if (high <= 0) continue;
    const double dx = where.x - c.x, dz = where.z - c.z;
    const double r = std::sqrt(dx * dx + dz * dz);
    if (r >= c.front + kFrontWidth) continue;
    // Radial speed: rising across the cold pool to the head just behind the edge, then gone over
    // the head's width past it.
    double u;
    double inside;
    if (r <= c.front) {
      const double q = r / c.front;
      u = c.outflow * q * (1.2 - 0.2 * q);
      inside = 1;
    } else {
      const double f = 1 - (r - c.front) / kFrontWidth;
      u = c.outflow * f * f;
      inside = f * f;
    }
    const double s = low * high;
    const double ex = r > 0 ? dx / r : 0, ez = r > 0 ? dz / r : 0;
    air.x += (ex * u + storm.vx * inside) * s;
    air.z += (ez * u + storm.vz * inside) * s;
    // The head lifts the air it undercuts.
    const double lift = 1 - smooth(0, kFrontWidth, std::fabs(r - c.front));
    air.y += 0.3 * u * lift * high;
    const double near = lift * smooth(0, 0.3 * c.outflow, u);
    if (near > head) head = near;
  }
  air.speed = std::sqrt(air.x * air.x + air.y * air.y + air.z * air.z);
  air.gust = head;
  return air;
}

double rain_at(const Storm& storm, double x, double z, double t) {
  double dry = 1;
  for (int k = 0; k < storm.cells; ++k) {
    const CellAt c = cell_at(storm, k, t);
    if (c.rain <= 0) continue;
    // The core widens as it matures and spreads under the anvil as it dies.
    const double rc =
        storm.cell[k].core *
        (0.6 + 0.4 * smooth(0, kTowering + 300, c.age) +
         0.5 * (c.stage == Stage::dissipating ? 1 : 0) *
             smooth(0, kDissipating, c.age - kTowering - mature_of(storm.cell[k].peak)));
    const double dx = x - c.x, dz = z - c.z;
    const double q = (dx * dx + dz * dz) / (rc * rc);
    const double fall = c.rain / ((1 + q) * (1 + q));
    dry *= 1 - fall;
  }
  return 1 - dry;
}

Storm lay_storm(const StormSpec& spec) {
  Storm s;
  s.vx = spec.vx;
  s.vz = spec.vz;
  s.base = spec.base;
  s.ground = spec.ground;
  const double drift = std::sqrt(spec.vx * spec.vx + spec.vz * spec.vz);
  // The drift's bearing, turned to its right (clockwise from above) where new cells rise.
  const double ux = drift > 0 ? spec.vx / drift : 0, uz = drift > 0 ? spec.vz / drift : 1;
  const double cs = core::libm::cos(kNewCellTurn), sn = core::libm::sin(kNewCellTurn);
  const double nx = ux * cs + uz * sn, nz = -ux * sn + uz * cs;
  double born = spec.start, x = spec.x, z = spec.z;
  for (int k = 0; k < kMaxCells; ++k) {
    Cell& c = s.cell[k];
    double peak = spec.severity * (0.75 + 0.5 * storm_hash(spec.seed, 1, k));
    if (peak > 1) peak = 1;
    c.x = x;
    c.z = z;
    c.born = born;
    c.peak = peak;
    c.core = 1800 + 1800 * peak * (0.8 + 0.4 * storm_hash(spec.seed, 2, k));
    c.seed = static_cast<uint32_t>(storm_hash(spec.seed, 3, k) * 4294967296.0);
    s.cells = k + 1;
    const double gap = 600 + 300 * storm_hash(spec.seed, 4, k);
    if (born + gap > spec.start + spec.renew) break;
    // The next cell rises where this one's gust front stands by then, on the forward-right flank.
    const double age = gap;
    const double front = age > kTowering ? (4 + 8 * peak) * (age - kTowering) : 0;
    const double reach = 4000 + front;
    x += spec.vx * age + nx * reach;
    z += spec.vz * age + nz * reach;
    born += gap;
  }
  return s;
}

}  // namespace ge::wind
