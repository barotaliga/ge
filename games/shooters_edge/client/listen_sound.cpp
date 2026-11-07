// The window's own game heard (listen_sound.hpp).
//
// ge:summary ListenHearing's run: the listen's sounds as a demo frame's, played
// ge:keywords listen-sound listen-hearing sound static-sound
// ge:skill audio shooter
#include "shooters_edge/client/listen_sound.hpp"

namespace shooters_edge::client {

ListenHearing::ListenHearing(ge::audio::Synth& synth, float volume)
    : synth_(synth), volume_(volume) {}

void ListenHearing::frame(Listen& l, const SearchPath& files, float dt) {
  if (l.server == nullptr || !l.map) {  // no game: what played of the last one stopped
    l.sounds.clear();
    stop();
    return;
  }
  const sv::RulesCommon& s = *l.server;
  DemoState& st = state_;
  st.format = DemoFormat::single;  // the entities by number, the player among them
  // The server's own entities where they are now: what a sound rides.
  st.entities.resize(s.sv.edicts.size());
  for (size_t i = 0; i < s.sv.edicts.size(); i++) {
    const sh::Edict& e = s.sv.edicts[i];
    st.entities[i].present = !e.free;
    st.entities[i].origin = e.origin;
  }
  const PlayerState me = player_state(l);
  st.eye = me.origin + me.view_ofs;
  st.angles = l.view;
  if (!sounds_ || files_ != &files) {
    if (sounds_) sounds_->silence();
    sounds_ = std::make_unique<DemoSounds>(synth_, files, volume_);
    files_ = &files;
    entries_ = -1;
  }
  if (l.entries != entries_ || s.sounds.size() != names_) {  // a new map, or more precached
    st.sounds = s.sounds;
    names_ = s.sounds.size();
    if (l.entries != entries_) st.static_sounds.clear();
    entries_ = l.entries;
    sounds_->level(st, l.map->from_file ? &l.map->bsp.map : nullptr);
  }
  if (st.static_sounds.size() != l.static_sounds.size()) st.static_sounds = l.static_sounds;
  sounds_->frame(st, l.sounds, {}, me.entity, dt);
  l.sounds.clear();
}

void ListenHearing::stop() {
  if (sounds_) sounds_->silence();
  sounds_.reset();
  files_ = nullptr;
  entries_ = -1;
  names_ = 0;
  state_ = DemoState{};
}

}  // namespace shooters_edge::client
