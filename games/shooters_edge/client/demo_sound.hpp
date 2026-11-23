// A RECORDED GAME HEARD: the sounds a demo's frames start and stop (demo.hpp's `DemoState::started`
// and `stopped`), the level's looping sounds (its static sounds), the map's own ambience by leaf
// (sound/ambience.hpp) and the players' voices (demo_voice.hpp), played on the engine's synth for
// a listener at the demo's eye. The demo-to-video export (demo_video.hpp) renders it offline beside
// the pictures; nothing here reads a clock, so the same frames played give the same synth calls.
//
// THE RECORDINGS: each name on the level's sound list is read by the game's adapter
// (game_adapter.hpp's `open_sound`: Quake's WAVs under `sound/`, any rate) the first time
// a level lists it, and kept for the
// player's whole life: the synth reads a recording in place, so none is freed while it may sound.
// With the enhanced sounds on (`enhance_from`: ce_sounds), classic enhance's copy in the packs'
// cache (classicenhance/cached.hpp's `cached_sound`, cleaned and at 48 kHz) is read in its place
// where the cache has one; turning the switch reads the other copy the next time a level lists it.
// A name the packs lack is counted (`missing`) and plays nothing. A name starting `*` is a
// player's own voice, read when it first sounds from that player's model's folder (`own`).
//
// HEARING: a sound's level falls off as the protocol says, `volume * (1 - distance *
// attenuation / 1000)` in map units (audio/channels.hpp's `hear` with attenuation / 1000 a unit);
// its entity's channel replaces what that channel played (channel 0 stacks); the listener's own
// entity (the demo's view entity, or the recording player's) is heard centred; a sound on an
// entity that moves follows it frame by frame. `volume` is the player's master volume over it all.
//
// ge:summary A recorded game's sounds on the synth: the pack's WAVs, channels, loops, ambience
// ge:keywords retired-sounds sounding use-after-free audio-thread
// ge:keywords demo-sound demo sounds sound-list static-sound loop ambience listener channels
// ge:keywords stopsound attenuation playback export soundlist sound-list-lines
// ge:skill audio shooter
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "audio/channels.hpp"
#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "brush/map.hpp"
#include "classic/q2_footsteps.hpp"
#include "classic/q2_fx_switches.hpp"
#include "classic/q2_loop_merge.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/sound/ambience.hpp"

namespace shooters_edge::client {

inline constexpr float kDemoVolume = 0.7f;  // the master volume a demo is heard at by default
inline constexpr int kDemoChannels = 24;    // sounds at once (static loops among them)

struct DemoSoundStats {
  int64_t started = 0;       // sounds the frames started that had a recording
  int64_t missing = 0;       // names on a sound list no pack holds
  int64_t loops = 0;         // static sounds started
  int64_t entity_loops = 0;  // loops an entity carries started (or changed, or come back)
};

class DemoSounds {
 public:
  DemoSounds(ge::audio::Synth& synth, const SearchPath& files, float volume = kDemoVolume,
             int bus = 0, const GameAdapter& game = qw_adapter());

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
  // Whether the synth still reads one of its recordings (a voice sounding or fading out): until
  // it does not, freeing it would free what the render thread reads (`RetiredSounds`).
  bool sounding() const;

  const DemoSoundStats& stats() const { return stats_; }
  // `soundlist`'s lines: each recording read, by name in order, with its length and rate ("not
  // found" for one the packs lack), then the count and the memory they hold.
  std::vector<std::string> list_lines() const;

  // The enhanced sounds' switch (look::Picture's `ce.sounds`, read at each load; null: off) and,
  // for a test, the cache folder to read them from (empty: the packs' own, found once).
  void enhance_from(const bool* on, std::filesystem::path cache = {});
  // Every sound heard as game `game`'s (classic remix's `cr_sounds`: profiles/remix_sounds.hpp's
  // `heard_as`; "" each as it is named), the sound list read again at once.
  void hear_as(std::string_view game);
  // Quake II's sound switches (null: Quake's, all on): `cl_footsteps` 0 starts no footstep
  // (`player/step*`), 1 a step by the surface under it (classic/q2_footsteps.hpp: the level's
  // `.mat` materials and step sets, read with the level), 2 the default set's; `s_ambient` 0 no
  // entity's loop, 2 only one with a model, 3 only the listener's own; `cl_hit_markers` under 2 no
  // `weapons/marker.wav` (the hit's sound).
  void switches_from(const ge::classic::Q2FxSwitches* q2) { q2_ = q2; }

 private:
  const ge::audio::Pcm* recording(int index);
  // A player's own sound (a `*` name) for `d`'s entity: its model's (`players/<model>/`, the
  // model from `DemoState::looks`, "male" when none), else the base voice's
  // (`sound/player/male/`); null when neither is there.
  const ge::audio::Pcm* own(const DemoState& s, const DemoSound& d);
  // A player's own `*` sound `name` for entity `entity`: its model's folder, else the base voice.
  const ge::audio::Pcm* own(const DemoState& s, const std::string& name, int entity);
  // The client's own sound by name (an entity's event: DemoSound::name).
  const ge::audio::Pcm* by_name(const DemoState& s, const DemoSound& d);
  const ge::audio::Pcm* load(const std::string& name);  // as the game names it
  void relist(bool count);  // the list's recordings read again, missing ones counted when `count`
  // The loops the entities carry (DemoEntity::loop): each started, changed or stopped with them.
  void entity_loops(const DemoState& s, int listener);
  void stop_entity_loops();
  // Quake II's loops merged (al_merge_looping: classic/q2_loop_merge.hpp): each sound's copies
  // one voice, on the pseudo-entity kMergedEntity0 - its index, below the static loops'.
  void merged_loops(const DemoState& s, int listener);
  void stop_merged_loops();
  static constexpr int kMergedEntity0 = -1000000;
  std::vector<ge::classic::Q2Ears> merged_sum_;  // by sound index: the ears' summed shares
  std::vector<uint8_t> merged_on_;               // by sound index: its merged voice plays

  ge::audio::Synth& synth_;
  const SearchPath& files_;
  const GameAdapter& game_;
  float volume_;
  int bus_;
  ge::audio::Channels channels_;
  std::vector<DemoSound> later_;  // started with a delay: each counts down to its start
  void start(const DemoState& s, const DemoSound& d);
  std::map<std::string, std::unique_ptr<ge::audio::Pcm>> loaded_;  // by path; null: missing
  std::vector<const ge::audio::Pcm*> list_;                        // by sound index
  std::vector<std::string> names_;                                 // the list as read
  // The level's static loops, by their index in DemoState::static_sounds: each plays as the
  // pseudo-entity static_entity(i), below every real one and the world's -1.
  static constexpr int kStaticEntity0 = -2;
  static int static_entity(size_t i) { return kStaticEntity0 - static_cast<int>(i); }
  void static_loops(const DemoState& s);
  static constexpr uint8_t kStaticTried = 1, kStaticDone = 2;  // a loop tried; never again
  std::vector<uint8_t> static_ever_;   // by index: 0 untried (the first try counts in loops)
  std::vector<uint8_t> static_heard_;  // a slot plays it now (this frame)
  // Quake's: by index, the static that plays it (itself, or the first looped static of its file).
  std::vector<int> static_lead_;
  // Where the lead `lead`'s voice stands so the channels hear its group's merged ears.
  ge::core::Vec3 merged_static_at(const DemoState& s, size_t lead) const;
  std::vector<int> entity_loop_;       // by entity number: the loop it was last heard with (0 none)
  std::vector<uint8_t> loop_heard_;    // by entity number: a slot plays its loop now (this frame)
  std::optional<Ambience> ambience_;
  const ge::brush::Map* map_ = nullptr;
  DemoSoundStats stats_;
  const bool* enhanced_ = nullptr;
  const ge::classic::Q2FxSwitches* q2_ = nullptr;
  // Quake II's footsteps (a version 38 level): its step sets, its tracer, the step heard last and
  // the steps heard, which roll the next variant (the same frames give the same steps).
  std::optional<ge::classic::Q2StepSets> steps_;
  std::unique_ptr<ge::brush::BrushTracer> step_tracer_;
  std::string last_step_;
  uint32_t steps_heard_ = 0;
  const ge::audio::Pcm* step_sound(const DemoState& s, const DemoSound& d);
  bool listed_enhanced_ = false;  // the switch as the list was read
  std::filesystem::path cache_;   // the packs' cache folder ("" none)
  bool cache_known_ = false;      // looked for (or given)
  std::string heard_;             // the game every sound is heard as ("" each its own)
};

// SOUNDS STOPPED BUT NOT YET SILENT: a stopped voice fades for a while on the render thread,
// reading its recording in place, so a `DemoSounds` let go (a demo over, another folder's) is
// kept here until the synth reads none of its recordings, then freed. Freed at once it would be
// read after it was freed (a crash on the audio thread when Escape stopped a demo).
class RetiredSounds {
 public:
  RetiredSounds() = default;
  RetiredSounds(const RetiredSounds&) = delete;
  RetiredSounds& operator=(const RetiredSounds&) = delete;
  // `s` silenced and kept (null: nothing).
  void retire(std::unique_ptr<DemoSounds> s);
  // Those the synth no longer reads freed. Called each frame and at each `retire`.
  void prune();
  size_t kept() const { return kept_.size(); }

 private:
  std::vector<std::unique_ptr<DemoSounds>> kept_;
};

}  // namespace shooters_edge::client
