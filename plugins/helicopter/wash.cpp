// The rotor's wash (wash.hpp).
//
// ge:summary wash_at and induced_of: the column under the disc and the wall jet along the snow
// ge:keywords helicopter wash_at induced_of column wall-jet
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#include "helicopter/wash.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "helicopter/spec.hpp"

namespace ge::helicopter {

namespace num = core::num;
using core::Vec3;

double induced_of(double thrust) {
  return std::sqrt(num::max(0.0, thrust) / (2 * spec::kDensity * spec::rotor::kArea));
}

Vec3 wash_at(const mountain::mapgen::Level& level, const HeliState& h, double x, double y,
             double z) {
  Vec3 out{0, 0, 0};
  if (h.crashed != HeliCrash::none || h.thrust <= 0) return out;
  constexpr double kR = spec::rotor::kRadius;
  const double vi = induced_of(h.thrust);
  const Vec3 hub = heli_point(h, Vec3{0, spec::rotor::kHub, spec::rotor::kAt});
  const double dx = x - hub.x;
  const double dz = z - hub.z;
  const double r = core::hypot(dx, dz);
  const double below = hub.y - y;
  // THE COLUMN under the disc: accelerating to the far wake's 2 v_i over a radius below it,
  // contracting, and spent a few diameters down.
  if (below > 0 && r < kR) {
    const double q = r / kR;
    const double edge = 1 - q * q * q * q;
    out.y -= vi * (1 + core::smoothstep(0, kR, below)) * edge *
             (1 - core::smoothstep(4 * kR, 8 * kR, below));
  }
  // THE WALL JET along the snow.
  const double height = hub.y - level.ground_at(hub.x, hub.z);
  constexpr double kReach = spec::wash::kReach * 2 * kR;
  if (height >= kReach || r < 0.1) return out;
  constexpr double kCore = spec::wash::kCore * kR;
  const double peak = spec::wash::kPeak * vi * (1 - core::smoothstep(kR, kReach, height));
  // ...and spent into the still air past a dozen radii out.
  const double u = (r < kCore ? peak * (r / kCore) : peak * (kCore / r)) *
                   (1 - core::smoothstep(6 * kR, 12 * kR, r));
  const double over = num::max(0.0, y - level.ground_at(x, z));
  const double depth = 0.3 + 0.06 * r;
  const double k = core::clamp(u * core::libm::exp(-over / depth), 0.0, 3 * vi);
  out.x += dx / r * k;
  out.z += dz / r * k;
  return out;
}

}  // namespace ge::helicopter
