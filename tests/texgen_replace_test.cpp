// engine/texgen/replace.hpp: a library recipe's own bake, at another seed, is matched back to its
// own recipe (four in five at least); the replacement is the original's size, opaque,
// and the same bytes twice; it takes the original's mean colour; the original's lights stay
// where they were; liquids and skies keep their own pixels; and an indexed texture goes through
// its palette to the same answer as its RGB.
//
// ge:summary Tests the likeness replacement: matching, size, determinism, colour, lights, skips
// ge:keywords test texgen replace replacement likeness match indexed palette full-bright
// ge:skill testing textures
#include "texgen/replace.hpp"

#include <cmath>
#include <string>

#include "check.hpp"
#include "texgen/bake.hpp"
#include "texgen/library.hpp"
#include "texgen/preview.hpp"

namespace tg = ge::texgen;
namespace lab = ge::lab;

namespace {

const tg::Matcher& matcher() {
  static const tg::Matcher m = tg::make_matcher(ge::test::root_path(tg::kLibraryDir));
  return m;
}

lab::Picture own_bake(const tg::Recipe& recipe, uint32_t seed, int size) {
  tg::Recipe r = recipe;
  r.size = size;
  return tg::map_picture(tg::bake(r, {r.name, seed, {}}).maps, tg::MapKind::albedo, size);
}

const tg::Recipe& recipe(const std::string& name) {
  for (const auto& e : matcher().entries) {
    if (e.recipe.name == name) return e.recipe;
  }
  return matcher().entries.front().recipe;
}

}  // namespace

TEST(library_bakes_find_their_own_recipe) {
  const tg::Matcher& m = matcher();
  CHECK(m.entries.size() >= 20);
  int own = 0;
  std::string misses;
  for (const auto& e : m.entries) {
    const auto r = tg::replace_texture(m, e.recipe.name, own_bake(e.recipe, 9, 64));
    if (r && r->recipe == e.recipe.name) {
      ++own;
    } else {
      misses += " " + e.recipe.name + "->" + (r ? r->recipe : std::string("none"));
    }
  }
  // Every recipe finds itself today; a new recipe too like an old one may cost one or two.
  CHECK_MSG(own * 5 >= static_cast<int>(m.entries.size()) * 4,
            "only " + std::to_string(own) + " recipes matched themselves:" + misses);
}

TEST(a_replacement_is_the_originals_size_opaque_and_repeatable) {
  const lab::Picture original = own_bake(recipe("brick_wall"), 4, 64);
  const lab::Picture odd = lab::resized(original, 48, 40);
  const auto a = tg::replace_texture(matcher(), "wall_a", odd);
  const auto b = tg::replace_texture(matcher(), "wall_a", odd);
  CHECK(a && b);
  CHECK(a->picture.width == 48 && a->picture.height == 40);
  CHECK(a->picture.rgba == b->picture.rgba && a->recipe == b->recipe);
  for (size_t i = 3; i < a->picture.rgba.size(); i += 4) CHECK(a->picture.rgba[i] == 255);
}

TEST(a_replacement_takes_the_originals_colour) {
  lab::Picture original = own_bake(recipe("brick_wall"), 4, 64);
  for (size_t i = 0; i < original.rgba.size(); i += 4) {  // tint it green
    original.rgba[i] = static_cast<uint8_t>(original.rgba[i] / 3);
    original.rgba[i + 2] = static_cast<uint8_t>(original.rgba[i + 2] / 3);
  }
  const auto r = tg::replace_texture(matcher(), "green", original);
  CHECK(r.has_value());
  for (int c = 0; c < 3; ++c) {
    double mo = 0, mr = 0;
    for (size_t i = 0; i < original.rgba.size(); i += 4) {
      mo += original.rgba[i + c];
      mr += r->picture.rgba[i + c];
    }
    CHECK_MSG(std::fabs(mo - mr) / (64.0 * 64) < 3, "channel " + std::to_string(c));
  }
}

TEST(the_originals_lights_stay_where_they_were) {
  const lab::Picture original = own_bake(recipe("concrete"), 2, 32);
  std::vector<uint8_t> lights(32 * 32, 0);
  lights[5 * 32 + 7] = 1;
  const auto r = tg::replace_texture(matcher(), "lamp", original, lights);
  CHECK(r.has_value());
  const size_t i = (5 * 32 + 7) * 4;
  CHECK(r->picture.rgba[i] == original.rgba[i] && r->picture.rgba[i + 1] == original.rgba[i + 1] &&
        r->picture.rgba[i + 2] == original.rgba[i + 2]);
}

TEST(liquids_and_skies_keep_their_own_pixels) {
  const lab::Picture original = own_bake(recipe("concrete"), 2, 32);
  CHECK(!tg::replace_texture(matcher(), "*lava1", original));
  CHECK(!tg::replace_texture(matcher(), "sky4", original));
  CHECK(tg::replace_texture(matcher(), "floor1", original).has_value());
  tg::Matcher strict = matcher();
  strict.max_distance = 0;
  CHECK(!tg::replace_texture(strict, "floor1", original));
}

TEST(an_indexed_texture_goes_through_its_palette) {
  std::vector<uint8_t> palette(768);
  for (int k = 0; k < 256; ++k) {
    palette[k * 3] = static_cast<uint8_t>(k);
    palette[k * 3 + 1] = static_cast<uint8_t>(k * 3 / 4);
    palette[k * 3 + 2] = static_cast<uint8_t>(k / 2);
  }
  const lab::Picture grey = own_bake(recipe("concrete"), 2, 32);
  std::vector<uint8_t> indexed(32 * 32);
  lab::Picture rgb = lab::create_picture(32, 32, {0, 0, 0});
  for (size_t i = 0; i < indexed.size(); ++i) {
    indexed[i] = static_cast<uint8_t>(grey.rgba[i * 4] * 223 / 255);  // no full-bright index
    for (int c = 0; c < 3; ++c) rgb.rgba[i * 4 + c] = palette[indexed[i] * 3 + c];
  }
  const auto a = tg::replace_indexed(matcher(), "floor1", 32, 32, indexed, palette);
  const auto b = tg::replace_texture(matcher(), "floor1", rgb);
  CHECK(a && b && a->picture.rgba == b->picture.rgba);
  CHECK(!tg::replace_indexed(matcher(), "floor1", 32, 32, indexed, {palette.data(), 100}));
}

TEST(an_animations_frames_share_their_replacement) {
  const lab::Picture original = own_bake(recipe("metal_panels"), 3, 32);
  const auto a = tg::replace_texture(matcher(), "+0slip", original);
  const auto b = tg::replace_texture(matcher(), "+1slip", original);
  const auto c = tg::replace_texture(matcher(), "slap", original);
  CHECK(a && b && c && a->picture.rgba == b->picture.rgba && a->picture.rgba != c->picture.rgba);
}
