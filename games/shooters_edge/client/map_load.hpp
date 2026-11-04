// A MAP LOADED BEHIND THE WINDOW: the window keeps drawing (the title, or the map still playing,
// and a spinner in the corner) while a map is read, its world made and its pipelines compiled.
// Three steps, the first two on a thread of their own:
//
//   1. the map's file read (`load_map`: listen.hpp's `load_named_map`); the caller starts it on
//      the listen server (which knows then which models the world needs) and asks for its world;
//   2. the world's CPU half read (`load_world`: world_draw.hpp's `read_world`, the seconds of a
//      load: the textures and skins decoded, the rebaked light read);
//   3. its GPU half begun on the window's thread and its pipelines compiled while frames go on
//      (`world_compiled`), then finished: `step_load` hands the world over.
//
// A thread reads only what the load holds itself (the map's bytes, the picture's settings, the
// model names, copied when the step starts) and the search path, which the caller keeps until
// `cancel_load` has waited the threads out. A world asked for while one is loading takes its
// place; the old one's thread is let run out and its result dropped.
//
// ge:summary A map loaded behind the window: read and decoded on a thread, compiled while drawing
// ge:keywords map-load background-load async loading spinner load_map load_world step_load
// ge:keywords cancel_load freeze hitch title
// ge:skill render shooter
#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "viewmodel/held.hpp"

namespace shooters_edge::client {

struct MapRead {
  std::unique_ptr<LoadedMap> map;
  std::string error;
};
struct WorldReadDone {
  WorldReadPtr read;
  std::string error;
};

struct MapLoad {
  std::string what;    // the map loading, for the spinner's caption ("" none)
  double since_s = 0;  // the window's clock when it began
  std::future<MapRead> map;
  std::future<WorldReadDone> world;
  WorldDrawPtr compiling;                           // its GPU half, its pipelines compiling
  std::vector<std::future<WorldReadDone>> dropped;  // reads let run out
};

// Whether `f` holds a result now; with `wait`, waited for (false only when it holds none).
template <class T>
bool arrived(std::future<T>& f, bool wait) {
  if (!f.valid()) return false;
  if (wait) f.wait();
  return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

// Whether any step is in flight.
bool busy(const MapLoad& load);
// The spinner's caption: LOADING and the map's name in capitals ("LOADING" before it has one);
// "" when nothing is in flight.
std::string load_caption(const MapLoad& load);

// Step 1: `name` read from `files` on a thread.
void load_map(MapLoad& load, const SearchPath& files, const std::string& name, double now_s);
// Step 2: `map`'s world (in `picture`'s look, with the server's `models` standing in it) read on
// a thread; a world already loading is dropped for it.
void load_world(MapLoad& load, const SearchPath& files, const LoadedMap& map,
                const look::Picture& picture, std::span<const std::string> models,
                const GameAdapter& game, double now_s);

// What a frame's step gave: a map read (start it on the server, then `load_world`), a world made
// (draw it from now on), or a step that failed (which, and its reason).
enum class LoadFailed : uint8_t { none, map, world };
struct LoadStep {
  std::unique_ptr<LoadedMap> map;
  WorldDrawPtr world;
  LoadFailed failed = LoadFailed::none;
  std::string error;
};
// One frame's look at the load: what has finished, and the next step begun. With `wait` each step
// in flight is waited for (a measured run, which must not draw a frame before its map).
LoadStep step_load(MapLoad& load, ge::gpu::Device& device, uint32_t width, uint32_t height,
                   wgpu::TextureFormat format, const look::Picture& picture,
                   std::span<const ge::viewmodel::HeldModel> weapons, bool wait);

// Every step stopped, its threads waited out (before the search path they read is let go).
void cancel_load(MapLoad& load);

}  // namespace shooters_edge::client
