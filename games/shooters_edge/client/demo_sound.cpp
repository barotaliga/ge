// A recorded game heard (demo_sound.hpp).
//
// ge:summary The demo's sounds: recordings loaded by the sound list, started, moved, stopped
// ge:keywords demo-sound recording sound-list channels listener ambience loops
// ge:skill audio shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "classicenhance/cached.hpp"
#include "shooters_edge/profiles/remix_sounds.hpp"

namespace shooters_edge::client {

namespace {

namespace au = ge::audio;

constexpr float kUnitsPerAttenuation = 1000;  // a sound of attenuation 1 dies out at this range
// An entity's loop: its own channel beside the eight a sound message names, heard as a static
// sound (attenuation 3: full within 80 units, gone past about 413) at full volume unless the
// extended protocol's bytes say otherwise (porting note: q2-render-fx.md § Entity loop
// sounds). The bytes' units are taken as the sound message's: volume b / 255, attenuation b / 64.
constexpr int kLoopChannel = 256;
constexpr float kLoopAttenuation = 3;

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

void DemoSounds::enhance_from(const bool* on, std::filesystem::path cache) {
  enhanced_ = on;
  if (!cache.empty()) {
    cache_ = std::move(cache);
    cache_known_ = true;
  }
}

void DemoSounds::hear_as(std::string_view game) {
  if (game == heard_) return;
  heard_ = std::string(game);
  if (!names_.empty()) relist(false);
}

const au::Pcm* DemoSounds::load(const std::string& named) {
  // Heard as another game's (`cr_sounds`): loaded, and kept, by that game's name.
  const std::string_view heard = profiles::heard_as(named, heard_);
  if (heard.empty()) return nullptr;  // that game makes no such sound
  const std::string name(heard);
  const bool enhanced = enhanced_ != nullptr && *enhanced_;
  const std::string key = enhanced ? "+" + name : name;  // each copy kept apart
  auto it = loaded_.find(key);
  if (it != loaded_.end()) return it->second.get();
  auto pcm = std::make_unique<au::Pcm>();
  std::string error;
  bool read = false;
  if (enhanced) {
    if (!cache_known_) {
      const std::vector<std::span<const uint8_t>> packs = content_packs(files_);
      if (!packs.empty()) cache_ = ge::classicenhance::find_cache(packs);
      cache_known_ = true;
    }
    std::vector<uint8_t> wav;
    read = ge::classicenhance::cached_sound(cache_, name, &wav) &&
           au::read_wav(wav, name, pcm.get(), &error);
  }
  if (!read && !game_.open_sound(game_, files_, name, *pcm, error)) pcm.reset();
  return loaded_.emplace(key, std::move(pcm)).first->second.get();
}

const au::Pcm* DemoSounds::own(const DemoState& s, const DemoSound& d) {
  if (d.sound <= 0 || static_cast<size_t>(d.sound) >= names_.size()) return nullptr;
  return own(s, names_[static_cast<size_t>(d.sound)], d.entity);
}

const au::Pcm* DemoSounds::by_name(const DemoState& s, const DemoSound& d) {
  const std::string name = d.name;
  if (!name.empty() && name[0] == '*') return own(s, name, d.entity);
  return load(name);  // read once, then kept by its name
}

const au::Pcm* DemoSounds::own(const DemoState& s, const std::string& name, int entity) {
  if (name.size() < 2 || name[0] != '*') return nullptr;
  const std::string file = name.substr(1);
  if (!heard_.empty()) {  // heard as another game's: by the base voice's row
    const std::string base = "player/male/" + file;
    if (profiles::heard_as(base, heard_) != base) return load(base);
  }
  const int slot = entity - 1;
  std::string model = slot >= 0 && slot < static_cast<int>(s.looks.size())
                          ? s.looks[static_cast<size_t>(slot)]
                          : "";
  model = model.substr(0, model.find('/'));
  if (model.empty() || model.find("..") != std::string::npos) model = "male";
  const std::string key = "/players/" + model + "/" + file;  // apart from the names under sound/
  auto it = loaded_.find(key);
  if (it == loaded_.end()) {
    auto pcm = std::make_unique<au::Pcm>();
    std::vector<uint8_t> store;
    const std::string path = key.substr(1);
    const std::span<const uint8_t> bytes = find_file(files_, path, store);
    std::string error;
    if (bytes.empty() || !au::read_wav(bytes, path, pcm.get(), &error)) pcm.reset();
    it = loaded_.emplace(key, std::move(pcm)).first;
  }
  if (it->second) return it->second.get();
  return load("player/male/" + file);  // the model's folder lacks it: the base voice
}

const au::Pcm* DemoSounds::recording(int index) {
  if (index <= 0 || static_cast<size_t>(index) >= list_.size()) return nullptr;
  return list_[static_cast<size_t>(index)];
}

void DemoSounds::relist(bool count) {
  listed_enhanced_ = enhanced_ != nullptr && *enhanced_;
  list_.assign(names_.size(), nullptr);
  for (size_t i = 1; i < names_.size(); i++) {
    if (!names_[i].empty() && names_[i][0] == '*') continue;  // a player's own: by its model
    list_[i] = load(names_[i]);
    if (!list_[i] && count) stats_.missing++;
  }
}

void DemoSounds::level(const DemoState& s, const ge::brush::Map* map) {
  if (s.sounds != names_) {
    names_ = s.sounds;
    relist(true);
  }
  if (map != map_) {
    channels_.stop(-1);  // the old level's world sounds
    for (size_t i = 0; i < static_ever_.size(); i++) channels_.stop(static_entity(i));
    static_ever_.clear();
    stop_entity_loops();
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
  entity_loop_.assign(entity_loop_.size(), 0);
  if (ambience_) ambience_->silence(synth_);
}

void DemoSounds::stop_entity_loops() {
  for (size_t n = 0; n < entity_loop_.size(); n++)
    if (entity_loop_[n] != 0) channels_.stop(static_cast<int>(n), kLoopChannel);
  entity_loop_.assign(entity_loop_.size(), 0);
}

void DemoSounds::entity_loops(const DemoState& s) {
  if (entity_loop_.size() < s.entities.size()) {
    entity_loop_.resize(s.entities.size(), 0);
    loop_heard_.resize(s.entities.size(), 0);
  }
  // A loop's slot can be taken by a louder sound: what still plays is read off the slots.
  std::fill(loop_heard_.begin(), loop_heard_.end(), uint8_t{0});
  for (const au::Channels::Slot& slot : channels_.slots())
    if (slot.voice.valid() && slot.channel == kLoopChannel && slot.entity > 0 &&
        static_cast<size_t>(slot.entity) < loop_heard_.size())
      loop_heard_[static_cast<size_t>(slot.entity)] = 1;
  for (size_t n = 1; n < entity_loop_.size(); n++) {
    const DemoEntity* d = n < s.entities.size() ? &s.entities[n] : nullptr;
    const int want = d != nullptr && d->present ? d->loop : 0;
    const au::Pcm* pcm = want > 0 ? recording(want) : nullptr;
    if (pcm == nullptr) {  // gone, quiet now, or a sound no pack holds
      if (entity_loop_[n] != 0) channels_.stop(static_cast<int>(n), kLoopChannel);
      entity_loop_[n] = 0;
      continue;
    }
    if (entity_loop_[n] == want && loop_heard_[n] != 0) continue;
    au::Emit e;
    e.entity = static_cast<int>(n);
    e.channel = kLoopChannel;  // replaces the loop it had
    e.at = at_of(d->origin);
    e.volume = (d->loop_volume >= 0 ? static_cast<float>(d->loop_volume) / 255.0f : 1.0f) * volume_;
    e.attenuation = (d->loop_attenuation >= 0 ? static_cast<float>(d->loop_attenuation) / 64.0f
                                              : kLoopAttenuation) /
                    kUnitsPerAttenuation;
    e.loop = au::Loop::whole;  // the whole file round, its cue (a one-shot's loop point) aside
    const bool heard = channels_.play(*pcm, e).valid();  // no slot: tried again next frame
    if (heard) stats_.entity_loops++;
    entity_loop_[n] = heard ? want : 0;
  }
}

void DemoSounds::static_loops(const DemoState& s) {
  // The level's loops (the signon sends them after the level starts), each on an entity of its
  // own. The first try may take the quietest slot; a loop that lost its slot or found none
  // starts again once a slot is free, so two loops never take a slot from each other in turn.
  const size_t n = s.static_sounds.size();
  if (static_ever_.size() < n) static_ever_.resize(n, 0);
  static_heard_.assign(n, 0);
  int free = 0;
  for (const au::Channels::Slot& slot : channels_.slots()) {
    if (!slot.voice.valid()) free++;
    if (!slot.voice.valid() || slot.entity > kStaticEntity0) continue;
    const size_t i = static_cast<size_t>(kStaticEntity0 - slot.entity);
    if (i < n) static_heard_[i] = 1;
  }
  for (size_t i = 0; i < n; i++) {
    if (static_heard_[i] != 0 || static_ever_[i] == kStaticDone ||
        (static_ever_[i] == kStaticTried && free == 0))
      continue;
    const DemoStaticSound& st = s.static_sounds[i];
    const au::Pcm* pcm = recording(st.sound);
    if (!pcm) {
      static_ever_[i] = kStaticDone;  // no pack holds it: never tried again
      continue;
    }
    au::Emit e;
    e.entity = static_entity(i);
    e.at = at_of(st.origin);
    e.volume = st.volume * volume_;
    e.attenuation = st.attenuation / kUnitsPerAttenuation;
    e.loop = au::Loop::file;
    const bool retry = static_ever_[i] != 0;
    static_ever_[i] = pcm->loops() ? kStaticTried : kStaticDone;  // a one-shot plays once
    if (!channels_.play(*pcm, e).valid()) continue;  // no slot: tried again once one frees
    if (retry)
      free--;
    else
      stats_.loops++;
  }
}

bool DemoSounds::sounding() const {
  for (const auto& [name, pcm] : loaded_)
    if (pcm && synth_.reads(*pcm)) return true;
  return false;
}

void RetiredSounds::retire(std::unique_ptr<DemoSounds> s) {
  if (s) {
    s->silence();
    kept_.push_back(std::move(s));
  }
  prune();
}

void RetiredSounds::prune() {
  std::erase_if(kept_, [](const std::unique_ptr<DemoSounds>& s) { return !s->sounding(); });
}

void DemoSounds::frame(const DemoState& s, int listener, float dt) {
  frame(s, s.started, s.stopped, listener, dt);
}

void DemoSounds::frame(const DemoState& s, std::span<const DemoSound> started,
                       std::span<const std::pair<int, int>> stopped, int listener, float dt) {
  // ce_sounds turned since the list was read: each sound read again from the copy it now names
  // (already playing ones finish as they are).
  if ((enhanced_ != nullptr && *enhanced_) != listed_enhanced_) relist(false);
  static_loops(s);
  for (const auto& [entity, channel] : stopped) channels_.stop(entity, channel);
  for (const DemoSound& d : started) {
    const au::Pcm* pcm = d.name != nullptr ? by_name(s, d) : recording(d.sound);
    if (!pcm && d.name == nullptr) pcm = own(s, d);
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
  entity_loops(s);
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
