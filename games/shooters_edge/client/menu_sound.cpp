// The main menu's clicks played from the game's recordings (menu_sound.hpp).
//
// ge:summary The main menu's clicks: the cue's recording read once and played centred
// ge:keywords menu-sound menu-cue click menu1 menu2 menu3
// ge:skill audio ui shooter
// ge:test tests/shooters_edge_menu_sound_test.cpp
#include "shooters_edge/client/menu_sound.hpp"

#include <algorithm>
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
  prune();
  const std::string_view name = menu_sound_name(cue);
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
  if (last_.valid()) synth_.sample_stop(last_, 0.01f);  // the last click cut by this one
  last_ = synth_.sample(*it->second, {.gain = volume}, bus_);
  if (!last_.valid()) return false;
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
