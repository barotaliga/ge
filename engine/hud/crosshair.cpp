// The crosshair's settings, its built marks painted in code and the picture a screen draws.
//
// ge:summary The crosshair: the marks painted in code, the colour words, the cvars, the picture
// ge:keywords crosshair crosshaircolor crosshairsize crosshairalpha crosshair-marks
// ge:skill ui
// ge:test tests/hud_crosshair_test.cpp
#include "hud/crosshair.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "ui/draw.hpp"
#include "ui/raster.hpp"

namespace ge::hud {

namespace gc = ge::console;

namespace {

constexpr float kMarkArt = 16;  // a mark's art pixels a side

ui::Image paint_mark(int32_t mark) {
  const float d = kMarkDensity, mid = kMarkArt / 2 * d;
  ui::DrawList dl;
  const int side = static_cast<int>(kMarkArt * d);
  dl.begin(side, side);
  const ui::Color ink{1, 1, 1, 1};
  const ui::Color rim{0, 0, 0, 0.55f};
  // Each stroke twice: a dark rim under it, so the mark reads on a bright wall as on a dark one.
  const auto stroke = [&](float x0, float y0, float x1, float y1) {
    dl.line({mid + x0 * d, mid + y0 * d}, {mid + x1 * d, mid + y1 * d}, 2.2f * d, rim);
    dl.line({mid + x0 * d, mid + y0 * d}, {mid + x1 * d, mid + y1 * d}, 1.0f * d, ink);
  };
  const auto dot = [&](float r) {
    ui::Box b;
    b.rect = {mid - (r + 0.6f) * d, mid - (r + 0.6f) * d, 2 * (r + 0.6f) * d, 2 * (r + 0.6f) * d};
    b.radius = (r + 0.6f) * d;
    b.fill = ui::solid(rim);
    dl.box(b);
    b.rect = {mid - r * d, mid - r * d, 2 * r * d, 2 * r * d};
    b.radius = r * d;
    b.fill = ui::solid(ink);
    dl.box(b);
  };
  const auto ring = [&](float r) {
    dl.arc({mid, mid}, r * d, 2.2f * d, 0, 6.2831853f, ui::solid(rim));
    dl.arc({mid, mid}, r * d, 1.0f * d, 0, 6.2831853f, ui::solid(ink));
  };
  switch (mark) {
    case 2:
      dot(1.2f);
      break;
    case 3:
      stroke(0, -4, 0, 4);
      stroke(-4, 0, 4, 0);
      break;
    case 4:
      stroke(0, -6, 0, -2);
      stroke(0, 2, 0, 6);
      stroke(-6, 0, -2, 0);
      stroke(2, 0, 6, 0);
      break;
    case 5:
      ring(4.5f);
      break;
    case 6:
      ring(4.5f);
      dot(0.9f);
      break;
    case 7:
      stroke(-3.5f, -3.5f, 3.5f, 3.5f);
      stroke(-3.5f, 3.5f, 3.5f, -3.5f);
      break;
    default:
      return {};
  }
  ui::Image img = ui::make_image(side, side, {0, 0, 0, 0});
  ui::render(dl, img);
  return img;
}

}  // namespace

const ui::Image& crosshair_mark(int32_t mark) {
  static const std::array<ui::Image, kCrosshairMarks + 1> marks = [] {
    std::array<ui::Image, kCrosshairMarks + 1> m;
    for (int32_t k = 2; k <= kCrosshairMarks; ++k) m[static_cast<size_t>(k)] = paint_mark(k);
    return m;
  }();
  static const ui::Image none;
  return mark >= 2 && mark <= kCrosshairMarks ? marks[static_cast<size_t>(mark)] : none;
}

ui::Color crosshair_colour(std::string_view text, std::span<const uint8_t> palette) {
  if (text.find_first_not_of(" \t") == std::string_view::npos) return {1, 1, 1, 1};
  double v[3];
  size_t n = 0, i = 0;
  while (i < text.size()) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i == text.size()) break;
    size_t end = i;
    while (end < text.size() && text[end] != ' ' && text[end] != '\t') ++end;
    if (n == 3) return {1, 1, 1, 1};
    const std::string word(text.substr(i, end - i));
    char* stop = nullptr;
    const double x = std::strtod(word.c_str(), &stop);
    if (stop == word.c_str() || *stop != '\0' || !(x >= 0)) return {1, 1, 1, 1};
    v[n++] = std::min(x, 255.0);
    i = end;
  }
  if (n == 3)
    return {static_cast<float>(v[0] / 255), static_cast<float>(v[1] / 255),
            static_cast<float>(v[2] / 255), 1};
  if (n != 1) return {1, 1, 1, 1};
  const auto index = static_cast<size_t>(v[0]);
  if (palette.size() == 768) {
    const uint8_t* p = &palette[std::min<size_t>(index, 255) * 3];
    return {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, 1};
  }
  const float grey = static_cast<float>(v[0] / 255);
  return {grey, grey, grey, 1};
}

void add_crosshair_cvars(gc::Cvars& cvars, CrosshairSettings& s) {
  // The registry copies the fallback (console/cvar.hpp's Cvar).
  const std::string mark = std::to_string(s.mark), size = gc::number_text(s.size),
                    alpha = gc::number_text(s.alpha);
  cvars.bind({.name = "crosshair",
              .fallback = mark,
              .flags = gc::kArchive,
              .help = "The crosshair's mark.",
              .kind = gc::CvarKind::integer,
              .min = 0,
              .max = kCrosshairMarks,
              .values = "0: none; 1: the game's own; 2: a dot; 3: a cross; 4: a cross open at the "
                        "middle; 5: a ring; 6: a ring and a dot; 7: an X"},
             &s.mark);
  cvars.bind({.name = "crosshaircolor",
              .fallback = s.colour,
              .flags = gc::kArchive,
              .help = "The crosshair's colour.",
              .kind = gc::CvarKind::text,
              .values = "\"\": the mark's own colours; TEXT: one number, a colour of the game's "
                        "palette, or red, green and blue, each 0 to 255"},
             &s.colour);
  cvars.bind({.name = "crosshairsize",
              .fallback = size,
              .flags = gc::kArchive,
              .help = "How big the crosshair is.",
              .min = 0.25,
              .max = 4,
              .values = "0.25..4: times its own size"},
             &s.size);
  cvars.bind({.name = "crosshairalpha",
              .fallback = alpha,
              .flags = gc::kArchive,
              .help = "How solid the crosshair is.",
              .min = 0,
              .max = 1,
              .values = "0: not seen; 0..1: see-through; 1: solid"},
             &s.alpha);
  const char* names[2] = {"cl_crossx", "cl_crossy"};
  const char* helps[2] = {"Moves the crosshair right, in the HUD's pixels.",
                          "Moves the crosshair down, in the HUD's pixels."};
  for (int k = 0; k < 2; ++k)
    cvars.bind(
        {.name = names[k],
         .fallback = gc::number_text(s.offset[k]),
         .flags = gc::kArchive,
         .help = helps[k],
         .min = -1000,
         .max = 1000,
         .values = k == 0
                       ? "0: the middle (the default); -1000..1000: that many pixels, negative left"
                       : "0: the middle (the default); -1000..1000: that many pixels, negative up"},
        &s.offset[k]);
}

ui::Color crosshair_health(int health) {
  if (health <= 0) return {0, 0, 0, 1};
  const auto ramp = [](int h, int from) {
    return static_cast<float>(std::clamp(h - from, 0, 33)) / 33.0f;
  };
  return {1, ramp(health, 33), ramp(health, 66), 1};
}

Picture crosshair_picture(const CrosshairSettings& s, const ui::Image& own, float own_density,
                          std::span<const uint8_t> palette, bool* shown, int health) {
  const double size = std::clamp(s.size, 0.25, 4.0);
  Picture p;
  if (!s.pictures && s.mark >= 2 && s.mark <= kCrosshairMarks) {
    p.image = &crosshair_mark(s.mark);
    p.density = static_cast<float>(kMarkDensity / size);
  } else {
    p.image = &own;
    p.density = static_cast<float>(own_density / size);
  }
  p.tint = s.by_health ? crosshair_health(health) : crosshair_colour(s.colour, palette);
  p.tint.r *= static_cast<float>(std::clamp(s.shade[0], 0.0, 1.0));
  p.tint.g *= static_cast<float>(std::clamp(s.shade[1], 0.0, 1.0));
  p.tint.b *= static_cast<float>(std::clamp(s.shade[2], 0.0, 1.0));
  p.tint.a = static_cast<float>(std::clamp(s.alpha, 0.0, 1.0));
  *shown = s.mark != 0 && p.tint.a > 0 && p.image->width > 0;
  return p;
}

}  // namespace ge::hud
