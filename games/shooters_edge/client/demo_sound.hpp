// A RECORDED GAME HEARD: the sounds a demo's frames start and stop (demo.hpp's `DemoState::started`
// and `stopped`), the level's looping sounds (its static sounds), the map's own ambience by leaf
// (sound/ambience.hpp) and the players' voices (demo_voice.hpp), played on the engine's synth for
// a listener at the demo's eye. The demo-to-video export (demo_video.hpp) renders it offline beside
// the pictures; nothing here reads a clock, so the same frames played give the same synth calls.
//
// THE RECORDINGS: each name on the level's sound list is read through the search path as
// `sound/<name>` (the pack's WAVs, any rate) the first time a level lists it, and kept for the
// player's whole life: the synth reads a recording in place, so none is freed while it may sound.
// A name the packs lack is counted (`missing`) and plays nothing.
//
// HEARING: a sound's level falls off as the protocol says, `volume * (1 - distance *
// attenuation / 1000)` in map units (audio/channels.hpp's `hear` with attenuation / 1000 a unit);
// its entity's channel replaces what that channel played (channel 0 stacks); the listener's own
// entity (the demo's view entity, or the recording player's) is heard centred; a sound on an
// entity that moves follows it frame by frame. `volume` is the player's master volume over it all.
//
// ge:summary A recorded game's sounds on the synth: the pack's WAVs, channels, loops, ambience
// ge:keywords demo-sound demo sounds sound-list static-sound loop ambience listener channels
// ge:keywords stopsound attenuation playback export
// ge:skill audio shooter
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "audio/channels.hpp"
#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "brush/map.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/sound/ambience.hpp"

namespace shooters_edge::client {

inline constexpr float kDemoVolume = 0.7f;  // the master volume a demo is heard at by default
inline constexpr int kDemoChannels = 24;    // sounds at once (static loops among them)

struct DemoSoundStats {
  int64_t started = 0;  // sounds the frames started that had a recording
  int64_t missing = 0;  // names on a sound list no pack holds
  int64_t loops = 0;    // static sounds started
};

class DemoSounds {
 public:
  DemoSounds(ge::audio::Synth& synth, const SearchPath& files, float volume = kDemoVolume,
             int bus = 0);

  // A level started (or its static sounds arrived): the sound list read again, the old loops
  // stopped and the level's started, the ambience over `map` (null: none).
  void level(const DemoState& s, const ge::brush::Map* map);
  // The frame just read: its sounds started and stopped, every sound on an entity moved with it,
  // the listener at the eye riding `listener` (its own sounds centred); `dt` moves the ambience.
  void frame(const DemoState& s, int listener, float dt);
  // The same with the sounds started and stopped given apart from the frame: a window that reads
  // several demo frames a draw plays what they all carried (timedemo.hpp's `DemoHeard`).
  void frame(const DemoState& s, std::span<const DemoSound> started,
             std::span<const std::pair<int, int>> stopped, int listener, float dt);
  // Every sound and loop stopped and the ambience silenced (the demo stopped).
  void silence();

  const DemoSoundStats& stats() const { return stats_; }

 private:
  const ge::audio::Pcm* recording(int index);
  const ge::audio::Pcm* load(const std::string& path);

  ge::audio::Synth& synth_;
  const SearchPath& files_;
  float volume_;
  int bus_;
  ge::audio::Channels channels_;
  std::map<std::string, std::unique_ptr<ge::audio::Pcm>> loaded_;  // by path; null: missing
  std::vector<const ge::audio::Pcm*> list_;                        // by sound index
  std::vector<std::string> names_;                                 // the list as read
  size_t statics_started_ = 0;
  std::optional<Ambience> ambience_;
  const ge::brush::Map* map_ = nullptr;
  DemoSoundStats stats_;
};

}  // namespace shooters_edge::client
