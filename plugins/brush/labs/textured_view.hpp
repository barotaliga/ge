// A MAP DRAWN WITH ITS OWN PICTURES for the `bsp` lab: every surface of a 29 or BSP2 map (or a 38
// map whose `Texture::indexed` the caller filled from its wall texture files, with the textures'
// sizes) textured from the palette-indexed pictures the map carries, through a
// 256-colour palette the caller hands in (a game's files hold it), and lit by its lightmap the
// way the family's light was meant: a luxel of 128 is the picture as painted, brighter up to
// twice, and the palette's last 32 colours glow at full brightness whatever the light. Sky is
// the sky picture's two layers seen along the view's direction; liquids are drawn unlit. Trigger
// volumes (brush models of a `trigger_` entity) are left out, as a game leaves them out.
//
// A texture may be handed in true colour in place of its palette picture (`TrueColour`: an
// enhanced copy at any multiple of its size, a renderer's replacement): sampled bilinear, lit the
// same way, its texels of alpha 255 glowing at full brightness. Sky and liquids keep the pack's.
// `start_eyes` names the views a map is looked at from: its starts at eye height.
//
// A perspective-correct software raster with a depth buffer, triangles cut at the near plane,
// drawn at twice the size and filtered down. A lab tool, not a renderer: the GPU's own map pass
// is docs/goals.md M18's.
//
// ge:summary Lab tool: a classic map drawn textured from its own pictures, palette and lightmaps
// ge:keywords lab bsp brush textured software raster palette miptex lightmap sky fullbright
// ge:keywords perspective-correct depth-buffer first-person view
// ge:skill brush labs
// ge:test tests/brush_read_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "lab/raster.hpp"

namespace ge::brush::labs {

struct TexturedEye {
  double at[3] = {};     // file units, z up
  double yaw_deg = 0;    // 0 along +x, counter-clockwise from above (the entity's `angle`)
  double pitch_deg = 0;  // up positive
  double fov_deg = 90;   // across the panel's width
  // The palette's last 32 colours glow at full brightness (Quake's rule); false: every texel is lit
  // by its lightmap (Quake II's).
  bool palette_glows = true;
  // Each channel lit by its own luxel channel (Quake II's coloured light); false: by their mean.
  bool coloured_light = false;
  // The display's gamma lift on the result: 0.7 brightens as the family's own drawing did; 1 is
  // the colours as worked out (a formula checked pixel by pixel).
  double gamma = 0.7;
};

// Draws `map` into the panel at (x0, y0), w × h px of `p`, seen from `eye`; `palette` is 768
// bytes (256 RGB triples). Returns the triangles drawn.
size_t draw_textured(lab::Picture& p, int x0, int y0, int w, int h, const Map& map,
                     std::span<const uint8_t> palette, const TexturedEye& eye);

// A texture in true colour: `width` x `height` RGBA, alpha 255 where it glows, 0 where its light
// falls on it. An empty one leaves the texture to the palette.
struct TrueColour {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;
};

// The same with `replaced[k]` (when not empty) drawn in place of texture k's palette picture on
// every surface but sky and liquids; `replaced` may be shorter than the map's textures.
size_t draw_textured(lab::Picture& p, int x0, int y0, int w, int h, const Map& map,
                     std::span<const uint8_t> palette, const TexturedEye& eye,
                     std::span<const TrueColour> replaced);

// Where to stand in a map to look at it, each view named: the player's start ("THE START"), then
// up to three deathmatch or co-op starts spread through the entity list ("START <n>"), each at eye
// height facing its `angle`.
std::vector<std::pair<std::string, TexturedEye>> start_eyes(const Map& map);

}  // namespace ge::brush::labs
