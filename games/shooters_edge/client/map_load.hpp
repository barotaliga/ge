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
// ge:keywords cancel_load take_load freeze hitch title warm-up no-visibility no_visibility
// ge:skill render shooter
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "console/view.hpp"
#include "gpu/device.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"
#include "viewmodel/held.hpp"

namespace shooters_edge::client {

struct MapRead {
  std::unique_ptr<LoadedMap> map;
  std::string error;
  std::string said;  // classic remix's line for another game's map ("" none)
};
struct WorldReadDone {
  WorldReadPtr read;
  std::string error;
};

struct MapLoad {
  std::string what;    // the map loading, for the spinner's caption ("" none)
  double since_s = 0;  // the window's clock when it began
  std::chrono::steady_clock::time_point began;  // the same, for the line a world's arrival says
  std::future<MapRead> map;
  std::future<WorldReadDone> world;
  WorldDrawPtr compiling;  // its GPU half, its pipelines compiling
  WorldDrawPtr warming;    // made, warmed up offscreen a few frames before it is shown (warm_world)
  int warmed = 0;          // the warm-up frames it has had
  std::vector<std::future<WorldReadDone>> dropped;  // reads let run out
  // Classic remix's other game (client/remix.hpp): a map its packs hold is read from them and its
  // world drawn with its own files; null or off: the played game's alone. The window's, outliving
  // the load.
  const RemixFiles* remix = nullptr;
  bool plaque_gone = false;  // the loading plaque gave up on this load (kPlaqueMostS)
};

// The most seconds the loading plaque stays over a load: then "Loading plaque timed out." and the
// spinner in its place, the load going on.
inline constexpr double kPlaqueMostS = 120;

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

// A CLASSIC GAME LOADS A MAP IN ITS CONSOLE, as the classic games did: the console held down over
// the screen and its last line "Loading base1... 3.6s", rewritten as the time goes (the spinner
// and the title's night are Shooter's Edge's own).
struct LoadLine {
  std::string what;   // the map the line is for
  uint64_t n = 0;     // the line's number in the scrollback
  bool held = false;  // the console is held down for it
};
// While `load` is busy at `now_s`: the line printed or rewritten on `con`/`view` and the console
// held. False when nothing loads.
bool console_load_line(const MapLoad& load, LoadLine& line, ge::console::Console& con,
                       ge::console::ConsoleView& view, double now_s);
// Once `load` is done: the console it held let go, unless `keep` (a game's start still holds it).
void release_load_line(const MapLoad& load, LoadLine& line, ge::console::ConsoleView& view,
                       bool keep);
// What a load in flight shows over the frame: a single-player game in its pack's pictures the
// loading plaque (gfx/loading) for up to kPlaqueMostS; anything else the spinner and its caption.
void draw_loading(ge::ui::DrawList& dl, const MapLoad& load, const Listen& l,
                  const shooters_edge::menu::ClassicArt& art, int w, int h, double now_s);

// Step 1: `name` read from `files` on a thread.
void load_map(MapLoad& load, const SearchPath& files, const std::string& name, double now_s);
// Step 2: `map`'s world (in `picture`'s look, with the server's `models` standing in it) read on
// a thread; a world already loading is dropped for it. `vweps`: the server's visible weapons'
// list (world_models.hpp's ModelLoad).
void load_world(MapLoad& load, const SearchPath& files, const LoadedMap& map,
                const look::Picture& picture, std::span<const std::string> models,
                const GameAdapter& game, double now_s, std::span<const std::string> vweps = {});

// What a Quake II map read with no visibility says (the map still plays, everything drawn).
inline constexpr std::string_view kNoVisibility =
    "Map has no Visibility, expect bugs and bad performance!";

// What a frame's step gave: a map read (start it on the server, then `load_world`), a world made
// (draw it from now on), or a step that failed (which, and its reason).
enum class LoadFailed : uint8_t { none, map, world };
struct LoadStep {
  std::unique_ptr<LoadedMap> map;
  WorldDrawPtr world;
  LoadFailed failed = LoadFailed::none;
  std::string error;
  std::string said;  // classic remix's line for another game's map ("" none)
};
// One frame's look at the load: what has finished, and the next step begun. With `wait` each step
// in flight is waited for (a measured run, which must not draw a frame before its map).
LoadStep step_load(MapLoad& load, ge::gpu::Device& device, uint32_t width, uint32_t height,
                   wgpu::TextureFormat format, const look::Picture& picture,
                   std::span<const ge::viewmodel::HeldModel> weapons, bool wait);

// THE WINDOW'S TURN: one frame's step taken (every step with `measured`): a map read started on
// `listen` (`game_folder.hpp`'s `start_map`; the arena when a game's first map does not read)
// and its world asked for; a rebuild that failed said on `con`, the old world kept; a world made
// warmed up from `at.view` a frame at a time (a measured run: not at all) and then handed over to
// `world` (`current`: it is the server's map's; `fresh`: a new map's, so what was
// seen of the last is let go). 0, or the window's exit code (1 the world, 2 the server) with the
// error line said.
struct LoadWindow {
  ge::gpu::Device* device = nullptr;
  uint32_t width = 0, height = 0;
  wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
  const look::Picture* picture = nullptr;
  std::span<const ge::viewmodel::HeldModel> weapons;
  const SearchPath* files = nullptr;
  const GameAdapter* adapter = nullptr;
  const GameContent* content = nullptr;
  // Where the player will first see the new world from (a warm-up frame's view).
  std::function<ge::render::FrameView()> view;
};
// A warm-up frame's new shadow cubes (a frame shown draws 2), and the most frames a world is
// warmed before it is shown whatever is left.
inline constexpr int kWarmCubes = 2, kMostWarmFrames = 40;
int take_load(MapLoad& load, const LoadWindow& at, Listen& listen, ge::console::Console& con,
              bool measured, double now_s, WorldDrawPtr& world, bool& current, bool& fresh);

// Every step stopped, its threads waited out (before the search path they read is let go).
void cancel_load(MapLoad& load);

}  // namespace shooters_edge::client
