// THE MAP'S OWN AMBIENCE: water nearby and open sky overhead, heard as two looped recordings under
// the fight, each as loud as the leaf the listener stands in says. A map's compiler stores, per
// leaf, how much open water, sky, slime and lava that leaf can see (`brush::Leaf::ambient`); the
// game's ambience plays the first two (`ambient/leaf_water`, `ambient/leaf_sky`) on an
// `audio::AmbientBed`, so walking out of a hall into a courtyard swells the wind over about two
// seconds instead of switching it on.
//
// - The targets (`leaf_ambience`) are the leaf's levels over 255 times the player's ambient volume
//   (`kAmbientVolume` unless set). A solid leaf, or a map that stores no levels (the 38 and 46
//   families), is silence.
// - The bed walks a level at `kAmbientRate` a second and treats a target under `kAmbientFloor`
//   as silence.
//
// ge:summary Shooter's Edge's ambience by leaf: water and sky levels off the map, on an ambient bed
// ge:keywords ambience ambient leaf water sky slime lava bed level volume map listener
// ge:skill audio
// ge:test tests/shooters_edge_ambience_test.cpp
#pragma once

#include "audio/ambient.hpp"
#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "brush/map.hpp"

namespace shooters_edge {

inline constexpr float kAmbientVolume = 0.3f;  // the player's ambient volume by default
inline constexpr float kAmbientRate = 0.4f;    // the most a level moves a second
inline constexpr float kAmbientFloor = 0.03f;  // a target under this is silence

// The four levels (water, sky, slime, lava; `brush::kAmbientWater` ...) a listener at `at` (map
// units) hears, each 0..1.
void leaf_ambience(const ge::brush::Map& map, const float at[3], float volume, float out[4]);

// The game's ambience: the water and sky recordings on one bed.
class Ambience {
 public:
  Ambience(const ge::audio::Pcm& water, const ge::audio::Pcm& sky, int bus = 0);
  // The listener is at `at` in `map`; move the bed on by `dt` seconds.
  void step(ge::audio::Synth& synth, const ge::brush::Map& map, const float at[3], float dt,
            float volume = kAmbientVolume);
  void silence(ge::audio::Synth& synth) { bed_.silence(synth); }
  const ge::audio::AmbientBed& bed() const { return bed_; }

 private:
  ge::audio::AmbientBed bed_;
  int water_, sky_;
};

}  // namespace shooters_edge
