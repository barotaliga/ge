// Particle effects as data (engine/particles/effect.hpp): the text read into a book (a later
// effect of a name replacing it, comments and blanks passed over), every fault refused with its
// file, line and fix and the book left as it was; a burst thrown inside its shape at its speeds,
// scaled by the game's unit, the same on the same seed; a trail laid at its spacing; a palette's
// entries as the specks' colours; the light given; and the render pool's copy (render/spark.hpp's
// fields mirrored, checked here on the effect's own struct).
//
// ge:summary Tests: the effect text, its faults by line, bursts, trails, palettes, lights, seeds
// ge:keywords test particles effect-text parse_effects throw_effect throw_trail fault book
// ge:skill render testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "particles/effect.hpp"

namespace pt = ge::particles;

namespace {

constexpr const char* kText = R"(# a blast and a flight
effect blast
  light 300 1 0.8 0.5 0.5
  burst 64            # the fireball
    box 16
    speed 100 200
    life 0.5 1
    size 2 3
    palette 104 111
    cool 0.4 0.2
  burst 8
    ball 4
    rise 20 40
    smoke 0.4 0.6
    fall 0

effect rocket_trail
  trail 3
    colour 1 0.5 0.1
    fall -40
)";

double dist(const float a[3], const double b[3]) {
  const double d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

}  // namespace

TEST(an_effect_text_reads_into_the_book_and_a_later_effect_replaces_its_name) {
  pt::Book book;
  std::string error;
  CHECK_MSG(pt::parse_effects(kText, "particles/test.fx", book, error), error);
  CHECK(book.effects.size() == 2);
  const pt::Effect* blast = book.find("blast");
  CHECK(blast != nullptr && blast->emitters.size() == 2 && blast->light_radius == 300);
  CHECK(blast != nullptr && blast->emitters[0].count == 64 && blast->emitters[0].palette[0] == 104);
  CHECK(blast != nullptr && blast->emitters[1].smoke && blast->emitters[1].falls_set);
  const pt::Effect* trail = book.find("rocket_trail");
  CHECK(trail != nullptr && trail->emitters.size() == 1 && trail->emitters[0].trail);
  CHECK(book.find("nothing") == nullptr);
  // A mod's file over the game's: its blast replaces the first, the trail stays.
  CHECK_MSG(pt::parse_effects("effect blast\n burst 3\n", "mod/particles/b.fx", book, error),
            error);
  CHECK(book.effects.size() == 2);
  CHECK(book.find("blast") != nullptr && book.find("blast")->emitters[0].count == 3);
  CHECK(book.find("blast") != nullptr && book.find("blast")->file == "mod/particles/b.fx");
}

TEST(a_fault_names_its_file_and_line_and_the_book_is_left_as_it_was) {
  pt::Book book;
  std::string error;
  CHECK(pt::parse_effects(kText, "a.fx", book, error));
  const struct {
    const char* text;
    const char* says;
  } faults[] = {
      {"effect x\n burst 4\n speed 9\n", "a.fx:3: `speed` takes 2 numbers, got 1"},
      {"effect x\n burst 4\n spin 9\n", "a.fx:3: `spin` is not a word an effect knows"},
      {"burst 4\n", "a.fx:1: `burst` comes before any `effect <name>`"},
      {"effect Big\n", "a.fx:1: `effect` takes one name"},
      {"effect x\n box 4\n", "a.fx:2: `box`: it belongs to an emitter"},
      {"effect x\n burst 4\n life 1 x\n", "a.fx:3: `x` is not a number"},
      {"effect x\n burst 0\n", "a.fx:2: `burst`: the count is a whole number"},
      {"effect x\n burst 1\n palette 250 300\n", "a.fx:3: `palette`: entries are whole numbers"},
      {"effect x\n\neffect y\n burst 1\n", "a.fx:1: the effect `x` throws nothing"},
  };
  for (const auto& f : faults) {
    error.clear();
    CHECK(!pt::parse_effects(f.text, "a.fx", book, error));
    CHECK_MSG(error.starts_with(f.says), error);
    CHECK_MSG(error.find("next:") != std::string::npos, error);
  }
  CHECK(book.effects.size() == 2 && book.find("x") == nullptr);
}

TEST(a_burst_lands_inside_its_shape_at_its_speeds_in_the_game_s_unit_and_repeats_on_its_seed) {
  pt::Book book;
  std::string error;
  CHECK(pt::parse_effects(kText, "a.fx", book, error));
  const pt::Effect& blast = *book.find("blast");
  pt::Throw t;
  t.at[0] = 10;
  t.at[1] = 2;
  t.at[2] = -5;
  t.metres = 0.025;  // a map unit
  t.seed = 7;
  std::vector<uint8_t> palette(768);
  for (size_t i = 0; i < 256; i++) palette[i * 3] = static_cast<uint8_t>(i);  // red = index
  t.palette = palette;
  std::vector<pt::Spark> a, b;
  std::vector<pt::Glow> glows;
  pt::throw_effect(blast, t, a, &glows);
  pt::throw_effect(blast, t, b);
  CHECK(a.size() == 72);
  for (size_t i = 0; i < 64 && i < a.size(); i++) {
    const pt::Spark& s = a[i];
    for (int k = 0; k < 3; k++) CHECK(std::abs(s.p[k] - t.at[k]) <= 16 * 0.025 + 1e-6);
    const double v = std::sqrt(s.v[0] * s.v[0] + s.v[1] * s.v[1] + s.v[2] * s.v[2]);
    CHECK(v >= 100 * 0.025 - 1e-5 && v <= 200 * 0.025 + 1e-5);
    CHECK(s.size >= 2 * 0.025f - 1e-6f && s.size <= 3 * 0.025f + 1e-6f);
    CHECK(s.life >= 0.5f && s.life <= 1);
    CHECK(s.kelvin < 0 && s.fall == 1.0f && s.cool_s == 0.4f && s.albedo == 0.2f);
    // Entries 104..111: red from 104/255 to 111/255, as linear light.
    CHECK(s.tint[0] > 0.13f && s.tint[0] < 0.17f && s.tint[1] == 0);
  }
  for (size_t i = 64; i < a.size(); i++) {
    CHECK(dist(a[i].p, t.at) <= 4 * 0.025 + 1e-6);
    CHECK(a[i].v[1] >= 20 * 0.025f - 1e-6f && a[i].kelvin == 0 && a[i].opacity == 0.6f);
    CHECK(a[i].fall == 0);
  }
  CHECK(a.size() == b.size());
  for (size_t i = 0; i < a.size() && i < b.size(); i++) CHECK_BITS(a[i].p[0], b[i].p[0]);
  CHECK(glows.size() == 1 && glows[0].radius_m == 300 * 0.025 && glows[0].fade_s == 0.5);
  // A shot of four pellets: four times the specks.
  t.times = 4;
  std::vector<pt::Spark> c;
  pt::throw_effect(blast, t, c);
  CHECK(c.size() == size_t{72} * 4);
}

TEST(a_trail_lays_a_speck_every_spacing_along_its_flight) {
  pt::Book book;
  std::string error;
  CHECK(pt::parse_effects(kText, "a.fx", book, error));
  const pt::Effect& trail = *book.find("rocket_trail");
  pt::Throw t;
  t.metres = 0.025;
  const double from[3] = {0, 1, 0}, to[3] = {3, 1, 0};  // 120 units: 40 specks 3 apart
  std::vector<pt::Spark> out;
  pt::throw_trail(trail, from, to, t, out);
  CHECK(out.size() == 40);
  for (const pt::Spark& s : out) {
    CHECK(s.p[0] >= 0 && s.p[0] <= 3 && s.p[1] == 1.0f && s.p[2] == 0);
    CHECK(s.tint[0] == 1.0f && s.tint[1] == 0.5f);
    CHECK(s.fall < 0);  // it rises
  }
  // Its bursts (none) are all a throw at a point gives.
  std::vector<pt::Spark> none;
  pt::throw_effect(trail, t, none);
  CHECK(none.empty());
}
