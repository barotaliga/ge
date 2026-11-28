// Shooter's Edge's picture quality: the table, a column a level.
//
// The levels are spaced for an even step in frame rate: each about a fifth faster than the one
// above, by the M3 Max's GPU span at 2560x1440 on dm3 (the `se-timedemo` lab's --price-cvars,
// docs/perf.md). The picture's size (r_scale 1, 0.5, 0.25) is the steepest lever, so the levels
// under 5 draw at half or a quarter of the window and buy back what they can at that size; 5 is
// the first at full size and the dear rows start again over it.
//
//  0  the classic look: the packs as they are, at a quarter of the window's size.
//  1  the modern look at a quarter size, crisp: the enhanced textures, skins and 2D art, the
//     rebaked light, the live flames' light, the glowing texels, our effects and the marks.
//  2  drawn at half the window's size, crisp.
//  3  smoothed and sharpened (r_scale_smooth, r_scale_sharpen 0.8); the surfaces' relief and
//     depth, fog, weather, bloom; the walls' shadows, our water (still) and sky (its mean cloud),
//     the heat haze and the fumes.
//  4  the cloud layer, the water's chop and reflection, the glossy floors', the contact shade and
//     the figures' shadows, still at half size.
//  5  the window's full size, without the shadows, water, sky, haze and fumes.
//  6  the walls' shadows, our water and sky, the depth, the haze and the fumes again.
//  7  the cloud layer, the water's chop and reflection, the glossy floors' reflections.
//  8  the contact shade, the figures' shadows hard-edged.
//  9  the shadows soft and from every light, the room mirrored whole in the water, the sun's
//     caustics and shafts under it, the far land: every default.
//  10 every map light throwing the figures' shadows (r_fill_shadows).
//
// Measured (the span, ms; 56bd669c7's levels, which had 4 and 5 apart): 9 6.54, 8 5.68, 7 4.87,
// 6 4.15, 5 3.67, 1 1.33, 0 1.14. Under about 2 ms a frame the CPU and the present decide the
// frame rate more than the GPU: the classic look at 0 draws on the GPU in less than 1 but its
// frame took longer (1.49 ms against 1.12).
//
// ge:summary Shooter's Edge's picture quality: the table of the eleven levels, the command
// ge:keywords quality r_quality quality_table add_quality preset-table
// ge:skill console render
// ge:test tests/shooters_edge_quality_test.cpp
#include "shooters_edge/menu/quality.hpp"

namespace shooters_edge::menu {

namespace {

using ge::console::PresetRow;

// A column a level, so clang-format leaves the grid alone.
// clang-format off
//                            0       1      2      3      4      5      6      7      8      9      10
constexpr PresetRow kRows[] = {
    {"r_look",             "classic modern modern modern modern modern modern modern modern modern modern"},
    {"r_scale",            "0.25   0.25   0.5    0.5    0.5    1      1      1      1      1      1"},
    {"r_scale_smooth",     "0      0      0      1      1      0      0      0      0      0      0"},
    {"r_scale_sharpen",    "0.5    0.5    0.5    0.8    0.8    0.5    0.5    0.5    0.5    0.5    0.5"},
    {"r_fill_shadows",     "0      0      0      0      0      0      0      0      0      0      1"},
    {"ce_enabled",         "1      1      1      1      1      1      1      1      1      1      1"},
    // The enhanced pack: from 1.
    {"ce_textures",        "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_pictures",        "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_models",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_smooth",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_weapons",         "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_rebake",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_flicker",         "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_glow",            "0      1      1      1      1      1      1      1      1      1      1"},
    // Our effects and the marks on the walls: from 1 (cheap where the picture is small).
    {"ce_decals",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_lava",            "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_slime",           "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_wade",            "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_teleports",       "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_flames",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_bonfires",        "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_muzzles",         "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_explosions",      "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_trails",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_hits",            "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_blood",           "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_shells",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_flares",          "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_bubbles",         "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_beams",           "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_warps",           "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_respawns",        "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_monsters",        "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_burning",         "0      1      1      1      1      1      1      1      1      1      1"},
    {"ce_wind",            "0      1      1      1      1      1      1      1      1      1      1"},
    // The surfaces' relief, fog, weather and bloom: from 3.
    {"ce_relief",          "0      0      0      1      1      1      1      1      1      1      1"},
    {"ce_fog",             "0      0      0      1      1      1      1      1      1      1      1"},
    {"ce_weather",         "0      0      0      1      1      1      1      1      1      1      1"},
    {"ce_bloom",           "0      0      0      1      1      1      1      1      1      1      1"},
    // The dear rows: 3 and 4 at half size, again from 6 at full size.
    {"ce_shadows",         "0      0      0      1      1      0      1      1      1      1      1"},
    {"ce_shadows_quality", "low    low    low    low    medium low    low    low    medium high   high"},
    {"ce_water",           "0      0      0      1      1      0      1      1      1      1      1"},
    {"ce_water_quality",   "low    low    low    low    medium low    low    medium medium high   high"},
    {"ce_skies",           "0      0      0      1      1      0      1      1      1      1      1"},
    {"ce_skies_quality",   "low    low    low    low    medium low    low    medium medium high   high"},
    {"ce_parallax",        "0      0      0      1      1      0      1      1      1      1      1"},
    {"ce_heathaze",        "0      0      0      1      1      0      1      1      1      1      1"},
    {"ce_fumes",           "0      0      0      1      1      0      1      1      1      1      1"},
    {"ce_clouds",          "0      0      0      0      1      0      0      1      1      1      1"},
    {"ce_reflections",     "0      0      0      0      1      0      0      1      1      1      1"},
    {"ce_ao",              "0      0      0      0      1      0      0      0      1      1      1"},
};
// clang-format on

constexpr ge::console::PresetTable kTable{"r_quality", kQualityLevels, kRows};

}  // namespace

const ge::console::PresetTable& quality_table() { return kTable; }

void add_quality(ge::console::Console& con) {
  ge::console::add_preset_command(
      con, kTable,
      "The picture's quality from 0 (the classic look) to 10, each level writing the picture's "
      "settings; alone, the level they make now.");
}

}  // namespace shooters_edge::menu
