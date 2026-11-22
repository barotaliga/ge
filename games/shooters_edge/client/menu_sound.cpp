// The main menu's clicks played from the game's recordings (menu_sound.hpp).
//
// ge:summary The main menu's clicks: the cue's recording read once and played centred
// ge:keywords menu-sound menu-cue click menu1 menu2 menu3
// ge:skill audio ui shooter
// ge:test tests/shooters_edge_menu_sound_test.cpp
#include "shooters_edge/client/menu_sound.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

namespace shooters_edge::client {

namespace au = ge::audio;
using ge::ui::MenuCue;

std::string_view menu_sound_name(MenuCue cue) {
  switch (cue) {
    case MenuCue::move:
      return "misc/menu1.wav";
    case MenuCue::enter:
      return "misc/menu2.wav";
    case MenuCue::change:
    case MenuCue::back:
      return "misc/menu3.wav";
    case MenuCue::none:
      break;
  }
  return {};
}

MenuSounds::MenuSounds(au::Synth& synth, int bus) : synth_(synth), bus_(bus) {}

bool MenuSounds::play(MenuCue cue, const GameAdapter& game, const SearchPath& files, float volume) {
  return play_named(menu_sound_name(cue), game, files, volume, true);
}

bool MenuSounds::play_named(std::string_view name, const GameAdapter& game, const SearchPath& files,
                            float volume, bool cut) {
  prune();
  if (name.empty() || game.open_sound == nullptr) return false;
  auto it = loaded_.find(name);
  if (it == loaded_.end()) {
    auto pcm = std::make_unique<au::Pcm>();
    std::string error;
    if (!game.open_sound(game, files, std::string(name), *pcm, error)) {
      pcm.reset();
      stats_.missing++;
    }
    it = loaded_.emplace(std::string(name), std::move(pcm)).first;
  }
  if (!it->second) return false;
  if (cut && last_.valid()) synth_.sample_stop(last_, 0.01f);  // the last click cut by this one
  const au::SampleId id = synth_.sample(*it->second, {.gain = volume}, bus_);
  if (!id.valid()) return false;
  if (cut) last_ = id;
  stats_.played++;
  return true;
}

void MenuSounds::forget() {
  for (auto& [name, pcm] : loaded_)
    if (pcm) retired_.push_back(std::move(pcm));
  loaded_.clear();
  prune();
}

void MenuSounds::prune() {
  std::erase_if(retired_, [&](const std::unique_ptr<au::Pcm>& p) { return !synth_.reads(*p); });
}

bool MenuSounds::sounding() const {
  for (const auto& [name, pcm] : loaded_)
    if (pcm && synth_.reads(*pcm)) return true;
  return std::any_of(retired_.begin(), retired_.end(),
                     [&](const std::unique_ptr<au::Pcm>& p) { return synth_.reads(*p); });
}

}  // namespace shooters_edge::client

namespace shooters_edge::client {

void add_local_sound_commands(ge::console::Console& con, MenuSounds& sounds,
                              const GameAdapter& game, const SearchPath& files,
                              ge::audio::Synth& synth, int music_bus) {
  // A sound's name as typed: `.wav` added when it has no extension.
  const auto named = [](std::string name) {
    if (name.find('.') == std::string::npos) name += ".wav";
    return name;
  };
  // play and playvol: each sound once, centred, at its volume (play's 1).
  const auto play = [&sounds, &game, &files, named](ge::console::Console& c,
                                                    const ge::console::Args& a, bool with_volume) {
    if (a.size() < 2) {
      c.print("Usage: " + a[0] + (with_volume ? " <sound> <volume> [...]" : " <sound> [...]"));
      return;
    }
    for (size_t i = 1; i < a.size(); i += with_volume ? 2 : 1) {
      const std::string name = named(a[i]);
      double volume = 1;
      if (with_volume && i + 1 < a.size()) volume = std::atof(a[i + 1].c_str());
      if (!sounds.play_named(name, game, files, static_cast<float>(volume), false))
        c.print("Couldn't load sound/" + name);
    }
  };
  con.add_command(
      "play", "play <sound> [...]: each sound heard once, as the menu's are",
      [play](ge::console::Console& c, const ge::console::Args& a) { play(c, a, false); });
  con.add_command(
      "playvol", "playvol <sound> <volume> [...]: each sound heard once at its volume",
      [play](ge::console::Console& c, const ge::console::Args& a) { play(c, a, true); });
  con.add_command("stopsound", "stopsound: every sound playing stopped (the music plays on)",
                  [&synth, music_bus](ge::console::Console&, const ge::console::Args&) {
                    synth.sample_stop_all(0.01f, music_bus);
                  });
}

}  // namespace shooters_edge::client
