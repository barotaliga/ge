// THE LIVE MAP IN THE WINDOW (live_map.hpp): what the window keeps to show it while a match is
// watched, on TV (`se -tv`) or in a recording played, and never on a server joined live.
//
// - THE LEVEL: each new level its plate is made on
//   another thread from the map file the window loaded; the map shows once it is done.
// - THE KEYS, before the play bar reads them: M flies between the whole map and the followed
//   player's eye (engine/overview/fly.hpp: the map fades into the level drawn from above, which
//   dives onto him and tilts up into his eye, the demo held while it dives and then caught up;
//   and back out the same way); `demo_map 2` (the corner) only by its setting. Space follows the
//   next player while the map shows (on TV always, a broadcast has nothing to pause), and is then
//   taken from the play bar (K still pauses).
// - THE PICTURE: `demo_map 1` the whole screen over the HUD's band (the HUD drawn after it, so it
//   stays under the map, the followed player's own), `demo_map 2` the top right corner of the
//   view, `demo_map_scale` of the screen's height.
// The view, the sound and the HUD stay the followed player's: they ride `Demo::track`, which the
// map's Space moves.
//
//   Watch w; add_live_map_cvars(cvars, w.settings);
//   every frame: int track = watch_keys(w, pressed, ds, tv, allowed); (track >= 0: ride him)
//                watch_fly_step(w, wall_dt); the demo played at watch_demo_rate(w) times
//                from_above(world, watch_fly_view(w, view, width, height, hud_px));
//                watch_frame(w, files, bsp, ds, overlay, ...)
//
// ge:summary The live map in the window: the plate made off the main thread, the keys, the layout
// ge:keywords live-map watch demo_map keys follow-next space-next-player corner whole-screen
// ge:keywords plate-thread tv-only
// ge:skill replay ui
// ge:test tests/shooters_edge_live_map_test.cpp
#pragma once

#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "overview/fly.hpp"
#include "render/view.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/livemap/live_map.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::livemap {

inline constexpr int kPlatePx = 1600;  // the plate's long side, px
inline constexpr int kKeyM = 16, kKeySpaceBar = 44;

struct Watch {
  LiveMapSettings settings;
  LiveMap map;
  std::string level;  // the map (and level count) the plate is for: "<name>#<levels>"
  std::future<std::shared_ptr<const Plate>> building;
  std::string building_for;
  std::string error;             // the last plate that did not build
  std::vector<uint8_t> palette;  // the game's, read with the level
  ge::overview::Fly fly;         // between the map (0) and the eye (1)
  ge::overview::FlyTimes fly_times;
};

// The keys (scancodes that went down this frame) when `allowed` (no menu or console up, not a
// server joined): M cycles the map, Space the player followed (taken from `pressed` when used).
// The slot to ride now, or -1 for no change.
int watch_keys(Watch& w, std::span<bool> pressed, const client::DemoState& ds, bool tv,
               bool allowed);

// The fly's clock on by `wall_dt` s (the window's, never the demo's); a dive that lands leaves the
// map off, a climb that ends leaves it whole. The demo's speed after it (watch_demo_rate).
double watch_fly_step(Watch& w, double wall_dt);
// The demo's speed the fly asks for: 0 while it dives, then over 1 catching up, else 1.
double watch_demo_rate(const Watch& w);
// The view this frame (`view` the followed player's eye, set in place): during a fly the eye
// between the map seen from above and his. True while the level is seen from above (the world
// drawn with its ceilings left out: client/world_draw.hpp's `from_above`).
bool watch_fly_view(Watch& w, ge::render::FrameView& view, int width, int height, float hud_px);

// The plate of `ds`'s level, started on another thread when the map shows and the level has none
// (`bsp` the level's map file, `files` where its models are read from and outliving the watch).
void watch_level(Watch& w, const client::SearchPath& files, std::span<const uint8_t> bsp,
                 const client::DemoState& ds);

// The map into `overlay` (`w` x `h` px; the HUD's band `hud_px` tall at the bottom) for `ds` at
// the demo's clock `now_s`. False when nothing was drawn (off, no plate yet).
bool draw_watch(Watch& w, const client::DemoState& ds, ge::ui::DrawList& overlay, int width,
                int height, float hud_px, double now_s);
// `watch_level`, then `draw_watch` at the demo's clock.
bool watch_frame(Watch& w, const client::SearchPath& files, std::span<const uint8_t> bsp,
                 const client::DemoState& ds, ge::ui::DrawList& overlay, int width, int height,
                 float hud_px);

}  // namespace shooters_edge::livemap
