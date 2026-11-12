// The files a demo draws from, read off its precache lists (demo_content.hpp).
//
// ge:summary A demo read through once for its maps, models and sounds
// ge:keywords demo content files precache
// ge:skill shooter net
// ge:test tests/shooters_edge_demo_content_test.cpp
#include "shooters_edge/client/demo_content.hpp"

#include <set>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace {

// What every picture of this game reads, whatever the demo holds.
constexpr const char* kBasics[] = {
    "gfx/palette.lmp",   "gfx/colormap.lmp", "progs/player.mdl",  "progs/h_player.mdl",
    "progs/gib1.mdl",    "progs/gib2.mdl",   "progs/gib3.mdl",    "progs/missile.mdl",
    "progs/grenade.mdl", "progs/spike.mdl",  "progs/s_spike.mdl", "particles/",
};

std::string lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

}  // namespace

bool demo_files(std::span<const uint8_t> demo, std::vector<std::string>& names,
                std::string& error) {
  names.clear();
  Demo d;
  if (!open_demo({demo.begin(), demo.end()}, d, error)) return false;
  std::set<std::string> out(std::begin(kBasics), std::end(kBasics));
  int levels = -1;
  size_t models = 0, sounds = 0;
  const auto take = [&] {  // at each level's start, and whenever its lists grow
    const DemoState& s = d.state;
    if (s.levels == levels && s.models.size() == models && s.sounds.size() == sounds) return;
    levels = s.levels;
    models = s.models.size();
    sounds = s.sounds.size();
    if (!s.map.empty()) {
      out.insert("maps/" + lower(s.map) + ".bsp");
      out.insert("maps/" + lower(s.map) + ".lit");
    }
    for (const std::string& m : s.models)
      if (!m.empty() && m[0] != '*') out.insert(lower(m));
    for (const std::string& snd : s.sounds)
      if (!snd.empty()) out.insert("sound/" + lower(snd));
  };
  std::string why;
  while (next_frame(d, why) == DemoRead::frame) take();
  take();
  names.assign(out.begin(), out.end());
  return true;
}

}  // namespace shooters_edge::client
