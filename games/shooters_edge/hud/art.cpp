// The HUD's free pictures, painted in code: every icon is a few boxes, polygons, lines and arcs
// on the 2D layer, drawn in art pixels by a `Canvas` that scales them by the set's density and
// painted by the CPU raster, so the set is the same bits on every platform and needs no file.
// The look: dark bronze panels with bevels and rivets, warm metal figures, and each icon a
// readable silhouette in one or two colours.
//
// ge:summary Shooter's Edge's free HUD pictures, painted in code on the CPU raster
// ge:keywords hud free-art paint canvas icons panel bevel rivet digits portrait weapons
// ge:keywords ammo armour keys powerups runes crosshair finish
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#include "shooters_edge/hud/art.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include "ui/draw.hpp"
#include "ui/raster.hpp"

namespace shooters_edge::hud {

namespace ui = ge::ui;

namespace {

constexpr float kDensity = 4;

// Painting in art pixels onto a picture `density` times finer.
class Canvas {
 public:
  Canvas(float w, float h) : w_(w), h_(h) {
    dl_.begin(static_cast<int>(w * kDensity), static_cast<int>(h * kDensity));
  }
  static float s(float v) { return v * kDensity; }

  void rect(float x, float y, float w, float h, const ui::Paint& p, float radius = 0) {
    ui::Box b;
    b.rect = {s(x), s(y), s(w), s(h)};
    b.radius = s(radius);
    b.fill = p;
    dl_.box(b);
  }
  void rect(float x, float y, float w, float h, ui::Color c, float radius = 0) {
    rect(x, y, w, h, ui::solid(c), radius);
  }
  // A box filled top to bottom from `a` to `b`.
  void ramp(float x, float y, float w, float h, ui::Color a, ui::Color b, float radius = 0) {
    const ui::Stop st[2] = {{0, a}, {1, b}};
    rect(x, y, w, h, ui::linear(0, 0, 0, s(h), st, 2), radius);
  }
  void frame(float x, float y, float w, float h, float t, ui::Color c, float radius = 0) {
    ui::Box b;
    b.rect = {s(x), s(y), s(w), s(h)};
    b.radius = s(radius);
    b.fill = ui::solid({0, 0, 0, 0});
    b.border = s(t);
    b.border_color = c;
    dl_.box(b);
  }
  void poly(std::initializer_list<ui::Vec2> pts, ui::Color c) {
    std::vector<ui::Vec2> v;
    for (const ui::Vec2& p : pts) v.push_back({s(p.x), s(p.y)});
    dl_.polygon(v, ui::solid(c));
  }
  void line(float x0, float y0, float x1, float y1, float width, ui::Color c) {
    dl_.line({s(x0), s(y0)}, {s(x1), s(y1)}, s(width), c);
  }
  void ring(float cx, float cy, float r, float width, ui::Color c, float from = 0,
            float to = 6.2831853f) {
    dl_.arc({s(cx), s(cy)}, s(r), s(width), from, to, ui::solid(c));
  }
  void dot(float cx, float cy, float r, ui::Color c) { rect(cx - r, cy - r, 2 * r, 2 * r, c, r); }
  // A bevelled plate: a ramp, a light top-left edge and a dark bottom-right one.
  void plate(float x, float y, float w, float h, ui::Color top, ui::Color bottom,
             float radius = 0) {
    ramp(x, y, w, h, top, bottom, radius);
    rect(x, y, w, 0.75f, ui::with_alpha(ui::hex(0xfff2d0), 0.35f));
    rect(x, y, 0.75f, h, ui::with_alpha(ui::hex(0xfff2d0), 0.2f));
    rect(x, y + h - 0.75f, w, 0.75f, ui::with_alpha(ui::hex(0x000000), 0.5f));
    rect(x + w - 0.75f, y, 0.75f, h, ui::with_alpha(ui::hex(0x000000), 0.4f));
  }
  // A recess: dark, its shadow on the top-left and its light on the bottom-right.
  void recess(float x, float y, float w, float h) {
    ramp(x, y, w, h, ui::hex(0x120c08), ui::hex(0x2a1f16));
    rect(x, y, w, 0.75f, ui::with_alpha(ui::hex(0x000000), 0.6f));
    rect(x, y + h - 0.5f, w, 0.5f, ui::with_alpha(ui::hex(0xffe0b0), 0.18f));
  }
  void rivet(float cx, float cy) {
    dot(cx + 0.3f, cy + 0.3f, 1.1f, ui::with_alpha(ui::hex(0x000000), 0.55f));
    dot(cx, cy, 1.0f, ui::hex(0x9c8360));
    dot(cx - 0.3f, cy - 0.3f, 0.45f, ui::hex(0xe8d7b0));
  }
  ui::DrawList& list() { return dl_; }

  ui::Image paint() const {
    ui::Image img = ui::make_image(static_cast<int>(w_ * kDensity), static_cast<int>(h_ * kDensity),
                                   {0, 0, 0, 0});
    ui::render(dl_, img);
    return img;
  }

 private:
  float w_, h_;
  ui::DrawList dl_;
};

ui::Color mix(ui::Color a, ui::Color b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
          a.a + (b.a - a.a) * t};
}

// Every pixel's colour times `k` (alpha kept), clamped: a picture lit up or dimmed.
ui::Image brightened(const ui::Image& src, float k, ui::Color add = {0, 0, 0, 0}) {
  ui::Image out = src;
  for (size_t i = 0; i < out.rgba.size(); i += 4) {
    if (out.rgba[i + 3] == 0) continue;
    const float extra[3] = {add.r, add.g, add.b};
    for (size_t c = 0; c < 3; ++c) {
      const float v = static_cast<float>(out.rgba[i + c]) * k + extra[c] * 255.0f;
      out.rgba[i + c] = static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
    }
  }
  return out;
}

// --- The panels ---------------------------------------------------------------------------

ui::Image paint_bar() {
  Canvas c(320, 24);
  c.plate(0, 0, 320, 24, ui::hex(0x5a4430), ui::hex(0x2c2018));
  // Brushed grain: faint horizontal streaks.
  for (int k = 0; k < 11; ++k)
    c.rect(0, 2.0f + static_cast<float>(k) * 2.0f, 320, 0.5f,
           ui::with_alpha(ui::hex(0xffe0b0), k % 3 == 0 ? 0.06f : 0.03f));
  // The icon wells and the number fields.
  for (const float x : {0.0f, 112.0f, 224.0f}) c.recess(x + 1, 1, 22, 22);
  for (const float x : {24.0f, 136.0f, 248.0f}) c.recess(x + 1, 2, 70, 20);
  for (const float x : {98.0f, 210.0f}) {
    c.rivet(x + 6, 6);
    c.rivet(x + 6, 18);
  }
  return c.paint();
}

ui::Image paint_inventory_bar() {
  Canvas c(320, 24);
  c.plate(0, 0, 320, 24, ui::hex(0x4a3828), ui::hex(0x251b14));
  // The four counts' boxes on the top line, the guns' row under them, the slots on the right.
  for (int k = 0; k < kAmmoKinds; ++k) c.recess(static_cast<float>(k) * 48 + 3, 0.5f, 28, 7.5f);
  c.recess(0.5f, 8, 191, 15.5f);
  c.recess(192.5f, 8, 95, 15.5f);
  c.recess(288.5f, 8, 31, 15.5f);
  for (int k = 1; k < 8; ++k)
    c.rect(static_cast<float>(k) * 24, 8.5f, 0.5f, 15, ui::with_alpha(ui::hex(0x000000), 0.4f));
  return c.paint();
}

// --- The icons ----------------------------------------------------------------------------

ui::Image paint_armour(ui::Color base) {
  Canvas c(24, 24);
  const ui::Color dark = mix(base, ui::hex(0x000000), 0.55f);
  const ui::Color light = mix(base, ui::hex(0xffffff), 0.45f);
  // A cuirass from convex pieces: the chest plate, the two shoulder guards, the neck's notch.
  c.poly({{4.5f, 6}, {19.5f, 6}, {19, 15}, {12, 21.5f}, {5, 15}}, dark);
  c.poly({{5.5f, 7}, {18.5f, 7}, {18, 14.5f}, {12, 20.2f}, {6, 14.5f}}, base);
  c.poly({{6.5f, 8}, {11, 8}, {11, 18}, {7, 14}}, light);
  c.poly({{3, 4}, {9, 3}, {10, 7}, {4, 9}}, dark);
  c.poly({{21, 4}, {15, 3}, {14, 7}, {20, 9}}, dark);
  c.poly({{3.8f, 4.6f}, {8.6f, 3.8f}, {9.2f, 6.4f}, {4.4f, 8}}, light);
  c.poly({{20.2f, 4.6f}, {15.4f, 3.8f}, {14.8f, 6.4f}, {19.6f, 8}}, base);
  c.poly({{9.5f, 6}, {14.5f, 6}, {12, 9.5f}}, ui::hex(0x1a120c));
  c.line(12, 10.5f, 12, 18.5f, 0.7f, dark);
  return c.paint();
}

ui::Image paint_health() {
  Canvas c(24, 24);
  c.rect(3, 3, 18, 18, ui::hex(0x3a0c08), 4);
  c.ramp(4, 4, 16, 16, ui::hex(0xf2ede4), ui::hex(0xb8b0a4), 3.5f);
  c.rect(10, 6.5f, 4, 11, ui::hex(0xd02a1c), 0.5f);
  c.rect(6.5f, 10, 11, 4, ui::hex(0xd02a1c), 0.5f);
  return c.paint();
}

ui::Image paint_shield_armour() {
  Canvas c(24, 24);
  c.dot(12, 12, 9.5f, ui::hex(0x3a0d08));
  c.dot(12, 12, 8.5f, ui::hex(0xc8281c));
  c.ring(12, 12, 6, 1.4f, ui::hex(0xffcf5a));
  c.dot(12, 12, 2.4f, ui::hex(0xffcf5a));
  return c.paint();
}

ui::Image paint_ammo(Ammo a) {
  Canvas c(24, 24);
  switch (a) {
    case Ammo::shells:
      for (const float x : {5.0f, 13.0f}) {
        c.ramp(x, 4, 6, 12, ui::hex(0xe0473a), ui::hex(0x8a1d14), 1);
        c.ramp(x - 0.5f, 15, 7, 5, ui::hex(0xf2d27a), ui::hex(0x9a7428), 0.5f);
        c.rect(x + 1, 5, 1, 9, ui::with_alpha(ui::hex(0xffffff), 0.35f));
      }
      break;
    case Ammo::nails:
      for (const float x : {6.0f, 11.0f, 16.0f}) {
        c.poly({{x - 1.2f, 6}, {x + 1.2f, 6}, {x + 0.4f, 20}, {x - 0.4f, 20}}, ui::hex(0xb9c3cc));
        c.rect(x - 2.2f, 4, 4.4f, 2.4f, ui::hex(0xe4e9ee), 0.6f);
      }
      c.rect(4, 11, 16, 2, ui::hex(0x6a4a2a));
      break;
    case Ammo::rockets:
      c.poly({{12, 2}, {15, 7}, {15, 17}, {9, 17}, {9, 7}}, ui::hex(0x8d9aa4));
      c.poly({{12, 2}, {15, 7}, {9, 7}}, ui::hex(0xd8432c));
      c.poly({{9, 13}, {5, 20}, {9, 18}}, ui::hex(0x5d6a74));
      c.poly({{15, 13}, {19, 20}, {15, 18}}, ui::hex(0x5d6a74));
      c.rect(10, 8, 1.2f, 8, ui::with_alpha(ui::hex(0xffffff), 0.4f));
      c.poly({{10, 17}, {14, 17}, {12, 22}}, ui::hex(0xffb03a));
      break;
    case Ammo::cells:
      c.ramp(5, 4, 14, 17, ui::hex(0x5d7f9a), ui::hex(0x2c4256), 1.5f);
      c.rect(9, 2.5f, 6, 2, ui::hex(0x9fb4c4), 0.5f);
      c.poly({{13.5f, 6}, {8, 13.5f}, {11.5f, 13.5f}, {10.5f, 19}, {16, 11}, {12.5f, 11}},
             ui::hex(0xfff07a));
      break;
    case Ammo::none:
      break;
  }
  return c.paint();
}

// The portrait: a helmeted head, `hurt` 0..1 how battered, wincing or not, with a powerup's look.
enum class Look : uint8_t { plain, cloak, shield, cloak_shield, damage };

ui::Image paint_face(float hurt, bool wince, Look look) {
  Canvas c(24, 24);
  const bool cloak = look == Look::cloak || look == Look::cloak_shield;
  const float body = cloak ? 0.25f : 1;
  const ui::Color skin = mix(ui::hex(0xc9926a), ui::hex(0x8a4a34), hurt);
  // The helmet's dome, the face under its brim, the jaw.
  c.rect(4, 2, 16, 12, ui::with_alpha(ui::hex(0x56606a), body), 7);
  c.rect(5, 2.5f, 7, 4, ui::with_alpha(ui::hex(0x9aa6b0), 0.5f * body), 3);
  c.rect(6, 9, 12, 12, ui::with_alpha(skin, body), 4);
  c.rect(3.5f, 8, 17, 3, ui::with_alpha(ui::hex(0x3a424a), body), 1);
  // The eyes: open, or narrowed in a wince.
  ui::Color eye = ui::hex(0xf4f0e0);
  if (look == Look::shield || look == Look::cloak_shield) eye = ui::hex(0xffd84a);
  if (look == Look::damage) eye = ui::hex(0x7ad8ff);
  const float eh = wince ? 0.8f : 2.0f;
  for (const float x : {8.5f, 13.5f}) {
    c.rect(x, 12, 3, eh, eye, 0.5f);
    if (!wince) c.rect(x + 1, 12.4f, 1.2f, 1.2f, ui::hex(0x1a1410));
  }
  // The mouth: a line, gritted when wincing or boosted.
  const bool teeth = wince || look == Look::damage;
  c.rect(9.5f, 17, 5, teeth ? 1.6f : 0.8f, teeth ? ui::hex(0xece4d0) : ui::hex(0x4a2418), 0.4f);
  // Battered: cuts and blood, more of them the lower the band.
  const ui::Color blood = ui::with_alpha(ui::hex(0xa0140c), body);
  if (hurt > 0.2f) c.line(7, 15, 9, 19, 0.7f, blood);
  if (hurt > 0.4f) c.line(16, 11, 15, 16, 0.7f, blood);
  if (hurt > 0.6f) {
    c.rect(13, 17.5f, 2, 3, blood, 0.6f);
    c.line(6, 10, 10, 11, 0.8f, blood);
  }
  if (hurt > 0.8f) {
    c.rect(6, 14, 3, 5, blood, 1);
    c.rect(15, 9.5f, 4, 3, ui::with_alpha(ui::hex(0x3a0c08), body), 1);
  }
  if (look == Look::shield || look == Look::cloak_shield)
    c.ring(12, 12, 10.5f, 1.2f, ui::with_alpha(ui::hex(0xffd84a), 0.85f));
  if (look == Look::damage) c.ring(12, 12, 10.5f, 1.2f, ui::with_alpha(ui::hex(0x5ac8ff), 0.85f));
  return c.paint();
}

// The guns, side on, muzzle to the right, in 24 x 16 (the beam gun 48 x 16).
ui::Image paint_gun(Weapon w) {
  const float wide = w == Weapon::beam_gun ? 48 : 24;
  Canvas c(wide, 16);
  const ui::Color steel = ui::hex(0xa8b0b6), dark = ui::hex(0x4c545a), wood = ui::hex(0x8a5a32);
  switch (w) {
    case Weapon::shotgun:
      c.poly({{2, 9}, {8, 8}, {8, 12}, {3, 13}}, wood);
      c.rect(8, 7, 15, 2, steel, 0.5f);
      c.rect(9, 9, 7, 2, dark);
      break;
    case Weapon::double_shotgun:
      c.poly({{2, 9}, {8, 8}, {8, 12}, {3, 13}}, wood);
      c.rect(8, 6, 15, 1.8f, steel, 0.5f);
      c.rect(8, 8.2f, 15, 1.8f, steel, 0.5f);
      c.rect(9, 10, 6, 2, dark);
      break;
    case Weapon::nailgun:
      c.poly({{2, 9}, {7, 8}, {7, 12}, {3, 13}}, dark);
      c.rect(7, 6, 12, 5, steel, 1);
      c.rect(19, 7.5f, 4, 2, dark);
      c.rect(10, 11, 3, 3, dark);
      break;
    case Weapon::heavy_nailgun:
      c.poly({{2, 9}, {6, 8}, {6, 12}, {3, 13}}, dark);
      c.rect(6, 5, 14, 7, steel, 1.5f);
      for (const float y : {6.0f, 8.0f, 10.0f}) c.rect(20, y, 3, 1.2f, dark);
      c.rect(9, 12, 4, 2.5f, dark);
      break;
    case Weapon::grenade_launcher:
      c.poly({{2, 9}, {7, 8}, {7, 12}, {3, 13}}, wood);
      c.rect(7, 6, 15, 5, ui::hex(0x5f6e48), 2.5f);
      c.rect(19, 5.5f, 3, 6, dark, 1);
      break;
    case Weapon::rocket_launcher:
      c.rect(3, 5, 19, 6, ui::hex(0x6a7480), 3);
      c.rect(20, 4.5f, 3, 7, dark, 1);
      c.rect(9, 11, 3, 3.5f, dark);
      c.rect(4, 6, 6, 1, ui::with_alpha(ui::hex(0xffffff), 0.3f));
      break;
    case Weapon::beam_gun:
      c.poly({{2, 9}, {8, 8}, {8, 12}, {3, 13}}, dark);
      c.rect(8, 6, 22, 5, steel, 1.5f);
      for (const float x : {11.0f, 15.0f, 19.0f, 23.0f})
        c.ring(x, 8.5f, 2.2f, 0.8f, ui::hex(0x6ab8ff));
      c.line(31, 8.5f, 37, 6, 0.8f, ui::hex(0x9ad8ff));
      c.line(37, 6, 41, 10, 0.8f, ui::hex(0x9ad8ff));
      c.line(41, 10, 46, 7.5f, 0.8f, ui::hex(0x9ad8ff));
      break;
    case Weapon::melee:
      break;
  }
  return c.paint();
}

ui::Image paint_key(ui::Color metal) {
  Canvas c(16, 16);
  const ui::Color dark = mix(metal, ui::hex(0x000000), 0.5f);
  c.ring(5, 8, 3, 1.6f, metal);
  c.rect(7.5f, 7.2f, 7, 1.7f, metal);
  c.rect(11, 8.9f, 1.4f, 2.5f, metal);
  c.rect(13.2f, 8.9f, 1.4f, 2, metal);
  c.rect(8, 7.4f, 6, 0.5f, ui::with_alpha(ui::hex(0xffffff), 0.5f));
  c.ring(5, 8, 3, 0.5f, dark, 1.2f, 3.0f);
  return c.paint();
}

ui::Image paint_power(Power p) {
  Canvas c(16, 16);
  switch (p) {
    case Power::cloak:
      c.dot(8, 8, 7, ui::hex(0x2a1640));
      c.poly({{2, 8}, {5, 5}, {8, 4.2f}, {11, 5}, {14, 8}, {11, 11}, {8, 11.8f}, {5, 11}},
             ui::hex(0xc8a8f0));
      c.dot(8, 8, 2.4f, ui::hex(0x5a2ca0));
      break;
    case Power::shield:
      c.poly({{8, 1.5f}, {14, 4}, {13, 10}, {8, 14.5f}, {3, 10}, {2, 4}}, ui::hex(0x8a1810));
      c.poly({{8, 3}, {12.5f, 5}, {11.8f, 9.5f}, {8, 13}, {4.2f, 9.5f}, {3.5f, 5}},
             ui::hex(0xe03a24));
      c.poly({{8, 4.5f}, {9, 7.2f}, {11.6f, 7.2f}, {9.5f, 8.8f}, {10.3f, 11.4f}, {8, 9.8f}},
             ui::hex(0xffd04a));
      c.poly({{8, 4.5f}, {8, 9.8f}, {5.7f, 11.4f}, {6.5f, 8.8f}, {4.4f, 7.2f}, {7, 7.2f}},
             ui::hex(0xffd04a));
      break;
    case Power::suit:
      c.rect(3, 2, 10, 12, ui::hex(0x2e6a2a), 4);
      c.rect(4.5f, 4, 7, 4, ui::hex(0xb8f0a0), 1.5f);
      c.dot(8, 11, 2, ui::hex(0x1a3a18));
      c.ring(8, 11, 2, 0.6f, ui::hex(0x8ad070));
      break;
    case Power::damage:
      c.dot(8, 8, 7, ui::hex(0x0e2a4a));
      // Eight rays from the centre: the cosines and sines of the eighths, written out.
      for (int k = 0; k < 8; ++k) {
        static constexpr float kCos[8] = {1, 0.7071f, 0, -0.7071f, -1, -0.7071f, 0, 0.7071f};
        static constexpr float kSin[8] = {0, 0.7071f, 1, 0.7071f, 0, -0.7071f, -1, -0.7071f};
        c.line(8, 8, 8 + 6 * kCos[k], 8 + 6 * kSin[k], 1.2f, ui::hex(0x6ad0ff));
      }
      c.dot(8, 8, 2.6f, ui::hex(0xe8f8ff));
      break;
  }
  return c.paint();
}

ui::Image paint_rune(int k) {
  static constexpr uint32_t kColour[4] = {0xd8402c, 0x3ac84a, 0x4a8ae8, 0xe8c83a};
  Canvas c(8, 16);
  const ui::Color col = ui::hex(kColour[k]);
  c.poly({{4, 2}, {7, 8}, {4, 14}, {1, 8}}, mix(col, ui::hex(0x000000), 0.4f));
  c.poly({{4, 3.5f}, {6, 8}, {4, 12.5f}, {2, 8}}, col);
  c.line(4, 5, 4, 11, 0.5f, ui::with_alpha(ui::hex(0xffffff), 0.6f));
  return c.paint();
}

// A figure from a charset, 24 x 24 art px, ramped from `top` to `bottom` over a dark outline.
ui::Image paint_digit(char ch, ui::Color top, ui::Color bottom) {
  const int px = static_cast<int>(24 * kDensity);
  ui::DrawList dl;
  dl.begin(px, px);
  const ge::hud::Face face = ge::hud::Face::bitmap(ge::charset::FontId::square);
  const float size = 26 * kDensity;  // a cell is twice as tall as wide: 13 x 26 art px
  const std::string s(1, ch);
  ge::hud::draw_text(dl, face, size, static_cast<float>(px) / 2, 21.5f * kDensity, s, {1, 1, 1, 1},
                     ui::Align::center);
  ui::Image mask = ui::make_image(px, px, {0, 0, 0, 0});
  ui::render(dl, mask);
  // Wider: the figure is stretched across 20 of the cell's 24 px.
  ui::Image wide = ui::make_image(px, px, {0, 0, 0, 0});
  for (int y = 0; y < px; ++y)
    for (int x = 0; x < px; ++x) {
      const float sx = static_cast<float>(px) / 2 +
                       (static_cast<float>(x) + 0.5f - static_cast<float>(px) / 2) * 0.68f;
      const ui::Color m = ui::sample(mask, sx, static_cast<float>(y) + 0.5f);
      wide.rgba[(static_cast<size_t>(y) * px + x) * 4 + 3] = static_cast<uint8_t>(m.a * 255);
    }
  ui::Image out = ui::make_image(px, px, {0, 0, 0, 0});
  const int o = static_cast<int>(kDensity);  // the outline: one art pixel
  for (int y = 0; y < px; ++y)
    for (int x = 0; x < px; ++x) {
      uint8_t near = 0;
      for (int dy = -o; dy <= o; dy += o)
        for (int dx = -o; dx <= o; dx += o) {
          const int sx = std::clamp(x + dx, 0, px - 1), sy = std::clamp(y + dy, 0, px - 1);
          near = std::max(near, wide.rgba[(static_cast<size_t>(sy) * px + sx) * 4 + 3]);
        }
      const uint8_t a = wide.rgba[(static_cast<size_t>(y) * px + x) * 4 + 3];
      const ui::Color ink = mix(top, bottom, static_cast<float>(y) / static_cast<float>(px));
      const float fa = static_cast<float>(a) / 255.0f, fo = static_cast<float>(near) / 255.0f;
      uint8_t* d = &out.rgba[(static_cast<size_t>(y) * px + x) * 4];
      const ui::Color line = ui::hex(0x140c06);
      d[0] = static_cast<uint8_t>((line.r + (ink.r - line.r) * fa) * 255);
      d[1] = static_cast<uint8_t>((line.g + (ink.g - line.g) * fa) * 255);
      d[2] = static_cast<uint8_t>((line.b + (ink.b - line.b) * fa) * 255);
      d[3] = static_cast<uint8_t>(std::max(fa, fo * 0.9f) * 255);
    }
  return out;
}

ui::Image paint_crosshair() {
  Canvas c(16, 16);
  const ui::Color ink = ui::with_alpha(ui::hex(0xffffff), 0.95f);
  const ui::Color rim = ui::with_alpha(ui::hex(0x000000), 0.6f);
  for (const ui::Color col : {rim, ink}) {
    const float w = col.r > 0.5f ? 1.0f : 2.0f;
    c.line(8, 2, 8, 5.5f, w, col);
    c.line(8, 10.5f, 8, 14, w, col);
    c.line(2, 8, 5.5f, 8, w, col);
    c.line(10.5f, 8, 14, 8, w, col);
  }
  c.dot(8, 8, 0.8f, ink);
  return c.paint();
}

}  // namespace

void HudArt::finish() {
  for (size_t k = 0; k < 11; ++k) {
    sheet.glyph[k] = digit[k].empty() ? nullptr : &digit[k];
    sheet.alt[k] = digit_low[k].empty() ? nullptr : &digit_low[k];
  }
  sheet.density = density;
  count_face = glyphs.empty() ? ge::hud::Face::bitmap(count_font)
                              : ge::hud::Face::glyph_sheet(glyphs, digits_at);
}

HudArt free_art() {
  HudArt a;
  a.density = kDensity;
  a.bar = paint_bar();
  a.inventory_bar = paint_inventory_bar();
  a.armour = {paint_armour(ui::hex(0x4caa3c)), paint_armour(ui::hex(0xd8b02c)),
              paint_armour(ui::hex(0xd03a28))};
  a.health = paint_health();
  a.armour_shield = paint_shield_armour();
  for (int k = 0; k < kAmmoKinds; ++k)
    a.ammo[static_cast<size_t>(k)] = paint_ammo(static_cast<Ammo>(k));
  for (int b = 0; b < kFaceBands; ++b) {
    const float hurt = static_cast<float>(kFaceBands - 1 - b) / static_cast<float>(kFaceBands - 1);
    a.face[static_cast<size_t>(b)] = {paint_face(hurt, false, Look::plain),
                                      paint_face(hurt, true, Look::plain)};
  }
  a.face_cloak = paint_face(0, false, Look::cloak);
  a.face_shield = paint_face(0, false, Look::shield);
  a.face_cloak_shield = paint_face(0, false, Look::cloak_shield);
  a.face_damage = paint_face(0, false, Look::damage);
  for (int k = 0; k < kGuns; ++k) {
    const auto i = static_cast<size_t>(k);
    a.gun[i] = brightened(paint_gun(static_cast<Weapon>(k + 1)), 0.8f);
    a.gun_lit[i] = brightened(paint_gun(static_cast<Weapon>(k + 1)), 1.15f, {0.12f, 0.1f, 0.0f});
    for (int f = 0; f < kFlashFrames; ++f) {
      const float glow = 0.3f - 0.06f * static_cast<float>(f);
      a.gun_flash[i][static_cast<size_t>(f)] =
          brightened(paint_gun(static_cast<Weapon>(k + 1)), 1.1f, {glow, glow * 0.8f, 0});
    }
  }
  a.key = {paint_key(ui::hex(0xc4ccd4)), paint_key(ui::hex(0xe8b830))};
  for (int k = 0; k < kPowers; ++k)
    a.power[static_cast<size_t>(k)] = paint_power(static_cast<Power>(k));
  for (int k = 0; k < 4; ++k) a.rune[static_cast<size_t>(k)] = paint_rune(k);
  for (int k = 0; k < 11; ++k) {
    const char ch = k < 10 ? static_cast<char>('0' + k) : '-';
    a.digit[static_cast<size_t>(k)] = paint_digit(ch, ui::hex(0xf2c46a), ui::hex(0xa8642a));
    a.digit_low[static_cast<size_t>(k)] = paint_digit(ch, ui::hex(0xff6a50), ui::hex(0xa8180e));
  }
  a.crosshair = paint_crosshair();
  a.count_font = ge::charset::FontId::terminal;
  a.count_color = ui::hex(0xf2c46a);
  a.finish();
  return a;
}

}  // namespace shooters_edge::hud
