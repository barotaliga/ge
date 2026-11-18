// The live map (live_map.hpp).
//
// ge:summary The live map's plate, figures, atlas, flashes, trails, glints and timers
// ge:keywords live-map plate figure-atlas glint respawn-timer trail flash follow-ring
// ge:skill map-design ui replay
#include "shooters_edge/livemap/live_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>
#include <utility>

#include "brush/app/draw_data.hpp"
#include "brush/map.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "ui/font.hpp"
#include "ui/paint.hpp"

namespace shooters_edge::livemap {

namespace {

namespace br = ge::brush;
namespace ba = ge::brush::app;
namespace cl = shooters_edge::client;
namespace ui = ge::ui;

// The health and ammo boxes (the small maps) seen straight down, flat on the floor like the plan.
constexpr double kStraightDownDeg = 89.5;
constexpr double kPlayerUnits = 56;               // a player's height, units
constexpr double kTrailS = 0.7;                   // a trail's smoke lasts, s
constexpr double kFlashS = 0.4;                   // an explosion's flash, s
constexpr double kSparkleS = 0.8;                 // an item come back, its glow, s
constexpr double kGlintS = 1.3;                   // a glint on the water, s
constexpr uint32_t kRotateFlag = 8;               // a model file's flag: it turns (an item)
constexpr uint32_t kRocketFlag = 1;               // ... it trails smoke and fire
constexpr uint32_t kGrenadeFlag = 2;              // ... it trails smoke
constexpr int kQuadGlow = 64, kShieldGlow = 128;  // a player's effects bits: blue, red

uint32_t mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

double frac(double v) { return v - std::floor(v); }

ui::Color rgb(uint32_t c, float a = 1) {
  return {float((c >> 16) & 255) / 255, float((c >> 8) & 255) / 255, float(c & 255) / 255, a};
}

// A soft disc: `c` in the middle fading to nothing at `r`.
void glow(ui::DrawList& dl, float x, float y, float r, ui::Color c) {
  if (r <= 0.5f || c.a <= 0) return;
  ui::Color clear = c;
  clear.a = 0;
  const ui::Stop stops[3] = {{0, c}, {0.45f, {c.r, c.g, c.b, c.a * 0.45f}}, {1, clear}};
  ui::Box b;
  b.rect = {x - r, y - r, 2 * r, 2 * r};
  b.radius = r;
  b.fill = ui::radial(x, y, r, stops, 3);
  dl.box(b);
}

void ring(ui::DrawList& dl, float x, float y, float r, float width, ui::Color c, float from = 0,
          float to = 6.2831853f) {
  if (r <= 0.5f || c.a <= 0) return;
  dl.arc({x, y}, r, width, from, to, ui::solid(c), false, 1, 1);
}

// The colour a player's pants paint, off the palette (a light shade of its row).
ui::Color team_colour(std::span<const uint8_t> palette, int bottom) {
  if (palette.size() < 768 || bottom < 0) return {0.85f, 0.85f, 0.85f, 1};
  const auto i = size_t(translate(96 + 4, -1, std::clamp(bottom, 0, 13)));
  ui::Color c{palette[i * 3] / 255.0f, palette[i * 3 + 1] / 255.0f, palette[i * 3 + 2] / 255.0f, 1};
  // Lifted toward white so a dark row still reads on the floors.
  c.r = 0.35f + 0.65f * c.r;
  c.g = 0.35f + 0.65f * c.g;
  c.b = 0.35f + 0.65f * c.b;
  return c;
}

bool origin_of(const br::Entity& e, double o[3]) {
  const std::string& s = e.get("origin");
  return !s.empty() && std::sscanf(s.c_str(), "%lf %lf %lf", &o[0], &o[1], &o[2]) == 3;
}

// The model `name`'s entry, read the first time it is asked for.
int art_of(LiveMap& lm, const std::string& name) {
  const auto found = lm.model_of.find(name);
  if (found != lm.model_of.end()) return found->second;
  auto a = std::make_unique<LiveMap::Art>();
  if (lm.files != nullptr) {
    const bool small = name.size() > 9 && name.compare(0, 7, "maps/b_") == 0;
    if (small) {
      a->ok = box_mesh(*lm.files, lm.palette, name, &a->box) && !a->box.idx.empty();
      a->small_map = true;
      for (size_t i = 0; i + 2 < a->box.pos.size(); i += 3)
        a->radius =
            std::max(a->radius, float(std::sqrt(double(a->box.pos[i]) * a->box.pos[i] +
                                                double(a->box.pos[i + 1]) * a->box.pos[i + 1] +
                                                double(a->box.pos[i + 2]) * a->box.pos[i + 2])));
    } else if (name.size() > 4 && (name.compare(name.size() - 4, 4, ".mdl") == 0 ||
                                   name.compare(name.size() - 4, 4, ".md2") == 0)) {
      a->ok = read_model_art(*lm.files, lm.palette, name, &a->art);
      if (a->ok)
        // The first frame's reach (a standing pose): a figure lying dead may reach past it and
        // is cut at its cell's edge, so the living keep their size.
        for (const ge::morph::Frame& f : std::span(a->art.model.frames).first(1))
          for (size_t i = 0; i + 2 < f.positions.size(); i += 3) {
            const double x = f.positions[i], y = f.positions[i + 1], z = f.positions[i + 2];
            a->radius = std::max(a->radius, float(std::sqrt(x * x + y * y + z * z)));
          }
    }
    if (a->radius <= 1) a->ok = false;
  }
  const int id = int(lm.models.size());
  lm.models.push_back(std::move(a));
  lm.model_of[name] = id;
  return id;
}

// The atlas's cell holding `key`, drawn now when it holds none and the frame's budget allows;
// -1 when it waits.
int figure_cell(LiveMap& lm, const FigureKey& key) {
  if (const auto it = lm.cells.find(key); it != lm.cells.end()) {
    lm.used[size_t(it->second)] = lm.frames;
    return it->second;
  }
  if (lm.drawn_this_frame >= kNewFiguresAFrame) return -1;
  const LiveMap::Art& a = *lm.models[size_t(key.model)];
  // The figure at the key's size: its radius fits the cell's middle.
  const double ppu = (double(key.size) - 4) / (2.0 * a.radius);
  const Mesh mesh = a.small_map ? a.box : art_mesh(a.art, key.frame, key.skin, key.top, key.bottom);
  if (mesh.idx.empty()) return -1;
  const ui::Image img = draw_figure(mesh, key.size, double(key.yaw) * 360.0 / kYawSteps,
                                    double(key.pitch) * 360.0 / kYawSteps,
                                    a.small_map ? kStraightDownDeg : kElevationDeg, ppu);
  // The least recently used cell (a free one first).
  const int n = kAtlasCells * kAtlasCells;
  if (lm.atlas.empty()) {
    lm.atlas = ui::make_image(kCell * kAtlasCells, kCell * kAtlasCells, {0, 0, 0, 0});
    lm.held.assign(size_t(n), FigureKey{});
    lm.used.assign(size_t(n), -1);
  }
  int cell = 0;
  for (int i = 0; i < n; ++i)
    if (lm.used[size_t(i)] < lm.used[size_t(cell)]) cell = i;
  if (lm.used[size_t(cell)] >= 0) lm.cells.erase(lm.held[size_t(cell)]);
  const int cx = (cell % kAtlasCells) * kCell, cy = (cell / kAtlasCells) * kCell;
  for (int y = 0; y < kCell; ++y)
    for (int x = 0; x < kCell; ++x) {
      uint8_t* to = &lm.atlas.rgba[(size_t(cy + y) * size_t(lm.atlas.width) + size_t(cx + x)) * 4];
      if (x < img.width && y < img.height) {
        const uint8_t* from = &img.rgba[(size_t(y) * size_t(img.width) + size_t(x)) * 4];
        to[0] = from[0], to[1] = from[1], to[2] = from[2], to[3] = from[3];
      } else {
        to[0] = to[1] = to[2] = to[3] = 0;
      }
    }
  lm.held[size_t(cell)] = key;
  lm.used[size_t(cell)] = lm.frames;
  lm.cells[key] = cell;
  if (lm.drawn_this_frame++ == 0) ++lm.atlas.revision;  // to the GPU once a frame at most
  ++lm.figures_drawn;
  return cell;
}

// Where a map point lands on the screen.
struct Place {
  const Plate* plate = nullptr;
  ui::Rect r;
  double k = 1;  // screen px a plate px
  const Frame* fr = nullptr;
  ui::Vec2 at(double x, double y) const {
    return {float(r.x + fr->px(x) * k), float(r.y + fr->py(y) * k)};
  }
  double ppu() const { return fr->scale * k; }
};

struct Figure {
  ui::Vec2 at;
  int cell = -1;
  int size = 0;
  float opacity = 1;
  int slot = -1;  // a player's
  uint32_t glow = 0;
  float glow_a = 0;
  ui::Color tint{1, 1, 1, 1};  // under a liquid: its colour over the whole figure, seen through
  int under_cell = -1;         // under a liquid: its cell in the cut atlas
  bool covered = false;        // under a floor: dimmed, its edge dotted
  bool flat = false;           // a box seen straight down: its dotted edge a square
};

// A dotted line from `a` to `b`.
void dotted(ui::DrawList& dl, ui::Vec2 a, ui::Vec2 b, ui::Color c) {
  const float len = std::hypot(b.x - a.x, b.y - a.y);
  const int n = std::max(1, int(len / 4));
  for (int i = 0; i < n; i += 2) {
    const float t0 = float(i) / float(n), t1 = float(i + 1) / float(n);
    dl.line({a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0},
            {a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1}, 1.2f, c);
  }
}

// The covered figure's dotted edge: a box's own outline (its cell's opaque pixels), else a ring.
void dotted_edge(ui::DrawList& dl, const LiveMap& lm, const Figure& f) {
  const ui::Color c{0.85f, 0.88f, 0.95f, 0.75f * f.opacity};
  if (f.flat) {
    const FigureKey& key = lm.held[size_t(f.cell)];
    const int ax = (f.cell % kAtlasCells) * kCell, ay = (f.cell / kAtlasCells) * kCell;
    int lo[2] = {key.size, key.size}, hi[2] = {-1, -1};
    for (int v = 0; v < key.size; ++v)
      for (int u = 0; u < key.size; ++u)
        if (lm.atlas.rgba[(size_t(ay + v) * size_t(lm.atlas.width) + size_t(ax + u)) * 4 + 3] >
            96) {
          lo[0] = std::min(lo[0], u), hi[0] = std::max(hi[0], u);
          lo[1] = std::min(lo[1], v), hi[1] = std::max(hi[1], v);
        }
    if (hi[0] >= lo[0]) {
      const float per = float(f.size) / float(key.size);
      const float x0 = f.at.x - float(f.size) / 2 + float(lo[0]) * per - 1.5f;
      const float y0 = f.at.y - float(f.size) / 2 + float(lo[1]) * per - 1.5f;
      const float x1 = f.at.x - float(f.size) / 2 + float(hi[0] + 1) * per + 1.5f;
      const float y1 = f.at.y - float(f.size) / 2 + float(hi[1] + 1) * per + 1.5f;
      dotted(dl, {x0, y0}, {x1, y0}, c);
      dotted(dl, {x1, y0}, {x1, y1}, c);
      dotted(dl, {x1, y1}, {x0, y1}, c);
      dotted(dl, {x0, y1}, {x0, y0}, c);
      return;
    }
  }
  const float r = std::max(4.0f, float(f.size) * 0.36f);
  constexpr int kDashes = 12;
  for (int d = 0; d < kDashes; ++d) {
    const float from = 6.2831853f * float(d) / kDashes;
    ring(dl, f.at.x, f.at.y, r, 1.2f, c, from, from + 6.2831853f / kDashes * 0.5f);
  }
}

constexpr int kUnderCells = 4;  // the cut atlas's cells a side
// A floor this far over a point (units) covers it: over a player's head, never a step or a stair.
constexpr double kCoverUnits = 48;
// Below the map's top a figure shrinks as seen from over the map (a unit further down is
// 1 / kDepthUnits smaller); under a liquid's surface faster (kWetUnits), as if seen into it.
constexpr double kDepthUnits = 1600, kWetUnits = 220;

// The figure `f` (its cell in the atlas) cut to the liquid over it: its pixels where the liquid's
// surface is over `z`, the rest clear. -1 when there is no room.
int under_cell(LiveMap& lm, const Place& pl, const Figure& f, double x, double y, double z) {
  const Plate& p = *pl.plate;
  const FigureKey& key = lm.held[size_t(f.cell)];
  LiveMap::UnderKey uk;
  uk.key = key;
  uk.x = int(std::floor(p.frame.px(x)));
  uk.y = int(std::floor(p.frame.py(y)));
  uk.size = f.size;
  uk.scale = int(std::lround(pl.k * 1000));
  constexpr int n = kUnderCells * kUnderCells;
  if (lm.under.empty()) {
    lm.under = ui::make_image(kCell * kUnderCells, kCell * kUnderCells, {0, 0, 0, 0});
    lm.under_held.assign(size_t(n), LiveMap::UnderKey{});
    lm.under_used.assign(size_t(n), -1);
  }
  int cell = 0;
  for (int i = 0; i < n; ++i) {
    if (lm.under_used[size_t(i)] >= 0 && lm.under_held[size_t(i)] == uk) {
      lm.under_used[size_t(i)] = lm.frames;
      return i;
    }
    if (lm.under_used[size_t(i)] < lm.under_used[size_t(cell)]) cell = i;
  }
  if (lm.under_used[size_t(cell)] == lm.frames) return -1;  // every cell in use this frame
  const int ax = (f.cell % kAtlasCells) * kCell, ay = (f.cell / kAtlasCells) * kCell;
  const int ux = (cell % kUnderCells) * kCell, uy = (cell / kUnderCells) * kCell;
  const float per = float(f.size) / float(key.size);  // screen px a cell px
  const ui::Image& layer = p.liquid.image;
  for (int v = 0; v < kCell; ++v)
    for (int u = 0; u < kCell; ++u) {
      uint8_t* to = &lm.under.rgba[(size_t(uy + v) * size_t(lm.under.width) + size_t(ux + u)) * 4];
      if (u >= key.size || v >= key.size) {
        to[0] = to[1] = to[2] = to[3] = 0;
        continue;
      }
      const uint8_t* from =
          &lm.atlas.rgba[(size_t(ay + v) * size_t(lm.atlas.width) + size_t(ax + u)) * 4];
      // Where this pixel lies on the plate.
      const float sx = f.at.x - float(f.size) / 2 + (float(u) + 0.5f) * per;
      const float sy = f.at.y - float(f.size) / 2 + (float(v) + 0.5f) * per;
      const int X = int(std::floor((sx - pl.r.x) / float(pl.k)));
      const int Y = int(std::floor((sy - pl.r.y) / float(pl.k)));
      float keep = 0;
      if (X >= 0 && Y >= 0 && X < layer.width && Y < layer.height) {
        const size_t at = size_t(Y) * size_t(layer.width) + size_t(X);
        if (p.liquid.height[at] > kNone && double(p.liquid.height[at]) > z)
          keep = std::min(1.0f, float(layer.rgba[at * 4 + 3]) / 255.0f * 2.5f);
      }
      to[0] = from[0], to[1] = from[1], to[2] = from[2];
      to[3] = uint8_t(std::lround(float(from[3]) * keep));
    }
  lm.under_held[size_t(cell)] = uk;
  lm.under_used[size_t(cell)] = lm.frames;
  if (lm.under_drawn_this_frame++ == 0) ++lm.under.revision;
  return cell;
}

// The figure for `e` (model `art`, frame number `group`) at the screen's scale.
Figure figure_of(LiveMap& lm, const Place& pl, const cl::DemoEntity& e, int art, double yaw,
                 double pitch, int top, int bottom, const LiveView& v, double now, bool player) {
  Figure f;
  f.at = pl.at(e.origin.x, e.origin.y);
  const LiveMap::Art& a = *lm.models[size_t(art)];
  if (!a.ok) return f;
  double ppu = pl.ppu() * v.figures;
  if (player) ppu = std::max(ppu, double(v.min_figure_px) / kPlayerUnits);
  // Depth: smaller the further down, and faster under a liquid.
  const double z = e.origin.z;
  ppu /= 1 + std::max(0.0, pl.fr->hi[2] - z) / kDepthUnits;
  const float surface = pl.plate->liquid_surface(e.origin.x, e.origin.y);
  if (surface > kNone && z < double(surface)) ppu /= 1 + (double(surface) - z) / kWetUnits;
  int size = int(std::ceil(2 * a.radius * ppu)) + 4;
  size = std::clamp((size + 3) / 4 * 4, 12, kCell);
  FigureKey key;
  key.model = art;
  key.frame = a.small_map ? 0 : frame_index(a.art, e.frame, now);
  key.skin = a.small_map ? 0 : e.skin;
  key.top = top;
  key.bottom = bottom;
  key.yaw = int(std::lround(frac(yaw / 360.0) * kYawSteps)) % kYawSteps;
  key.pitch = int(std::lround(frac(pitch / 360.0) * kYawSteps)) % kYawSteps;
  key.size = size;
  f.cell = figure_cell(lm, key);
  // The drawn size: the cell's figure stretched back when the cell capped it.
  const double want = 2 * a.radius * ppu + 4;
  f.size = int(std::lround(std::max(double(size), std::min(want, double(size) * 1.6))));
  f.tint = pl.plate->liquid_tint(e.origin.x, e.origin.y, z);
  f.covered = pl.plate->covered(e.origin.x, e.origin.y, z);
  f.flat = a.small_map;
  if (f.cell >= 0 && surface > kNone && z < double(surface))
    f.under_cell = under_cell(lm, pl, f, e.origin.x, e.origin.y, z);
  return f;
}

void draw_figure_box(ui::DrawList& dl, const LiveMap& lm, const Figure& f) {
  if (f.cell < 0) return;
  ui::Box b;
  const float s = float(f.size);
  b.rect = {f.at.x - s / 2, f.at.y - s / 2, s, s};
  b.image = &lm.atlas;
  const int row = f.cell / kAtlasCells;
  b.uv = {float((f.cell % kAtlasCells) * kCell), float(row * kCell), float(kCell), float(kCell)};
  if (f.under_cell >= 0) {
    b.image = &lm.under;
    b.uv.x = float((f.under_cell % kUnderCells) * kCell);
    const int under_row = f.under_cell / kUnderCells;
    b.uv.y = float(under_row * kCell);
  }
  // The cell's figure fills `size` px of it from the corner.
  const auto key_size = float(lm.held[size_t(f.cell)].size);
  b.uv.w = key_size;
  b.uv.h = key_size;
  b.fill = ui::solid({f.tint.r, f.tint.g, f.tint.b, 1});
  b.opacity = f.opacity * f.tint.a * (f.covered ? 0.35f : 1.0f);
  b.feather = 0;
  dl.box(b);
}

constexpr int kHaloPad = 8;  // px round the cell the halo spreads into

// The followed figure's halo: the cell's outline spread `kHaloPad` px by a blur, a pale blue
// that is brightest at the figure's edge. Made again only when the cell shows another figure.
void draw_halo(ui::DrawList& dl, LiveMap& lm, const Figure& f, float strength) {
  if (f.cell < 0) return;
  const FigureKey& key = lm.held[size_t(f.cell)];
  const int n = key.size + 2 * kHaloPad;
  if (lm.halo.empty() || lm.halo_for < key || key < lm.halo_for) {
    constexpr int side = kCell + 2 * kHaloPad;
    if (lm.halo.empty()) lm.halo = ui::make_image(side, side, {0, 0, 0, 0});
    const int cx = (f.cell % kAtlasCells) * kCell, cy = (f.cell / kAtlasCells) * kCell;
    std::vector<float> a(size_t(side) * size_t(side), 0), b(a.size(), 0);
    const auto at = [](int x, int y) { return size_t(y) * size_t(side) + size_t(x); };
    for (int y = 0; y < key.size; ++y)
      for (int x = 0; x < key.size; ++x)
        a[at(x + kHaloPad, y + kHaloPad)] =
            float(
                lm.atlas.rgba[(size_t(cy + y) * size_t(lm.atlas.width) + size_t(cx + x)) * 4 + 3]) /
            255.0f;
    // Two box blurs each way, about a Gaussian.
    constexpr int r = 3;
    for (int pass = 0; pass < 2; ++pass) {
      for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
          float sum = 0;
          for (int k = -r; k <= r; ++k) sum += a[at(std::clamp(x + k, 0, side - 1), y)];
          b[at(x, y)] = sum / float(2 * r + 1);
        }
      for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
          float sum = 0;
          for (int k = -r; k <= r; ++k) sum += b[at(x, std::clamp(y + k, 0, side - 1))];
          a[at(x, y)] = sum / float(2 * r + 1);
        }
    }
    for (size_t i = 0; i < a.size(); ++i) {
      uint8_t* to = &lm.halo.rgba[i * 4];
      to[0] = 140, to[1] = 200, to[2] = 255;
      to[3] = uint8_t(std::lround(std::min(1.0f, a[i] * 1.6f) * 255.0f));
    }
    lm.halo_for = key;
    ++lm.halo.revision;
  }
  const float scale = float(f.size) / float(key.size);
  const float s = float(n) * scale;
  ui::Box box;
  box.rect = {f.at.x - s / 2, f.at.y - s / 2, s, s};
  box.image = &lm.halo;
  box.uv = {0, 0, float(n), float(n)};
  box.fill = ui::solid({1, 1, 1, 1});
  box.opacity = f.opacity * strength;
  box.feather = 0;
  dl.box(box);
}

bool playing(const cl::DemoState& s, int slot) {
  if (slot < 0 || slot >= 32) return false;
  if (s.players.size() > size_t(slot))
    return !s.names[size_t(slot)].empty() && !s.spectator[size_t(slot)] &&
           s.players[size_t(slot)].model > 0;
  return false;
}

}  // namespace

bool FigureKey::operator<(const FigureKey& o) const {
  return std::tie(model, frame, skin, top, bottom, yaw, pitch, size) <
         std::tie(o.model, o.frame, o.skin, o.top, o.bottom, o.yaw, o.pitch, o.size);
}

bool LiveMap::UnderKey::operator==(const UnderKey& o) const {
  return !(key < o.key) && !(o.key < key) && x == o.x && y == o.y && size == o.size &&
         scale == o.scale;
}

bool Plate::covered(double x, double y, double z) const {
  const int X = int(std::floor(frame.px(x))), Y = int(std::floor(frame.py(y)));
  if (X < 0 || Y < 0 || X >= frame.width || Y >= frame.height || roof.empty()) return false;
  const float r = roof[size_t(Y) * size_t(frame.width) + size_t(X)];
  return r > kNone && double(r) > z + kCoverUnits;
}

float Plate::liquid_surface(double x, double y) const {
  const int X = int(std::floor(frame.px(x))), Y = int(std::floor(frame.py(y)));
  if (X < 0 || Y < 0 || X >= frame.width || Y >= frame.height || liquid.height.empty())
    return kNone;
  return liquid.height[size_t(Y) * size_t(frame.width) + size_t(X)];
}

ui::Color Plate::liquid_tint(double x, double y, double z) const {
  const int X = int(std::floor(frame.px(x))), Y = int(std::floor(frame.py(y)));
  if (X < 0 || Y < 0 || X >= frame.width || Y >= frame.height || liquid.height.empty())
    return {1, 1, 1, 1};
  const size_t at = size_t(Y) * size_t(frame.width) + size_t(X);
  const float h = liquid.height[at];
  if (h <= kNone || z >= double(h)) return {1, 1, 1, 1};
  // Seen through the liquid: its colour laid over (a little more than the floor's, so a figure
  // reads as under it) and the liquid showing through.
  const uint8_t* c = &liquid.image.rgba[at * 4];
  const float a = std::min(0.9f, float(c[3]) / 255.0f * 1.5f);
  const auto mixed = [&](uint8_t q) { return 1 - a + a * (0.35f + 0.65f * float(q) / 255.0f); };
  return {mixed(c[0]), mixed(c[1]), mixed(c[2]), 1 - 0.35f * a};
}

bool build_plate(std::span<const uint8_t> bytes, const std::string& name,
                 std::span<const uint8_t> palette, int max_px, Plate* out, std::string* error) {
  *out = Plate{};
  out->map = name;
  ba::Pack pack;
  pack.bytes.assign(bytes.begin(), bytes.end());
  pack.files.push_back({"maps/" + name + ".bsp", 0, uint32_t(bytes.size())});
  ba::Scene scene;
  if (!ba::load_scene(pack, pack.files[0].name, palette, &scene, error)) return false;
  Frame& fr = out->frame;
  const double k = 1 / br::kUnitM;
  fr.lo[0] = scene.mins[0] * k, fr.hi[0] = scene.maxs[0] * k;
  fr.lo[1] = scene.mins[2] * k, fr.hi[1] = scene.maxs[2] * k;
  fr.lo[2] = scene.mins[1] * k, fr.hi[2] = scene.maxs[1] * k;
  fr.pad = 24;
  const double size = std::max(256, max_px);
  fr.scale =
      (size - 2 * fr.pad) / std::max(1.0, std::max(fr.hi[0] - fr.lo[0], fr.hi[1] - fr.lo[1]));
  fr.width = int(std::ceil((fr.hi[0] - fr.lo[0]) * fr.scale)) + 2 * fr.pad;
  fr.height = int(std::ceil((fr.hi[1] - fr.lo[1]) * fr.scale)) + 2 * fr.pad;
  const Floors floors = draw_floors(scene, fr, 2);
  out->image = shade_floors(floors, fr, &out->liquid);
  out->roof.assign(size_t(fr.width) * size_t(fr.height), kNone);
  for (int y = 0; y < fr.height; ++y)
    for (int x = 0; x < fr.width; ++x) {
      float high = kNone;
      for (int j = 0; j < floors.ss; ++j)
        for (int i = 0; i < floors.ss; ++i)
          high = std::max(
              high,
              floors.top[size_t(y * floors.ss + j) * size_t(floors.w) + size_t(x * floors.ss + i)]);
      out->roof[size_t(y) * size_t(fr.width) + size_t(x)] = high;
    }
  const uint8_t* v0 = out->image.rgba.data();
  out->void_colour = {v0[0] / 255.0f, v0[1] / 255.0f, v0[2] / 255.0f, 1};
  // The liquids' pixels, one in every 3 x 3.
  for (int y = 1; y < fr.height; y += 3)
    for (int x = 1; x < fr.width; x += 3) {
      const size_t at = size_t(y * floors.ss) * size_t(floors.w) + size_t(x * floors.ss);
      // Only where the liquid is the top surface: never glinting through a floor built over it.
      if (at < floors.wkind.size() && floors.wkind[at] != 0 && floors.wet[at] > kNone &&
          (floors.top[at] <= kNone || floors.wet[at] >= floors.top[at])) {
        out->water.push_back({uint16_t(x), uint16_t(y)});
        out->water_kind.push_back(floors.wkind[at]);
      }
    }
  // The items, and the teleporters to where they lead.
  std::map<std::string, std::pair<double, double>> dests;
  std::vector<std::pair<Link, std::string>> teles;
  for (const br::Entity& e : scene.map.entities) {
    const std::string& cls = e.get("classname");
    double o[3] = {0, 0, 0};
    const bool has = origin_of(e, o);
    if (cls == "info_teleport_destination" && has) {
      dests[e.get("targetname")] = {o[0], o[1]};
      continue;
    }
    if (cls == "trigger_teleport") {
      const std::string& model = e.get("model");
      if (model.size() > 1 && model[0] == '*') {
        const auto mi = size_t(std::atoi(model.c_str() + 1));
        if (mi < scene.map.models.size()) {
          const auto& md = scene.map.models[mi];
          Link l;
          l.from[0] = (md.mins[0] + md.maxs[0]) / 2;
          l.from[1] = (md.mins[1] + md.maxs[1]) / 2;
          teles.push_back({l, e.get("target")});
        }
      }
      continue;
    }
    const ItemKind* kind = item_of(e);
    if (kind == nullptr || !has) continue;
    Spot s;
    s.kind = kind;
    for (int q = 0; q < 3; ++q) s.at[q] = o[q];
    out->spots.push_back(s);
  }
  for (auto& [l, target] : teles) {
    const auto d = dests.find(target);
    if (d == dests.end()) continue;
    l.to[0] = d->second.first;
    l.to[1] = d->second.second;
    out->links.push_back(l);
  }
  return true;
}

void live_map_take(LiveMap& lm, const cl::SearchPath& files, std::span<const uint8_t> palette,
                   std::shared_ptr<const Plate> plate) {
  lm.plate = std::move(plate);
  lm.files = &files;
  if (lm.palette.size() != palette.size() ||
      !std::equal(palette.begin(), palette.end(), lm.palette.begin())) {
    lm.palette.assign(palette.begin(), palette.end());
    lm.model_of.clear();
    lm.models.clear();
    lm.cells.clear();
    std::fill(lm.used.begin(), lm.used.end(), -1);
  }
  lm.spots.assign(lm.plate ? lm.plate->spots.size() : 0, LiveMap::Gone{});
  lm.flashes.clear();
  lm.trails.clear();
  lm.effects_frame = -1;
  lm.last_now = -1;
}

bool live_map_level(LiveMap& lm, const cl::SearchPath& files, std::span<const uint8_t> palette,
                    std::span<const uint8_t> bsp, const std::string& name, int max_px,
                    std::string* error) {
  auto plate = std::make_shared<Plate>();
  if (!build_plate(bsp, name, palette, max_px, plate.get(), error)) {
    live_map_take(lm, files, palette, nullptr);
    return false;
  }
  live_map_take(lm, files, palette, std::move(plate));
  return true;
}

ui::Rect live_map_rect(const Plate& p, ui::Rect area) {
  if (p.image.empty() || area.w <= 0 || area.h <= 0) return {area.x, area.y, 0, 0};
  const float k = std::min(area.w / float(p.image.width), area.h / float(p.image.height));
  const float w = float(p.image.width) * k, h = float(p.image.height) * k;
  return {area.x + (area.w - w) / 2, area.y + (area.h - h) / 2, w, h};
}

int next_followed(const cl::DemoState& s, int from, int dir) {
  dir = dir < 0 ? -1 : 1;
  for (int i = 1; i <= 32; ++i) {
    const int slot = ((from < 0 ? (dir > 0 ? -1 : 0) : from) + dir * i + 64) % 32;
    if (playing(s, slot)) return slot;
  }
  return -1;
}

std::string following_text(const cl::DemoState& s, int slot) {
  if (slot < 0 || slot >= 32) return {};
  return "Following " + cl::player_name(s.names, slot);
}

void draw_live_map(LiveMap& lm, const cl::DemoState& s, const LiveView& v, ui::DrawList& dl) {
  if (!lm.plate || lm.plate->image.empty()) return;
  const Plate& p = *lm.plate;
  ++lm.frames;
  lm.drawn_this_frame = 0;
  const double now = v.now_s;
  // A seek back: what was running is forgotten.
  if (lm.last_now >= 0 && now < lm.last_now - 0.5) {
    lm.spots.assign(p.spots.size(), LiveMap::Gone{});
    lm.flashes.clear();
    lm.trails.clear();
  }
  lm.last_now = now;
  if (v.fill) dl.rect(v.area, p.void_colour);
  Place pl;
  pl.r = live_map_rect(p, v.area);
  pl.k = pl.r.w / float(p.image.width);
  pl.fr = &p.frame;
  pl.plate = &p;
  lm.under_drawn_this_frame = 0;
  {
    ui::Box b;
    b.rect = pl.r;
    b.image = &p.image;
    b.uv = {0, 0, float(p.image.width), float(p.image.height)};
    b.fill = ui::solid({1, 1, 1, 1});
    b.feather = 0;
    if (!v.fill) {  // the corner's map: a dark edge and rounded corners over the view
      b.radius = 6;
      b.border = 1.5f;
      b.border_color = {0.02f, 0.03f, 0.05f, 0.9f};
      b.feather = 1;
    }
    dl.box(b);
  }
  const float px = float(std::max(0.4, pl.ppu() * 24));  // a figure-sized unit of marks, px
  // The water glinting (over the liquids' layer).
  const auto glints = [&] {
    if (p.water.empty()) return;
    const int n = std::clamp(int(p.water.size() / 160), 4, 36);
    for (int i = 0; i < n; ++i) {
      const double phase = double(mix(uint32_t(i) * 2654435761U) % 1000) / 1000.0;
      const double run = now / kGlintS + phase;
      const auto cycle = uint32_t(int64_t(std::floor(run)));
      const double a = frac(run);
      const size_t at = mix(uint32_t(i) * 7919U + cycle * 104729U) % p.water.size();
      const float shine = float(4 * a * (1 - a));
      const ui::Vec2 q = {float(pl.r.x + (p.water[at][0] + 0.5) * pl.k),
                          float(pl.r.y + (p.water[at][1] + 0.5) * pl.k)};
      const uint8_t kind = p.water_kind[at];
      const ui::Color c = kind == ba::kLava    ? ui::Color{1.0f, 0.62f, 0.25f, 0.85f * shine}
                          : kind == ba::kSlime ? ui::Color{0.6f, 1.0f, 0.45f, 0.75f * shine}
                                               : ui::Color{0.85f, 0.95f, 1.0f, 0.7f * shine};
      const float arm = std::max(1.5f, px * 0.22f) * (0.5f + shine);
      glow(dl, q.x, q.y, arm * 1.6f, {c.r, c.g, c.b, c.a * 0.5f});
      dl.line({q.x - arm, q.y}, {q.x + arm, q.y}, 0.6f, c);
      dl.line({q.x, q.y - arm * 0.8f}, {q.x, q.y + arm * 0.8f}, 0.6f, c);
    }
  };
  // The liquids' layer over the floors under them, glinting.
  if (!p.liquid.image.empty()) {
    ui::Box b;
    b.rect = pl.r;
    b.image = &p.liquid.image;
    b.uv = {0, 0, float(p.liquid.image.width), float(p.liquid.image.height)};
    b.fill = ui::solid({1, 1, 1, 1});
    b.feather = 0;
    dl.box(b);
  }
  glints();
  // The teleporters' links, flowing.
  for (const Link& l : p.links) {
    const ui::Vec2 a = pl.at(l.from[0], l.from[1]), b = pl.at(l.to[0], l.to[1]);
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    const int dots = std::clamp(int(len / std::max(6.0f, px * 0.6f)), 3, 40);
    for (int j = 0; j < dots; ++j) {
      const double f = frac(double(j) / dots + now * 0.18);
      const float fade = float(std::min(f, 1 - f) * 4);
      dl.rect(
          {float(a.x + (b.x - a.x) * f) - 1.2f, float(a.y + (b.y - a.y) * f) - 1.2f, 2.4f, 2.4f},
          {0.82f, 0.62f, 1.0f, 0.65f * std::min(1.0f, fade)}, 1.2f);
    }
    ring(dl, a.x, a.y, std::max(3.0f, px * 0.45f), 1.2f, {0.82f, 0.62f, 1.0f, 0.7f});
  }
  // The entities: which spots hold their item, the figures, the trails.
  std::vector<bool> held(p.spots.size(), false);
  std::vector<Figure> figures;
  const auto take_entity = [&](const cl::DemoEntity& e, int number, bool fixed) {
    if (e.model <= 0 || size_t(e.model) >= s.models.size()) return;
    const std::string& name = s.models[size_t(e.model)];
    if (name.empty() || name[0] == '*') return;
    for (size_t i = 0; i < p.spots.size(); ++i)
      if (std::fabs(p.spots[i].at[0] - e.origin.x) < 24 &&
          std::fabs(p.spots[i].at[1] - e.origin.y) < 24 &&
          std::fabs(p.spots[i].at[2] - e.origin.z) < 64)
        held[i] = true;
    const int art = art_of(lm, name);
    const LiveMap::Art& a = *lm.models[size_t(art)];
    if (!a.ok) return;
    const uint32_t flags = a.small_map ? 0 : a.art.model.flags;
    if (!fixed && (flags & (kRocketFlag | kGrenadeFlag)) != 0) {
      LiveMap::Trail& t = lm.trails[number];
      t.rocket = (flags & kRocketFlag) != 0;
      t.seen = lm.frames;
      if (t.points.empty() ||
          std::hypot(t.points.back()[0] - e.origin.x, t.points.back()[1] - e.origin.y) > 6)
        t.points.push_back({e.origin.x, e.origin.y, float(now)});
    }
    const bool turns = (flags & kRotateFlag) != 0 || (s.protocol == 34 && (e.effects & 1) != 0);
    Figure f = figure_of(lm, pl, e, art, turns ? now * kItemSpinDegS : e.angles.y, e.angles.x, -1,
                         -1, v, now, false);
    if (name.find("flame") != std::string::npos) {
      f.glow = 0xffa040;
      f.glow_a = 0.5f;
    }
    figures.push_back(f);
  };
  // The players.
  int followed = -1;
  for (int slot = 0; slot < 32; ++slot) {
    if (!playing(s, slot)) continue;
    const cl::DemoEntity& e = s.players[size_t(slot)];
    const std::string& name = s.models[size_t(std::min<int>(e.model, int(s.models.size()) - 1))];
    if (name.empty()) continue;
    const int art = art_of(lm, name);
    const bool colours = name.find("player") != std::string::npos;
    Figure f = figure_of(lm, pl, e, art, e.angles.y, 0, colours ? s.top[size_t(slot)] : -1,
                         colours ? s.bottom[size_t(slot)] : -1, v, now, true);
    f.slot = slot;
    f.opacity = e.dead ? 0.6f : 1;
    if ((e.effects & kQuadGlow) != 0 && (e.effects & kShieldGlow) != 0) {
      f.glow = 0xd060ff;
      f.glow_a = 0.8f;
    } else if ((e.effects & kQuadGlow) != 0) {
      f.glow = 0x4f8cff;
      f.glow_a = 0.8f;
    } else if ((e.effects & kShieldGlow) != 0) {
      f.glow = 0xff4b4b;
      f.glow_a = 0.8f;
    }
    if (f.size == 0) {  // no model: a dot in his colour
      f.size = int(std::max(6.0, double(v.min_figure_px) * 0.5));
    }
    if (slot == v.follow) followed = int(figures.size());
    figures.push_back(f);
  }
  for (size_t i = 1; i < s.entities.size(); ++i)
    if (s.entities[i].present) take_entity(s.entities[i], int(i), false);
  for (const cl::DemoEntity& e : s.statics) take_entity(e, -1, true);
  // The spots: taken, counting down; back, sparkling.
  for (size_t i = 0; i < p.spots.size(); ++i) {
    LiveMap::Gone& g = lm.spots[i];
    if (held[i] != g.present) {
      if (held[i] && g.since >= 0)
        lm.flashes.push_back({{p.spots[i].at[0], p.spots[i].at[1]}, now, 2});
      g.present = held[i];
      g.since = lm.frames > 1 && lm.last_now >= 0 ? now : -1;
    }
    const Spot& sp = p.spots[i];
    const ui::Vec2 q = pl.at(sp.at[0], sp.at[1]);
    const float r = std::max(5.0f, px * (sp.kind->tier == Tier::power ? 0.95f : 0.75f));
    if (g.present) {
      if (sp.kind->tier == Tier::power) {
        const float pulse = float(0.75 + 0.25 * (4 * frac(now * 0.7) * (1 - frac(now * 0.7))));
        glow(dl, q.x, q.y, r * 1.5f, rgb(sp.kind->colour, 0.55f * pulse));
      }
    } else if (sp.kind->respawn > 0 && sp.kind->tier != Tier::minor) {
      const double left = g.since < 0 ? -1 : sp.kind->respawn - (now - g.since);
      ring(dl, q.x, q.y, r, 1.2f, rgb(sp.kind->colour, 0.3f));
      if (left > 0) {
        ring(dl, q.x, q.y, r, 2.4f, rgb(sp.kind->colour, 0.95f), 0,
             float(6.2831853 * left / sp.kind->respawn));
        if (r >= 7) {
          const std::string sec = std::to_string(int(std::ceil(left)));
          dl.text(ui::ui_font(), std::max(9.0f, r * 0.8f), q.x, q.y + r * 0.3f, sec,
                  rgb(sp.kind->colour, 1), ui::Align::center);
        }
      }
    }
  }
  // Trails under the figures; gone ones fade out.
  for (auto it = lm.trails.begin(); it != lm.trails.end();) {
    LiveMap::Trail& t = it->second;
    while (!t.points.empty() && now - t.points.front()[2] > kTrailS)
      t.points.erase(t.points.begin());
    if (t.points.empty() || (t.seen != lm.frames && now - t.points.back()[2] > kTrailS)) {
      it = lm.trails.erase(it);
      continue;
    }
    for (size_t j = 1; j < t.points.size(); ++j) {
      const ui::Vec2 a = pl.at(t.points[j - 1][0], t.points[j - 1][1]);
      const ui::Vec2 b = pl.at(t.points[j][0], t.points[j][1]);
      const float life = float(1 - (now - t.points[j][2]) / kTrailS);
      const float w = std::max(1.0f, px * (t.rocket ? 0.28f : 0.18f)) * (1.6f - life);
      dl.line(a, b, w, {0.78f, 0.78f, 0.8f, 0.55f * life}, 1.5f);
    }
    if (t.rocket && t.seen == lm.frames) {
      const ui::Vec2 h = pl.at(t.points.back()[0], t.points.back()[1]);
      glow(dl, h.x, h.y, std::max(3.0f, px * 0.5f), {1, 0.65f, 0.25f, 0.9f});
    }
    ++it;
  }
  // New effects: explosions and teleports flash.
  if (s.frames != lm.effects_frame) {
    lm.effects_frame = s.frames;
    for (const cl::DemoEffect& fx : s.effects) {
      if (fx.kind == cl::DemoEffectKind::explosion)
        lm.flashes.push_back({{fx.at.x, fx.at.y}, now, 0});
      if (fx.kind == cl::DemoEffectKind::teleport)
        lm.flashes.push_back({{fx.at.x, fx.at.y}, now, 1});
    }
  }
  // The figures, back to front (the lower on the screen nearer the eye).
  std::vector<size_t> order(figures.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return figures[a].at.y < figures[b].at.y; });
  for (const size_t i : order) {
    const Figure& f = figures[i];
    if (f.glow != 0) glow(dl, f.at.x, f.at.y, float(f.size) * 0.55f, rgb(f.glow, f.glow_a));
    if (int(i) == followed) {  // a faint blue glow off the followed player's own figure
      const float pulse = float(4 * frac(now * 0.6) * (1 - frac(now * 0.6)));
      draw_halo(dl, lm, f, 0.5f + 0.15f * pulse);
    }
    if (f.cell >= 0) {
      draw_figure_box(dl, lm, f);
      if (f.covered) dotted_edge(dl, lm, f);  // under a bridge or a ledge, seen through
    } else if (f.slot >= 0) {
      const ui::Color c = team_colour(lm.palette, s.bottom[size_t(f.slot)]);
      dl.rect({f.at.x - 3, f.at.y - 3, 6, 6}, {c.r, c.g, c.b, f.opacity}, 3);
    }
  }
  // Names under the players.
  if (v.names)
    for (const Figure& f : figures) {
      if (f.slot < 0) continue;
      const float size = std::clamp(float(f.size) * 0.3f, 10.0f, 15.0f);
      const std::string who = cl::player_name(s.names, f.slot);
      const float y = f.at.y + float(f.size) * 0.42f + size;
      const ui::Color c = team_colour(lm.palette, s.bottom[size_t(f.slot)]);
      dl.text(ui::ui_font(), size, f.at.x + 1, y + 1, who, {0, 0, 0, 0.75f * f.opacity},
              ui::Align::center);
      dl.text(ui::ui_font(), size, f.at.x, y, who, {c.r, c.g, c.b, f.opacity}, ui::Align::center);
    }
  // The flashes.
  for (auto it = lm.flashes.begin(); it != lm.flashes.end();) {
    const double age = now - it->t;
    const double life = it->kind == 2 ? kSparkleS : kFlashS;
    if (age < 0 || age > life) {
      it = lm.flashes.erase(it);
      continue;
    }
    const ui::Vec2 q = pl.at(it->at[0], it->at[1]);
    const auto a = float(age / life);
    if (it->kind == 0) {  // an explosion: a short soft flash, nothing thrown
      glow(dl, q.x, q.y, px * (0.7f + 0.5f * a), {1, 0.7f, 0.32f, 0.55f * (1 - a) * (1 - a)});
    } else if (it->kind == 1) {
      ring(dl, q.x, q.y, px * (0.3f + 0.8f * a), 1.2f, {0.75f, 0.6f, 1.0f, 0.5f * (1 - a)});
    } else {
      glow(dl, q.x, q.y, px * (0.9f + 0.4f * a), {1, 0.97f, 0.75f, 0.5f * (1 - a)});
    }
    ++it;
  }
  // Following <name>, over the map.
  if (v.follow >= 0 && playing(s, v.follow)) {
    const std::string text = following_text(s, v.follow);
    const float size = std::clamp(pl.r.h * 0.035f, 11.0f, 28.0f);
    const ui::Font& font = ui::ui_font(ui::Weight::semibold);
    const float w = font.advance(text, size) + size * 1.2f;
    const float x = pl.r.x + pl.r.w / 2, y = pl.r.y + size * 0.6f;
    dl.rect({x - w / 2, y, w, size * 1.55f}, {0.02f, 0.03f, 0.06f, 0.72f}, size * 0.3f);
    dl.text(font, size, x, y + size * 1.15f, text, {1, 0.92f, 0.62f, 1}, ui::Align::center);
  }
}

void add_live_map_cvars(ge::console::Cvars& cvars, LiveMapSettings& s) {
  using ge::console::CvarKind;
  cvars.bind({.name = "demo_map",
              .fallback = "0",
              .flags = ge::console::kArchive,
              .help = "the live map of a match watched on TV or in a recording (never on a server "
                      "joined): M cycles it",
              .kind = CvarKind::integer,
              .min = 0,
              .max = 2,
              .values = "0: off; 1: the whole screen, the HUD under it; 2: in the top right corner "
                        "of the view"},
             &s.mode);
  cvars.bind({.name = "demo_map_scale",
              .fallback = "0.34",
              .flags = ge::console::kArchive,
              .help = "the corner map's height, a share of the screen's",
              .kind = CvarKind::number,
              .min = 0.1,
              .max = 0.9},
             &s.scale);
  cvars.bind({.name = "demo_map_names",
              .fallback = "1",
              .flags = ge::console::kArchive,
              .help = "the players' names under them on the whole-screen live map: 1 on, 0 off",
              .kind = CvarKind::integer,
              .min = 0,
              .max = 1},
             &s.names);
  cvars.bind({.name = "demo_map_figures",
              .fallback = "2.4",
              .flags = ge::console::kArchive,
              .help = "the live map's players and items drawn this many times their size",
              .kind = CvarKind::number,
              .min = 1,
              .max = 6},
             &s.figures);
}

}  // namespace shooters_edge::livemap
