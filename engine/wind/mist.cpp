// Fog from the air: each kind's strength, the strongest's density, depth and look.
//
// ge:summary Fog from the air: the formulas behind mist.hpp
// ge:keywords fog mist radiation valley hill steam advection visibility density depth
// ge:skill water render
// ge:test tests/wind_mist_test.cpp
#include "wind/mist.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {

namespace lm = core::libm;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double smooth(double t) {
  const double s = clamp01(t);
  return s * s * (3 - 2 * s);
}

// The visual range, m, from `clear` at no strength to `thick` at full, even in the logarithm.
double range_between(double clear, double thick, double strength) {
  return clear * lm::exp(strength * lm::log(thick / clear));
}

// How much water air with a dew point `dew_c` can hand a fog, against a 5 C dew point's.
double water_share(double dew_c) {
  return std::clamp(saturation_hpa(dew_c) / saturation_hpa(5), 0.3, 1.3);
}

struct Kind {
  MistKind kind;
  double strength, depth, clear, thick, noise, rise;
  double water_dew = -100;  // the dew point of the water's source when it is not the air's
};

}  // namespace

const char* mist_name(MistKind kind) {
  switch (kind) {
    case MistKind::none:
      return "none";
    case MistKind::radiation:
      return "radiation";
    case MistKind::valley:
      return "valley";
    case MistKind::hill:
      return "hill";
    case MistKind::steam:
      return "steam";
    case MistKind::advection:
      return "advection";
  }
  return "none";
}

Mist mist_at(const Climate& c, const Site& s, double hour) {
  const Thermal air = air_at(c, s, hour);
  const double wind = std::max(c.wind, 0.0);
  const double open = clamp01(s.sky);
  const double chill = night_chill(c, hour) * open;

  // SATURATION: how far the air has cooled past the dew point it carried (the water it must give
  // up), less how far it still lies above the dew point it now has. On dry ground most of that
  // water settles as dew or frost and the drainage off the slopes mixes the rest; a lake, wet
  // ground or a hollow's pooled air keeps feeding the fog.
  const double carried = dew_point_of(c.mean_c, c.humidity) - kDewLapse * (s.y - c.ref_y);
  const double supply = std::max({lm::exp(-std::max(s.water_m, 0.0) / 300), clamp01(s.wet),
                                  0.5 * smooth(std::max(s.hollow, 0.0) / 150)}) *
                        lm::exp(-std::max(s.above - 2, 0.0) / 50);
  const double past = std::max(0.0, carried - air.temp_c);
  const double spread = air.temp_c - air.dew_c;
  const double saturated = smooth((past * (0.25 + 0.75 * supply) + 2 * supply - spread) / 2.5);
  // Lifted into its own cloud (by height or under an overcast) the air keeps all its water.
  const double lifted = smooth((past - spread) / 2.5);
  Kind best{MistKind::none, 0, 0, 20000, 20000, 0.5, 0};
  const auto offer = [&best](const Kind& k) {
    if (k.strength > best.strength) best = k;
  };
  {
    // Cooled from the ground on a clear night: still air lays a shallow bank, a breeze deepens
    // it, a wind mixes it away.
    const double mixed = 1 / (1 + lm::pow(wind / 5, 4.0));
    const double night = smooth(chill / 0.15);
    const double strength = saturated * night * mixed;
    const double hollow = std::max(s.hollow, 0.0);
    const double depth = 5 + 40 * smooth(wind / 2) + 0.25 * std::min(hollow, 600.0);
    offer(
        {hollow > 20 ? MistKind::valley : MistKind::radiation, strength, depth, 1000, 50, 0.55, 0});
    // Saturated with no night to cool it: the air's own lift into its cloud (overcast or by
    // height).
    offer({MistKind::hill, lifted * (1 - night) * smooth((c.cloud - 0.3) / 0.4), 200, 1000, 40, 0.4,
           0});
  }
  {
    // Steam off water far warmer than the air over it.
    const double gap = c.water_c - air.temp_c;
    const double strength = smooth((gap - 6) / 8) * lm::exp(-std::max(s.water_m, 0.0) / 30);
    offer({MistKind::steam, strength, std::min(15.0, 2 + 0.5 * std::max(gap, 0.0)), 2000, 100, 0.85,
           0.3});
  }
  {
    // Moist air over a colder surface: a cold lake, or snow in a thaw (lying at 0 C at most).
    const double lake = lm::exp(-std::max(s.water_m, 0.0) / 100);
    const double cold =
        std::max(smooth((carried - c.water_c) / 2) * lake, smooth(carried / 2) * clamp01(s.snow));
    const double brought = smooth(wind / 2) * (1 - smooth((wind - 10) / 5));
    offer({MistKind::advection, cold * brought, 30 + 5 * wind, 1000, 60, 0.45, 0});
  }

  Mist out;
  if (best.strength < kNoMist) return out;
  out.kind = best.kind;
  out.strength = best.strength;
  out.depth = best.depth;
  // The fog's water is the air's own (its dew point), not what the cold leaves it; steam's is the
  // water's.
  const double source = std::max({carried, air.dew_c, best.water_dew});
  out.visibility = range_between(best.clear, best.thick, best.strength * water_share(source));
  out.visibility = std::min(out.visibility, 20000.0);
  out.density = 3 / out.visibility;
  out.noise = best.noise;
  out.rise = best.rise;
  out.freezing = air.temp_c < 0;
  return out;
}

}  // namespace ge::wind
