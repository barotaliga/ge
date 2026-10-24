// The 2D layer's CPU raster, defined: a box's coverage from its rounded rectangle's signed
// distance (in its own turned frame), its fill (paint times image, whole or nine-sliced), its
// border; a segment's, an arc's and a polygon's coverage from theirs; a text run's glyphs; each
// clipped and blended over the target.
//
// ge:summary The CPU raster defined: box, segment, arc and polygon coverage, glyph blits, blending
// ge:keywords ui raster coverage signed-distance rounded border glyph blend clip capsule
// ge:keywords annular-sector convex-polygon rotation nine-slice
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#include "ui/raster.hpp"

#include <algorithm>
#include <cmath>

#include "ui/font.hpp"

namespace ge::ui {

namespace {

uint8_t to_byte(float v) {
  return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}

// The signed distance from (px, py) to a box of half-size (hw, hh) with corner radius r,
// centred at the origin: negative inside.
float rounded_distance(float px, float py, float hw, float hh, float r) {
  const float qx = std::fabs(px) - hw + r, qy = std::fabs(py) - hh + r;
  const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
  return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

float coverage(float d, float feather) { return std::clamp(0.5f - d / feather, 0.0f, 1.0f); }

// Every pixel whose centre is inside `reach` and `clip`: `each(px, py, pixel)`.
template <class F>
void for_pixels(Rect reach, Rect clip, Image& target, F each) {
  reach = intersect(intersect(reach, clip),
                    {0, 0, static_cast<float>(target.width), static_cast<float>(target.height)});
  if (reach.w <= 0 || reach.h <= 0) return;
  const int x0 = static_cast<int>(std::floor(reach.x)),
            x1 = static_cast<int>(std::ceil(reach.right()));
  const int y0 = static_cast<int>(std::floor(reach.y)),
            y1 = static_cast<int>(std::ceil(reach.bottom()));
  for (int y = y0; y < y1; y++) {
    const float py = static_cast<float>(y) + 0.5f;
    if (py < clip.y || py >= clip.bottom()) continue;
    uint8_t* row = target.rgba.data() + static_cast<size_t>(y) * target.width * 4;
    for (int x = x0; x < x1; x++) {
      const float px = static_cast<float>(x) + 0.5f;
      if (px < clip.x || px >= clip.right()) continue;
      each(px, py, row + static_cast<size_t>(x) * 4);
    }
  }
}

void draw_box(const Box& b, Rect clip, Image& target) {
  if (b.rect.w <= 0 || b.rect.h <= 0 || b.opacity <= 0) return;
  const float feather = std::max(b.feather, 0.5f);
  const Turn turn = turn_of(b);
  const float hw = b.rect.w / 2, hh = b.rect.h / 2;
  const float r = std::clamp(b.radius, 0.0f, std::min(hw, hh));
  const bool sampled = b.image != nullptr && !b.image->empty();
  const bool sliced = sampled && !b.slice.empty();
  const Insets to = b.slice_to.empty() ? b.slice : b.slice_to;
  const float su = sampled ? b.uv.w / b.rect.w : 0, sv = sampled ? b.uv.h / b.rect.h : 0;
  for_pixels(turned_bounds(turn, b, feather), clip, target, [&](float px, float py, uint8_t* at) {
    const Vec2 l = unturn(turn, b, {px, py});
    const float d = rounded_distance(l.x - hw, l.y - hh, hw, hh, r);
    const float outer = coverage(d, feather);
    if (outer <= 0) return;
    const float inner = b.border > 0 ? coverage(d + b.border, feather) : outer;
    if (inner > 0) {
      Color c = b.fill.at(l.x, l.y);
      if (sampled) {
        const float u = sliced ? slice_map(l.x, b.rect.w, to.left, to.right, b.uv.x, b.uv.w,
                                           b.slice.left, b.slice.right)
                               : b.uv.x + l.x * su;
        const float v = sliced ? slice_map(l.y, b.rect.h, to.top, to.bottom, b.uv.y, b.uv.h,
                                           b.slice.top, b.slice.bottom)
                               : b.uv.y + l.y * sv;
        const Color s = sample(*b.image, u, v);
        c = {c.r * s.r, c.g * s.g, c.b * s.b, c.a * s.a};
      }
      c.a *= inner * b.opacity;
      blend_over(at, c);
    }
    if (b.border > 0 && outer > inner) {
      Color c = b.border_color;
      c.a *= (outer - inner) * b.opacity;
      blend_over(at, c);
    }
  });
}

float segment_distance(Vec2 p, Vec2 a, Vec2 b) {
  const float pax = p.x - a.x, pay = p.y - a.y, bax = b.x - a.x, bay = b.y - a.y;
  const float len2 = bax * bax + bay * bay;
  const float h = len2 > 0 ? std::clamp((pax * bax + pay * bay) / len2, 0.0f, 1.0f) : 0.0f;
  const float dx = pax - bax * h, dy = pay - bay * h;
  return std::sqrt(dx * dx + dy * dy);
}

void draw_segment(const Segment& s, Rect clip, Image& target) {
  const float feather = std::max(s.feather, 0.5f);
  const float grow = s.half + feather;
  const Rect reach{std::min(s.a.x, s.b.x) - grow, std::min(s.a.y, s.b.y) - grow,
                   std::fabs(s.b.x - s.a.x) + 2 * grow, std::fabs(s.b.y - s.a.y) + 2 * grow};
  for_pixels(reach, clip, target, [&](float px, float py, uint8_t* at) {
    const float cov = coverage(segment_distance({px, py}, s.a, s.b) - s.half, feather);
    if (cov <= 0) return;
    Color c = s.color;
    c.a *= cov * s.opacity;
    blend_over(at, c);
  });
}

}  // namespace

float arc_distance(const Arc& a, float px, float py) {
  const float dx = px - a.centre.x, dy = py - a.centre.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  const bool inside =
      len > 0 ? (dx * a.dir_mid.x + dy * a.dir_mid.y) >= a.cos_half * len : a.cos_half < -1;
  if (inside) return std::fabs(len - a.radius) - a.half;
  const Vec2 p{px, py};
  if (a.round) {
    const Vec2 e0{a.centre.x + a.dir_from.x * a.radius, a.centre.y + a.dir_from.y * a.radius};
    const Vec2 e1{a.centre.x + a.dir_to.x * a.radius, a.centre.y + a.dir_to.y * a.radius};
    return std::min(segment_distance(p, e0, e0), segment_distance(p, e1, e1)) - a.half;
  }
  const float r0 = a.radius - a.half, r1 = a.radius + a.half;
  const Vec2 f0{a.centre.x + a.dir_from.x * r0, a.centre.y + a.dir_from.y * r0};
  const Vec2 f1{a.centre.x + a.dir_from.x * r1, a.centre.y + a.dir_from.y * r1};
  const Vec2 t0{a.centre.x + a.dir_to.x * r0, a.centre.y + a.dir_to.y * r0};
  const Vec2 t1{a.centre.x + a.dir_to.x * r1, a.centre.y + a.dir_to.y * r1};
  return std::min(segment_distance(p, f0, f1), segment_distance(p, t0, t1));
}

float polygon_distance(const PolyPoint* pts, uint32_t n, float px, float py) {
  float d = -1e30f;
  for (uint32_t i = 0; i < n; ++i) {
    d = std::max(d, (px - pts[i].at.x) * pts[i].normal.x + (py - pts[i].at.y) * pts[i].normal.y);
  }
  return d;
}

namespace {

void draw_arc(const Arc& a, Rect clip, Image& target) {
  const float feather = std::max(a.feather, 0.5f);
  const float out = a.radius + a.half;
  const Rect frame{a.centre.x - out, a.centre.y - out, 2 * out, 2 * out};
  for_pixels(inset(frame, feather), clip, target, [&](float px, float py, uint8_t* at) {
    const float cov = coverage(arc_distance(a, px, py), feather);
    if (cov <= 0) return;
    Color c = a.fill.at(px - frame.x, py - frame.y);
    c.a *= cov * a.opacity;
    blend_over(at, c);
  });
}

void draw_polygon(const Polygon& g, const std::vector<PolyPoint>& points, Rect clip,
                  Image& target) {
  if (g.first + g.count > points.size()) return;
  const float feather = std::max(g.feather, 0.5f);
  const PolyPoint* pts = points.data() + g.first;
  for_pixels(inset(g.bounds, feather), clip, target, [&](float px, float py, uint8_t* at) {
    const float cov = coverage(polygon_distance(pts, g.count, px, py), feather);
    if (cov <= 0) return;
    Color c = g.fill.at(px - g.bounds.x, py - g.bounds.y);
    c.a *= cov * g.opacity;
    blend_over(at, c);
  });
}

void draw_text(const TextRun& t, Rect clip, Image& target) {
  if (t.font == nullptr || !t.font->loaded() || t.color.a <= 0) return;
  const Rect bounds =
      intersect(clip, {0, 0, static_cast<float>(target.width), static_cast<float>(target.height)});
  const int cx0 = static_cast<int>(std::ceil(bounds.x - 0.5f)),
            cx1 = static_cast<int>(std::ceil(bounds.right() - 0.5f));
  const int cy0 = static_cast<int>(std::ceil(bounds.y - 0.5f)),
            cy1 = static_cast<int>(std::ceil(bounds.bottom() - 0.5f));
  place_glyphs(*t.font, t.text, t.size, t.x, t.baseline, t.tracking,
               [&](const GlyphMask& m, int ox, int oy) {
                 for (int y = 0; y < m.h; y++) {
                   const int ty = oy + y;
                   if (ty < cy0 || ty >= cy1) continue;
                   uint8_t* row = target.rgba.data() + static_cast<size_t>(ty) * target.width * 4;
                   for (int x = 0; x < m.w; x++) {
                     const int tx = ox + x;
                     if (tx < cx0 || tx >= cx1) continue;
                     const uint8_t a = m.alpha[static_cast<size_t>(y) * m.w + x];
                     if (a == 0) continue;
                     Color c = t.color;
                     c.a *= static_cast<float>(a) / 255.0f;
                     blend_over(row + static_cast<size_t>(tx) * 4, c);
                   }
                 }
               });
}

}  // namespace

void blend_over(uint8_t* dst, Color src) {
  const float sa = std::clamp(src.a, 0.0f, 1.0f);
  if (sa <= 0) return;
  const float da = static_cast<float>(dst[3]) / 255.0f;
  const float oa = sa + da * (1 - sa);
  const float keep = da * (1 - sa);
  const float s[3] = {src.r, src.g, src.b};
  for (int k = 0; k < 3; k++) {
    const float d = static_cast<float>(dst[k]) / 255.0f;
    dst[k] = to_byte((s[k] * sa + d * keep) / oa);
  }
  dst[3] = to_byte(oa);
}

void render(const DrawList& list, Image& target) {
  if (target.empty()) return;
  for (const Cmd& c : list.cmds()) {
    switch (c.kind) {
      case CmdKind::box:
        draw_box(c.box, c.clip, target);
        break;
      case CmdKind::text:
        draw_text(c.text, c.clip, target);
        break;
      case CmdKind::segment:
        draw_segment(c.segment, c.clip, target);
        break;
      case CmdKind::arc:
        draw_arc(c.arc, c.clip, target);
        break;
      case CmdKind::polygon:
        draw_polygon(c.polygon, list.points(), c.clip, target);
        break;
    }
  }
}

}  // namespace ge::ui
