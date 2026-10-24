// engine/render/grade.hpp: the knobs at their defaults change nothing; every knob holds black and
// white; contrast steepens the middle and keeps middle grey; saturation 0 is grey and keeps the
// luminance; temperature warms (red up, blue down) without moving grey's luminance; gamma above 1
// brightens the middle; the table's entries are each colour through grade_colour.
//
// ge:summary Tests for the grade's knobs: neutral, ends held, contrast, saturation, warmth, gamma
// ge:keywords test render grade knobs contrast saturation temperature gamma lut
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "render/grade.hpp"
#include "render/post.hpp"

namespace {

namespace gr = ge::render;

float lum(const float c[3]) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

TEST(the_defaults_change_nothing) {
  CHECK(gr::grade_neutral({}));
  std::vector<float> t = gr::identity_grade();
  const std::vector<float> before = t;
  gr::apply_grade_knobs(t, {});
  CHECK(t == before);
}

TEST(every_knob_holds_black_and_white) {
  const gr::GradeKnobs turned[] = {{.contrast = 1.6},  {.contrast = 0.6}, {.saturation = 2},
                                   {.temperature = 1}, {.tint = -1},      {.gamma = 1.8}};
  for (const gr::GradeKnobs& k : turned) {
    float black[3] = {0, 0, 0}, white[3] = {1, 1, 1};
    gr::grade_colour(k, black);
    gr::grade_colour(k, white);
    for (int c = 0; c < 3; ++c) {
      CHECK(black[c] < 1e-4f);
      // Warmth moves white's channels apart (a white balance): its luminance stays.
      CHECK(std::fabs(lum(white) - 1.0f) < 0.05f);
    }
  }
}

TEST(contrast_steepens_the_middle_round_middle_grey) {
  float grey[3] = {0.5f, 0.5f, 0.5f}, dark[3] = {0.3f, 0.3f, 0.3f}, light[3] = {0.7f, 0.7f, 0.7f};
  const gr::GradeKnobs k{.contrast = 1.5};
  gr::grade_colour(k, grey);
  gr::grade_colour(k, dark);
  gr::grade_colour(k, light);
  CHECK(std::fabs(grey[0] - 0.5f) < 1e-4f);
  CHECK(dark[0] < 0.3f && light[0] > 0.7f);
}

TEST(saturation_zero_is_grey_at_the_same_luminance) {
  // Luminance is kept in linear light: compare against the encoded grey of the linear luminance.
  float c[3] = {0.8f, 0.4f, 0.2f};
  gr::grade_colour({.saturation = 0}, c);
  CHECK(std::fabs(c[0] - c[1]) < 1e-4f && std::fabs(c[1] - c[2]) < 1e-4f);
  float more[3] = {0.8f, 0.4f, 0.2f};
  gr::grade_colour({.saturation = 1.5}, more);
  CHECK(more[0] > 0.8f && more[2] < 0.2f);
}

TEST(warmth_lifts_red_and_lowers_blue) {
  float c[3] = {0.5f, 0.5f, 0.5f};
  gr::grade_colour({.temperature = 0.5}, c);
  CHECK(c[0] > 0.5f && c[2] < 0.5f);
  float cold[3] = {0.5f, 0.5f, 0.5f};
  gr::grade_colour({.temperature = -0.5}, cold);
  CHECK(cold[0] < 0.5f && cold[2] > 0.5f);
}

TEST(gamma_above_one_brightens_the_middle) {
  float c[3] = {0.4f, 0.4f, 0.4f};
  gr::grade_colour({.gamma = 1.5}, c);
  CHECK(c[0] > 0.4f);
}

TEST(the_table_is_each_entry_through_grade_colour) {
  const gr::GradeKnobs k{.contrast = 1.2, .saturation = 1.1, .temperature = 0.2};
  std::vector<float> t = gr::identity_grade();
  gr::apply_grade_knobs(t, k);
  const std::vector<float> id = gr::identity_grade();
  for (size_t i : {size_t{0}, size_t{3 * 1000}, size_t{3 * 17000}, id.size() - 3}) {
    float c[3] = {id[i], id[i + 1], id[i + 2]};
    gr::grade_colour(k, c);
    CHECK(c[0] == t[i] && c[1] == t[i + 1] && c[2] == t[i + 2]);
  }
}

TEST(the_lens_dirt_is_the_same_for_a_seed_and_differs_between_seeds) {
  const std::vector<uint8_t> a = gr::lens_dirt(96, 54, 1), b = gr::lens_dirt(96, 54, 1);
  const std::vector<uint8_t> c = gr::lens_dirt(96, 54, 2);
  CHECK(a.size() == size_t{96} * 54 * 4);
  CHECK(a == b);
  CHECK(a != c);
  size_t marked = 0;
  for (size_t i = 0; i < a.size(); i += 4) marked += a[i] > 8 ? 1 : 0;
  // Smudges cover some of the lens and leave most of it clear.
  CHECK(marked > a.size() / 4 / 20 && marked < a.size() / 4 * 3 / 4);
}

}  // namespace
