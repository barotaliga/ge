// THE FAR LAND'S KINDS (render/far_land.hpp): eleven recipes, eight of the Earth's and three of
// other worlds'. A kind's snow line is where its valley's air, cooled at the standard lapse rate,
// reaches freezing (`temp_c / kLapse` over the valley floor); its tree line a height of its own.
// Colours are linear albedo, read off the surfaces they name: fresh snow near 0.85, granite near
// 0.2, a meadow's green near 0.1, a conifer forest near 0.03, deep water a dark scattering colour.
//
// ge:summary The far land's eleven kinds: alps, fjord, highlands, winter, dolomites, sierra, …
// ge:keywords render far-land kinds catalogue alps fjord highlands winter dolomites sierra
// ge:keywords volcano arctic xeno ember rime alien earthly snow-line tree-line colours
// ge:skill render
// ge:test tests/render_far_land_test.cpp
#include <string_view>

#include "render/far_land.hpp"

namespace ge::render {

namespace {

// clang-format off
// Every field in the struct's order: name, what, sky;
//   hill_drop, hill_run, near_m, peak, span, ridged, towers, cones, terraces, lakes, water;
//   temp_c, tree_m, strata_m, clarity_km;
//   rock, rock2, grass, dry, forest, snow, sand, deep, shallow; alien, liquid.
constexpr FarLandKind kKinds[] = {
    {"alps", "high ice-capped ranges over green valleys and a lake, forest to 1500 m",
     "earth",
     350, 900, 4000, 4200, 5200, 0.85, 0.05, 0, 0, 0.5, 10,
     12, 1500, 0, 70,
     {0.20f, 0.19f, 0.18f}, {0.13f, 0.12f, 0.115f}, {0.07f, 0.13f, 0.035f}, {0.17f, 0.16f, 0.07f},
     {0.022f, 0.040f, 0.020f}, {0.86f, 0.89f, 0.94f}, {0.30f, 0.28f, 0.24f},
     {0.008f, 0.030f, 0.045f}, {0.040f, 0.085f, 0.080f}, false, FarLiquid::water},
    {"fjord", "steep green walls out of a deep sea inlet, snow on the tops", "earth",
     420, 700, 1600, 2300, 3600, 0.75, 0, 0, 0, 0.6, 330,
     7, 520, 0, 50,
     {0.15f, 0.15f, 0.15f}, {0.09f, 0.09f, 0.095f}, {0.06f, 0.11f, 0.035f}, {0.14f, 0.14f, 0.07f},
     {0.020f, 0.036f, 0.022f}, {0.85f, 0.88f, 0.93f}, {0.22f, 0.21f, 0.19f},
     {0.004f, 0.020f, 0.030f}, {0.020f, 0.060f, 0.065f}, false, FarLiquid::water},
    {"highlands", "rolling moors of grass and heather round long lochs; no snow, few trees", "earth",
     140, 700, 2500, 1150, 5200, 0.35, 0, 0, 0, 0.7, 12,
     12, 180, 0, 45,
     {0.14f, 0.13f, 0.12f}, {0.10f, 0.08f, 0.08f}, {0.08f, 0.12f, 0.04f}, {0.14f, 0.10f, 0.07f},
     {0.025f, 0.040f, 0.022f}, {0.84f, 0.87f, 0.92f}, {0.24f, 0.22f, 0.18f},
     {0.006f, 0.018f, 0.025f}, {0.030f, 0.050f, 0.045f}, false, FarLiquid::water},
    {"winter", "every range under snow in deep frost, a frozen lake, the forest snowed on", "earth",
     300, 800, 3000, 2800, 4600, 0.8, 0, 0, 0, 0.55, 10,
     -8, 1300, 0, 90,
     {0.13f, 0.13f, 0.14f}, {0.07f, 0.07f, 0.08f}, {0.07f, 0.11f, 0.04f}, {0.15f, 0.14f, 0.08f},
     {0.020f, 0.034f, 0.024f}, {0.88f, 0.91f, 0.96f}, {0.30f, 0.28f, 0.25f},
     {0.006f, 0.022f, 0.035f}, {0.030f, 0.070f, 0.080f}, false, FarLiquid::water},
    {"dolomites", "pale limestone towers out of high pastures, snow in the gullies",
     "earth",
     500, 1200, 3200, 2400, 3200, 0.55, 0.85, 0, 0, 0.4, 10,
     14, 1500, 60, 60,
     {0.46f, 0.43f, 0.38f}, {0.34f, 0.30f, 0.26f}, {0.08f, 0.13f, 0.04f}, {0.18f, 0.16f, 0.08f},
     {0.022f, 0.040f, 0.020f}, {0.86f, 0.89f, 0.94f}, {0.40f, 0.37f, 0.32f},
     {0.006f, 0.035f, 0.045f}, {0.050f, 0.110f, 0.100f}, false, FarLiquid::water},
    {"sierra", "red mesas and buttes in bands of sandstone over a dry plain; no water",
     "earth",
     260, 500, 2200, 1500, 4200, 0.3, 0.5, 0, 110, 0, -1,
     31, 0, 22, 110,
     {0.40f, 0.18f, 0.08f}, {0.52f, 0.33f, 0.18f}, {0.12f, 0.12f, 0.05f}, {0.36f, 0.27f, 0.15f},
     {0.040f, 0.050f, 0.025f}, {0.80f, 0.80f, 0.80f}, {0.50f, 0.36f, 0.22f},
     {0.010f, 0.030f, 0.040f}, {0.050f, 0.090f, 0.080f}, false, FarLiquid::water},
    {"volcano", "black cones out of a warm sea, green on their flanks, the highest capped white", "earth",
     450, 900, 5000, 1200, 6000, 0.3, 0, 0.55, 0, 0.8, 380,
     15, 900, 0, 60,
     {0.050f, 0.048f, 0.050f}, {0.12f, 0.07f, 0.05f}, {0.05f, 0.12f, 0.03f}, {0.13f, 0.13f, 0.06f},
     {0.018f, 0.040f, 0.018f}, {0.85f, 0.88f, 0.93f}, {0.06f, 0.06f, 0.06f},
     {0.004f, 0.025f, 0.040f}, {0.020f, 0.090f, 0.100f}, false, FarLiquid::water},
    {"arctic", "white hills down to a dark open sea, no tree, the rock in black ribs",
     "earth",
     160, 500, 2600, 1800, 4200, 0.85, 0, 0, 0, 0.5, 120,
     -0.5, 0, 0, 90,
     {0.08f, 0.08f, 0.09f}, {0.05f, 0.05f, 0.055f}, {0.06f, 0.08f, 0.04f}, {0.13f, 0.12f, 0.08f},
     {0.020f, 0.030f, 0.020f}, {0.86f, 0.89f, 0.95f}, {0.20f, 0.19f, 0.18f},
     {0.003f, 0.012f, 0.020f}, {0.015f, 0.040f, 0.050f}, false, FarLiquid::water},
    {"xeno", "violet meadows and needles of teal stone round green-glass lakes", "alien",
     300, 800, 3000, 2700, 4000, 0.5, 1.0, 0, 0, 0.6, 15,
     20, 900, 40, 40,
     {0.11f, 0.12f, 0.16f}, {0.06f, 0.16f, 0.15f}, {0.16f, 0.05f, 0.20f}, {0.26f, 0.13f, 0.20f},
     {0.050f, 0.015f, 0.070f}, {0.72f, 0.88f, 0.86f}, {0.36f, 0.32f, 0.46f},
     {0.000f, 0.080f, 0.060f}, {0.050f, 0.240f, 0.180f}, true, FarLiquid::water},
    {"ember", "black glass crags round lakes of lava under a red sky; nothing grows",
     "hell",
     400, 900, 2500, 2300, 3500, 0.9, 0.3, 0.12, 0, 0.7, 20,
     45, 0, 30, 30,
     {0.045f, 0.038f, 0.036f}, {0.15f, 0.06f, 0.03f}, {0.09f, 0.08f, 0.075f}, {0.14f, 0.11f, 0.09f},
     {0.030f, 0.025f, 0.020f}, {0.70f, 0.70f, 0.70f}, {0.12f, 0.10f, 0.09f},
     {0.030f, 0.020f, 0.018f}, {1.000f, 0.330f, 0.060f}, true, FarLiquid::lava},
    {"rime", "a frozen moon: blue ice crags and spires over a sea frozen hard", "ice",
     300, 700, 2500, 3000, 3000, 1.0, 0.4, 0, 0, 0.6, 15,
     -30, 0, 0, 120,
     {0.04f, 0.05f, 0.08f}, {0.22f, 0.32f, 0.46f}, {0.10f, 0.12f, 0.15f}, {0.16f, 0.18f, 0.22f},
     {0.030f, 0.035f, 0.050f}, {0.76f, 0.85f, 0.95f}, {0.30f, 0.34f, 0.40f},
     {0.004f, 0.012f, 0.030f}, {0.150f, 0.260f, 0.400f}, true, FarLiquid::water},
};
// clang-format on

}  // namespace

std::span<const FarLandKind> far_land_kinds() { return kKinds; }

const FarLandKind* find_far_land(std::string_view name) {
  for (const FarLandKind& k : kKinds) {
    if (name == k.name) return &k;
  }
  return nullptr;
}

}  // namespace ge::render
