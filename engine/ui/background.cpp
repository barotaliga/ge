// Backdrops, defined: the starts and finishes as layers, the drift's eased framing, and each layer
// drawn as one box over the area.
//
// ge:summary Backdrops defined: layer builders, the drift's framing, layers drawn as boxes
// ge:keywords ui background layer scrim vignette drift framing ease draw
// ge:skill ui
// ge:test tests/ui_background_test.cpp
#include "ui/background.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::ui {

namespace {

Layer paint_layer(const Paint& p) {
  Layer l;
  l.kind = LayerKind::paint;
  l.paint = p;
  return l;
}

// A unit-space paint mapped onto a w x h box.
Paint scaled(Paint p, float w, float h) {
  p.from_x *= w, p.to_x *= w;
  p.from_y *= h, p.to_y *= h;
  if (p.kind == PaintKind::radial) {
    p.radius *= w;
    p.aspect *= h / w;
  }
  return p;
}

}  // namespace

Background Background::color(Color c) {
  Background b;
  b.layers.push_back(paint_layer(solid(c)));
  return b;
}

Background Background::gradient(float degrees, const Stop* stops, int n) {
  Background b;
  b.layers.push_back(paint_layer(linear_at_angle(1, 1, degrees, stops, n)));
  return b;
}

Background Background::gradient(float degrees, Color from, Color to) {
  const Stop s[2] = {{0, from}, {1, to}};
  return gradient(degrees, s, 2);
}

Background Background::image(const Image& img, Fit fit, Drift drift) {
  Background b;
  Layer l;
  l.kind = LayerKind::image;
  l.image = &img;
  l.fit = fit;
  l.drift = drift;
  b.layers.push_back(l);
  return b;
}

Background Background::scene(Drift drift) {
  Background b;
  Layer l;
  l.kind = LayerKind::scene;
  l.drift = drift;
  b.layers.push_back(l);
  return b;
}

Background& Background::paint(const Paint& unit_paint) {
  layers.push_back(paint_layer(unit_paint));
  return *this;
}

Background& Background::glow(float cx, float cy, float radius, Color c) {
  const Stop s[3] = {{0, c}, {0.45f, with_alpha(c, c.a * 0.35f)}, {1, with_alpha(c, 0)}};
  return paint(radial(cx, cy, radius, s, 3));
}

Background& Background::scrim(Side side, float strength, float reach, Color c) {
  // Dark at the side, eased off to nothing `reach` of the way across.
  const Stop s[4] = {{0, with_alpha(c, strength)},
                     {reach * 0.45f, with_alpha(c, strength * 0.7f)},
                     {reach * 0.8f, with_alpha(c, strength * 0.2f)},
                     {reach, with_alpha(c, 0)}};
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  switch (side) {
    case Side::left:
      x1 = 1;
      break;
    case Side::right:
      x0 = 1;
      break;
    case Side::top:
      y1 = 1;
      break;
    case Side::bottom:
      y0 = 1;
      break;
  }
  return paint(linear(x0, y0, x1, y1, s, 4));
}

Background& Background::vignette(float strength, Color c) {
  const Stop s[3] = {{0.45f, with_alpha(c, 0)},
                     {0.85f, with_alpha(c, strength * 0.6f)},
                     {1.2f, with_alpha(c, strength)}};
  return paint(radial(0.5f, 0.5f, 0.75f, s, 3));
}

Background& Background::dim(float amount, Color c) { return paint(solid(with_alpha(c, amount))); }

Background& Background::tint(Color c) {
  for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
    if (it->kind != LayerKind::paint) {
      it->tint = c;
      break;
    }
  }
  return *this;
}

Framing framing_at(const Drift& d, double time_s) {
  if (d.period_s <= 0) return {d.zoom_from, d.focus_from_x, d.focus_from_y};
  // Out and back once a period, easing at both ends: 0 at t = 0, 1 at half the period.
  const auto s = static_cast<float>(0.5 - 0.5 * core::libm::cos(time_s / d.period_s * core::kTau));
  return {d.zoom_from + (d.zoom_to - d.zoom_from) * s,
          d.focus_from_x + (d.focus_to_x - d.focus_from_x) * s,
          d.focus_from_y + (d.focus_to_y - d.focus_from_y) * s};
}

void draw_background(DrawList& list, const Background& bg, Rect area, double time_s,
                     const Image* scene) {
  list.push_clip(area);
  for (const Layer& l : bg.layers) {
    Box b;
    b.rect = area;
    b.feather = 0.5f;  // a backdrop fills its area to the pixel: no soft edge
    if (l.kind == LayerKind::paint) {
      b.fill = scaled(l.paint, area.w, area.h);
      list.box(b);
      continue;
    }
    const Image* img = l.kind == LayerKind::image ? l.image : scene;
    if (img == nullptr || img->empty()) continue;
    const Framing f = framing_at(l.drift, time_s);
    b.fill = solid(l.tint);
    b.image = img;
    if (l.fit == Fit::contain) {
      b.rect = contain_rect(*img, area);
      b.uv = {0, 0, static_cast<float>(img->width), static_cast<float>(img->height)};
    } else {
      b.uv = fit_uv(*img, area.w, area.h, l.fit, f.focus_x, f.focus_y, f.zoom);
    }
    list.box(b);
  }
  list.pop_clip();
}

}  // namespace ge::ui
