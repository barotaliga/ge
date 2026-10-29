// The 2D draw list, defined: commands appended with the clip in force, text aligned by its
// measured advance, a box's turn, an arc's ends and a polygon's edge normals worked once a command.
//
// ge:summary The 2D draw list defined: boxes, text, segments, arcs and polygons under the clips
// ge:keywords ui draw draw-list clip push-clip text align measure rotation nine-slice arc polygon
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#include "ui/draw.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/libm/libm.hpp"
#include "core/output.hpp"
#include "ui/font.hpp"

namespace ge::ui {

namespace {

constexpr double kTwoPi = 6.283185307179586;

// The unit direction at angle `a` (radians, 0 at twelve o'clock, clockwise, y down).
Vec2 direction(double a) {
  return {static_cast<float>(core::libm::sin(a)), static_cast<float>(-core::libm::cos(a))};
}

}  // namespace

Turn turn_of(const Box& b) {
  Turn t;
  const Vec2 local = b.pivot ? *b.pivot : Vec2{b.rect.w / 2, b.rect.h / 2};
  t.pivot = {b.rect.x + local.x, b.rect.y + local.y};
  if (b.rotation == 0) return t;
  t.cos = static_cast<float>(core::libm::cos(b.rotation));
  t.sin = static_cast<float>(core::libm::sin(b.rotation));
  t.turned = true;
  return t;
}

Rect turned_bounds(const Turn& t, const Box& b, float grow) {
  if (!t.turned) return inset(b.rect, grow);
  float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
  for (const Vec2 c : {Vec2{b.rect.x, b.rect.y}, Vec2{b.rect.right(), b.rect.y},
                       Vec2{b.rect.x, b.rect.bottom()}, Vec2{b.rect.right(), b.rect.bottom()}}) {
    const float dx = c.x - t.pivot.x, dy = c.y - t.pivot.y;
    const float x = t.pivot.x + dx * t.cos - dy * t.sin, y = t.pivot.y + dx * t.sin + dy * t.cos;
    x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
  }
  return {x0 - grow, y0 - grow, x1 - x0 + 2 * grow, y1 - y0 + 2 * grow};
}

float slice_map(float l, float size, float d0, float d1, float uv_from, float uv_size, float s0,
                float s1) {
  if (l < d0) return uv_from + l * (d0 > 0 ? s0 / d0 : 0);
  if (l > size - d1) return uv_from + uv_size - (size - l) * (d1 > 0 ? s1 / d1 : 0);
  const float mid = size - d0 - d1;
  return uv_from + s0 + (mid > 0 ? (l - d0) * (uv_size - s0 - s1) / mid : 0);
}

Cmd DrawList::make_cmd(CmdKind kind) const {
  Cmd c;
  c.kind = kind;
  c.clip = clip();
  return c;
}

void DrawList::begin(int width, int height) {
  width_ = width;
  height_ = height;
  cmds_.clear();
  clips_.clear();
  points_.clear();
}

void DrawList::box(const Box& b) {
  Cmd c = make_cmd(CmdKind::box);
  c.box = b;
  cmds_.push_back(std::move(c));
}

void DrawList::rect(Rect r, Color c, float radius) {
  Box b;
  b.rect = r;
  b.radius = radius;
  b.fill = solid(c);
  box(b);
}

float DrawList::text(const Font& font, float size, float x, float baseline, std::string_view text,
                     Color c, Align align, float tracking) {
  const float w = font.advance(text, size, tracking);
  if (align == Align::center) x -= w / 2;
  if (align == Align::right) x -= w;
  Cmd cmd = make_cmd(CmdKind::text);
  cmd.text = {&font, size, x, baseline, std::string(text), c, tracking};
  cmds_.push_back(std::move(cmd));
  return w;
}

void DrawList::line(Vec2 a, Vec2 b, float width, Color c, float feather, float opacity) {
  if (width <= 0 || opacity <= 0) return;
  Cmd cmd = make_cmd(CmdKind::segment);
  cmd.segment = {a, b, width / 2, c, feather, opacity};
  cmds_.push_back(std::move(cmd));
}

void DrawList::polyline(std::span<const Vec2> pts, float width, Color c, bool closed, float feather,
                        float opacity) {
  for (size_t i = 1; i < pts.size(); ++i) line(pts[i - 1], pts[i], width, c, feather, opacity);
  if (closed && pts.size() > 2) line(pts.back(), pts.front(), width, c, feather, opacity);
}

void DrawList::arc(Vec2 centre, float radius, float width, float from, float to, const Paint& fill,
                   bool round, float feather, float opacity) {
  if (width <= 0 || radius <= 0 || opacity <= 0 || to <= from) return;
  Cmd cmd = make_cmd(CmdKind::arc);
  Arc& a = cmd.arc;
  a.centre = centre, a.radius = radius, a.half = width / 2;
  a.from = from, a.to = to, a.round = round;
  a.fill = fill, a.feather = feather, a.opacity = opacity;
  const double sweep = std::min(static_cast<double>(to) - from, kTwoPi);
  a.dir_from = direction(from);
  a.dir_to = direction(from + sweep);
  a.dir_mid = direction(from + sweep / 2);
  a.cos_half = sweep >= kTwoPi ? -2.0f : static_cast<float>(core::libm::cos(sweep / 2));
  cmds_.push_back(std::move(cmd));
}

void DrawList::polygon(std::span<const Vec2> pts, const Paint& fill, float feather, float opacity) {
  if (pts.size() < 3 || opacity <= 0) return;
  if (pts.size() > static_cast<size_t>(kMaxPolygon)) {
    core::output::error("ui: DrawList::polygon was handed " + std::to_string(pts.size()) +
                        " points, over kMaxPolygon (" + std::to_string(kMaxPolygon) +
                        "); it is not drawn. next: split the shape into convex polygons of at "
                        "most that many points (ui/draw.hpp); skill: ui.");
    return;
  }
  // Clockwise on the screen (y down) is a positive shoelace sum.
  float area = 0;
  for (size_t i = 0; i < pts.size(); ++i) {
    const Vec2 p = pts[i], q = pts[(i + 1) % pts.size()];
    area += p.x * q.y - q.x * p.y;
  }
  if (area == 0) return;
  Cmd cmd = make_cmd(CmdKind::polygon);
  Polygon& g = cmd.polygon;
  g.first = static_cast<uint32_t>(points_.size());
  g.count = static_cast<uint32_t>(pts.size());
  g.fill = fill, g.feather = feather, g.opacity = opacity;
  float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
  const size_t n = pts.size();
  for (size_t k = 0; k < n; ++k) {
    const size_t i = area > 0 ? k : n - 1 - k;
    const size_t j = area > 0 ? (i + 1) % n : (i + n - 1) % n;
    const Vec2 p = pts[i], q = pts[j];
    const float ex = q.x - p.x, ey = q.y - p.y;
    const float len = std::sqrt(ex * ex + ey * ey);
    // Clockwise on screen, the outside of an edge is to its left: (ey, -ex).
    const Vec2 normal = len > 0 ? Vec2{ey / len, -ex / len} : Vec2{0, 0};
    points_.push_back({p, normal});
    x0 = std::min(x0, p.x), y0 = std::min(y0, p.y), x1 = std::max(x1, p.x), y1 = std::max(y1, p.y);
  }
  g.bounds = {x0, y0, x1 - x0, y1 - y0};
  cmds_.push_back(std::move(cmd));
}

void DrawList::push_clip(Rect r) { clips_.push_back(intersect(clip(), r)); }

void DrawList::pop_clip() {
  if (!clips_.empty()) clips_.pop_back();
}

void DrawList::fade(size_t from, float alpha) {
  for (size_t i = from; i < cmds_.size(); ++i) {
    Cmd& c = cmds_[i];
    switch (c.kind) {
      case CmdKind::box:
        c.box.opacity *= alpha;
        break;
      case CmdKind::text:
        c.text.color.a *= alpha;
        break;
      case CmdKind::segment:
        c.segment.opacity *= alpha;
        break;
      case CmdKind::arc:
        c.arc.opacity *= alpha;
        break;
      case CmdKind::polygon:
        c.polygon.opacity *= alpha;
        break;
    }
  }
}

}  // namespace ge::ui
