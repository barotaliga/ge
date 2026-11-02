// What Shooter's Edge's movement labs (`se-moves`, `se-hop`) share: a map found the way a lab is
// told to (a `.bsp` path, a game folder and a map name, or the brush plugin's fixture halls), read
// and made the move's world, and a recorded game read into every player's track (one point a
// frame: the clock, the origin, the view, dead or not) with his name and frags. Header-only,
// beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The movement labs' loaders: a map as the move's world, a demo as players' tracks
// ge:keywords lab map-load bsp basedir fixture demo tracks players names frags move-world
// ge:skill shooter labs
// ge:test tests/shooter_hops_test.cpp
#pragma once

#include <array>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "shooter/bsp_world.hpp"
#include "shooter/hops.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"

namespace shooters_edge::labs {

namespace sh = ge::shooter;
namespace cl = shooters_edge::client;

inline std::vector<uint8_t> read_whole(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return {std::istreambuf_iterator<char>(in), {}};
}

struct StudyMap {
  std::string name, from;  // the map's name; where it was read ("fixture" when written in code)
  ge::brush::Map map;
  sh::BspWorld bsp;
  std::unique_ptr<sh::MoveWorld> world = std::make_unique<sh::MoveWorld>();
};

// The map: `path` (a .bsp) when given, else `name` under the game folder `basedir`/`game` when
// `basedir` is given, else the fixture halls. False with the reason and the next step.
inline bool load_study_map(const std::string& path, const std::string& basedir,
                           const std::string& game, const std::string& name, StudyMap& out,
                           std::string& error) {
  std::vector<uint8_t> bytes;
  if (!path.empty()) {
    bytes = read_whole(path);
    if (bytes.empty()) {
      error = "the map " + path + " does not open. Next: pass --map=<a .bsp that exists>";
      return false;
    }
    out.from = path;
    const size_t slash = path.find_last_of("/\\");
    out.name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (out.name.size() > 4 && out.name.substr(out.name.size() - 4) == ".bsp")
      out.name.resize(out.name.size() - 4);
  } else if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, game, files, error)) return false;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      error = "no maps/" + name + ".bsp in " + basedir + "/" + game +
              ". Next: name a map the game folder holds (--mapname)";
      return false;
    }
    bytes.assign(f.begin(), f.end());
    out.name = name;
    out.from = basedir + "/" + game;
  } else {
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes, &error))
      return false;
    out.name = "halls";
    out.from = "fixture";
  }
  if (!ge::brush::read_bsp(bytes, &out.map, &error)) return false;
  if (!sh::bsp_world(out.map, &out.bsp, &error)) return false;
  sh::map_world(out.bsp.models[0], *out.world);
  return true;
}

struct DemoTracks {
  std::string map;
  double seconds = 0;
  int64_t frames = 0;
  std::array<std::vector<sh::TrackPoint>, 32> tracks;
  std::array<std::string, 32> names;
  std::array<int32_t, 32> frags{};
};

// Every player's track through the demo `bytes` (any format; gzip unpacked). False with the
// reason when it does not open or a frame does not read.
inline bool read_demo_tracks(std::vector<uint8_t> bytes, DemoTracks& out, std::string& error) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) return false;
  cl::DemoRead r;
  while ((r = cl::next_frame(d, error)) == cl::DemoRead::frame) {
    const cl::DemoState& s = d.state;
    if (s.map.empty()) continue;
    out.frames++;
    for (size_t k = 0; k < 32 && k < s.players.size(); k++) {
      const cl::DemoEntity& p = s.players[k];
      if (!p.present) continue;
      out.tracks[k].push_back({s.time, p.origin, p.angles.y, p.angles.x, p.dead});
    }
  }
  if (r == cl::DemoRead::error) return false;
  out.map = d.state.map;
  out.seconds = d.state.time;
  out.names = d.state.names;
  out.frags = d.state.frags;
  return true;
}

}  // namespace shooters_edge::labs
