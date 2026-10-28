// The water's sound (sound.hpp): what is heard, the bed's layers, the one-shots.
//
// ge:summary The water's sound: hear's rings, the bed's targets, the bank, bubbles, splashes
// ge:keywords water sound hear rings intensity bed targets bank splash bubble minnaert swim
// ge:skill water audio
// ge:test tests/water_sound_test.cpp
#include "water/sound.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::water {

namespace {

namespace lm = core::libm;
using audio::Color;
using audio::FilterType;
using audio::LayerSpec;
using audio::Noise;
using audio::SoundDef;
using audio::SoundVoice;
using audio::Tone;
using audio::Wave;

constexpr double kPi = 3.14159265358979323846;
constexpr double kRho = 1000, kG = 9.81;
constexpr double kLn2 = 0.69314718055994531;
// The ripples on water no wind is blowing over, m: the lapping never quite stops.
constexpr double kRipple = 0.005;
// Open water's share of a shore's lapping: waves slap on each other and on nothing.
constexpr double kOpenWater = 0.08;
// A wet point this shallow, m, is the shore's edge (or the next ring out is dry).
constexpr double kShoreDepth = 1.2;
// The nearest a sheet's landing is heard from, m: closer, the ear is in its spray.
constexpr double kNearestFall = 3;
// The water's own voices heard from under it keep their level and lose their highs to here.
constexpr float kUnderWaterHz = 500;

// An intensity summed with where it comes from and how far.
struct Acc {
  double i = 0, side = 0, dist = 0;
  void add(double intensity, double s, double d) {
    i += intensity;
    side += intensity * s;
    dist += intensity * d;
  }
  Heard heard(double full) const {
    Heard h;
    h.level = level_of(i, full);
    if (i > 0) {
      h.pan = static_cast<float>(core::clamp(side / i, -1, 1));
      h.distance = static_cast<float>(dist / i);
    }
    return h;
  }
};

LayerSpec noise(Color color, FilterType filter, float q) {
  LayerSpec s;
  s.kind = LayerSpec::Kind::noise;
  s.color = color;
  s.filter = filter;
  s.q = q;
  return s;
}

// THE BANK: what each sound should feel like is its sentence.
constexpr SoundVoice kSplash[] = {
    // the crack of the surface broken, sweeping down as the cavity opens
    audio::noise_voice({.duration_ms = 80,
                        .volume = 0.09f,
                        .color = Color::pink,
                        .filter = {FilterType::bandpass, 2200, 700, 0.8f}}),
    // the drops falling back
    audio::noise_voice({.duration_ms = 520,
                        .volume = 0.022f,
                        .delay_ms = 90,
                        .color = Color::white,
                        .filter = {FilterType::highpass, 2600, 0, 0},
                        .attack_ms = 60}),
    // the cavity closing: one big bubble's plop, rising
    audio::tone_voice({.wave = Wave::sine,
                       .from_hz = 480,
                       .to_hz = 900,
                       .duration_ms = 90,
                       .volume = 0.035f,
                       .delay_ms = 45,
                       .attack_ms = 2}),
};
// the cavity a big body opens, closing: a hollow whump that grows with the body
constexpr SoundVoice kPlunge[] = {
    audio::noise_voice({.duration_ms = 280,
                        .volume = 0.07f,
                        .color = Color::brown,
                        .filter = {FilterType::lowpass, 600, 180, 0},
                        .attack_ms = 6,
                        .hold_ms = 30}),
};
constexpr SoundVoice kDrip[] = {
    audio::noise_voice({.duration_ms = 60,
                        .volume = 0.03f,
                        .color = Color::pink,
                        .filter = {FilterType::bandpass, 1300, 0, 1}}),
    audio::noise_voice({.duration_ms = 420,
                        .volume = 0.02f,
                        .color = Color::white,
                        .filter = {FilterType::highpass, 3200, 0, 0},
                        .attack_ms = 25}),
};
constexpr SoundVoice kStroke[] = {
    // the arm pulling through
    audio::noise_voice({.duration_ms = 300,
                        .volume = 0.12f,
                        .color = Color::pink,
                        .filter = {FilterType::bandpass, 900, 450, 1.2f},
                        .attack_ms = 40,
                        .hold_ms = 60}),
    // the water running off it on the way back
    audio::noise_voice({.duration_ms = 320,
                        .volume = 0.03f,
                        .delay_ms = 150,
                        .color = Color::white,
                        .filter = {FilterType::highpass, 2800, 0, 0},
                        .attack_ms = 20}),
    // the hand going in again
    audio::noise_voice({.duration_ms = 60,
                        .volume = 0.07f,
                        .delay_ms = 300,
                        .color = Color::pink,
                        .filter = {FilterType::bandpass, 1700, 0, 1}}),
};
constexpr SoundVoice kBreath[] = {
    audio::noise_voice({.duration_ms = 500,
                        .volume = 0.14f,
                        .color = Color::white,
                        .filter = {FilterType::bandpass, 1500, 0, 1.4f},
                        .attack_ms = 150,
                        .hold_ms = 120}),
};
constexpr SoundVoice kGasp[] = {
    audio::noise_voice({.duration_ms = 340,
                        .volume = 0.22f,
                        .color = Color::white,
                        .filter = {FilterType::bandpass, 1100, 2600, 2},
                        .attack_ms = 20,
                        .hold_ms = 50}),
    audio::tone_voice({.wave = Wave::saw,
                       .from_hz = 220,
                       .to_hz = 280,
                       .duration_ms = 220,
                       .volume = 0.03f,
                       .attack_ms = 20,
                       .filter = {FilterType::bandpass, 900, 0, 1}}),
};

constexpr SoundDef kBank[] = {
    {"splash",
     "a body going into the water at 5 m/s, about a ball's size: the surface cracked, the "
     "cavity's plop and the drops falling back",
     kSplash},
    {"plunge",
     "the hollow whump under a splash as the cavity a body opened closes; under a ball's it is "
     "as loud as the crack, under a boulder's it is the splash",
     kPlunge},
    {"drip", "a body coming out of the water: a slap and the water running off it", kDrip},
    {"stroke", "a swimmer's arm pulling through the water, the drops off it, the hand going in",
     kStroke},
    {"breath", "a swimmer's breath taken at the surface, a soft rush of air", kBreath},
    {"gasp", "the eyes back out after a held breath: a sharp, rising gasp with a voice in it",
     kGasp},
};

void play_named(audio::Synth& synth, std::string_view id, const audio::PlayShape& shape, int bus) {
  if (const SoundDef* def = audio::find_sound(kBank, id)) audio::play_def(synth, *def, shape, bus);
}

// A water-borne one-shot heard from under the water: its highs lost, its level kept.
void under_water(audio::PlayShape& shape, float under) {
  shape.pitch *= 1 - 0.35f * audio::clamp01(under);
}

}  // namespace

const std::span<const audio::SoundDef> kWaterBank{kBank};

float level_of(double intensity, double full) {
  if (intensity <= 0 || full <= 0) return 0;
  const double l = std::sqrt(intensity / full);
  return static_cast<float>(l < 1.5 ? l : 1.5);
}

Hearing hear(const World& world, double x, double y, double z, double t, double heading) {
  Hearing h;
  const Sample here = sample(world, x, y, z, t);
  if (here.body >= 0 && here.depth > 0) {
    h.depth = static_cast<float>(here.depth);
    h.under = static_cast<float>(core::smoothstep(0.05, 0.4, here.depth));
  }
  // The ear's right, heading 0 = +z clockwise from above: +x.
  const double rx = lm::cos(heading), rz = -lm::sin(heading);
  constexpr int kRings = static_cast<int>(kHearRings.size());
  double dir_x[kHearDirs], dir_z[kHearDirs];
  for (int j = 0; j < kHearDirs; ++j) {
    const double a = 2 * kPi * j / kHearDirs;
    dir_x[j] = lm::sin(a);
    dir_z[j] = lm::cos(a);
  }
  Sample s[kRings][kHearDirs];
  for (int k = 0; k < kRings; ++k) {
    for (int j = 0; j < kHearDirs; ++j) {
      const double r = kHearRings[static_cast<size_t>(k)];
      s[k][j] = sample(world, x + r * dir_x[j], y, z + r * dir_z[j], t);
    }
  }
  Acc lap, surf, rush, roar;
  double period = 0, period_w = 0, speed = 0, speed_w = 0;
  for (int k = 0; k < kRings; ++k) {
    const double r = kHearRings[static_cast<size_t>(k)];
    for (int j = 0; j < kHearDirs; ++j) {
      const Sample& p = s[k][j];
      if (p.body < 0) continue;
      const Body& body = world.bodies[static_cast<size_t>(p.body)];
      const double dy = y - p.surface;
      const double r3 = core::hypot(r, dy);
      // The ring's share of an even sheet (the sector's area over 2 pi r^2), less for water
      // heard from above or below it.
      const double w = kLn2 / kHearDirs * (r / r3) * (r / r3);
      const double side = (dir_x[j] * rx + dir_z[j] * rz) * (r / r3);
      if (body.kind == Kind::lake || body.kind == Kind::ocean) {
        // The whole sea's variance here (Hs^2 / 16), the laid waves' crests rising over it as
        // they pass, and the ripples no wind leaves off.
        const double eta = p.surface - body.level;
        const double m0 = p.hs * p.hs / 16;
        const double energy = kRho * kG * ((eta * eta > m0 ? eta * eta : m0) + kRipple * kRipple);
        const bool edge =
            p.surface - p.bed < kShoreDepth || (k + 1 < kRings && s[k + 1][j].body < 0);
        const double spent = energy * (edge ? 1 : kOpenWater) * w;
        lap.add(spent, side, r3);
        const double tp = body.waves.sea.tp > 0 ? body.waves.sea.tp : 1;
        period += spent * tp;
        period_w += spent;
        const double br = core::clamp(p.breaking.crest + p.breaking.shoal + p.breaking.wind, 0, 1);
        surf.add(energy * br * w, side, r3);
      } else if (body.kind == Kind::channel || body.kind == Kind::flow) {
        const double v = core::hypot3(p.vx, p.vy, p.vz);
        const double spent = kRho * kBedFriction * v * v * v * w;
        rush.add(spent, side, r3);
        speed += spent * v;
        speed_w += spent;
      }
    }
  }
  double loudest = 0;
  for (const Flow& flow : world.flows) {
    for (const Fall& f : flow.falls) {
      const double drop = f.y - f.land_y;
      if (drop <= 0 || f.discharge <= 0) continue;
      const double dx = f.land_x - x, dy = f.land_y - y, dz = f.land_z - z;
      const double r3 = core::hypot3(dx, dy, dz);
      const double rr = r3 > kNearestFall ? r3 : kNearestFall;
      const double i = kRho * kG * f.discharge * drop / (2 * kPi * rr * rr);
      const double flat = core::hypot(dx, dz);
      const double side = flat > 0 ? (dx * rx + dz * rz) / rr : 0;
      roar.add(i, side, r3);
      if (i > loudest) {
        loudest = i;
        h.drop = static_cast<float>(drop);
      }
    }
  }
  h.lap = lap.heard(kFullLap);
  h.surf = surf.heard(kFullSurf);
  h.rush = rush.heard(kFullRush);
  h.roar = roar.heard(kFullRoar);
  if (period_w > 0) h.period = static_cast<float>(period / period_w);
  if (speed_w > 0) h.speed = static_cast<float>(speed / speed_w);
  return h;
}

const std::array<LayerSpec, kWaterLayers> kWaterSpecs = {
    noise(Color::pink, FilterType::bandpass, 0.9f),   // lap
    noise(Color::pink, FilterType::bandpass, 0.6f),   // surf
    noise(Color::pink, FilterType::lowpass, 0.6f),    // rush
    noise(Color::white, FilterType::bandpass, 2.5f),  // babble
    noise(Color::brown, FilterType::lowpass, 0.8f),   // roar
    noise(Color::pink, FilterType::highpass, 0.7f),   // spray
    noise(Color::brown, FilterType::lowpass, 1.0f),   // room
};

// The lapping follows the waves past the shore; a river and a fall are steady.
const std::array<float, kWaterLayers> kWaterGlides = {0.08f, 0.08f, 0.3f, 0.15f, 0.3f, 0.3f, 0.25f};

std::array<audio::LayerTarget, kWaterLayers> water_targets(const Hearing& heard,
                                                           const WaterMix& mix) {
  const float bright = 0.5f + 0.5f * audio::clamp01(mix.tone);
  // The air takes a far voice's highs.
  const auto air = [](float d) { return 1 / (1 + d / 150); };
  const float period = heard.period > 0.8f ? heard.period : 0.8f;
  std::array<audio::LayerTarget, kWaterLayers> t{};
  t[kLap].level = 0.05f * heard.lap.level * mix.level;
  t[kLap].cutoff = 1400 / (1 + 0.35f * period) * air(heard.lap.distance) * bright;
  t[kLap].pan = 0.7f * heard.lap.pan;
  t[kSurf].level = 0.02f * heard.surf.level * mix.level;
  t[kSurf].cutoff = 2400 * air(heard.surf.distance) * bright;
  t[kSurf].pan = 0.7f * heard.surf.pan;
  t[kRush].level = 0.03f * heard.rush.level * mix.level;
  t[kRush].cutoff = (500 + 450 * heard.speed) * air(heard.rush.distance) * bright;
  t[kRush].pan = 0.7f * heard.rush.pan;
  t[kBabble].level = 0.012f * heard.rush.level * mix.level;
  t[kBabble].cutoff = (900 + 350 * heard.speed) * air(heard.rush.distance) * bright;
  t[kBabble].pan = 0.7f * heard.rush.pan;
  t[kRoar].level = 0.045f * heard.roar.level * mix.level;
  t[kRoar].cutoff = (120 + 900 / (1 + heard.drop / 8)) * air(heard.roar.distance);
  t[kRoar].pan = 0.7f * heard.roar.pan;
  // The spray is the drops landing round the ear: only near the foot, where the roar's hiss
  // is; from afar a fall is its low roar.
  const float near = heard.roar.distance / 20;
  t[kSpray].level = 0.016f * heard.roar.level / (1 + near * near) * mix.level;
  t[kSpray].cutoff = 1800 * air(heard.roar.distance) * bright;
  t[kSpray].pan = 0.7f * heard.roar.pan;
  // Under water the water's own voices keep their level (they reach the ear through the water)
  // and lose their highs; the room, the pressure in the ears and the water's movement carried
  // far, comes up.
  const float u = audio::clamp01(heard.under);
  for (int i = 0; i < kRoom; ++i) {
    const float c = t[i].cutoff;
    t[i].cutoff = c + ((c < kUnderWaterHz ? c : kUnderWaterHz) - c) * u;
  }
  t[kRoom].level = u * (0.01f + 0.02f * (heard.roar.level + 0.5f * heard.rush.level)) * mix.level;
  t[kRoom].cutoff = 140;
  for (audio::LayerTarget& x : t) x.cutoff = audio::safe_cutoff(x.cutoff);
  return t;
}

void muffle(audio::LayerTarget& target, float under) {
  const float u = audio::clamp01(under);
  const float c = target.cutoff;
  target.cutoff = c + ((c < kMuffleHz ? c : kMuffleHz) - c) * u;
  target.level *= 1 + (kMuffleGain - 1) * u;
}

void muffle(audio::PlayShape& shape, float under) {
  const float u = audio::clamp01(under);
  shape.pitch *= 1 - 0.55f * u;
  shape.gain *= 1 + (kMuffleGain - 1) * u;
}

WaterBed make_water_bed(int bus, uint32_t seed) {
  WaterBed bed;
  bed.rack.specs = kWaterSpecs;
  bed.rack.glides_s = kWaterGlides;
  bed.rack.bus = bus;
  bed.rng = core::make_rng(seed);
  return bed;
}

Hearing water_bed_update(WaterBed& bed, audio::Synth& synth, const World& world, double x, double y,
                         double z, double t, double heading, float dt, const WaterMix& mix) {
  const Hearing heard = hear(world, x, y, z, t, heading);
  audio::rack_apply(bed.rack, synth, water_targets(heard, mix));
  water_bubbles(synth, bed.rng, heard, dt, bed.rack.bus);
  return heard;
}

void water_bed_silence(WaterBed& bed, audio::Synth& synth) { audio::rack_stop(bed.rack, synth); }

double minnaert_hz(double radius) { return 3.26 / (radius > 1e-5 ? radius : 1e-5); }

Tone bubble(double radius, float volume, float delay_ms, float pan) {
  const double hz = minnaert_hz(radius);
  const double ms = core::clamp(30 * std::sqrt(radius / 0.003), 15, 150);
  return Tone{.wave = Wave::sine,
              .from_hz = static_cast<float>(hz),
              .to_hz = static_cast<float>(hz * 1.12),
              .duration_ms = static_cast<float>(ms),
              .volume = volume,
              .delay_ms = delay_ms,
              .attack_ms = audio::kMinAttackMs,
              .pan = pan};
}

void play_bubbles(audio::Synth& synth, core::Rng& rng, int count, double lo, double hi,
                  float spread_ms, float volume, int bus) {
  const double ratio = hi / lo;
  for (int i = 0; i < count; ++i) {
    const double r = lo * lm::pow(ratio, core::next(rng));
    const float delay = static_cast<float>(core::next(rng)) * spread_ms;
    const float v = volume * (0.6f + 0.4f * static_cast<float>(core::next(rng)));
    const float pan = static_cast<float>(core::range(rng, -0.4, 0.4));
    synth.tone(bubble(r, v, delay, pan), bus);
  }
}

void water_bubbles(audio::Synth& synth, core::Rng& rng, const Hearing& heard, float dt, int bus) {
  const float u = audio::clamp01(heard.under);
  // A river's babble is bubbles in its eddies, small and bright; under a fall the plunge's churn
  // is bubbles of every size, and under water all of it is near.
  const double babble = (1 - u) * 18 * heard.rush.level + (1 - u) * 6 * heard.roar.level;
  const double churn = u * (1.5 + 30 * heard.roar.level + 8 * heard.rush.level);
  const auto loose = [&](double rate, double lo, double hi, float volume) {
    const double lambda = rate * dt;
    int n = static_cast<int>(lambda);
    if (core::chance(rng, lambda - n)) ++n;
    if (n > 8) n = 8;
    if (n > 0) play_bubbles(synth, rng, n, lo, hi, dt * 1000, volume, bus);
  };
  loose(babble, 0.0015, 0.005, 0.005f * (heard.rush.level + heard.roar.level));
  loose(churn, 0.002, 0.012, 0.012f);
}

audio::PlayShape splash_shape(const Splash& splash) {
  const double size = (splash.size > 0.005 ? splash.size : 0.005) / 0.15;
  const double speed = splash.speed / 5;
  audio::PlayShape shape;
  shape.gain = static_cast<float>(core::clamp(
      core::clamp(speed, 0.15, 2.0) * core::clamp(lm::pow(size, 0.2), 0.5, 1.4), 0, 2.2));
  shape.pitch = static_cast<float>(core::clamp(lm::pow(size, -0.5), 0.3, 2.4));
  shape.stretch = static_cast<float>(core::clamp(lm::pow(size, 0.4), 0.6, 2.4));
  if (!splash.entering) shape.gain *= 0.6f;
  return shape;
}

void play_splash(audio::Synth& synth, core::Rng& rng, const Splash& splash, float under, int bus,
                 float pan) {
  audio::PlayShape shape = splash_shape(splash);
  shape.place = true;
  shape.pan = pan;
  under_water(shape, under);
  play_named(synth, splash.entering ? "splash" : "drip", shape, bus);
  if (!splash.entering) return;
  // The cavity's whump grows with the body faster than the crack does: a pebble plips, a
  // boulder booms.
  const double size = (splash.size > 0.005 ? splash.size : 0.005) / 0.15;
  audio::PlayShape plunge = shape;
  plunge.gain *= static_cast<float>(core::clamp(lm::pow(size, 0.9), 0.05, 2.5));
  play_named(synth, "plunge", plunge, bus);
  // A body going in drags a cloud of air down with it, which rings as it breaks up.
  const double speed = splash.speed / 5 < 2 ? splash.speed / 5 : 2;
  const int count = static_cast<int>(core::clamp(2 + 6 * std::sqrt(size) * speed, 2, 24));
  const double lo = core::clamp(0.001 * lm::pow(size, 0.6), 0.0006, 0.01);
  const float volume = 0.006f * (1 + 0.6f * audio::clamp01(under)) *
                       static_cast<float>(std::sqrt(speed > 0.1 ? speed : 0.1));
  play_bubbles(synth, rng, count, lo, 5 * lo, 350 * shape.stretch, volume, bus);
}

void play_swim(audio::Synth& synth, WaterBed& bed, const SwimEvents& events, const SwimState& state,
               float under, int bus) {
  if (events.entered || events.left) {
    const Splash s{.speed = events.splash_speed > 0.5 ? events.splash_speed : 0.5,
                   .size = 0.25,
                   .entering = events.entered};
    play_splash(synth, bed.rng, s, under, bus);
  }
  if (events.stroke) {
    const double v = core::hypot3(state.v.x, state.v.y, state.v.z);
    audio::PlayShape shape;
    shape.gain = 1.0f + 0.6f * static_cast<float>(v / 1.5 < 1 ? v / 1.5 : 1);
    under_water(shape, under);
    play_named(synth, "stroke", shape, bus);
    if (state.level >= 3) {
      // Under, a stroke lets a little of the held breath go.
      play_bubbles(synth, bed.rng, 3 + core::int_in(bed.rng, 0, 2), 0.003, 0.008, 300, 0.012f, bus);
    } else if (state.level == 2 && ++bed.strokes % 2 == 0) {
      play_named(synth, "breath", {}, bus);
    }
  }
  if (events.gasp) play_named(synth, "gasp", {}, bus);
}

}  // namespace ge::water
