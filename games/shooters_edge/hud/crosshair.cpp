// The crosshair's settings, its built marks painted in code and its content each frame.
//
// ge:summary Shooter's Edge's crosshair: the marks painted in code, the colour words, the content
// ge:keywords crosshair crosshaircolor crosshairsize crosshairalpha crosshair-marks
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#include "shooters_edge/hud/crosshair.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "ui/draw.hpp"
#include "ui/raster.hpp"

namespace shooters_edge::hud {

namespace ui = ge::ui;
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

ui::Color crosshair_colour(std::string_view text, const HudArt& art) {
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
  if (art.palette.size() == 768) {
    const uint8_t* p = &art.palette[index * 3];
    return {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, 1};
  }
  const float grey = static_cast<float>(v[0] / 255);
  return {grey, grey, grey, 1};
}

void add_crosshair_cvars(gc::Console& con, CrosshairSettings& s) {
  con.cvars().bind({.name = "crosshair",
                    .fallback = "1",
                    .flags = gc::kArchive,
                    .help = "The crosshair's mark.",
                    .kind = gc::CvarKind::integer,
                    .min = 0,
                    .max = kCrosshairMarks,
                    .values = "0: none; 1: the HUD's own (the default); 2: a dot; 3: a cross; 4: a "
                              "cross open at the middle; 5: a ring; 6: a ring and a dot; 7: an X"},
                   &s.mark);
  con.cvars().bind({.name = "crosshaircolor",
                    .fallback = "",
                    .flags = gc::kArchive,
                    .help = "The crosshair's colour.",
                    .kind = gc::CvarKind::text,
                    .values = "\"\": the mark's own colours (the default); TEXT: one number, a "
                              "colour of the classic palette, or red, green and blue, each 0 to "
                              "255"},
                   &s.colour);
  con.cvars().bind({.name = "crosshairsize",
                    .fallback = "1",
                    .flags = gc::kArchive,
                    .help = "How big the crosshair is.",
                    .min = 0.25,
                    .max = 4,
                    .values = "0.25..4: times its own size (1 the default)"},
                   &s.size);
  con.cvars().bind({.name = "crosshairalpha",
                    .fallback = "1",
                    .flags = gc::kArchive,
                    .help = "How solid the crosshair is.",
                    .min = 0,
                    .max = 1,
                    .values = "0: not seen; 0..1: see-through; 1: solid (the default)"},
                   &s.alpha);
}

void fill_crosshair(ge::hud::Content& c, const CrosshairSettings& s, const HudArt& art, bool dead) {
  const double size = std::clamp(s.size, 0.25, 4.0);
  if (s.mark >= 2 && s.mark <= kCrosshairMarks) {
    c.image = &crosshair_mark(s.mark);
    c.density = static_cast<float>(kMarkDensity / size);
  } else {
    c.image = &art.crosshair;
    c.density = static_cast<float>(art.density / size);
  }
  c.color = crosshair_colour(s.colour, art);
  c.color.a = static_cast<float>(std::clamp(s.alpha, 0.0, 1.0));
  c.on = c.on && !dead && s.mark != 0 && c.color.a > 0;
}

}  // namespace shooters_edge::hud
