// THE SKIS ON THE SNOW, as five noise layers: what a skier hears of his own machine is the bases
// and the steel on whatever lies under them, so this is the bed every run is made of under the
// wind.
//
//   hiss     the bases running on the groomer: a bright pink sheet that climbs with the speed
//   powder   the skis ploughing loose snow: a dark brown hush, loudest slow, where he is still
//            sunk in and shoving snow aside, thinning as he planes up on top
//   edge     the steel biting in a carve on firm snow: a white tear that comes with the edge and
//            the speed together, and goes when the skis are skidded
//   chatter  the edge skipping on firm snow at speed: a narrow mid buzz, nothing slow or soft
//   skid     the skis pivoted across the way scraping the snow off ahead of their edges: a broad
//            rasp with the skid and the speed, brighter on firm snow, softer in loose
//
// WHAT LIES UNDER THE SKIS is the snow's packed share (`SkierState::packed`, the field the
// physics' grip reads), so skiing off the piste is heard as the hiss giving way to the hush
// before anything else changes. THE PAIR COLOURS IT (`SkiVoice`, off its spec): a fat ski throws
// a broader, darker hush, a long one drones lower on the groomer.
//
// A pure function of the moment, like the wind: `snow_targets` says where every layer should
// be, `ride_bed.hpp` steers them there.
//
// ge:summary The skis-on-snow bed: hiss, powder hush, edge, chatter and skid off the snow
// ge:keywords snow bed hiss powder edge chatter skid packed groomer loose layers sound ski-voice
// ge:skill skier audio
// ge:test tests/skier_ride_bed_test.cpp
#pragma once

#include <array>

#include "audio/voice.hpp"
#include "skier/skis.hpp"

namespace ge::skier {

// The pair as heard, off its spec.
struct SkiVoice {
  float width = 0.44f;   // 0 a 66 mm race waist .. 1 a 116 mm powder waist
  float length = 0.37f;  // 0 a 1.55 m slalom ski .. 1 a 2.18 m downhill ski
};

SkiVoice ski_voice_of(const SkiSpec& spec);

// One moment on the snow.
struct SnowVoice {
  float speed = 0;   // m/s
  float pace = 0;    // the speed as a share of the pair's top speed
  float packed = 1;  // the snow's packed share under the skis, 0 powder .. 1 groomer
  float edge = 0;    // the edge the skis stand on, 0..1 of the most (the sign is not heard)
  float skid = 0;    // 0..1, how far across the way the skis are pivoted
  bool on_snow = true;
  SkiVoice ski{};
};

// What the listener's seat does to the snow.
struct SnowMix {
  float snow = 1;
};

enum SnowLayer : int { kHiss, kPowder, kEdgeTear, kChatter, kSkidRasp, kSnowLayers };

extern const std::array<audio::LayerSpec, kSnowLayers> kSnowSpecs;
// How fast each layer follows, s: the ground layers on a tenth (leaving the snow is a cross-fade,
// not a switch), the edge and the skid quicker, since an edge set is a moment.
extern const std::array<float, kSnowLayers> kSnowGlides;

std::array<audio::LayerTarget, kSnowLayers> snow_targets(const SnowVoice& voice,
                                                         const SnowMix& mix);

}  // namespace ge::skier
