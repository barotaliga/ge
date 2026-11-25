// LAB `se-q2-map-sweep`: EVERY MAP OF A QUAKE II FOLDER LOADED AND RUN. Each map of the folder
// (`--mod`: a mission pack's maps, those its packs add; none: the base game's) is started on the
// window's listen server the way the game starts it (deathmatch where the map has no single
// player exit, else a new game), with the folder's rules (a mission pack's own), and run for
// `--seconds`. A row a map: its entities and their kinds, the frames run, the server console's
// lines (spawn faults: an item stuck in a wall, a door with no distance), the models and sounds the
// rules listed that no file of the search path holds, and whether it started at all. The notes
// name each distinct console line and missing file with the maps it came from, so one fault on
// many maps is one line to fix.
//
// Without `--game` the map is our own hall (a start, a key, a speaker whose sound is missing and an
// item in a wall), so CI needs no game data and the table is a digest. With `--game=<folder>` (the
// folder holding Quake II's base folder and the pack's) the maps are the packs', for PRIVATE runs
// only.
//
// ge:summary Lab: every map of a Quake II folder started and run: faults, missing models and sounds
// ge:keywords lab se-q2-map-sweep q2 map sweep mission-pack ground-zero reckoning spawn-faults
// ge:keywords missing-models missing-sounds entities classnames load every-map
// ge:skill shooter labs
// ge:test tests/shooters_edge_listen_test.cpp
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "lab/lab.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

const ge::progs::Progs& progs() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs q;
    std::string why;
    cl::baseline_progs(q, why);
    return q;
  }();
  return p;
}

// A floored hall walled round: a start, a key, a speaker whose sound no file holds, and a shard
// of armour half in the floor (a spawn fault the server names).
bool hall(std::vector<uint8_t>& out, std::string& error) {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 256);
  box(512, -512, -16, 528, 512, 256);
  box(-528, 512, -16, 528, 528, 256);
  box(-528, -528, -16, 528, -512, 256);
  const auto put = [&](std::vector<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    e.pairs = std::move(pairs);
    world.entities.push_back(e);
  };
  put({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "90"}});
  put({{"classname", "key_blue_key"}, {"origin", "-256 -256 24"}});
  put({{"classname", "target_speaker"},
       {"origin", "128 0 64"},
       {"noise", "world/unheard.wav"},
       {"spawnflags", "1"}});
  put({{"classname", "item_armor_shard"}, {"origin", "256 256 -8"}});
  return br::write_brush_world(world, &out, &error);
}

struct Row {
  std::string map;
  bool started = false;
  size_t entities = 0, kinds = 0;
  int64_t frames = 0;
  std::vector<std::string> console;  // the server console's lines while it loaded and ran
  std::vector<std::string> missing;  // models and sounds listed that no file holds
  std::string mode;                  // "deathmatch" or "single"
  std::string problem;
};

// A deathmatch map: deathmatch starts and no exit to another level (a unit's maps have both).
bool deathmatch_only(const br::Map& m) {
  bool exit = false, dm = false;
  for (const br::Entity& e : m.entities) {
    const std::string c = e.get("classname");
    exit = exit || c == "target_changelevel";
    dm = dm || c == "info_player_deathmatch";
  }
  return dm && !exit;
}

Row sweep(const cl::SearchPath& files, const std::string& name, int frames) {
  Row r;
  r.map = name;
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, name, error);
  br::Map bsp;
  if (!map || !br::read_bsp(map->file, &bsp, &error)) {
    r.problem = error;
    return r;
  }
  std::set<std::string> kinds;
  for (const br::Entity& e : bsp.entities) kinds.insert(e.get("classname"));
  r.entities = bsp.entities.size();
  r.kinds = kinds.size();
  const bool dm = deathmatch_only(bsp);
  r.mode = dm ? "deathmatch" : "single";
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {dm ? "deathmatch 1" : "deathmatch 0", "coop 0", "skill 1"};
  opt.files = &files;  // the game folder: a mission pack's plays its rules
  cl::Listen l;
  if (!cl::listen_start(l, progs(), std::move(map), opt, error)) {
    r.problem = error;
    return r;
  }
  r.started = true;
  for (int i = 0; i < frames && l.server != nullptr && l.pending_map.empty(); i++)
    cl::listen_frame(l, {});
  if (l.server == nullptr) {
    r.problem = "the server stopped";
    return r;
  }
  r.frames = l.frames;
  if (!l.pending_map.empty()) r.problem = "left for " + l.pending_map + " at once";
  r.console = l.server->console;
  std::vector<uint8_t> store;
  const auto lacks = [&](const std::string& path) {
    return cl::find_file(files, path, store).empty();
  };
  for (const std::string& m : l.server->models) {
    if (m.empty() || m[0] == '*' || m[0] == '#') continue;  // brush models; a weapon's own
    if (lacks(m)) r.missing.push_back(m);
  }
  for (const std::string& s : l.server->sounds) {
    if (s.empty() || s[0] == '*') continue;  // a player's own, by the model he wears
    if (lacks("sound/" + s)) r.missing.push_back("sound/" + s);
  }
  cl::listen_stop(l);
  return r;
}

void run(gl::Lab& lab) {
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  std::string error;
  cl::SearchPath files;
  std::vector<std::string> maps;
  const bool own = lab.arg("game").empty();
  const std::string mod = lab.arg("mod");
  if (own) {
    std::vector<uint8_t> a, pak;
    if (!hall(a, error) || !ge::vfs::write_pak({{"maps/hall.bsp", a}}, &pak, &error) ||
        !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The fixture hall should write and mount. Skill: brush.");
      return;
    }
    maps = {"hall"};
  } else {
    const std::string base(content.base_dir);
    if (!cl::open_game_dir(lab.arg("game"), mod.empty() ? base : mod, files, error, content)) {
      lab.fail(error, "Pass --game=<the folder holding " + base +
                          "/pak0.pak and the pack's folder>. Skill: shooter.");
      return;
    }
    maps = cl::map_names(files, content);
    if (!mod.empty()) {  // the pack's own maps: those the base folder lacks
      cl::SearchPath base_files;
      if (cl::open_game_dir(lab.arg("game"), base, base_files, error, content)) {
        const std::vector<std::string> theirs = cl::map_names(base_files, content);
        std::erase_if(maps, [&](const std::string& m) {
          return std::find(theirs.begin(), theirs.end(), m) != theirs.end();
        });
      }
    }
    if (const std::string only = lab.arg("maps"); !only.empty()) {
      std::vector<std::string> keep;
      size_t at = 0;
      while (at <= only.size()) {
        const size_t comma = std::min(only.find(',', at), only.size());
        keep.push_back(only.substr(at, comma - at));
        at = comma + 1;
      }
      maps = keep;
    }
  }
  const int frames = static_cast<int>(lab.arg_num("seconds") * 40);  // the listen's 40 Hz frame
  gl::LabTable t;
  t.title = "Every map of " +
            (own           ? std::string("the fixture")
             : mod.empty() ? "the base game"
                           : mod) +
            " started and run";
  t.columns = {"map",    "mode",          "entities",      "kinds",
               "frames", "console lines", "missing files", "problem"};
  std::map<std::string, std::vector<std::string>> lines, lacking;  // a line, a file: its maps
  int failed = 0, faults = 0, missing = 0;
  for (const std::string& name : maps) {
    const Row r = sweep(files, name, frames);
    failed += r.started && r.problem.empty() ? 0 : 1;  // a map left at once counts too
    faults += static_cast<int>(r.console.size());
    missing += static_cast<int>(r.missing.size());
    for (const std::string& c : r.console) {
      std::vector<std::string>& on = lines[c];
      if (on.empty() || on.back() != name) on.push_back(name);
    }
    for (const std::string& f : r.missing) lacking[f].push_back(name);
    t.rows.push_back({name, r.mode, std::to_string(r.entities), std::to_string(r.kinds),
                      std::to_string(r.frames), std::to_string(r.console.size()),
                      std::to_string(r.missing.size()), r.problem.empty() ? "-" : r.problem});
  }
  const gl::Det det = own ? gl::Det::deterministic : gl::Det::measured;
  lab.table(t, det);
  lab.metric("maps", static_cast<double>(maps.size()), "count", det);
  lab.metric("maps_failed", failed, "count", det);
  lab.metric("console_lines", faults, "count", det);
  lab.metric("missing_files", missing, "count", det);
  const auto joined = [](const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size() && i < 8; i++) s += (i == 0 ? "" : ", ") + v[i];
    if (v.size() > 8) s += ", +" + std::to_string(v.size() - 8);
    return s;
  };
  for (const auto& [line, on] : lines) lab.note("console: " + line + " (" + joined(on) + ")");
  for (const auto& [file, on] : lacking) lab.note("missing: " + file + " (" + joined(on) + ")");
  if (failed != 0)
    lab.fail(std::to_string(failed) + " map(s) did not start or stopped",
             "Read the problem column; run that map alone with --maps=<name>. Skill: shooter.");
}

const gl::LabSpec kSpec{
    "se-q2-map-sweep",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "Every map of a Quake II folder started on the listen server and run: spawn faults, missing "
    "models and sounds",
    "shooter",
    "Does every map of the folder (a mission pack's own) start and run on its rules, and which "
    "spawn faults, models and sounds does each lack?",
    {{"game", "",
      "the folder holding Quake II's base folder and the pack's (none: a fixture hall)"},
     {"mod", "", "the pack's folder (rogue, xatrix; none: the base game)"},
     {"maps", "", "only these maps, by commas (none: every one the folder adds)"},
     {"seconds", "10", "how long each map runs"}},
    "--seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
