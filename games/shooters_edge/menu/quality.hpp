// SHOOTER'S EDGE'S PICTURE QUALITY, 0 TO 10: one table of the settings each level writes
// (console/preset.hpp), the `r_quality` command over it and the menu's QUALITY row reading it.
//
// - LEVEL 0 is the classic look: the packs as they are, every classic-enhance switch off. LEVEL 9
//   is the picture as it ships, every default (a fresh config reads 9). LEVEL 10 adds what the
//   defaults leave out for its cost: every map light throwing the figures' shadows.
// - EACH STEP DOWN gives up the dearest thing left for the least of the look, by the measured
//   price of each setting (the perf cost table in docs/perf.md and the `se-quality` lab), and each
//   is a change a player sees: the shadows' soft edges, the mirrored water and the far land, the
//   contact shade and the figures' shadows, the cloud and the reflections, then shadows, water
//   and sky altogether; then the picture drawn at half the window's size and at a quarter, the
//   effects going back to the pack's own on the way.
// - `r_quality <level>` WRITES the cvars and nothing else: each is the player's to change after,
//   and a config saves them as it saves any. `r_quality` alone, and the menu's row, read the level
//   back from the cvars (`preset_level`): the level they match, else CUSTOM.
// - NOT IN IT: the switches that change what is baked into the cache (the trained upscaler, the
//   rounded models: a level must never start a bake) and the sound's.
//
// The table is the one place the level's cvars are named: a renamed cvar is one word here, and
// tests/shooters_edge_quality_test.cpp fails a row the game's console does not register.
//
// ge:summary Shooter's Edge's picture quality 0 to 10: the table of what each level writes
// ge:keywords quality r_quality quality-preset quality-slider graphics-preset preset-table
// ge:keywords picture-quality classic-look custom menu-quality quality_table add_quality
// ge:skill console render
// ge:test tests/shooters_edge_quality_test.cpp
#pragma once

#include "console/console.hpp"
#include "console/preset.hpp"

namespace shooters_edge::menu {

inline constexpr int kQualityLevels = 11;  // 0 .. 10
inline constexpr int kQualityShipped = 9;  // every default

// The table: a row a cvar, its value at each level, level 0 first.
const ge::console::PresetTable& quality_table();

// The `r_quality` command on `con` (after the picture's cvars are registered).
void add_quality(ge::console::Console& con);

}  // namespace shooters_edge::menu
