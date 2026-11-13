// The files a demo draws from, read off its precache lists (demo_content.hpp).
//
// ge:summary A demo read through once for its maps, models and sounds
// ge:keywords sequel wal textures content_needs
// ge:keywords demo content files precache
// ge:skill shooter net
// ge:test tests/shooters_edge_demo_content_test.cpp
#include "shooters_edge/client/demo_content.hpp"

#include <set>

#include "brush/read.hpp"
#include "brush/wal.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

namespace {

// What every picture of this game reads, whatever the demo holds.
constexpr const char* kBasics[] = {
    "gfx/palette.lmp",   "gfx/colormap.lmp", "progs/player.mdl",  "progs/h_player.mdl",
    "progs/gib1.mdl",    "progs/gib2.mdl",   "progs/gib3.mdl",    "progs/missile.mdl",
    "progs/grenade.mdl", "progs/spike.mdl",  "progs/s_spike.mdl", "particles/",
};

// What every picture of the sequel reads.
constexpr const char* kSequelBasics[] = {"pics/colormap.pcx", "particles/"};

std::string lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

}  // namespace

void game_basics(bool sequel, std::vector<std::string>& out) {
  if (sequel)
    out.insert(out.end(), std::begin(kSequelBasics), std::end(kSequelBasics));
  else
    out.insert(out.end(), std::begin(kBasics), std::end(kBasics));
}

bool demo_files(std::span<const uint8_t> demo, std::vector<std::string>& names,
                std::string& error) {
  names.clear();
  Demo d;
  if (!open_demo({demo.begin(), demo.end()}, d, error)) return false;
  const bool sequel = d.state.format == DemoFormat::sequel;
  std::set<std::string> out;
  if (sequel)
    out.insert(std::begin(kSequelBasics), std::end(kSequelBasics));
  else
    out.insert(std::begin(kBasics), std::end(kBasics));
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
      if (!sequel) out.insert("maps/" + lower(s.map) + ".lit");
    }
    for (const std::string& m : s.models) {
      if (m.empty() || m[0] == '*' || m[0] == '#') continue;
      const size_t slash = m.rfind('/');
      // A sequel's model sits in a folder of its own with its skins.
      if (sequel && m.rfind("models/", 0) == 0 && slash != std::string::npos)
        out.insert(lower(m.substr(0, slash + 1)));
      else
        out.insert(lower(m));
    }
    for (const std::string& snd : s.sounds)
      if (!snd.empty() && snd[0] != '*') out.insert("sound/" + lower(snd));
    if (!sequel) return;
    for (const std::string& img : s.images)
      if (!img.empty()) out.insert("pics/" + lower(img) + ".pcx");
    for (const std::string& look : s.looks) {
      const size_t slash = look.find('/');
      if (slash != std::string::npos && slash > 0)
        out.insert("players/" + lower(look.substr(0, slash + 1)));
    }
  };
  std::string why;
  while (next_frame(d, why) == DemoRead::frame) take();
  take();
  names.assign(out.begin(), out.end());
  return true;
}

void content_needs(std::string_view name, std::span<const uint8_t> bytes,
                   std::vector<std::string>& more) {
  if (name.size() < 4 || name.substr(name.size() - 4) != ".bsp") return;
  ge::brush::Map map;
  std::string error;
  if (!ge::brush::read_bsp(bytes, &map, &error)) return;
  if (map.family == ge::brush::Family::v38)
    for (const ge::brush::Texture& t : map.textures)
      if (!t.name.empty()) more.push_back(ge::brush::wal_path(t.name));
  // Its own sky box (world_draw.cpp's `sky_box_files`): the six faces under env/, in the two
  // forms a game keeps them (the window takes the first it finds).
  if (map.entities.empty()) return;
  const ge::brush::Entity& world = map.entities[0];
  std::string sky = world.get("sky");
  if (sky.empty()) sky = world.get("_sky");
  if (sky.empty()) sky = world.get("skyname");
  if (sky.empty() || sky.find_first_of("./\\") != std::string::npos) return;
  for (const char* side : {"rt", "bk", "lf", "ft", "up", "dn"})
    for (const char* ext : {".tga", ".pcx"}) more.push_back("env/" + sky + side + ext);
}

}  // namespace shooters_edge::client
