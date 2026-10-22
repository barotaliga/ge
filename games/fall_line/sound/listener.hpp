// WHERE THE EAR IS: what each seat of the camera does to the mix. The picture moves from the ski
// tips to a crane shot and the sound moves with it, or a high shot is a helmet's ear on a long
// lens. Every number is a multiplier on one part of the mix, read by the ride bed every frame and
// by the route for the one-shots:
//
//   tips    the snow an arm's length away: the bases hissing, the edges tearing, the wind full
//   helmet  you are the skier: the wind roaring round the helmet, the skis a little muffled
//   chase   behind and above, the seat the game is tuned at: everything in proportion
//   far     stood back: the skier a small thing on a big hill, the wind going, the snow thinner
//   high    craned up: the wind a thread, the one-shots softened by the air between
//
// ge:summary The listener table: the mix each camera seat hears
// ge:keywords listener seat camera mix wind snow events muffle tips helmet chase far high
// ge:skill audio sim
// ge:test tests/fall_line_sound_test.cpp
#pragma once

#include <array>
#include <cstdint>

namespace fall_line::sound {

enum class Seat : uint8_t { tips, helmet, chase, far, high };

struct Listener {
  float wind;    // the skier's own wind
  float tone;    // how bright the wind is, 0..1
  float snow;    // the skis on the snow
  float events;  // every one-shot the run makes
  float muffle;  // a pitch on those one-shots; under 1 moves every filter down with it
};

inline constexpr std::array<Listener, 5> kListeners = {{
    {1.3f, 1, 1.4f, 1.1f, 1},         // tips
    {1.45f, 0.85f, 1.05f, 1, 1},      // helmet
    {0.85f, 1, 1.05f, 1, 1},          // chase
    {0.4f, 0.9f, 0.8f, 0.9f, 0.95f},  // far
    {0.2f, 0.8f, 0.6f, 0.8f, 0.9f},   // high
}};

inline const Listener& listener_for(Seat seat) { return kListeners[static_cast<size_t>(seat)]; }

}  // namespace fall_line::sound
