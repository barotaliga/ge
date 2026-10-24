// REPLACEMENT BY LIKENESS: a level's own painted texture swapped for one of ours that looks like
// it. A renderer that loads a map whose textures were painted by hand can offer, as an option,
// to draw procedural textures instead: each texture is measured (its look, `look.hpp`, and which
// way its lines run), the nearest library recipe is picked, and that recipe is baked at the
// texture's size under a seed of its name, then its colours are moved onto the original's (each
// channel's mean and spread), so the replacement keeps the room's palette and brightness. The
// original's own lights (the texels the palette marks as full-bright) are kept where they were,
// so a lamp still glows where the map put it.
//
// A texture keeps its own pixels (nothing is returned) when no recipe comes near enough, or when
// its name marks it as something a renderer draws itself: a liquid (a leading '*') or a sky.
// The match is a pure function of the texture and the library: the same pixels pick the same
// recipe and bake the same replacement on every machine.
//
// ge:summary Replaces a level's painted texture with the most alike library recipe, recoloured
// ge:keywords replace replacement similarity likeness match nearest recipe swap level-textures
// ge:keywords miptex palette indexed full-bright colour-transfer procedural option
// ge:skill textures texture-references
// ge:test tests/texgen_replace_test.cpp
#pragma once

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
  double max_distance = 1.6;  // no recipe nearer than this: the texture keeps its own pixels
};

// Every recipe in `dir`, each baked at 64 px under seeds 1 and 2 and measured. Recipes that do
// not parse are left out.
Matcher make_matcher(const std::string& dir);

struct Replacement {
  lab::Picture picture;  // w x h, RGBA8, alpha 255
  std::string recipe;    // the recipe it was baked from
  double distance = 0;   // how unalike the original and the recipe were
};

// The replacement for an RGB(A) texture named `name`; nothing when it keeps its own pixels.
// `lights` (w x h, may be empty) marks the original's full-bright texels, kept as they were.
std::optional<Replacement> replace_texture(const Matcher& m, std::string_view name,
                                           const lab::Picture& original,
                                           std::span<const uint8_t> lights = {});

// The same for an indexed texture: `indexed` w x h palette indices, `palette` 768 bytes of RGB.
// Indices at or above `first_bright` are full-bright (256: none).
std::optional<Replacement> replace_indexed(const Matcher& m, std::string_view name, uint32_t w,
                                           uint32_t h, std::span<const uint8_t> indexed,
                                           std::span<const uint8_t> palette,
                                           int first_bright = 224);

}  // namespace ge::texgen
