// The instruments defined: the dial's sweep and its knee, the needle's spring, the heading-up
// turn, and each instrument drawn with the 2D layer's arcs, lines and polygons.
//
// ge:summary The HUD's instruments defined: dial angles, the eased needle, the three drawn
// ge:keywords hud instruments dial_angle ease_needle heading_up draw dial wind compass needle
// ge:keywords arc polygon tick figure knee red-band gale calm cardinal
// ge:skill ui
// ge:test tests/hud_instruments_test.cpp
#include "hud/instruments.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::hud {

namespace {

namespace libm = core::libm;

// A point `r` px from `c` at the draw list's angle `a`.
ui::Vec2 at_angle(ui::Vec2 c, float r, double a) {
  return {c.x + r * static_cast<float>(libm::sin(a)), c.y - r * static_cast<float>(libm::cos(a))};
}

ui::Vec2 centre_of(ui::Rect r, float size) { return {r.x + size / 2, r.y + size / 2}; }

void disc(ui::DrawList& dl, ui::Vec2 c, float r, ui::Color color) {
  dl.rect({c.x - r, c.y - r, 2 * r, 2 * r}, color, r);
}

// An arrowhead (or a needle) as a triangle: its tip `tip` px out along `a` from `base`, `half`
// px either side at the base.
void triangle(ui::DrawList& dl, ui::Vec2 base, double a, float tip, float half, ui::Color color) {
  const ui::Vec2 t = at_angle(base, tip, a);
  const ui::Vec2 l = at_angle(base, half, a - core::kPi / 2);
  const ui::Vec2 r = at_angle(base, half, a + core::kPi / 2);
  const std::array<ui::Vec2, 3> pts{t, r, l};
  dl.polygon(pts, ui::solid(color));
}

float line_h(const Face& face, float size) { return ascent(face, size) + descent(face, size); }

// The size a reading's figure is drawn at under a wind meter, a share of the meter's.
constexpr float kWindFigure = 0.16f;

}  // namespace

double dial_angle(const Dial& d, double v) {
  const double span = d.max - d.min;
  if (span <= 0) return d.from;
  v = core::clamp(v, d.min, d.max);
  double u;
  if (d.knee > d.min && d.knee < d.max) {
    u = v <= d.knee ? d.knee_share * (v - d.min) / (d.knee - d.min)
                    : d.knee_share + (1 - d.knee_share) * (v - d.knee) / (d.max - d.knee);
  } else {
    u = (v - d.min) / span;
  }
  return d.from + u * (static_cast<double>(d.to) - d.from);
}

double ease_needle(Needle& n, double target, double t, double settle) {
  const double dt = t - n.t;
  if (n.t < 0 || settle <= 0 || dt < 0 || dt > 0.5) {
    n.at = target;
    n.rate = 0;
  } else if (dt > 0) {
    // The critically damped spring's own solution over the step, so no step size makes it
    // ring: x(t) = (x0 + (v0 + w x0) t) e^-wt about the target. About 6 / w settles it to 2 %.
    const double w = 6 / settle;
    const double x0 = n.at - target, v0 = n.rate;
    const double e = libm::exp(-w * dt);
    const double k = v0 + w * x0;
    n.at = target + (x0 + k * dt) * e;
    n.rate = (v0 - w * k * dt) * e;
  }
  n.t = t;
  return n.at;
}

Size measure(const Frame& f, const Dial& d) { return {f.px(d.size), f.px(d.size)}; }

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Dial& d, const DialReading& r,
          ui::Rect at) {
  const float s = f.px(d.size), rad = s / 2;
  const ui::Vec2 c = centre_of(at, s);
  disc(dl, c, rad, st.plate);
  const float ring = rad * 0.86f, ring_w = rad * 0.06f;
  dl.arc(c, ring, ring_w, d.from, d.to, ui::solid(st.track));
  if (d.red > d.min && d.red < d.max)
    dl.arc(c, ring, ring_w, static_cast<float>(dial_angle(d, d.red)), d.to, ui::solid(st.bad));
  if (d.major > 0) {
    const int majors = static_cast<int>(std::floor((d.max - d.min) / d.major + 1e-9));
    const int parts = std::max(d.minor, 0) + 1;
    const float fs = s * 0.11f;
    const float asc = ascent(st.caption_face, fs) - descent(st.caption_face, fs);
    for (int i = 0; i <= majors * parts; ++i) {
      const double v = d.min + d.major * i / parts;
      if (v > d.max) break;
      const double a = dial_angle(d, v);
      const bool major = i % parts == 0;
      dl.line(at_angle(c, major ? rad * 0.76f : rad * 0.82f, a), at_angle(c, rad * 0.92f, a),
              f.px(major ? 3.0f : 1.5f), st.ink);
      if (major && d.figures > 0) {
        const ui::Vec2 p = at_angle(c, rad * 0.6f, a);
        draw_text(dl, st.caption_face, fs, p.x, p.y + asc / 2,
                  core::num::to_string(std::round(v / d.figures)), st.caption, ui::Align::center);
      }
    }
  }
  // The reading in the gap the sweep leaves at the bottom, under the figures' ring.
  const float fs = s * 0.15f, us = s * 0.065f, drop = f.px(st.shadow_drop);
  const float base = c.y + rad * 0.66f;
  draw_shadowed(dl, st.value_face, fs, c.x, base, r.figure, r.color, st.shadow, drop,
                ui::Align::center);
  if (!r.unit.empty())
    draw_text(dl, st.caption_face, us, c.x, base + line_h(st.caption_face, us), r.unit, st.caption,
              ui::Align::center, f.px(st.caption_tracking));
  // The needle: a long thin triangle from a short tail through the hub.
  const double a = dial_angle(d, r.needle);
  const ui::Vec2 tail = at_angle(c, rad * 0.16f, a + core::kPi);
  triangle(dl, tail, a, rad * 1.0f, s * 0.028f, r.color);
  disc(dl, c, s * 0.05f, r.color);
  disc(dl, c, s * 0.022f, st.plate);
}

double heading_up(double heading, double x, double z) {
  const double sh = libm::sin(heading), ch = libm::cos(heading);
  const double ahead = x * sh + z * ch, right = x * ch - z * sh;
  return libm::atan2(right, ahead);
}

Size measure(const Frame& f, const Style& st, const WindGauge& g) {
  const float s = f.px(g.size);
  return {s, s + f.px(4) + line_h(st.caption_face, s * kWindFigure)};
}

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const WindGauge& g,
          const WindReading& r, ui::Rect at) {
  const float s = f.px(g.size), rad = s / 2;
  const ui::Vec2 c = centre_of(at, s);
  disc(dl, c, rad, st.plate);
  dl.arc(c, rad * 0.9f, f.px(2), 0, static_cast<float>(core::kTau), ui::solid(st.track));
  // The quarters, heading-up: a notch ahead, ticks right, behind and left.
  triangle(dl, at_angle(c, rad * 0.98f, 0), core::kPi, rad * 0.16f, rad * 0.09f, st.ink);
  for (const double q : {core::kPi / 2, core::kPi, core::kPi * 1.5})
    dl.line(at_angle(c, rad * 0.8f, q), at_angle(c, rad * 0.94f, q), f.px(2), st.track);
  if (r.weather && (r.weather_x != 0 || r.weather_z != 0)) {
    // On the rim on the side it blows FROM, pointing in.
    const double from = heading_up(r.heading, -r.weather_x, -r.weather_z);
    triangle(dl, at_angle(c, rad * 0.97f, from), from + core::kPi, rad * 0.24f, rad * 0.1f,
             st.caption);
  }
  const double speed = core::hypot(r.felt_x, r.felt_z);
  // A gale is the weather's when it is given: his own speed is no alarm.
  const bool gale = (r.weather ? core::hypot(r.weather_x, r.weather_z) : speed) > g.gale;
  const ui::Color ink = gale ? st.bad : r.color;
  if (speed >= g.calm) {
    const double a = heading_up(r.heading, r.felt_x, r.felt_z);
    const float len =
        rad * 1.1f * static_cast<float>(core::clamp(g.full > 0 ? speed / g.full : 1, 0.35, 1.0));
    const float head = s * 0.12f;
    const ui::Vec2 tail = at_angle(c, len / 2, a + core::kPi);
    const ui::Vec2 neck = at_angle(c, len / 2 - head, a);
    dl.line(tail, neck, s * 0.05f, ink);
    triangle(dl, neck, a, head, s * 0.08f, ink);
  } else {
    disc(dl, c, s * 0.03f, st.track);
  }
  const float fs = s * kWindFigure;
  std::string said = r.figure;
  if (!r.unit.empty()) said += " " + r.unit;
  draw_shadowed(dl, st.caption_face, fs, c.x, at.y + s + f.px(4) + ascent(st.caption_face, fs),
                said, ink, st.shadow, f.px(st.shadow_drop), ui::Align::center,
                f.px(st.caption_tracking));
}

Size measure(const Frame& f, const Compass& c) { return {f.px(c.w), f.px(c.h)}; }

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Compass& c,
          const CompassReading& r, ui::Rect at) {
  dl.rect(at, st.plate, f.px(6));
  if (c.span <= 0 || c.step <= 0) return;
  const float mid = at.x + at.w / 2;
  const double half = c.span / 2;
  auto x_of = [&](double bearing) {
    return mid + static_cast<float>(core::angle_diff(r.heading, bearing) / half) * at.w / 2;
  };
  dl.push_clip(at);
  const int quarter = static_cast<int>(std::lround(core::kPi / 2 / c.step));
  const auto lo = static_cast<int>(std::floor((r.heading - half) / c.step));
  const auto hi = static_cast<int>(std::ceil((r.heading + half) / c.step));
  const float ls = at.h * 0.36f;
  static constexpr std::array<const char*, 4> kCardinal = {"N", "E", "S", "W"};
  for (int k = lo; k <= hi; ++k) {
    const double b = k * c.step;
    const float x = x_of(b);
    const bool cardinal = quarter > 0 && k % quarter == 0;
    // The ticks hang from the top, under the notch; the cardinal letters stand at the bottom.
    const float low = at.y + at.h * (cardinal ? 0.36f : 0.2f);
    dl.line({x, at.y + f.px(3)}, {x, low}, f.px(cardinal ? 2.5f : 1.5f),
            cardinal ? st.ink : st.track);
    if (cardinal) {
      const int q = ((k / quarter) % 4 + 4) % 4;
      draw_text(dl, st.caption_face, ls, x, at.bottom() - f.px(3) - descent(st.caption_face, ls),
                kCardinal[static_cast<size_t>(q)], st.ink, ui::Align::center);
    }
  }
  for (const CompassMark& m : r.marks) {
    const float x = std::clamp(x_of(m.bearing), at.x + f.px(6), at.right() - f.px(6));
    const float y = at.y + at.h * 0.5f, w = f.px(6);
    const std::array<ui::Vec2, 4> diamond{ui::Vec2{x, y - w}, ui::Vec2{x + w, y},
                                          ui::Vec2{x, y + w}, ui::Vec2{x - w, y}};
    dl.polygon(diamond, ui::solid(m.color));
  }
  dl.pop_clip();
  triangle(dl, {mid, at.y}, core::kPi, at.h * 0.22f, at.h * 0.14f, r.color);
}

}  // namespace ge::hud
