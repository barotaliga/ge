// The ride bed (ride_bed.hpp).
//
// ge:summary The ride bed implemented: the voices read off the skier and the racks steered
// ge:keywords ride bed skid pace follow racks
// ge:skill skier audio
// ge:test tests/skier_ride_bed_test.cpp
#include "skier/app/ride_bed.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "skier/limits.hpp"

namespace ge::skier {

RideBed make_ride_bed(int wind_bus, int snow_bus) {
  RideBed bed;
  bed.wind.specs = kWindSpecs;
  bed.wind.glides_s = kWindGlides;
  bed.wind.bus = wind_bus;
  bed.snow.specs = kSnowSpecs;
  bed.snow.glides_s = kSnowGlides;
  bed.snow.bus = snow_bus;
  return bed;
}

WindVoice wind_voice_of(const SkierState& skier, float air) {
  return {air, static_cast<float>(skier.tuck), !skier.on_snow, 0};
}

SnowVoice snow_voice_of(const SkierState& skier, const SkiSpec& spec) {
  SnowVoice v;
  v.speed = static_cast<float>(skier.speed);
  const double top = top_speed(spec);
  v.pace = top > 0 ? static_cast<float>(skier.speed / top) : 0;
  v.packed = static_cast<float>(skier.packed);
  const double most = spec.edge_max > 0.3 ? spec.edge_max : 0.3;
  v.edge = static_cast<float>((skier.edge < 0 ? -skier.edge : skier.edge) / most);
  // THE SKID: the share of his way over the snow that is across the skis, |sin| of the angle
  // between the velocity and the heading (heading 0 = +z, clockwise from above).
  const double flat = std::sqrt(skier.vx * skier.vx + skier.vz * skier.vz);
  if (flat > 1) {
    const double across =
        skier.vx * core::libm::cos(skier.heading) - skier.vz * core::libm::sin(skier.heading);
    v.skid = static_cast<float>((across < 0 ? -across : across) / flat);
  }
  // A body sliding on the snow is a skid with no edge in it.
  if (skier.down) {
    v.skid = 1;
    v.edge = 0;
  }
  v.on_snow = skier.on_snow;
  v.ski = ski_voice_of(spec);
  return v;
}

void ride_bed_update(RideBed& bed, audio::Synth& synth, const SkierState& skier,
                     const SkiSpec& spec, float dt, const RideHeard& heard) {
  const float frame = dt < 1.0f / 240 ? 1.0f / 240 : dt > 0.1f ? 0.1f : dt;
  // In still air the apparent wind is his own speed.
  const float share =
      1 - static_cast<float>(core::libm::exp(-static_cast<double>(frame) / kRideAirTau));
  bed.air += (static_cast<float>(skier.speed) - bed.air) * share;
  const auto wind = wind_targets(wind_voice_of(skier, bed.air), heard.wind);
  const auto snow = snow_targets(snow_voice_of(skier, spec), heard.snow);
  audio::rack_apply(bed.wind, synth, wind);
  audio::rack_apply(bed.snow, synth, snow);
}

void ride_bed_silence(RideBed& bed, audio::Synth& synth) {
  audio::rack_stop(bed.wind, synth);
  audio::rack_stop(bed.snow, synth);
}

}  // namespace ge::skier
