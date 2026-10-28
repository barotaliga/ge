// The `menu` lab's scenes, shared with `menu-gpu` (gpu_ui_menu_lab.cpp), which draws the same
// pages on the GPU: the painted range, the front door and its style, the five backdrops, the
// viewports a `--viewports` flag names, and an instrument panel that uses every shape the draw
// list has (a dial's arc band, ticks and needles, a ring meter, a nine-sliced panel, a turned
// map with a chevron, a line graph).
//
// ge:summary The menu labs' shared scenes: the painted range, the front door, the backdrops,
// ge:summary the instrument panel
// ge:keywords lab menu scenes painted-range backdrop front-door viewports instruments gauge
// ge:keywords dial needle ring-meter nine-slice minimap shapes
// ge:skill ui labs
// ge:test tests/ui_menu_test.cpp
#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "ui/background.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"
#include "ui/raster.hpp"

namespace ge::lab::menu_scenes {

using namespace ge::ui;

// A winter range under a clear sky, from value noise: the sky's gradient, three ridgelines (ridged
// noise over four octaves, so the peaks come to points), each hazier with distance, snow down to
// a line that wanders, the faces lit from the left by the ridge's slope, dark woods low on the
// nearest.
inline double ridge_height(double x, double seed) {
  double h = 0, amp = 1, scale = 420, sum = 0;
  for (int o = 0; o < 4; o++) {
    const double n = ge::core::value_noise(x, 0, scale, seed + o * 17);
    h += (1 - std::fabs(2 * n - 1)) * amp;  // ridged: a crease at every crossing of 0.5
    sum += amp;
    amp *= 0.48, scale *= 0.45;
  }
  return h / sum;
}

inline Image painted_range(int w, int h, double seed) {
  Image img = make_image(w, h, hex(0x000000));
  const Color sky_top = hex(0x1f4f8f), sky_low = hex(0xc3d8ec), haze = hex(0xc9dbea);
  struct Ridge {
    double base, amp, seed_off;
    Color rock, snow, shade;
    float fade;
  };
  const Ridge ridges[3] = {{0.50, 0.30, 1, hex(0x8193ab), hex(0xe4ecf5), hex(0xa9bbd2), 0.5f},
                           {0.62, 0.34, 2, hex(0x4e5d72), hex(0xf1f5fa), hex(0x9fb2cc), 0.22f},
                           {0.80, 0.30, 3, hex(0x2c3640), hex(0xf8fafc), hex(0x95aac6), 0.0f}};
  const double span = 2400.0;  // the noise's metres across the picture
  for (int x = 0; x < w; x++) {
    const double fx = x * span / w, step = 6 * span / w;
    for (int y = 0; y < h; y++) {
      const float t = static_cast<float>(y) / static_cast<float>(h);
      Color c = lerp(sky_top, sky_low, std::min(1.0f, t * 1.5f));
      for (int r = 0; r < 3; r++) {
        const Ridge& rd = ridges[r];
        const double s = seed + rd.seed_off * 101;
        const double top = (rd.base - rd.amp * ridge_height(fx, s)) * h;
        if (y < top) continue;
        // The face's slope across: positive where the ridge rises to the right (lit from the left).
        const double slope =
            (ridge_height(fx + step, s) - ridge_height(fx - step, s)) * rd.amp * h / 2;
        const double depth = (y - top) / h;
        const double snowline = 0.12 + 0.12 * ge::core::value_noise(fx, 0, 35, s + 5);
        // Lit faces (rising to the right) bright, the others in the sky's blue, blended across
        // the crease so a ridge reads as two planes, not as stripes.
        // Under the crest the faces break into ribs and gullies (2D noise stretched down the
        // fall line), which take over from the crest's slope a little way down.
        const double crest = ge::core::clamp(1 - depth / 0.08, 0, 1);
        const double ribs = ge::core::value_noise(fx + y * 0.35, y * 0.25, 22, s + 7) - 0.5;
        const auto lit = static_cast<float>(
            ge::core::clamp(0.5 + slope / 6 * crest + ribs * 1.6 * (1 - crest * 0.6), 0, 1));
        Color g =
            depth < snowline ? lerp(rd.shade, rd.snow, lit) : lerp(rd.rock, rd.shade, lit * 0.3f);
        if (r == 2 && depth > 0.07) {
          const double n = ge::core::value_noise(fx, y * 2.0, 9, s + 11);
          g = lerp(hex(0x16241e), hex(0x31443a), static_cast<float>(n));  // the woods
        }
        c = lerp(g, haze, rd.fade);
      }
      uint8_t* p = img.rgba.data() + (static_cast<size_t>(y) * w + x) * 4;
      blend_over(p, c);
    }
  }
  return img;
}

inline Menu front_door() {
  Menu m;
  Page& root = m.add_page("root");
  root.items = {
      {.id = "play",
       .text = "PLAY",
       .hint = "Ride the mountain, from the top of any lift",
       .prominent = true},
      {.id = "quick", .text = "QUICK START", .hint = "A mountain dealt at random, no questions"},
      {.id = "options", .text = "OPTIONS", .hint = "Picture, sound, controls", .opens = "options"},
      {.id = "replays", .text = "REPLAYS", .hint = "Runs kept, to watch again", .disabled = true},
      {.id = "quit", .text = "QUIT"}};
  Page& opt = m.add_page("options", "OPTIONS");
  opt.items = {{.id = "h-picture", .text = "PICTURE", .kind = ItemKind::label},
               {.id = "quality",
                .text = "QUALITY",
                .kind = ItemKind::choice,
                .hint = "Fitted to this machine on first start",
                .choices = {"LOW", "MEDIUM", "HIGH", "MAX"},
                .choice = 2},
               {.id = "fps", .text = "SHOW FRAME RATE", .kind = ItemKind::toggle},
               {.id = "h-sound", .text = "SOUND", .kind = ItemKind::label},
               {.id = "volume",
                .text = "MASTER VOLUME",
                .kind = ItemKind::slider,
                .value = 80,
                .min = 0,
                .max = 100,
                .step = 5,
                .unit = "%"},
               {.id = "sound", .text = "SOUND", .kind = ItemKind::toggle, .on = true},
               {.id = "back", .text = "BACK", .kind = ItemKind::back}};
  return m;
}

inline MenuStyle front_style() {
  MenuStyle st;
  st.logo = "FALL LINE";
  st.help = "ENTER SELECT    ESC BACK    ARROWS MOVE";
  st.footer = "v0.1.0 · lab";
  return st;
}

struct Viewport {
  int w, h;
  const char* name;
};

// "1280x720,390x844": every well-formed WxH in the list (empty when none is).
inline std::vector<Viewport> parse_viewports(const std::string& list) {
  std::vector<Viewport> out;
  size_t at = 0;
  while (at < list.size()) {
    const size_t comma = std::min(list.find(',', at), list.size());
    const std::string one = list.substr(at, comma - at);
    int w = 0, h = 0;
    const char* end = one.data() + one.size();
    const auto [wx, we] = std::from_chars(one.data(), end, w);
    const bool by = we == std::errc{} && wx != end && *wx == 'x';
    const auto [hx, he] = by ? std::from_chars(wx + 1, end, h) : std::from_chars_result{};
    if (by && he == std::errc{} && hx == end && w > 0 && h > 0)
      out.push_back({w, h, h > w ? "portrait" : "landscape"});
    at = comma + 1;
  }
  return out;
}

struct Backdrop {
  const char* id;
  const char* what;
  Background bg;
  bool scene = false;
};

// The painted range, its blurred copy (the "live scene"), the drift and the five backdrops over
// them, in the order the labs show them: colour, gradient, glow, image, scene.
// The image backdrop holds `range` by reference, so a Scenes stays where it is made.
struct Scenes {
  Image range, soft;
  Drift drift;
  std::vector<Backdrop> backdrops;
  Scenes() = default;
  Scenes(const Scenes&) = delete;
  Scenes& operator=(const Scenes&) = delete;
};

inline void make_scenes(Scenes& s, double seed) {
  s.range = painted_range(1600, 900, seed);
  s.soft = blurred(s.range, 18);
  Drift& drift = s.drift;
  drift.period_s = 24;
  drift.zoom_from = 1.0f, drift.zoom_to = 1.18f;
  drift.focus_from_x = 0.62f, drift.focus_to_x = 0.4f;
  drift.focus_from_y = drift.focus_to_y = 0.45f;
  std::vector<Backdrop>& backdrops = s.backdrops;
  backdrops.push_back({"color", "a flat colour", Background::color(hex(0x101b27)), false});
  {
    const Stop stops[3] = {{0, hex(0x0e2a47)}, {0.6f, hex(0x14385e)}, {1, hex(0x5d86b0)}};
    backdrops.push_back({"gradient", "a three-stop gradient, top to bottom",
                         Background::gradient(180, stops, 3), false});
  }
  {
    Background bg = Background::gradient(160, hex(0x0b1524), hex(0x1d3550));
    bg.glow(0.72f, 0.3f, 0.55f, hex(0xffc58a, 0.55f)).vignette(0.7f);
    backdrops.push_back({"glow", "a gradient with a warm glow and a vignette", bg, false});
  }
  {
    Background bg = Background::image(s.range, Fit::cover, drift);
    bg.scrim(Side::left, 0.7f, 0.55f).vignette(0.35f);
    backdrops.push_back(
        {"image", "a painted range drifting slowly, a scrim on the column's side", bg, false});
  }
  {
    Background bg = Background::scene();
    bg.tint(hex(0xd6e2ef)).scrim(Side::left, 0.6f, 0.6f).dim(0.12f);
    backdrops.push_back(
        {"scene", "the live scene (here the range, blurred) softened behind a scrim", bg, true});
  }
}

// The instrument panel's nine-slice frame, 24 px: a dark glass panel with a light rim, drawn by
// the raster itself; its corners are 8 px.
inline Image panel_frame() {
  Image img = make_image(24, 24, {0, 0, 0, 0});
  DrawList dl;
  dl.begin(24, 24);
  Box b;
  b.rect = {0.5f, 0.5f, 23, 23};
  b.radius = 7;
  b.fill = solid(hex(0x0b1524, 0.78f));
  b.border = 2;
  b.border_color = hex(0xd6e2ef, 0.85f);
  dl.box(b);
  ge::ui::render(dl, img);
  return img;
}

// An instrument panel over `range` at w x h, laid out for 1280x720 and scaled to the short side:
// a speed dial (a graded arc band, ticks, a polygon needle and a hub) bottom right, a second
// needle drawn as a turned box about its foot, a round-ended ring meter, a nine-sliced panel
// with a line graph, and a turned map of the range under a chevron, top right.
inline void instruments(DrawList& dl, int w, int h, const Image& range, const Image& frame,
                        const Font& font) {
  const float s = static_cast<float>(std::min(w, h)) / 720.0f;
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  Box bg;
  bg.rect = {0, 0, fw, fh};
  bg.image = &range;
  bg.uv = fit_uv(range, fw, fh, Fit::cover);
  dl.box(bg);
  auto dir = [](double a) {
    return Vec2{static_cast<float>(core::libm::sin(a)), static_cast<float>(-core::libm::cos(a))};
  };
  // The dial.
  const Vec2 c{fw - 190 * s, fh - 180 * s};
  const float r = 120 * s;
  dl.arc(c, r + 18 * s, 4 * s, 0, 6.3f, solid(hex(0x000000, 0.35f)), false, 6 * s);
  const Stop band[3] = {{0, hex(0x4caf50)}, {0.55f, hex(0xffd54f)}, {1, hex(0xd8312b)}};
  dl.arc(c, r, 14 * s, -2.3f, 2.3f, linear(0, 0, 2 * (r + 7 * s), 0, band, 3));
  for (int k = 0; k <= 23; ++k) {
    const double a = -2.3 + 4.6 * k / 23.0;
    const Vec2 d = dir(a);
    const bool major = k % 5 == 0;
    const float in = r - (major ? 34 : 24) * s, out = r - 12 * s;
    dl.line({c.x + d.x * in, c.y + d.y * in}, {c.x + d.x * out, c.y + d.y * out},
            (major ? 4.0f : 2.0f) * s, hex(0xffffff, major ? 1.0f : 0.7f));
  }
  {
    const double a = 0.85;  // the needle's angle
    const Vec2 d = dir(a), n{-d.y, d.x};
    const float tip = r - 16 * s, tail = 22 * s, wb = 7 * s;
    const Vec2 needle[4] = {{c.x + d.x * tip, c.y + d.y * tip},
                            {c.x + n.x * wb, c.y + n.y * wb},
                            {c.x - d.x * tail, c.y - d.y * tail},
                            {c.x - n.x * wb, c.y - n.y * wb}};
    const Stop red[2] = {{0, hex(0xff8a80)}, {1, hex(0xd8312b)}};
    dl.polygon(needle, linear(0, 0, 2 * tip, 2 * tip, red, 2));
    Box hub;
    hub.rect = {c.x - 12 * s, c.y - 12 * s, 24 * s, 24 * s};
    hub.radius = 12 * s;
    hub.fill = solid(hex(0x1d3550));
    hub.border = 3 * s;
    hub.border_color = hex(0xd6e2ef);
    dl.box(hub);
  }
  dl.text(font, 44 * s, c.x, c.y + 70 * s, "87", hex(0xffffff), Align::center);
  dl.text(font, 18 * s, c.x, c.y + 94 * s, "KM/H", hex(0xd6e2ef, 0.8f), Align::center, 2 * s);
  // A needle drawn as a turned box about its foot, on a small dial left of the big one.
  const Vec2 c2{fw - 420 * s, fh - 120 * s};
  dl.arc(c2, 50 * s, 6 * s, -1.6f, 1.6f, solid(hex(0x1d3550, 0.85f)));
  Box pin;
  pin.rect = {c2.x - 3 * s, c2.y - 46 * s, 6 * s, 46 * s};
  pin.radius = 3 * s;
  pin.fill = solid(hex(0x80d8ff));
  pin.pivot = Vec2{3 * s, 46 * s};
  pin.rotation = -0.7f;
  dl.box(pin);
  // A ring meter with round ends over its dim track.
  const Vec2 c3{fw - 420 * s, fh - 250 * s};
  dl.arc(c3, 36 * s, 10 * s, 0, 6.3f, solid(hex(0xffffff, 0.18f)));
  dl.arc(c3, 36 * s, 10 * s, 0, 4.4f, solid(hex(0xffd54f)), true);
  dl.text(font, 22 * s, c3.x, c3.y + 8 * s, "70%", hex(0xffffff), Align::center);
  // A nine-sliced panel holding a line graph.
  Box panel;
  // On a phone held upright the small gauges stand left of the dial, so the panel goes above them.
  panel.rect = {40 * s, fh - (h > w ? 470 : 200) * s, 360 * s, 160 * s};
  panel.image = &frame;
  panel.uv = {0, 0, 24, 24};
  panel.slice = {8, 8, 8, 8};
  panel.slice_to = {12 * s, 12 * s, 12 * s, 12 * s};
  dl.box(panel);
  dl.text(font, 20 * s, panel.rect.x + 20 * s, panel.rect.y + 34 * s, "ALTITUDE", hex(0xd6e2ef),
          Align::left, 2 * s);
  std::vector<Vec2> graph;
  for (int k = 0; k <= 32; ++k) {
    const float t = static_cast<float>(k) / 32;
    const double y = 0.5 + 0.35 * core::libm::sin(t * 7.0) * (1 - t * 0.6) - 0.2 * t;
    graph.push_back({panel.rect.x + (24 + t * 312) * s,
                     panel.rect.y + (60 + static_cast<float>(1 - y) * 80) * s});
  }
  dl.polyline(graph, 3 * s, hex(0x80d8ff));
  // The map, a square of the range turned under the skier's heading, clipped to its frame.
  const Rect map{fw - 250 * s, 30 * s, 220 * s, 220 * s};
  Box rim;
  rim.rect = inset(map, 4 * s);
  rim.radius = 18 * s;
  rim.fill = solid(hex(0x0b1524, 0.8f));
  dl.box(rim);
  dl.push_clip(map);
  Box ground;
  ground.rect = {map.x - 50 * s, map.y - 50 * s, map.w + 100 * s, map.h + 100 * s};
  ground.image = &range;
  ground.uv = {500, 250, 600, 600};
  ground.rotation = 0.45f;
  ground.radius = 40 * s;
  dl.box(ground);
  dl.pop_clip();
  const Vec2 mc{map.x + map.w / 2, map.y + map.h / 2};
  const Vec2 chevron[4] = {{mc.x, mc.y - 16 * s},
                           {mc.x + 11 * s, mc.y + 12 * s},
                           {mc.x, mc.y + 5 * s},
                           {mc.x - 11 * s, mc.y + 12 * s}};
  const Vec2 left[3] = {chevron[0], chevron[2], chevron[3]};
  const Vec2 right[3] = {chevron[0], chevron[1], chevron[2]};
  dl.polygon(left, solid(hex(0xffd54f)));  // a dart is concave: two convex halves
  dl.polygon(right, solid(hex(0xffd54f)));
  const Vec2 north[3] = {
      {mc.x, map.y + 8 * s}, {mc.x + 8 * s, map.y + 22 * s}, {mc.x - 8 * s, map.y + 22 * s}};
  dl.polygon(north, solid(hex(0xd8312b)));
  Box edge;
  edge.rect = map;
  edge.radius = 18 * s;
  edge.fill = solid({0, 0, 0, 0});
  edge.border = 3 * s;
  edge.border_color = hex(0xd6e2ef, 0.9f);
  dl.box(edge);
}

}  // namespace ge::lab::menu_scenes
