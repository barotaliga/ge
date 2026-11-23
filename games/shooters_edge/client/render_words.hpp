// THREE OF THE RENDERER'S CONSOLE WORDS, for any classic game played in the window:
//
// - `sky <name> [rotate] [axis x y z]`: the sky box of that name (its six pictures under env/ or
//   gfx/env/) drawn through the map's sky, turning `rotate` degrees a second about the axis (the
//   map's own settings' units; 0 0 1 when none is given), until the next map. The world is made
//   again behind the window with it (classic/sky_typed.hpp; world_draw.cpp's sky box).
// - `r_reload`: the level's world made again, its textures and models read again from the files
//   (a texture or a model changed on disk shows without loading the map again).
// - `timerefresh [one]`: the world drawn 128 times from the eye, turning a whole circle, then
//   "<seconds> seconds (<frames a second> fps)"; each frame shown, or with any word all drawn into
//   one frame shown once (world_draw.hpp's `time_refresh`).
//
// Any of them outside a level answers "No map loaded."; `sky` alone answers its usage line.
//
// ge:summary The window's sky and timerefresh words: a sky box typed for the map, the view timed
// ge:keywords sky skybox sky_typed skyrotate skyaxis timerefresh refresh-timing render-words
// ge:keywords r_reload reload-textures reload-models
// ge:skill render console
// ge:test tests/shooters_edge_render_words_test.cpp
#pragma once

#include <functional>
#include <string>

#include "classic/sky_typed.hpp"
#include "console/console.hpp"

namespace shooters_edge::client {

inline constexpr int kTimeRefreshFrames = 128;

struct RenderWords {
  bool sky_asked = false;     // a sky was typed: the map's world made again (the window clears it)
  bool reload_asked = false;  // `r_reload`: the world made again (the window clears it)
  int refresh_asked = 0;  // `timerefresh`: 1 each frame shown, 2 all in one (the window clears it)
};

// `sky`, `r_reload` and `timerefresh` on `c`, `sky` into `sky` (the look's); `map` names the map
// played
// ("" none).
void add_render_words(ge::console::Console& c, RenderWords& words, ge::classic::SkyTyped& sky,
                      std::function<std::string()> map);

// timerefresh's answer for `frames` drawn in `seconds`: "0.512000 seconds (250.000000 fps)".
std::string refresh_line(double seconds, int frames);

}  // namespace shooters_edge::client
