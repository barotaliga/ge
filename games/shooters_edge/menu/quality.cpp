// Shooter's Edge's picture quality: the table, a column a level.
//
// What each level adds over the one under it (the prices are the Mac's span at 2560x1440 on dm3,
// docs/perf.md; the `se-quality` lab draws each level and times it on the machine it runs on):
//
//  1  the modern look at a quarter of the window's size, crisp: the enhanced textures, skins and
//     2D art, the rebaked light, the live flames' light, the glowing texels.
//  2  drawn at half the window's size, crisp (a quarter of the pixels of the full picture).
//  3  smoothed and sharpened (r_scale_smooth, r_scale_sharpen 0.8); the effects as ours (fire,
//     explosions, trails, hits, blood, shells, beams, the monsters' attacks...), the marks on the
//     walls, the surfaces' relief, lava, slime, teleporters, fog, weather, wind and bloom.
//  4  drawn at the window's size (r_scale 1: about 3.3 ms).
//  5  the shadows (the walls' alone), our water (still) and our sky (its mean cloud), the surfaces'
//     depth, the heat haze and the fumes (about 1 ms).
//  6  the cloud layer, the water's chop and the room reflected in it, the glossy floors'
//     reflections (about 0.7 ms).
//  7  the contact shade, the figures' shadows hard-edged (about 0.8 ms).
//  8  the room mirrored whole in the water, the sun's caustics and shafts under it, the far land
//     round the map (about 0.5 ms, and the frames under water: about 4 ms of the slowest).
//  9  the shadows soft by each flame's size and from every light (about 0.5 ms): every default.
//  10 every map light throwing the figures' shadows (r_fill_shadows).
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
    {"r_scale",            "0.25    0.25   0.5    0.5    1      1      1      1      1      1      1"},
    {"r_scale_smooth",     "0       0      0      1      0      0      0      0      0      0      0"},
    {"r_scale_sharpen",    "0.5     0.5    0.5    0.8    0.5    0.5    0.5    0.5    0.5    0.5    0.5"},
    {"r_fill_shadows",     "0       0      0      0      0      0      0      0      0      0      1"},
    {"ce_enabled",         "1       1      1      1      1      1      1      1      1      1      1"},
    // The enhanced pack: from 1.
    {"ce_textures",        "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_pictures",        "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_models",          "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_smooth",          "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_weapons",         "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_rebake",          "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_flicker",         "0       1      1      1      1      1      1      1      1      1      1"},
    {"ce_glow",            "0       1      1      1      1      1      1      1      1      1      1"},
    // Our effects and the world's dressing: from 3.
    {"ce_relief",          "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_decals",          "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_lava",            "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_slime",           "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_wade",            "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_teleports",       "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_flames",          "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_bonfires",        "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_muzzles",         "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_explosions",      "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_trails",          "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_hits",            "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_blood",           "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_shells",          "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_flares",          "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_bubbles",         "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_beams",           "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_warps",           "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_respawns",        "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_monsters",        "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_burning",         "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_fog",             "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_weather",         "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_wind",            "0       0      0      1      1      1      1      1      1      1      1"},
    {"ce_bloom",           "0       0      0      1      1      1      1      1      1      1      1"},
    // The dear ones, each at its stop (look.hpp's Stops: low, medium, high; low while it is off).
    {"ce_shadows",         "0       0      0      0      0      1      1      1      1      1      1"},
    {"ce_shadows_quality", "low     low    low    low    low    low    low    medium medium high   high"},
    {"ce_water",           "0       0      0      0      0      1      1      1      1      1      1"},
    {"ce_water_quality",   "low     low    low    low    low    low    medium medium high   high   high"},
    {"ce_skies",           "0       0      0      0      0      1      1      1      1      1      1"},
    {"ce_skies_quality",   "low     low    low    low    low    low    medium medium high   high   high"},
    {"ce_parallax",        "0       0      0      0      0      1      1      1      1      1      1"},
    {"ce_heathaze",        "0       0      0      0      0      1      1      1      1      1      1"},
    {"ce_fumes",           "0       0      0      0      0      1      1      1      1      1      1"},
    {"ce_clouds",          "0       0      0      0      0      0      1      1      1      1      1"},
    {"ce_reflections",     "0       0      0      0      0      0      1      1      1      1      1"},
    {"ce_ao",              "0       0      0      0      0      0      0      1      1      1      1"},
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
