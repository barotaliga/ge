// A recorded game heard (demo_sound.hpp).
//
// ge:summary The demo's sounds: recordings loaded by the sound list, started, moved, stopped
// ge:keywords demo-sound recording sound-list channels listener ambience loops
// ge:skill audio shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_sound.hpp"

#include <algorithm>

#include <cmath>

namespace shooters_edge::client {

namespace {

namespace au = ge::audio;

constexpr float kUnitsPerAttenuation = 1000;  // a sound of attenuation 1 dies out at this range

ge::core::Vec3 at_of(sh::Vec3f p) { return {p.x, p.y, p.z}; }

}  // namespace

DemoSounds::DemoSounds(au::Synth& synth, const SearchPath& files, float volume, int bus,
                       const GameAdapter& game)
    : synth_(synth),
      files_(files),
      game_(game),
      volume_(volume),
      bus_(bus),
      channels_(synth, kDemoChannels, bus) {}

const au::Pcm* DemoSounds::load(const std::string& name) {
  auto it = loaded_.find(name);
  if (it != loaded_.end()) return it->second.get();
  auto pcm = std::make_unique<au::Pcm>();
  std::string error;
  if (!game_.open_sound(game_, files_, name, *pcm, error)) pcm.reset();
  return loaded_.emplace(name, std::move(pcm)).first->second.get();
}

const au::Pcm* DemoSounds::recording(int index) {
  if (index <= 0 || static_cast<size_t>(index) >= list_.size()) return nullptr;
  return list_[static_cast<size_t>(index)];
}

void DemoSounds::level(const DemoState& s, const ge::brush::Map* map) {
  if (s.sounds != names_) {
    names_ = s.sounds;
    list_.assign(names_.size(), nullptr);
    for (size_t i = 1; i < names_.size(); i++) {
      list_[i] = load(names_[i]);
      if (!list_[i]) stats_.missing++;
    }
  }
  if (map != map_) {
    channels_.stop(-1);  // the old level's loops
    statics_started_ = 0;
    map_ = map;
    if (ambience_) ambience_->silence(synth_);
    ambience_.reset();
    const au::Pcm* water = load("ambience/water1.wav");
    const au::Pcm* sky = load("ambience/wind2.wav");
    if (map && water && sky) ambience_.emplace(*water, *sky, bus_);
  }
}

void DemoSounds::silence() {
  std::vector<int> entities;  // every entity sounding, once (stopping changes the slots)
  for (const au::Channels::Slot& slot : channels_.slots())
    if (slot.voice.valid() &&
        std::find(entities.begin(), entities.end(), slot.entity) == entities.end())
      entities.push_back(slot.entity);
  for (int e : entities) channels_.stop(e);
  if (ambience_) ambience_->silence(synth_);
}

void DemoSounds::frame(const DemoState& s, int listener, float dt) {
  frame(s, s.started, s.stopped, listener, dt);
}

void DemoSounds::frame(const DemoState& s, std::span<const DemoSound> started,
                       std::span<const std::pair<int, int>> stopped, int listener, float dt) {
  // The level's loops as they arrive (the signon sends them after the level starts).
  for (; statics_started_ < s.static_sounds.size(); statics_started_++) {
    const DemoStaticSound& st = s.static_sounds[statics_started_];
    const au::Pcm* pcm = recording(st.sound);
    if (!pcm) continue;
    au::Emit e;
    e.entity = -1;
    e.at = at_of(st.origin);
    e.volume = st.volume * volume_;
    e.attenuation = st.attenuation / kUnitsPerAttenuation;
    e.loop = au::Loop::file;
    if (channels_.play(*pcm, e).valid()) stats_.loops++;
  }
  for (const auto& [entity, channel] : stopped) channels_.stop(entity, channel);
  for (const DemoSound& d : started) {
    const au::Pcm* pcm = recording(d.sound);
    if (!pcm) continue;
    au::Emit e;
    e.entity = d.entity;
    e.channel = d.channel;
    e.at = at_of(d.origin);
    e.volume = d.volume * volume_;
    e.attenuation = d.attenuation / kUnitsPerAttenuation;
    e.loop = au::Loop::once;
    channels_.play(*pcm, e);
    stats_.started++;
  }
  // Sounds ride their entities: a door grinding as it opens, a rocket's roar in flight.
  for (const au::Channels::Slot& slot : channels_.slots()) {
    const int n = slot.entity;
    if (n <= 0) continue;
    if (static_cast<size_t>(n) < s.entities.size() && s.entities[static_cast<size_t>(n)].present)
      channels_.move(n, at_of(s.entities[static_cast<size_t>(n)].origin));
    else if (n - 1 < static_cast<int>(s.players.size()) && s.format != DemoFormat::single &&
             s.players[static_cast<size_t>(n - 1)].present)
      channels_.move(n, at_of(s.players[static_cast<size_t>(n - 1)].origin));
  }
  au::Listener l;
  l.at = at_of(s.eye);
  const double yaw = s.angles.y * (3.14159265358979323846 / 180.0);
  l.right = {std::sin(yaw), -std::cos(yaw), 0};  // the map's yaw turns counter-clockwise from +x
  l.entity = listener;
  channels_.update(l);
  if (ambience_ && map_) {
    const float at[3] = {s.eye.x, s.eye.y, s.eye.z};
    ambience_->step(synth_, *map_, at, dt, kAmbientVolume * volume_ / kDemoVolume);
  }
}

}  // namespace shooters_edge::client
