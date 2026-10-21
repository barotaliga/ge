// THE WIND ON A SKIER, as five noise layers that never stop. A skier has no motor, so what he
// hears of his own speed is the AIR, and the bed is built from it in the order the ear finds it:
//
//   rush     the broad pink rush past the helmet, climbing with the square of the airspeed (the
//            pressure does), louder in the air with the snow gone from under it
//   roar     the low brown roar of the tuck: the head down behind the hands, the wind breaking
//            over the back; the layer that says TUCK before the speedo does
//   whistle  a thin white band that comes in past 40 km/h of air: the goggles' strap, the poles
//            under the arms, the helmet's edge
//   flutter  the suit flapping, loudest stood up and gone quiet in a tuck, where the cloth is
//            pulled tight; caught side-on, it is louder still
//   buffet   the gale: a battering under everything that comes in past 90 km/h of air
//
// WHAT IS HEARD IS THE APPARENT WIND, never the speedo: the air less the skier's own velocity,
// so a headwind adds to his speed and a storm is heard standing still. Past a schuss the rush
// climbs on the cube of the air rather than the square, because air torn to turbulence round a
// helmet is that loud; joined at `kWindFull`, so nothing under it moves.
//
// THE TUCK IS WHY THIS SOUNDS LIKE SKIING: folding down drops the drag (the physics) and moves
// the ear out of the clean air into the roar behind the hands (this file), so the player hears his
// own body. `wind_targets` is a pure function of the moment; `ride_bed.hpp` steers the layers.
//
// ge:summary The skier's wind bed: five noise layers off the apparent wind and the tuck
// ge:keywords wind bed rush roar whistle flutter buffet apparent-wind tuck layers sound
// ge:skill skier audio
// ge:test tests/skier_ride_bed_test.cpp
#pragma once

#include <array>

#include "audio/voice.hpp"

namespace ge::skier {

// The apparent wind the layers are scaled to, m/s: a downhill ski flat out in a tuck.
inline constexpr float kWindFull = 40;
// The apparent wind past which nothing gets louder, m/s (about 250 km/h).
inline constexpr float kWindTop = 70;

// One skier's wind at one moment.
struct WindVoice {
  float wind = 0;         // the apparent wind at his head, m/s
  float tuck = 0;         // how far the body is folded, 0..1
  bool airborne = false;  // off the snow: the rush comes up
  float side = 0;         // which ear the wind lands on, -1 left .. 1 right (screen space), 0 ahead
};

// What the listener's seat does to the wind.
struct WindMix {
  float wind = 1;  // the level
  float tone = 1;  // 0..1, how bright: the rush's and the whistle's cutoffs are scaled by it
};

enum WindLayer : int { kRush, kRoar, kWhistle, kFlutter, kBuffet, kWindLayers };

// What each layer is built from (decided once) and how fast it follows, s: the rush on a slow
// glide (a gust is not a switch), the roar slower still (a tuck is folded into).
extern const std::array<audio::LayerSpec, kWindLayers> kWindSpecs;
extern const std::array<float, kWindLayers> kWindGlides;

std::array<audio::LayerTarget, kWindLayers> wind_targets(const WindVoice& voice,
                                                         const WindMix& mix);

}  // namespace ge::skier
