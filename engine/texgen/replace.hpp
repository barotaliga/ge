// REPLACEMENT BY LIKENESS: a level's own painted texture swapped for one of ours that looks like
// it. A renderer that loads a map whose textures were painted by hand can offer, as an option,
// to draw procedural textures instead: each texture is measured (its look, `look.hpp`, and which
// way its lines run), the nearest library recipe is picked, and that recipe is baked at the
// texture's size under a seed of its name, then its colours are moved onto the original's (each
// channel's mean and spread), so the replacement keeps the room's palette and brightness. The
// original's own lights (the texels the palette marks as full-bright) are kept where they were,
// so a lamp still glows where the map put it.
//
// The match is also what a tool that converts another game's map writes in place of each
// texture: a recipe reference (`match_texture`), never the pixels. A renderer drawing the map
// keeps a texture's own pixels when no recipe comes near enough, or when its name marks it as
// something drawn otherwise: a liquid (a leading '*') or a sky. The match is a pure function of
// the texture and the library: the same pixels pick the same reference on every machine.
//
// ge:summary Replaces a level's painted texture with the most alike library recipe, recoloured
// ge:keywords replace replacement similarity likeness match nearest recipe swap level-textures
// ge:keywords miptex palette indexed full-bright colour-transfer procedural option
// ge:skill textures texture-references
// ge:test tests/texgen_replace_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lab/raster.hpp"
#include "texgen/look.hpp"
#include "texgen/recipe.hpp"

namespace ge::texgen {

// What the matcher reads off a texture: its look, which way its lines run, and how often its
// pattern repeats each way (a grid of studs every eighth of the tile, courses of bricks every
// fifth): the strongest repeat of the luma's autocorrelation, as a share of the tile, and how
// strong it is (0: no repeat, 1: an exact one).
struct Likeness {
  Look look;
  double across = 0;  // the share of texels with a hard step to the right neighbour
  double down = 0;    // ... to the neighbour below
  double period_x = 0.5, period_y = 0.5;
  double repeat_x = 0, repeat_y = 0;
};

Likeness likeness_of(const lab::Picture& p);

// How unalike two textures are: the look's distance, the lines' directions and the repeats.
double likeness_distance(const Likeness& a, const Likeness& b);

// The recipes a replacement may pick from, each with its likeness measured once.
struct Matcher {
  struct Entry {
    Recipe recipe;
    Likeness likeness;
  };
  std::vector<Entry> entries;
  double max_distance = 0.8;  // no recipe nearer than this: a renderer keeps the texture's pixels
  int refine = 6;             // the structure's nearest recipes baked and measured whole
};

// Every recipe in `dir`, each baked at 64 px under seeds 1 and 2 and measured. Recipes that do
// not parse are left out.
Matcher make_matcher(const std::string& dir);

// What a texture is, by its name: a surface the matcher replaces, or something a renderer draws
// itself (a liquid, named with a leading '*'; a sky, named starting "sky").
enum class TextureKind : uint8_t { surface, liquid, sky };

// The recipe reference that stands in for a texture. Only a recipe's name, a seed made from the
// texture's name and the original's mean colour and spread (`tint`, `spread`) leave the call:
// nothing of its pixels, so a level written from it carries no one else's picture.
struct TextureMatch {
  TextureKind kind = TextureKind::surface;
  TextureRef ref;    // the recipe, its seed, tint and spread (empty name off a surface)
  std::string text;  // ref_text(ref): what a level stores (empty off a surface)
  std::string
      liquid;  // a liquid's stem, digits off: "*04water1" is "water" (empty unless kind is liquid)
  double distance = 0;  // how unalike the texture and the recipe are; lower is nearer
  bool near = false;    // distance within the matcher's max_distance
};

// The match for an RGBA texture (alpha ignored). Every surface gets a recipe, near or not.
TextureMatch match_texture(const Matcher& m, std::string_view name, const lab::Picture& original);
// The same for packed RGB (w x h x 3 bytes).
TextureMatch match_rgb(const Matcher& m, std::string_view name, std::span<const uint8_t> rgb,
                       uint32_t w, uint32_t h);
// The same for an indexed texture: `indexed` w x h palette indices, `palette` 768 bytes of RGB.
TextureMatch match_indexed(const Matcher& m, std::string_view name, uint32_t w, uint32_t h,
                           std::span<const uint8_t> indexed, std::span<const uint8_t> palette);

struct Replacement {
  lab::Picture picture;  // w x h, RGBA8, alpha 255
  TextureMatch match;
};

// The replacement a renderer draws for an RGB(A) texture: its match baked at the texture's size;
// nothing when it keeps its own pixels (not a surface, or no recipe near enough). `lights`
// (w x h, may be empty) marks the original's full-bright texels, kept as they were.
std::optional<Replacement> replace_texture(const Matcher& m, std::string_view name,
                                           const lab::Picture& original,
                                           std::span<const uint8_t> lights = {});

// The same for an indexed texture. Indices at or above `first_bright` are full-bright (256: none).
std::optional<Replacement> replace_indexed(const Matcher& m, std::string_view name, uint32_t w,
                                           uint32_t h, std::span<const uint8_t> indexed,
                                           std::span<const uint8_t> palette,
                                           int first_bright = 224);

// The texture a match stands for, baked at w x h (the recipe at the next power of two, resized).
lab::Picture bake_match(const Matcher& m, const TextureMatch& match, int w, int h);

// WHAT AN AGENT ARGUES FROM. A match is argued, not taken on a number: the agent says what kind
// of surface a texture is (bricks, planks, a riveted panel), what matters for that kind (how the
// bricks are laid and how many courses, the boards' direction and width, the panels' count and
// seams, the colour), and checks the reference meets it. The evidence is the texture's structure
// in the units that argument uses; the mismatches are where a reference's bake differs from the
// original in a way the eye catches before the colour: the wrong scale, lines the wrong way.
struct Evidence {
  int width = 0, height = 0;
  double repeat_x = 0, repeat_y = 0;      // the pattern's repeat in texels (0: none)
  double per_tile_x = 0, per_tile_y = 0;  // how many times it repeats across and down a tile
  std::string lines;                      // "horizontal", "vertical", "mixed", "flat"
  // Masonry read off the joints: the dark lines running right across the tile split it into
  // courses, and the dark lines down each course into its pieces. A texture with no joints has
  // one course of one piece.
  int courses = 0;
  std::vector<int> pieces;  // per course, top first
  double brightness = 0, contrast = 0;
  std::array<uint8_t, 3> mean{};
  // The seams: how far the colour jumps where the tile meets its own other edge, over the
  // typical jump between neighbouring strips inside it (strips three texels wide, row by row,
  // so a joint on the edge is not a seam). About 1 tiles cleanly; over kSeam a seam shows.
  double seam_x = 0, seam_y = 0;
};

inline constexpr double kSeam = 1.8;

Evidence evidence_of(const lab::Picture& p);
// One line: "4 courses, pieces a course 2 3 2 3; repeat 16x8 px (4x8 a tile), horizontal
// lines, contrast 0.12, #825540".
std::string evidence_text(const Evidence& e);
// Each difference worth an argument, as a sentence an agent can act on; empty when none.
// `coursed` false (a plain surface, rock, metal: nothing laid in courses) leaves out the courses
// and the pieces a course, which on such a surface count noise.
std::vector<std::string> evidence_mismatches(const Evidence& original, const Evidence& ours,
                                             bool coursed = true);

// A texture's name from its PNG's file name: a file system cannot always hold '*' or '+', so a
// leading "liq_" stands for a liquid's '*' and "anim_" for an animation's '+'.
std::string texture_name_of_file(std::string_view stem);

// THE AGENT'S LOOP. A converter's matches are steered by an agent (`ge texture match`): it
// reads each texture's nearest candidates and how far its current reference is, looks at the
// bakes beside the original, and pins a recipe, a seed, params or another tint by hand.

// A recipe the matcher could pick for a texture, and how unalike they are (colour left out).
struct Candidate {
  std::string recipe;
  double distance = 0;
};

// The `n` nearest recipes for a texture by structure alone (no bake), nearest first.
// With a class ("bricks"), only the recipes tagged with it.
std::vector<Candidate> rank_recipes(const Matcher& m, const lab::Picture& original, int n,
                                    std::string_view klass = {});

// The reference a texture's match starts from: the seed of its name, its tint and spread.
TextureRef colour_ref(std::string_view name, const lab::Picture& original);

// The `n` nearest recipes for a texture, measured whole (colour included) as `colour_ref` baked
// with each: the structure's max(n, refine) nearest, re-ranked. What the match picks from.
std::vector<Candidate> nearest_refs(const Matcher& m, std::string_view name,
                                    const lab::Picture& original, int n,
                                    std::string_view klass = {});

// Whether a recipe's tags hold the class word.
bool recipe_has_class(const Recipe& r, std::string_view klass);

// Any reference (a recipe of the matcher's, its seed, params, tint and spread) baked at w x h.
// Errors (an unknown recipe or param) go to `errors` and the picture is grey.
lab::Picture bake_ref(const Matcher& m, const TextureRef& ref, int w, int h,
                      std::vector<Diagnostic>& errors);

// How unalike a texture and a reference's bake are, colour included: the number an agent
// drives down by turning a reference's knobs. Under about 0.5 they read as the same surface.
double ref_distance(const Matcher& m, const TextureRef& ref, const lab::Picture& original,
                    std::vector<Diagnostic>& errors);

}  // namespace ge::texgen
