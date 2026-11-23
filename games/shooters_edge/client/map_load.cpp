// A map loaded behind the window (map_load.hpp).
//
// ge:summary The map loader's threads and steps: the file, the world's CPU half, the compile
// ge:keywords map-load load_map load_world step_load cancel_load future thread
// ge:skill render shooter
#include "shooters_edge/client/map_load.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include "core/num_text.hpp"
#include "core/output.hpp"
#include "hud/spinner.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/level_screens.hpp"

namespace shooters_edge::client {

namespace {

// A thread's step that threw (out of memory, a file system's error): the step fails with it.
std::string failed_on(const std::string& what, const std::exception& e) {
  return what + " did not read: " + e.what();
}

// The threads' steps: a throw is the step's failure, with its reason.
MapRead read_map(const SearchPath* files, const RemixFiles* remix, const std::string& name) {
  MapRead r;
  try {
    r.map = load_remixed_map(*files, name, remix, r.error, &r.said);
    // A Quake II map with no visibility loads, everything seen from everywhere: the player told.
    if (r.map && r.map->bsp.map.family == ge::brush::Family::v38 && r.map->bsp.map.pvs.empty())
      r.said += std::string(r.said.empty() ? "" : "\n") + std::string(kNoVisibility);
  } catch (const std::exception& e) {
    r.map.reset();
    r.error = failed_on("the map " + name, e);
  }
  return r;
}

WorldReadDone read_world_of(const SearchPath* files, const GameAdapter* game,
                            const std::shared_ptr<const LoadedMap>& map,
                            const std::shared_ptr<const std::vector<std::string>>& names,
                            const std::shared_ptr<const std::vector<std::string>>& vweps,
                            const look::Picture& picture) {
  WorldReadDone r;
  try {
    r.read = read_world(*files, *map, picture, {*names, {}, *vweps}, r.error, *game);
  } catch (const std::exception& e) {
    r.read.reset();
    r.error = failed_on("the world of " + map->name, e);
  }
  return r;
}

template <class T>
bool done(const std::future<T>& f) {
  return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

}  // namespace

bool busy(const MapLoad& l) {
  return l.map.valid() || l.world.valid() || l.compiling != nullptr || l.warming != nullptr;
}

std::string load_caption(const MapLoad& l) {
  if (!busy(l)) return "";
  std::string s = "LOADING";
  if (!l.what.empty()) s += " ";
  for (const char c : l.what) s += (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
  return s;
}

bool console_load_line(const MapLoad& load, LoadLine& line, ge::console::Console& con,
                       ge::console::ConsoleView& view, double now_s) {
  if (!busy(load)) return false;
  hold_load_line(load.what, load.since_s, line, con, view, now_s);
  return true;
}

void release_load_line(const MapLoad& load, LoadLine& line, ge::console::ConsoleView& view,
                       bool keep, bool elsewhere) {
  release_load_line(busy(load), elsewhere, line, view, keep);
}

void load_map(MapLoad& l, const SearchPath& files, const std::string& name, double now_s) {
  l.what = name;
  l.since_s = now_s;
  l.began = std::chrono::steady_clock::now();
  l.plaque_gone = false;
  l.map = std::async(std::launch::async, read_map, &files, l.remix, name);
}

void load_world(MapLoad& l, const SearchPath& files, const LoadedMap& map,
                const look::Picture& picture, std::span<const std::string> models,
                const GameAdapter& game, double now_s, std::span<const std::string> vweps) {
  if (l.world.valid()) l.dropped.push_back(std::move(l.world));
  l.compiling.reset();
  l.warming.reset();  // a world made again replaces one still warming
  if (l.what.empty() || l.what != map.name) {
    l.what = map.name;
    l.since_s = now_s;
    l.began = std::chrono::steady_clock::now();
    l.plaque_gone = false;
  }
  // The thread's own copies: the map's bytes (the server keeps its own map), the names.
  auto copy = std::make_shared<LoadedMap>();
  copy->name = map.name;
  copy->from_file = map.from_file;
  copy->file = map.file;
  copy->from_game = map.from_game;
  std::shared_ptr<const LoadedMap> at = std::move(copy);
  // Another game's map (classic remix): its world from both games' files, its textures in its own
  // palette (world_draw.cpp); the same files for a game's own map whose rules wear the other
  // game's weapons (their models are the other game's).
  const bool other =
      l.remix != nullptr && l.remix->state.on && (!map.from_game.empty() || l.remix->wearing());
  const SearchPath* from = other ? &l.remix->files : &files;
  auto listed = std::make_shared<const std::vector<std::string>>(models.begin(), models.end());
  auto listed_vweps = std::make_shared<const std::vector<std::string>>(vweps.begin(), vweps.end());
  l.world =
      std::async(std::launch::async, read_world_of, from, &game, at, listed, listed_vweps, picture);
}

LoadStep step_load(MapLoad& l, ge::gpu::Device& device, uint32_t width, uint32_t height,
                   wgpu::TextureFormat format, const look::Picture& picture,
                   std::span<const ge::viewmodel::HeldModel> weapons, bool wait) {
  LoadStep out;
  // The reads let run out, dropped once they have.
  std::erase_if(l.dropped, [](const std::future<WorldReadDone>& f) { return done(f); });
  if (l.map.valid() && (wait || done(l.map))) {
    MapRead r = l.map.get();
    out.map = std::move(r.map);
    out.said = std::move(r.said);
    if (!out.map) {
      out.failed = LoadFailed::map;
      out.error = r.error;
      l.what.clear();
    }
    return out;  // the caller starts it on the server, then asks for its world
  }
  if (l.world.valid() && (wait || done(l.world))) {
    WorldReadDone r = l.world.get();
    if (!r.read) {
      out.failed = LoadFailed::world;
      out.error = r.error;
      l.what.clear();
      return out;
    }
    l.compiling =
        begin_world(device, std::move(r.read), width, height, format, picture, weapons, out.error);
    if (!l.compiling) {
      out.failed = LoadFailed::world;
      l.what.clear();
      return out;
    }
  }
  if (l.compiling && (wait || world_compiled(device, *l.compiling))) {
    if (!finish_world(device, *l.compiling, width, height, out.error)) {
      out.failed = LoadFailed::world;
      l.compiling.reset();
      l.what.clear();
      return out;
    }
    out.world = std::move(l.compiling);  // `what` stays up while it warms
  }
  return out;
}

int take_load(MapLoad& l, const LoadWindow& at, Listen& listen, ge::console::Console& con,
              bool measured, double now_s, WorldDrawPtr& world, bool& current, bool& fresh) {
  namespace out = ge::core::output;
  fresh = false;
  if (busy(l) && listen.campaign_host && !l.plaque_gone && now_s - l.since_s >= kPlaqueMostS) {
    l.plaque_gone = true;
    con.print("Loading plaque timed out.");
  }
  while (busy(l)) {
    LoadStep step =
        step_load(l, *at.device, at.width, at.height, at.format, *at.picture, at.weapons, measured);
    std::string error;
    if (step.failed == LoadFailed::world && current) {
      con.print("map: " + step.error);  // a rebuild that failed: the old world stays
    } else if (step.failed == LoadFailed::world) {
      out::error("shooters_edge: the new world draw did not load: " + step.error +
                 "; see docs/rendering.md");
      return 1;
    } else if (step.failed == LoadFailed::map) {
      con.print("map: " + step.error);
      listen.pending_map.clear();
      if (!listen.map)  // a game just loaded and its first map did not: the arena
        step.map = load_named_map(*at.files, "arena", error);
    }
    if (step.map) {
      for (size_t at_line = 0; at_line < step.said.size();) {  // a line each
        const size_t end = std::min(step.said.find('\n', at_line), step.said.size());
        con.print(step.said.substr(at_line, end - at_line));
        at_line = end + 1;
      }
      if (!start_map(listen, *at.content, std::move(step.map), error)) {
        out::error("shooters_edge: the map change failed: " + error +
                   "; see docs/shooters-edge.md § Playing it");
        return 2;
      }
      current = false;  // the server plays the new map; its world is made next
      load_world(l, *at.files, *listen.map, *at.picture, listen.server->models, *at.adapter, now_s,
                 listen.server->vwep);
    }
    if (step.world) {
      l.warming = std::move(step.world);
      l.warmed = 0;
      if (!measured) break;  // warmed from the next frame
    }
    // Warmed up offscreen until every cube near the player's eyes is in (a measured run shows it at
    // once: its first frame draws them all, as a map's first frame does).
    if (l.warming && (measured || warm_world(*at.device, *l.warming, at.view(), kWarmCubes) ||
                      ++l.warmed >= kMostWarmFrames)) {
      fresh = fresh || !current;
      const std::chrono::duration<double> took = std::chrono::steady_clock::now() - l.began;
      out::status("shooters_edge: " + listen.map->name + " drawn " +
                  ge::core::num::to_fixed(took.count(), 2) + " s after it was asked for");
      world = std::move(l.warming);
      current = true;
      l.what.clear();
    }
    if (!measured) break;  // one step a frame; a measured run waits them all out
  }
  return 0;
}

void cancel_load(MapLoad& l) {
  if (l.map.valid()) l.map.wait();
  l.map = {};
  if (l.world.valid()) l.world.wait();
  l.world = {};
  for (std::future<WorldReadDone>& f : l.dropped) f.wait();
  l.dropped.clear();
  l.compiling.reset();
  l.warming.reset();
  l.what.clear();
}

void draw_loading(ge::ui::DrawList& dl, const MapLoad& load, const Listen& l,
                  const shooters_edge::menu::ClassicArt& art, int w, int h, double now_s) {
  if (!busy(load)) return;
  if (l.campaign_host && !load.plaque_gone && art.find("loading") != nullptr) {
    shooters_edge::menu::draw_plaque(dl, art, w, h, "loading");
    return;
  }
  const ge::hud::Style style = shooters_edge::hud::hud_style();
  ge::hud::draw_spinner(dl, ge::hud::frame_for(w, h, style), style, now_s, load_caption(load),
                        now_s - load.since_s);
}

}  // namespace shooters_edge::client
