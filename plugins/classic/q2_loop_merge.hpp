// QUAKE II'S LOOPS MERGED (`al_merge_looping`): the entities carrying ONE looped sound (a row of
// machines humming, a lift's motors) heard as one voice, as the game's own mixer hears them, so a
// dozen copies are not a dozen times as loud. Each entity's share (`q2_loop_ear`): its loop's
// volume times the fall-off, 1 within 80 units of the ear and then down by the loop's distance
// multiplier a unit (`q2_loop_multiplier`), split between the ears by its side, the right ear
// half of 1 plus the dot of the way to it with the listener's right; a loop with no attenuation, or
// the listener's own, is centred. The shares are summed ear by ear and each sum held to 1
// (`q2_merge`); the voice's gain is the two sums' total, held to 1, and its pan their difference
// over their total.
//
// QUAKE'S STATIC LOOPS (both Quake and QuakeWorld, always, as their own mixer does): the level's
// static sounds playing ONE file are heard as one voice too. Each source's ears by Quake's law
// (`classic_static_ear`): its volume times the fall-off (1 - distance × attenuation / 1000, no
// near zone), the right ear times 1 plus its side and the left 1 minus it (so a source square to
// one side is twice as loud in that ear); the ears summed, each held to 1 (a centred source at full
// volume), and the voice's gain their mean and its pan their balance (`classic_static_merge`).
//
// `al_merge_looping` 0 never merges (each copy its own voice), 1 merges while the output is plain
// stereo (the two ears of s_hrtf 2 are not), 2 always (`q2_merges`).
//
// ge:summary Copies of one looped sound merged into one voice: Quake II's loops, Quake's statics
// ge:keywords al_merge_looping loop-merge looped-sounds entity-loops merged-loop q2_merge
// ge:keywords q2_loop_ear q2_loop_multiplier q2_merges static-sounds classic_static_merge
// ge:skill classic audio
// ge:test tests/classic_q2_footsteps_test.cpp
#pragma once

#include <algorithm>

namespace ge::classic {

inline constexpr double kQ2FullVolumeUnits = 80;  // within this a sound is heard whole

// The two ears' shares of one loop.
struct Q2Ears {
  double left = 0, right = 0;
};

// A loop's fall-off a unit for the attenuation `att` it was sent with (`stated`; unstated: the
// static loop's): none 0, static (3) and unstated 0.003, any other att × 0.0006.
inline double q2_loop_multiplier(bool stated, double att) {
  if (!stated || att == 3) return 0.003;
  return att <= 0 ? 0 : att * 0.0006;
}

// One entity's loop of volume `volume` (0..1) `distance` units away, `side` the dot of the way
// to it with the listener's right (-1..1), heard centred when `centred`.
inline Q2Ears q2_loop_ear(double volume, double distance, double side, double multiplier,
                          bool centred) {
  const double past = std::max(0.0, distance - kQ2FullVolumeUnits);
  const double v = volume * std::max(0.0, 1 - past * multiplier);
  if (centred || multiplier <= 0) return {v, v};
  return {v * 0.5 * (1 - side), v * 0.5 * (1 + side)};
}

// The merged voice of summed shares: its gain (0..1, 0 silent) and pan (-1 left .. 1 right).
struct Q2Merged {
  double gain = 0, pan = 0;
};
inline Q2Merged q2_merge(Q2Ears sum) {
  const double l = std::min(sum.left, 1.0), r = std::min(sum.right, 1.0);
  if (l + r <= 0) return {};
  return {std::min(l + r, 1.0), (r - l) / (r + l)};
}

// One Quake static source's ears: `volume` (0..1), `distance` units, `side` -1..1, `attenuation`
// the protocol's factor (3 static).
inline Q2Ears classic_static_ear(double volume, double distance, double side, double attenuation) {
  const double v = volume * std::max(0.0, 1 - distance * attenuation / 1000);
  return {v * (1 - side), v * (1 + side)};
}
// The merged voice of Quake's summed static ears: each held to 1, the gain their mean.
inline Q2Merged classic_static_merge(Q2Ears sum) {
  const double l = std::min(sum.left, 1.0), r = std::min(sum.right, 1.0);
  if (l + r <= 0) return {};
  return {(l + r) / 2, (r - l) / (r + l)};
}

// Whether `al_merge_looping` `mode` merges on an output that is plain stereo or not.
inline bool q2_merges(int mode, bool plain_stereo) {
  return mode >= 2 || (mode == 1 && plain_stereo);
}

}  // namespace ge::classic
