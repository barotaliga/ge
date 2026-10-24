// The layer table (every kind, every key, every default: the one place they are named) and the
// base painters: fill, noise and stripes.
//
// ge:summary The layer kinds' table and the base painters: fill, noise, stripes
// ge:keywords layer kinds table keys fill noise stripes blend overlay multiply screen
// ge:skill textures
// ge:test tests/texgen_recipe_test.cpp tests/texgen_bake_test.cpp
#include "texgen/layers.hpp"

#include <array>

#include "core/num.hpp"
#include "texgen/noise.hpp"

namespace ge::texgen {
namespace {

using K = KeyType;

constexpr const char* kBlends = "mix multiply overlay screen add";
constexpr const char* kKinds = "value perlin cells edges voronoi ridged turbulence white";

constexpr KeyDef kFill[] = {
    {"color", K::colour, 0, 0, 1, "#808080", "", "the colour"},
    {"height", K::number, 0.5, 0, 1, "", "", "the height, 0 deep .. 1 proud"},
    {"rough", K::number, 0.8, 0, 1, "", "", "roughness"},
    {"metal", K::number, 0, 0, 1, "", "", "metalness"},
};

constexpr KeyDef kNoise[] = {
    {"kind", K::word, 0, 0, 1, "perlin", kKinds, "the noise"},
    {"period", K::number, 8, 1, 512, "", "", "cells across the tile, first octave"},
    {"octaves", K::number, 4, 1, 10, "", "", "octaves, each twice as fine"},
    {"gain", K::number, 0.5, 0, 1, "", "", "each octave's weight against the last"},
    {"jitter", K::number, 1, 0, 1, "", "", "cells, edges, voronoi: feature points' stray"},
    {"warp", K::number, 0, 0, 2, "", "", "domain warp, tile units"},
    {"stretch", K::number, 1, 1, 64, "", "", "the field squeezed this many times along y"},
    {"lo", K::number, 0, 0, 1, "", "", "the field's value read as 0"},
    {"hi", K::number, 1, 0, 1, "", "", "the field's value read as 1"},
    {"invert", K::number, 0, 0, 1, "", "", "1 flips the field"},
    {"color", K::colour, 0, 0, 1, "#000000", "", "the colour at 0"},
    {"color2", K::colour, 0, 0, 1, "#ffffff", "", "the colour at 1"},
    {"blend", K::word, 0, 0, 1, "overlay", kBlends, "how the colour meets what is there"},
    {"amount", K::number, 0.3, 0, 1, "", "", "how much of the colour"},
    {"height", K::number, 0, -1, 1, "", "", "the field added to the height, signed"},
    {"rough", K::number, 0, -1, 1, "", "", "the field added to roughness, signed"},
    {"salt", K::number, 0, 0, 1000000, "", "", "layers with one salt share one pattern"},
};

constexpr KeyDef kStripes[] = {
    {"count", K::number, 6, 1, 256, "", "", "stripes across the tile"},
    {"dir", K::word, 0, 0, 1, "diagonal", "x y diagonal", "x: vertical bands, y: rows"},
    {"width", K::number, 0.5, 0, 1, "", "", "the coloured share of each stripe"},
    {"color", K::colour, 0, 0, 1, "#e8b820", "", "the stripe"},
    {"color2", K::colour, 0, 0, 1, "#1c1c1c", "", "between stripes"},
    {"amount", K::number, 1, 0, 1, "", "", "how much"},
};

constexpr KeyDef kBricks[] = {
    {"rows", K::number, 8, 1, 128, "", "", "courses down the tile"},
    {"cols", K::number, 4, 1, 128, "", "", "bricks across the tile"},
    {"offset", K::number, 0.5, 0, 1, "", "", "each course's shift, of a brick"},
    {"mortar", K::number, 0.12, 0, 0.6, "", "", "the joint, a share of a course's height"},
    {"bevel", K::number, 0.2, 0, 1, "", "", "the rounded edge, a share of a course"},
    {"depth", K::number, 0.4, 0, 1, "", "", "how far the joint sits below the face"},
    {"color", K::colour, 0, 0, 1, "#8c4a32", "", "the brick"},
    {"vary", K::number, 0.15, 0, 1, "", "", "how much bricks differ in colour"},
    {"mortar_color", K::colour, 0, 0, 1, "#a39c8e", "", "the joint"},
    {"rough", K::number, 0.85, 0, 1, "", "", "the brick's roughness"},
    {"color2", K::colour, 0, 0, 1, "#3a4a5a", "", "the second colour of a checkerboard"},
    {"checker", K::number, 0, 0, 1, "", "",
     "every other piece toward color2: 1 a full checkerboard"},
};

constexpr KeyDef kTiles[] = {
    {"rows", K::number, 4, 1, 128, "", "", "tiles down the tile"},
    {"cols", K::number, 4, 1, 128, "", "", "tiles across"},
    {"offset", K::number, 0, 0, 1, "", "", "each row's shift, of a tile"},
    {"mortar", K::number, 0.05, 0, 0.6, "", "", "the grout, a share of a row's height"},
    {"bevel", K::number, 0.08, 0, 1, "", "", "the rounded edge, a share of a row"},
    {"depth", K::number, 0.25, 0, 1, "", "", "how far the grout sits below the face"},
    {"color", K::colour, 0, 0, 1, "#d8d4cc", "", "the tile"},
    {"vary", K::number, 0.05, 0, 1, "", "", "how much tiles differ in colour"},
    {"mortar_color", K::colour, 0, 0, 1, "#6e6a62", "", "the grout"},
    {"rough", K::number, 0.35, 0, 1, "", "", "the tile's roughness"},
    {"color2", K::colour, 0, 0, 1, "#3a4a5a", "", "the second colour of a checkerboard"},
    {"checker", K::number, 0, 0, 1, "", "",
     "every other piece toward color2: 1 a full checkerboard"},
};

constexpr KeyDef kPlanks[] = {
    {"rows", K::number, 6, 1, 64, "", "", "boards down the tile"},
    {"color", K::colour, 0, 0, 1, "#8a5a34", "", "the wood"},
    {"vary", K::number, 0.2, 0, 1, "", "", "how much boards differ"},
    {"seam", K::number, 0.06, 0, 0.5, "", "", "the gap, a share of a board's width"},
    {"grain", K::number, 0.5, 0, 1, "", "", "how strong the grain reads"},
    {"knots", K::number, 0.3, 0, 1, "", "", "how many knots"},
    {"depth", K::number, 0.2, 0, 1, "", "", "how deep the gaps are"},
    {"rough", K::number, 0.7, 0, 1, "", "", "roughness"},
};

constexpr KeyDef kPanels[] = {
    {"rows", K::number, 2, 1, 32, "", "", "panels down the tile"},
    {"cols", K::number, 2, 1, 32, "", "", "panels across"},
    {"seam", K::number, 0.02, 0, 0.3, "", "", "the seam, a share of a panel"},
    {"bevel", K::number, 0.03, 0, 0.3, "", "", "the pressed edge, a share of a panel"},
    {"color", K::colour, 0, 0, 1, "#7d848c", "", "the plate"},
    {"vary", K::number, 0.06, 0, 1, "", "", "how much panels differ"},
    {"rivets", K::number, 6, 0, 32, "", "", "rivets along each edge (0: none)"},
    {"rivet_size", K::number, 0.018, 0, 0.1, "", "", "a rivet's radius, a share of a panel"},
    {"rivet_height", K::number, 0.12, 0, 1, "", "", "how far a rivet's head stands out"},
    {"rivet_sides", K::word, 0, 0, 1, "all", "all x y",
     "which edges carry rivets: all, x (left and right) or y (top and bottom)"},
    {"depth", K::number, 0.35, 0, 1, "", "", "how deep the seams are"},
    {"rough", K::number, 0.4, 0, 1, "", "", "roughness"},
    {"metal", K::number, 1, 0, 1, "", "", "metalness"},
};

constexpr KeyDef kBlocks[] = {
    {"rows", K::number, 4, 1, 64, "", "", "courses down the tile"},
    {"cols", K::number, 3, 1, 64, "", "", "blocks across a course, on average"},
    {"vary_size", K::number, 0.5, 0, 1, "", "", "how uneven the courses and blocks are"},
    {"span", K::number, 0, 0, 1, "", "", "the share of blocks running down through two courses"},
    {"mortar", K::number, 0.06, 0, 0.6, "", "", "the joint, a share of a course's height"},
    {"bevel", K::number, 0.1, 0, 1, "", "", "the rounded edge, a share of a course"},
    {"depth", K::number, 0.4, 0, 1, "", "", "how far the joint sits below the face"},
    {"chip", K::number, 0.4, 0, 1, "", "", "how ragged the joints are"},
    {"color", K::colour, 0, 0, 1, "#8a7a62", "", "the stone"},
    {"vary", K::number, 0.12, 0, 1, "", "", "how much blocks differ in colour"},
    {"mortar_color", K::colour, 0, 0, 1, "#3a342c", "", "the joint"},
    {"rough", K::number, 0.85, 0, 1, "", "", "the stone's roughness"},
};

constexpr KeyDef kStuds[] = {
    {"rows", K::number, 8, 1, 256, "", "", "studs down the tile (even, with an offset)"},
    {"cols", K::number, 8, 1, 256, "", "", "studs across"},
    {"shape", K::word, 0, 0, 1, "dome", "dome diamond bar hole",
     "dome, a pyramid, diamond-plate bars or holes"},
    {"size", K::number, 0.4, 0, 1, "", "", "a stud's width, a share of its cell"},
    {"offset", K::number, 0, 0, 1, "", "", "each row's shift, of a cell"},
    {"depth", K::number, 0.25, 0, 1, "", "", "how far a stud stands out (a hole sinks)"},
    {"color", K::colour, 0, 0, 1, "#a0a0a0", "", "the studs' colour, only when written"},
    {"amount", K::number, 1, 0, 1, "", "", "how much of the colour"},
};

constexpr KeyDef kScales[] = {
    {"rows", K::number, 8, 2, 128, "", "", "rows of scales down the tile (even)"},
    {"cols", K::number, 6, 1, 128, "", "", "scales across"},
    {"dir", K::word, 0, 0, 1, "up", "up down", "up: arches stacked upward; down: hanging"},
    {"rim", K::number, 0.12, 0, 0.75, "", "", "the rounded rim, in cells"},
    {"depth", K::number, 0.4, 0, 1, "", "", "how far each scale stands over the next"},
    {"color", K::colour, 0, 0, 1, "#5a6a62", "", "the scales"},
    {"vary", K::number, 0.1, 0, 1, "", "", "how much scales differ in colour"},
    {"rim_color", K::colour, 0, 0, 1, "#202020", "", "the rim's colour, only when written"},
    {"rough", K::number, 0.5, 0, 1, "", "", "roughness"},
};

constexpr KeyDef kCracks[] = {
    {"amount", K::number, 0.5, 0, 1, "", "", "the share of the surface cracked"},
    {"period", K::number, 5, 1, 128, "", "", "cells across the tile"},
    {"width", K::number, 0.03, 0, 0.3, "", "", "a crack's width, of a cell"},
    {"warp", K::number, 0.12, 0, 1, "", "", "how much the cracks wander"},
    {"depth", K::number, 0.4, 0, 1, "", "", "how deep they cut"},
    {"color", K::colour, 0, 0, 1, "#2a2622", "", "the crack"},
    {"salt", K::number, 0, 0, 1000000, "", "", "layers with one salt share one pattern"},
};

constexpr KeyDef kScratches[] = {
    {"count", K::number, 60, 0, 2000, "", "", "how many"},
    {"length", K::number, 0.15, 0, 1, "", "", "the longest, of the tile"},
    {"angle", K::number, 20, -180, 180, "", "", "the way most run, degrees from x"},
    {"spread", K::number, 25, 0, 180, "", "", "how far they stray from it, degrees"},
    {"amount", K::number, 0.5, 0, 1, "", "", "how bright"},
    {"color", K::colour, 0, 0, 1, "#d0d0d0", "", "the scratch"},
};

constexpr KeyDef kDirt[] = {
    {"amount", K::number, 0.5, 0, 1, "", "", "how dirty"},
    {"period", K::number, 4, 1, 64, "", "", "blotches across the tile"},
    {"color", K::colour, 0, 0, 1, "#3b3226", "", "the dirt"},
    {"cavity", K::number, 0.6, 0, 1, "", "", "how much it gathers in the low places"},
};

constexpr KeyDef kDust[] = {
    {"amount", K::number, 0.4, 0, 1, "", "", "how dusty"},
    {"color", K::colour, 0, 0, 1, "#cfc6b4", "", "the dust"},
    {"ledges", K::number, 0.7, 0, 1, "", "", "how much more lies on up-facing ledges"},
};

constexpr KeyDef kGrime[] = {
    {"amount", K::number, 0.6, 0, 1, "", "", "how much"},
    {"color", K::colour, 0, 0, 1, "#1e1a16", "", "the grime"},
    {"reach", K::number, 0.012, 0.002, 0.1, "", "", "how far round a joint, of the tile"},
};

constexpr KeyDef kWear[] = {
    {"amount", K::number, 0.4, 0, 1, "", "", "how worn"},
    {"color", K::colour, 0, 0, 1, "#d9cfc0", "", "what shows through"},
    {"reach", K::number, 0.008, 0.002, 0.1, "", "", "how far round an edge, of the tile"},
    {"rough", K::number, -0.25, -1, 1, "", "", "roughness change where worn"},
    {"metal", K::number, 0, 0, 1, "", "", "metalness where worn (bare metal under paint)"},
};

constexpr KeyDef kRust[] = {
    {"amount", K::number, 0.4, 0, 1, "", "", "how rusted"},
    {"period", K::number, 6, 1, 64, "", "", "patches across the tile"},
    {"color", K::colour, 0, 0, 1, "#6e3218", "", "deep rust"},
    {"color2", K::colour, 0, 0, 1, "#b8662c", "", "fresh rust"},
};

constexpr KeyDef kStreaks[] = {
    {"amount", K::number, 0.4, 0, 1, "", "", "how dark"},
    {"count", K::number, 40, 0, 1000, "", "", "how many"},
    {"length", K::number, 0.3, 0, 1, "", "", "the longest, of the tile"},
    {"width", K::number, 0.012, 0.001, 0.2, "", "", "the widest, of the tile"},
    {"color", K::colour, 0, 0, 1, "#2c2620", "", "the stain"},
};

constexpr KeyDef kMoss[] = {
    {"amount", K::number, 0.4, 0, 1, "", "", "how overgrown"},
    {"period", K::number, 5, 1, 64, "", "", "patches across the tile"},
    {"color", K::colour, 0, 0, 1, "#3f5222", "", "shaded moss"},
    {"color2", K::colour, 0, 0, 1, "#7f9038", "", "lit moss"},
};

constexpr KeyDef kSnow[] = {
    {"amount", K::number, 0.5, 0, 1, "", "", "how much"},
    {"color", K::colour, 0, 0, 1, "#f2f5fa", "", "the snow"},
    {"period", K::number, 8, 1, 64, "", "", "drifts across the tile"},
};

constexpr KeyDef kGlow[] = {
    {"pattern", K::word, 0, 0, 1, "cavity", "cavity cracks spots lines noise",
     "where it glows: the low places, cracks, dots, a grid of lines, noise"},
    {"color", K::colour, 0, 0, 1, "#ff7a2a", "", "the light"},
    {"intensity", K::number, 1, 0, 8, "", "", "how bright"},
    {"amount", K::number, 1, 0, 1, "", "", "how much of the pattern lights"},
    {"period", K::number, 8, 1, 256, "", "", "cracks, spots, lines, noise: cells across"},
    {"width", K::number, 0.05, 0, 0.5, "", "", "a line's or a spot's size, of a cell"},
    {"threshold", K::number, 0.3, 0, 1, "", "", "cavity: the height under which it glows"},
    {"warp", K::number, 0.12, 0, 1, "", "",
     "cracks: how much they wander (match the cracks layer)"},
    {"salt", K::number, 0, 0, 1000000, "", "", "layers with one salt share one pattern"},
};

constexpr KeyDef kShade[] = {
    {"light", K::word, 0, 0, 1, "top-left", "top-left top top-right left right",
     "where the baked light comes from"},
    {"strength", K::number, 1, 0, 4, "", "", "how strongly the slopes are lit and shadowed"},
    {"soft", K::number, 0, 0, 16, "", "", "the height blurred first, texels (0: crisp)"},
    {"highlight", K::number, 0.25, 0, 1, "", "", "how far lit edges go toward white"},
    {"occlusion", K::number, 0.3, 0, 1, "", "", "how dark the joints and pits go"},
};

constexpr KeyDef kSpeckle[] = {
    {"size", K::number, 1, 1, 16, "", "", "a grain's width, texels (a power of two)"},
    {"contrast", K::number, 0.15, 0, 1, "", "", "how much each grain's brightness jumps"},
    {"amount", K::number, 0, 0, 1, "", "", "the share of grains that take the speck colour"},
    {"color", K::colour, 0, 0, 1, "#3c8c78", "", "the specks"},
    {"cavity", K::number, 0.5, 0, 1, "", "", "how much the specks gather in the pits"},
    {"height", K::number, 0.04, -1, 1, "", "", "the grain added to the height, signed"},
};

constexpr KeyDef kPalette[] = {
    {"color", K::colour, 0, 0, 1, "#8c6a48", "", "a ramp's colour"},
    {"color2", K::colour, 0, 0, 1, "#606060", "", "a second ramp, only when written"},
    {"color3", K::colour, 0, 0, 1, "#606060", "", "a third ramp, only when written"},
    {"color4", K::colour, 0, 0, 1, "#606060", "", "a fourth ramp, only when written"},
    {"steps", K::number, 16, 2, 32, "", "", "shades in each ramp"},
    {"top", K::number, 0.35, 0, 1, "", "", "how far the brightest shade goes toward white"},
    {"dither", K::number, 0, 0, 2, "", "", "ordered dither, in shades"},
    {"amount", K::number, 1, 0, 1, "", "", "how much the colour snaps"},
};

constexpr KeyDef kTrim[] = {
    {"at", K::number, 0, 0, 1, "", "", "the band's top, down the tile (0: the top edge)"},
    {"height", K::number, 0.15, 0.01, 1, "", "", "the band's height, a share of the tile"},
    {"beads", K::number, 2, 1, 16, "", "",
     "rounded mouldings down the band (its top half with teeth)"},
    {"teeth", K::number, 0, 0, 64, "", "", "crenellation teeth across the tile; 0 for none"},
    {"depth", K::number, 0.5, 0, 1, "", "", "how far the mouldings stand proud"},
    {"color", K::colour, 0, 0, 1, "#8a7a64", "", "the trim"},
    {"vary", K::number, 0.05, 0, 1, "", "", "how much each tooth differs in colour"},
    {"shadow", K::number, 0.4, 0, 1, "", "", "the shadow it casts on what is below"},
    {"rough", K::number, 0.8, 0, 1, "", "", "the trim's roughness"},
};

constexpr std::array<LayerKind, 25> kKindsTable = {{
    {"fill", "base", "the whole tile one colour, height, roughness and metal", kFill, paint_fill},
    {"noise", "base", "a noise field blended into the colour, height and roughness", kNoise,
     paint_noise},
    {"stripes", "base", "hazard and awning stripes", kStripes, paint_stripes},
    {"bricks", "pattern", "courses of bricks in mortar, each its own colour", kBricks,
     paint_bricks},
    {"tiles", "pattern", "a grid of tiles in grout (bricks with tile defaults)", kTiles,
     paint_bricks},
    {"planks", "pattern", "boards with grain, knots, ends and gaps", kPlanks, paint_planks},
    {"panels", "pattern", "metal plates with pressed seams and rivets", kPanels, paint_panels},
    {"blocks", "pattern", "dressed stone blocks of uneven sizes in their joints", kBlocks,
     paint_blocks},
    {"studs", "pattern", "a field of domes, pyramids, diamond-plate bars or holes", kStuds,
     paint_studs},
    {"scales", "pattern", "overlapping scales or arches, row over row", kScales, paint_scales},
    {"trim", "pattern", "a band across the tile: mouldings, crenellation teeth, a shadow below",
     kTrim, paint_trim},
    {"cracks", "life", "a web of cracks cut into the surface", kCracks, paint_cracks},
    {"scratches", "life", "thin bright scratches, most one way", kScratches, paint_scratches},
    {"dirt", "life", "blotches of dirt, thicker in the low places", kDirt, paint_dirt},
    {"dust", "life", "a fine veil of dust, thick on ledges", kDust, paint_dust},
    {"grime", "life", "dark grime in the joints and pits", kGrime, paint_grime},
    {"wear", "life", "edges rubbed through to what is under", kWear, paint_wear},
    {"rust", "life", "rust eating in from the pits and edges", kRust, paint_rust},
    {"streaks", "life", "stains run down from above", kStreaks, paint_streaks},
    {"moss", "life", "moss in the joints and the damp low places", kMoss, paint_moss},
    {"snow", "life", "snow on the ledges and in the joints", kSnow, paint_snow},
    {"glow", "life", "light: in the low places, cracks, spots, lines or noise", kGlow, paint_glow},
    {"speckle", "finish", "grain a texel at a time, and specks gathering in the pits", kSpeckle,
     paint_speckle},
    {"shade", "finish", "the light baked into the colour off the height", kShade, paint_shade},
    {"palette", "finish", "every colour snapped onto a few ramps of shades", kPalette,
     paint_palette},
}};

size_t key_index(std::span<const KeyDef> keys, std::string_view key) {
  for (size_t k = 0; k < keys.size(); ++k) {
    if (key == keys[k].name) return k;
  }
  return 0;  // a painter asking for a key its kind lacks is a bug the tests catch
}

double blend_channel(const std::string& mode, double base, double top) {
  if (mode == "multiply") return base * top;
  if (mode == "overlay") return base < 0.5 ? 2 * base * top : 1 - 2 * (1 - base) * (1 - top);
  if (mode == "screen") return 1 - (1 - base) * (1 - top);
  if (mode == "add") return base + top - 0.5;
  return top;
}

}  // namespace

double Args::n(std::string_view key) const { return num[key_index(keys, key)]; }
int Args::i(std::string_view key) const {
  return static_cast<int>(core::num::round(num[key_index(keys, key)]));
}
Rgb Args::c(std::string_view key) const { return rgb[key_index(keys, key)]; }
const std::string& Args::w(std::string_view key) const { return word[key_index(keys, key)]; }
bool Args::given(std::string_view key) const { return written[key_index(keys, key)] != 0; }

std::span<const LayerKind> layer_kinds() { return kKindsTable; }

int find_layer_kind(std::string_view name) {
  for (size_t k = 0; k < kKindsTable.size(); ++k) {
    if (name == kKindsTable[k].name) return static_cast<int>(k);
  }
  return -1;
}

const KeyDef* find_key(const LayerKind& kind, std::string_view key) {
  for (const KeyDef& d : kind.keys) {
    if (key == d.name) return &d;
  }
  return nullptr;
}

void paint_fill(Surface& s, const Args& a) {
  const Rgb c = a.c("color");
  const double h = a.n("height"), r = a.n("rough"), m = a.n("metal");
  for_texels(s.size, [&](int x, int y, double, double) {
    const size_t i = s.at(x, y);
    set_colour(s, i, c);
    s.plane[kHeight][i] = static_cast<float>(h);
    s.plane[kRough][i] = static_cast<float>(r);
    s.plane[kMetal][i] = static_cast<float>(m);
  });
}

void paint_noise(Surface& s, const Args& a) {
  NoiseSpec spec;
  find_noise_kind(a.w("kind"), spec.kind);
  spec.period = a.i("period");
  spec.octaves = a.i("octaves");
  spec.gain = a.n("gain");
  spec.jitter = a.n("jitter");
  spec.warp = a.n("warp");
  spec.seed = a.seed;
  const int stretch = a.i("stretch");
  const double lo = a.n("lo"), hi = a.n("hi"), amount = a.n("amount");
  const double dh = a.n("height"), dr = a.n("rough");
  const bool invert = a.n("invert") >= 0.5;
  const Rgb c0 = a.c("color"), c1 = a.c("color2");
  const std::string& mode = a.w("blend");
  for_texels(s.size, [&](int x, int y, double u, double v) {
    double f = sample(spec, u, v * stretch);
    f = hi > lo ? clamp01((f - lo) / (hi - lo)) : (f >= lo ? 1.0 : 0.0);
    if (invert) f = 1 - f;
    const size_t i = s.at(x, y);
    if (amount > 0) {
      const Rgb top = mix(c0, c1, f), base = colour_at(s, i);
      const Rgb out{blend_channel(mode, base.r, top.r), blend_channel(mode, base.g, top.g),
                    blend_channel(mode, base.b, top.b)};
      set_colour(s, i, mix(base, out, amount));
    }
    if (dh != 0)
      s.plane[kHeight][i] = static_cast<float>(clamp01(s.plane[kHeight][i] + dh * (f - 0.5)));
    if (dr != 0)
      s.plane[kRough][i] = static_cast<float>(clamp01(s.plane[kRough][i] + dr * (f - 0.5)));
  });
}

void paint_stripes(Surface& s, const Args& a) {
  const int count = a.i("count");
  const std::string& dir = a.w("dir");
  const double width = a.n("width"), amount = a.n("amount");
  const Rgb c0 = a.c("color"), c1 = a.c("color2");
  // An anti-aliased edge a texel wide, in stripe units.
  const double aa = count / static_cast<double>(s.size);
  for_texels(s.size, [&](int x, int y, double u, double v) {
    const double t = dir == "x" ? u : (dir == "y" ? v : u + v);
    const double f = t * count - std::floor(t * count);
    // The distance to the stripe's nearer edge on the wrapped stripe, signed by side.
    const double in = f < width ? clamp01(0.5 + std::min(f, width - f) / aa)
                                : clamp01(0.5 - std::min(f - width, 1 - f) / aa);
    const size_t i = s.at(x, y);
    set_colour(s, i, mix(colour_at(s, i), mix(c1, c0, clamp01(in)), amount));
  });
}

}  // namespace ge::texgen
