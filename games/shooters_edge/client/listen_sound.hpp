// THE WINDOW'S OWN GAME HEARD: the sounds the listen server (listen.hpp) starts for the player, a
// match's or a campaign's (a shot, a door grinding open, a lift, a pickup, a death), and the
// map's looping sounds, played through the same players a demo's go through (demo_sound.hpp:
// the pack's recordings, the channels, the fall with distance, the pan, the map's ambience by
// leaf), for a listener at the player's eye. The listen keeps what the server sent the player
// (`Listen::sounds`, `static_sounds`) and each `frame` plays and empties it; a sound on an entity
// follows the entity where the server has it now; the player's own sounds are centred.
//
// A NEW MAP (a map change, the campaign's next level, a new game): the sound list read again
// and the old map's loops stopped, found by the map the listen plays.
//
// ge:summary The window's own game heard: the listen server's sounds and loops on the synth
// ge:keywords listen-sound listen-hearing sound sounds door lift shot ambience listener
// ge:keywords listen_sound static-sound ambientsound channels
// ge:skill audio shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <memory>

#include "audio/synth.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_sound.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

class ListenHearing {
 public:
  explicit ListenHearing(ge::audio::Synth& synth, float volume = kDemoVolume);

  // One drawn frame: what the server started for the player since the last played (and emptied
  // from `l`), the loops, the listener at his eye; `files` where the recordings are read; `dt`
  // moves the ambience, s. Nothing plays when no game runs.
  void frame(Listen& l, const SearchPath& files, float dt);
  // Every sound stopped (the game ended, or another took the window).
  void stop();

  // The players' counts (null before the first game).
  const DemoSoundStats* stats() const { return sounds_ ? &sounds_->stats() : nullptr; }
  // The state the players read, for a test: the sound list, the loops, the entities, the eye.
  const DemoState& state() const { return state_; }

 private:
  ge::audio::Synth& synth_;
  float volume_;
  std::unique_ptr<DemoSounds> sounds_;
  const SearchPath* files_ = nullptr;
  int entries_ = -1;  // the listen's map entry the loops are of
  size_t names_ = 0;  // the sound list's length when last read
  DemoState state_;
};

}  // namespace shooters_edge::client
