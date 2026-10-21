// The rack's build-once-then-steer loop.
//
// ge:summary The rack's apply and stop
// ge:keywords rack rack_apply rack_stop
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#include "audio/rack.hpp"

namespace ge::audio {

void rack_apply(Rack& rack, Synth& synth, std::span<const LayerTarget> targets) {
  const size_t n = rack.specs.size() < static_cast<size_t>(kRackMax)
                       ? rack.specs.size()
                       : static_cast<size_t>(kRackMax);
  if (!rack.built) {
    for (size_t i = 0; i < n; i++) rack.ids[i] = synth.layer(rack.specs[i], rack.bus);
    rack.built = true;
  }
  for (size_t i = 0; i < n && i < targets.size(); i++) {
    synth.set(rack.ids[i], targets[i], i < rack.glides_s.size() ? rack.glides_s[i] : 0.1f);
  }
}

void rack_stop(Rack& rack, Synth& synth) {
  if (!rack.built) return;
  for (LayerId& id : rack.ids) {
    synth.stop(id);
    id = LayerId{};
  }
  rack.built = false;
}

}  // namespace ge::audio
