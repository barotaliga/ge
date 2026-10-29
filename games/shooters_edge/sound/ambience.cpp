// Shooter's Edge's ambience: the leaf's levels read as targets, the bed stepped.
//
// ge:summary The ambience's targets off the listener's leaf and the bed's step
// ge:keywords ambience leaf target level volume step
// ge:skill audio
// ge:test tests/shooters_edge_ambience_test.cpp
#include "shooters_edge/sound/ambience.hpp"

namespace shooters_edge {

void leaf_ambience(const ge::brush::Map& map, const float at[3], float volume, float out[4]) {
  for (int k = 0; k < 4; k++) out[k] = 0;
  if (map.leaves.empty() || map.nodes.empty()) return;
  const ge::brush::Leaf& leaf = map.leaves[ge::brush::leaf_at(map, at)];
  if (leaf.solid) return;
  for (int k = 0; k < 4; k++) out[k] = static_cast<float>(leaf.ambient[k]) / 255.0f * volume;
}

Ambience::Ambience(const ge::audio::Pcm& water, const ge::audio::Pcm& sky, int bus)
    : bed_(kAmbientRate, kAmbientFloor, bus), water_(bed_.add(water)), sky_(bed_.add(sky)) {}

void Ambience::step(ge::audio::Synth& synth, const ge::brush::Map& map, const float at[3], float dt,
                    float volume) {
  float levels[4];
  leaf_ambience(map, at, volume, levels);
  bed_.target(water_, levels[ge::brush::kAmbientWater]);
  bed_.target(sky_, levels[ge::brush::kAmbientSky]);
  bed_.step(synth, dt);
}

}  // namespace shooters_edge
