// THE RIDE BED: the one place a skier's state becomes the wind's and the snow's targets, steered
// once a frame. NOTHING IS BOOKED AHEAD: the layers run on the render side and each frame only
// tells them where to go next, over their glides, so a late frame leaves every layer holding its
// last value instead of leaving a hole. Silencing has to be SAID (`ride_bed_silence`): a bed
// that is merely not fed holds its last note, which is a wind blowing on behind a paused run.
//
// What it reads off the skier: the speed (the apparent wind in still air, and the pace against
// the pair's top speed), the tuck, whether he is on the snow, the snow's packed share, the edge
// against the pair's most, and the skid (the share of his way that is across his skis). A game
// adds nothing but where the ear is (`RideHeard`, its listener's numbers) and which buses the two
// racks play on (its faders).
//
// ge:summary The ride bed: a skier's state steered into the wind and snow racks every frame
// ge:keywords ride bed wind snow rack steer skid pace apparent-wind silence frame listener
// ge:skill skier audio
// ge:test tests/skier_ride_bed_test.cpp
#pragma once

#include "audio/rack.hpp"
#include "audio/synth.hpp"
#include "skier/app/snow_bed.hpp"
#include "skier/app/wind_bed.hpp"
#include "skier/skier.hpp"

namespace ge::skier {

// Where the ear is, as the two beds' mixes.
struct RideHeard {
  WindMix wind;
  SnowMix snow;
};

struct RideBed {
  audio::Rack wind;
  audio::Rack snow;
  float air = 0;  // the apparent wind, followed on `kRideAirTau`
};

// How quickly the wind follows the air, s: a time constant rather than a per-frame share, which
// would only be true at the frame rate it was tuned at.
inline constexpr float kRideAirTau = 0.25f;

// A bed whose wind plays on `wind_bus` and whose snow plays on `snow_bus`.
RideBed make_ride_bed(int wind_bus, int snow_bus);

// What the bed hears of `skier` on `spec` this frame (pure; the tests and the audition lab read
// it). `air` is the followed apparent wind, m/s.
WindVoice wind_voice_of(const SkierState& skier, float air);
SnowVoice snow_voice_of(const SkierState& skier, const SkiSpec& spec);

// Steer every layer for this frame; `dt` is the frame's own elapsed time, s.
void ride_bed_update(RideBed& bed, audio::Synth& synth, const SkierState& skier,
                     const SkiSpec& spec, float dt, const RideHeard& heard);

// Tear the layers down (the pause card, a run left); the next update builds them again.
void ride_bed_silence(RideBed& bed, audio::Synth& synth);

}  // namespace ge::skier
