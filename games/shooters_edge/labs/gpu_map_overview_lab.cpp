// LAB `map-overview`: A MAP'S INTENDED GAMEPLAY FROM ABOVE, for any version 29 map. The map is
// drawn straight down on the CPU from its own faces: every floor's texture under its lightmap, the
// highest floor in each column, the levels told apart by a soft shade where a wall or a higher
// floor stands over a lower one and by a low sun's shadows; water, slime and lava laid over what
// lies under them. Over it, the INTENT (the plan's `area` and `trick` lines, or the same lines in
// an `.intent` file beside a map made anywhere else): each area washed and outlined in its
// intent's colour (a camp hatched), numbered, named; each trick an arrow from where it starts to
// where it lands, its kind on it and its colour its level. The ITEMS are pictures of the game's
// own models (its pack's armour, powers and weapons; its health and ammo boxes, which are small
// maps) drawn on the CPU from three quarters above, cut out and set larger than in the game, the
// powers glowing and the respawn beside them; the starts and the teleporters marked. A panel
// beside it keys the colours and lists the areas with their notes.
//
// THE PHOTO PASS: the lab writes `views.txt`, a place and a look for each area (or, with no
// intent, spots spread over the floors), in the form `se-client --views` takes, so
// `.agents/skills/map-design/scripts/photos.sh` photographs every room in the game's own window.
//
// With `--game` (a game folder, `<basedir>/<game>`: its packs and loose files) and `--map` (a
// name under maps/ or a path to a .bsp) it is that map with the pack's models; without, the lab's
// sample plan compiled with patterns and the items drawn as badges, so it runs on any machine.
//
// ge:summary Lab: a map's intended gameplay from above: floors, areas, tricks, model item icons
// ge:keywords lab map-overview overview intended-gameplay top-down plan-view floor-plan areas
// ge:keywords tricks item-icons model-icons camp hold fight route power perch spawn legend
// ge:keywords photo-pass views map-design intent
// ge:skill map-design brush labs
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "assets/pcx.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "brush/map.hpp"
#include "brush/read.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "morph/model.hpp"
#include "morph/read.hpp"
#include "shooter/bsp_world.hpp"
#include "shooter/jump_finder.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/labs/item_icons.hpp"
#include "shooters_edge/labs/map_floors.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace ba = ge::brush::app;
namespace mc = ge::brush::mapc;
namespace cl = shooters_edge::client;
namespace ui = ge::ui;
using shooters_edge::labs::icon_size;
using shooters_edge::labs::ItemKind;
using shooters_edge::labs::Tier;
namespace icons = shooters_edge::labs;
using shooters_edge::labs::Floors;
using shooters_edge::labs::Frame;
using shooters_edge::labs::kNone;
using shooters_edge::labs::wrap;
namespace sh = ge::shooter;

// The sample: two halls, a bridge between them over a pool, a high ledge, and the intent.
constexpr const char* kSample = R"(# map-overview's sample
modes duel,2on2
room west z=0..384 box 0,0 512,768
room east z=0..384 box 896,0 1408,768
room bridge z=96..224 box 512,320 896,448
room pit z=-128..0 box 512,0 896,768 floor=floor01_5
room pitup z=0..96 box 512,0 896,768
liquid z=-128..-48 box 512,0 896,768 kind=water tex=*water1
solid ledge z=0..208 box 1152,512 1408,768
stairs z=0..96 box 384,288 512,480 up=+x
stairs z=0..96 box 896,288 1024,480 up=-x
light 256,384,300 300
light 1152,384,300 300
light 704,384,200 200
entity info_player_deathmatch 128,128,24 angle=45
entity info_player_deathmatch 1280,128,24 angle=135
entity item_armorInv 1280,640,232
entity weapon_rocketlauncher 256,640,24
entity item_health 704,384,120 spawnflags=2
entity item_rockets 320,96,24
entity item_shells 1088,96,24
area west z=0..384 box 0,0 512,768 intent=hold note="the west team's room"
area ledge z=208..384 box 1152,512 1408,768 intent=camp note="red armour: one way up"
area bridge z=96..224 box 512,320 896,448 intent=fight note="over the pool, seen from both rooms"
area pit z=-128..0 box 512,0 896,768 intent=route note="the long way under the bridge"
trick ledge-rj from 1056,640,24 to 1280,640,232 kind=rj level=1
trick pit-jump from 448,96,24 to 960,96,24 kind=hop level=2
)";

// ---- colours ------------------------------------------------------------------------------

struct Hue {
  const char* intent;
  ui::Color c;
  const char* word;  // the legend's
};
constexpr Hue kHues[] = {
    {"camp", ui::hex(0xff5a5f), "one player holds it; few ways in"},
    {"hold", ui::hex(0x4f9dff), "a team's base, held together"},
    {"fight", ui::hex(0xffb340), "where the sides meet"},
    {"route", ui::hex(0x9fd3c7), "a way through, not a place to stay"},
    {"power", ui::hex(0xc77dff), "a power's spot: timed and fought for"},
    {"spawn", ui::hex(0x63e68a), "where players come in"},
    {"perch", ui::hex(0xf2e86d), "high ground that shoots down"},
};
ui::Color hue_of(const std::string& intent) {
  for (const Hue& h : kHues)
    if (intent == h.intent) return h.c;
  return ui::hex(0xcccccc);
}
constexpr ui::Color kLevel[3] = {ui::hex(0x63e68a), ui::hex(0xffc24b), ui::hex(0xff5d5d)};
constexpr const char* kLevelWord[3] = {"anyone", "practised", "experts"};
const char* kind_tag(const std::string& k) {
  if (k == "jump") return "J";
  if (k == "hop") return "HOP";
  if (k == "head") return "HEAD";
  if (k == "rj") return "RJ";
  if (k == "flip") return "FLIP";
  if (k == "gj") return "GJ";
  if (k == "boost-rocket") return "B-RL";
  if (k == "boost-double") return "B-2";
  return "?";
}

bool origin_of(const br::Entity& e, double o[3]) {
  std::istringstream in(e.get("origin"));
  return bool(in >> o[0] >> o[1] >> o[2]);
}

// ---- the overlay --------------------------------------------------------------------------

// A rounded box with a border.
ui::Box ringed(ui::Rect r, float radius, ui::Color fill, float border, ui::Color edge) {
  ui::Box b;
  b.rect = r;
  b.radius = radius;
  b.fill = ui::solid(fill);
  b.border = border;
  b.border_color = edge;
  return b;
}

ui::Color shade(ui::Color c, float k) { return {c.r * k, c.g * k, c.b * k, c.a}; }

// Whether (x, y) is inside the polygon (even-odd).
bool inside(const std::vector<std::pair<double, double>>& p, double x, double y) {
  bool in = false;
  for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
    const auto [xi, yi] = p[i];
    const auto [xj, yj] = p[j];
    if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
  }
  return in;
}

// An area's wash straight into the picture: its colour at `a`, a camp's diagonal hatch stronger.
void wash(ui::Image& img, const Frame& fr, const mc::PlanArea& area, ui::Color c, bool hatch) {
  double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
  for (const auto& [x, y] : area.poly) {
    lo[0] = std::min(lo[0], x), hi[0] = std::max(hi[0], x);
    lo[1] = std::min(lo[1], y), hi[1] = std::max(hi[1], y);
  }
  const int X0 = std::max(0, int(fr.px(lo[0])) - 1),
            X1 = std::min(img.width - 1, int(fr.px(hi[0])) + 1);
  const int Y0 = std::max(0, int(fr.py(hi[1])) - 1),
            Y1 = std::min(img.height - 1, int(fr.py(lo[1])) + 1);
  for (int Y = Y0; Y <= Y1; ++Y)
    for (int X = X0; X <= X1; ++X) {
      int cover = 0;
      for (int k = 0; k < 4; ++k) {
        const double sx = X + 0.25 + 0.5 * (k & 1), sy = Y + 0.25 + 0.5 * (k >> 1);
        const double wx = fr.lo[0] + (sx - fr.pad) / fr.scale,
                     wy = fr.hi[1] - (sy - fr.pad) / fr.scale;
        cover += inside(area.poly, wx, wy) ? 1 : 0;
      }
      if (cover == 0) continue;
      float a = 0.16f;
      if (hatch && ((X + Y) % 14) < 4) a = 0.34f;
      a *= float(cover) / 4;
      uint8_t* o = &img.rgba[(size_t(Y) * size_t(img.width) + size_t(X)) * 4];
      o[0] = uint8_t(o[0] * (1 - a) + c.r * 255 * a);
      o[1] = uint8_t(o[1] * (1 - a) + c.g * 255 * a);
      o[2] = uint8_t(o[2] * (1 - a) + c.b * 255 * a);
    }
}

// A pill with text: its width.
float pill(ui::DrawList& dl, float x, float y, const std::string& text, float size, ui::Color fill,
           ui::Color ink, const ui::Font& font) {
  const float tw = dl.text(font, size, -10000, -10000, text, {0, 0, 0, 0});
  const float h = size + 8, w = tw + 14;
  ui::Box b;
  b.rect = {x, y, w, h};
  b.radius = h / 2;
  b.fill = ui::solid(fill);
  dl.box(b);
  dl.text(font, size, x + 7, y + h - 5.5f, text, ink);
  return w;
}

void glow(ui::DrawList& dl, float x, float y, float r, ui::Color c, float strength) {
  const ui::Stop stops[3] = {{0, ui::with_alpha(c, strength)},
                             {0.45f, ui::with_alpha(c, strength * 0.4f)},
                             {1, ui::with_alpha(c, 0)}};
  ui::Box b;
  b.rect = {x - r, y - r, 2 * r, 2 * r};
  b.radius = r;
  b.fill = ui::radial(r, r, r, stops, 3);
  dl.box(b);
}

struct Placed {
  float x, y;
  bool under = false;  // a higher floor over it: drawn dim
  const ItemKind* kind;
  const ui::Image* icon;
};

// The trick's curve: from, a bend to the left of the way, to.
std::vector<ui::Vec2> curve(ui::Vec2 a, ui::Vec2 b, int n = 28) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const ui::Vec2 m{(a.x + b.x) / 2 - dy * 0.22f, (a.y + b.y) / 2 + dx * 0.22f};
  std::vector<ui::Vec2> pts;
  for (int i = 0; i <= n; ++i) {
    const float t = float(i) / float(n), s = 1 - t;
    pts.push_back(
        {s * s * a.x + 2 * s * t * m.x + t * t * b.x, s * s * a.y + 2 * s * t * m.y + t * t * b.y});
  }
  return pts;
}

void arrow(ui::DrawList& dl, const mc::PlanTrick& tr, const Frame& fr, const ui::Font& bold) {
  const ui::Vec2 a{fr.px(tr.from[0]), fr.py(tr.from[1])}, b{fr.px(tr.to[0]), fr.py(tr.to[1])};
  const ui::Color c = kLevel[std::clamp(tr.level, 0, 2)];
  std::vector<ui::Vec2> pts = curve(a, b);
  // Stop short of the head.
  const ui::Vec2 end = pts.back(), before = pts[pts.size() - 3];
  const float hx = end.x - before.x, hy = end.y - before.y,
              hl = std::max(1e-3f, std::hypot(hx, hy));
  const ui::Vec2 dir{hx / hl, hy / hl};
  pts.back() = {end.x - dir.x * 9, end.y - dir.y * 9};
  const bool air = tr.kind != "jump" && tr.kind != "hop";
  dl.polyline(pts, 7, ui::with_alpha({0, 0, 0, 1}, 0.55f), false, 3);
  if (air) {
    for (size_t i = 0; i + 1 < pts.size(); i += 2) dl.line(pts[i], pts[i + 1], 3, c);
  } else {
    dl.polyline(pts, 3, c);
  }
  const ui::Vec2 tip = end, l{end.x - dir.x * 14 - dir.y * 7, end.y - dir.y * 14 + dir.x * 7},
                 r{end.x - dir.x * 14 + dir.y * 7, end.y - dir.y * 14 - dir.x * 7};
  const ui::Vec2 head[3] = {tip, l, r};
  dl.polygon(head, ui::solid(c));
  dl.box(ringed({a.x - 5, a.y - 5, 10, 10}, 5, c, 2, {0, 0, 0, 0.7f}));
  // The badge at the middle: the kind, the height it gains.
  const ui::Vec2 mid = pts[pts.size() / 2];
  std::string tag = kind_tag(tr.kind);
  const int rise = int(std::lround(tr.to[2] - tr.from[2]));
  if (std::abs(rise) >= 16) tag += (rise > 0 ? " +" : " ") + std::to_string(rise);
  const float tw = dl.text(bold, 13, -10000, -10000, tag, {0, 0, 0, 0});
  ui::Box bx;
  bx.rect = {mid.x - tw / 2 - 7, mid.y - 10, tw + 14, 20};
  bx.radius = 10;
  bx.fill = ui::solid(ui::hex(0x0d1117, 0.92f));
  bx.border = 1.5f;
  bx.border_color = c;
  dl.box(bx);
  dl.text(bold, 13, mid.x, mid.y + 5, tag, c, ui::Align::center);
}

// A jump the finder calculated: its arc from above as a dotted cyan line (violet when a ramp or a
// lift carries it past a standing jump's reach), a ring where the jump and the shot are, and how
// it is done on its badge.
constexpr ui::Color kFound = ui::hex(0x4fe3ff), kLifted = ui::hex(0xd18cff);
std::string found_tag(const sh::FoundJump& j) {
  std::string t = j.back ? "FLIP" : "RJ";
  if (j.by == sh::JumpLift::ramp) t += " RAMP";
  if (j.by == sh::JumpLift::lift) t += " LIFT";
  t += " +" + std::to_string(int(std::lround(j.rise)));
  return t;
}
void draw_found(ui::DrawList& dl, const sh::FoundJump& j, const Frame& fr, const ui::Font& bold) {
  const ui::Color c = j.lifted ? kLifted : kFound;
  const ui::Vec2 l{fr.px(j.launch.x), fr.py(j.launch.y)}, b{fr.px(j.to.x), fr.py(j.to.y)};
  std::vector<ui::Vec2> pts;
  for (const sh::Vec3f& q : j.arc) pts.push_back({fr.px(q.x), fr.py(q.y)});
  if (pts.size() < 4) pts = curve(l, b, 24);
  // A dot every 10 pixels along it.
  float since = 10;
  for (size_t i = 1; i < pts.size(); ++i) {
    since += std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    if (since < 10) continue;
    since = 0;
    ui::Box d;
    d.rect = {pts[i].x - 2.2f, pts[i].y - 2.2f, 4.4f, 4.4f};
    d.radius = 2.2f;
    d.fill = ui::solid(c);
    dl.box(d);
  }
  const ui::Vec2 end = pts.back(), before = pts[pts.size() >= 4 ? pts.size() - 4 : 0];
  const float hx = end.x - before.x, hy = end.y - before.y,
              hl = std::max(1e-3f, std::hypot(hx, hy));
  const ui::Vec2 dir{hx / hl, hy / hl};
  const ui::Vec2 head[3] = {end,
                            {end.x - dir.x * 12 - dir.y * 6, end.y - dir.y * 12 + dir.x * 6},
                            {end.x - dir.x * 12 + dir.y * 6, end.y - dir.y * 12 - dir.x * 6}};
  dl.polygon(head, ui::solid(c));
  dl.arc(l, 7, 2.5f, 0, 6.3f, ui::solid(c));
  const ui::Vec2 mid = pts[pts.size() / 2];
  const std::string tag = found_tag(j);
  const float tw = dl.text(bold, 12, -10000, -10000, tag, {0, 0, 0, 0});
  dl.box(ringed({mid.x - tw / 2 - 6, mid.y - 9, tw + 12, 18}, 9, ui::hex(0x0d1117, 0.9f), 1.5f, c));
  dl.text(bold, 12, mid.x, mid.y + 4.5f, tag, c, ui::Align::center);
}

void draw_spawn(ui::DrawList& dl, float x, float y, double angle_deg) {
  const double a = angle_deg * 3.14159265358979 / 180;
  const float dx = float(std::cos(a)), dy = float(-std::sin(a));
  dl.box(ringed({x - 9, y - 9, 18, 18}, 9, ui::hex(0x0d1117, 0.75f), 2.5f, ui::hex(0x63e68a)));
  const ui::Vec2 tri[3] = {{x + dx * 17, y + dy * 17},
                           {x + dx * 8 - dy * 6, y + dy * 8 + dx * 6},
                           {x + dx * 8 + dy * 6, y + dy * 8 - dx * 6}};
  dl.polygon(tri, ui::solid(ui::hex(0x63e68a)));
}

// ---- the photo pass's places --------------------------------------------------------------

struct View {
  std::string name;
  double at[3];
  double yaw, pitch;
};

// For an area: stand a quarter in from the side of its box farthest from the map's middle, at its
// floor, and look across it; for no intent, spots spread over the floors.
std::vector<View> photo_views(const mc::PlanIntent& intent, const Floors& f, const Frame& fr) {
  std::vector<View> out;
  const double mx = (fr.lo[0] + fr.hi[0]) / 2, my = (fr.lo[1] + fr.hi[1]) / 2;
  const auto floor_at = [&](double x, double y, double z0, double z1) -> double {
    const int X = int(double(fr.px(x)) * f.ss), Y = int(double(fr.py(y)) * f.ss);
    if (X < 0 || Y < 0 || X >= f.w || Y >= f.h) return z0;
    const float h = f.top[size_t(Y) * size_t(f.w) + size_t(X)];
    return h != kNone && h >= z0 - 8 && h <= z1 ? h : z0;
  };
  // How far the floor runs open from (x, y) at height z along `yaw`, units: until the void or a
  // floor more than a step and a half off z.
  const auto open_run = [&](double x, double y, double z, double yaw_deg) {
    const double c = std::cos(yaw_deg * 3.14159265358979 / 180),
                 sn = std::sin(yaw_deg * 3.14159265358979 / 180);
    double run = 0;
    for (double r = 16; r <= 2048; r += 16) {
      const int X = int(double(fr.px(x + c * r)) * f.ss), Y = int(double(fr.py(y + sn * r)) * f.ss);
      if (X < 0 || Y < 0 || X >= f.w || Y >= f.h) break;
      const float h = f.top[size_t(Y) * size_t(f.w) + size_t(X)];
      if (h == kNone || std::fabs(h - (z - 25)) > 28) break;
      run = r;
    }
    return run;
  };
  // The look with the longest open run within `spread` degrees of `yaw`.
  const auto best_yaw = [&](double x, double y, double z, double yaw, double spread) {
    double best = yaw, far = -1;
    for (double d = -spread; d <= spread + 1e-9; d += 15) {
      const double r = open_run(x, y, z, yaw + d);
      if (r > far + 1e-9) far = r, best = yaw + d;
    }
    return best;
  };
  for (const mc::PlanArea& a : intent.areas) {
    double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
    for (const auto& [x, y] : a.poly) {
      lo[0] = std::min(lo[0], x), hi[0] = std::max(hi[0], x);
      lo[1] = std::min(lo[1], y), hi[1] = std::max(hi[1], y);
    }
    const double cx = (lo[0] + hi[0]) / 2, cy = (lo[1] + hi[1]) / 2;
    double sx = cx, sy = cy;
    if (hi[0] - lo[0] >= hi[1] - lo[1])
      sx = cx > mx ? hi[0] - (hi[0] - lo[0]) / 4 : lo[0] + (hi[0] - lo[0]) / 4;
    else
      sy = cy > my ? hi[1] - (hi[1] - lo[1]) / 4 : lo[1] + (hi[1] - lo[1]) / 4;
    if (!inside(a.poly, sx, sy)) sx = cx, sy = cy;
    View v;
    v.name = a.name + (a.copy ? "-2" : "");
    v.at[0] = sx;
    v.at[1] = sy;
    v.at[2] = floor_at(sx, sy, a.z0, a.z1) + 25;
    v.yaw = std::atan2(cy - sy, cx - sx) * 180 / 3.14159265358979;
    if (sx == cx && sy == cy) v.yaw = std::atan2(my - cy, mx - cx) * 180 / 3.14159265358979 + 180;
    v.yaw = best_yaw(sx, sy, v.at[2], v.yaw, 60);
    v.pitch = 2;
    out.push_back(v);
  }
  if (!out.empty()) return out;
  // No intent: the floor's samples farthest from every chosen one, 384 units apart at least.
  std::vector<std::array<double, 3>> pts;
  const int step = std::max(1, int(48 * fr.scale * f.ss));
  for (int Y = 0; Y < f.h; Y += step)
    for (int X = 0; X < f.w; X += step) {
      const float h = f.top[size_t(Y) * size_t(f.w) + size_t(X)];
      if (h == kNone) continue;
      pts.push_back({fr.lo[0] + (X / double(f.ss) - fr.pad) / fr.scale,
                     fr.hi[1] - (Y / double(f.ss) - fr.pad) / fr.scale, double(h)});
    }
  std::vector<double> best(pts.size(), 1e30);
  for (int n = 0; n < 12 && !pts.empty(); ++n) {
    size_t pick = 0;
    double far = -1;
    for (size_t i = 0; i < pts.size(); ++i)
      if (best[i] > far) far = best[i], pick = i;
    if (n > 0 && far < 384) break;
    const auto p = pts[pick];
    View v;
    v.name = "spot-" + std::to_string(n + 1);
    v.at[0] = p[0];
    v.at[1] = p[1];
    v.at[2] = p[2] + 25;
    v.yaw = best_yaw(p[0], p[1], v.at[2], std::atan2(my - p[1], mx - p[0]) * 180 / 3.14159265358979,
                     180);
    v.pitch = 2;
    out.push_back(v);
    for (size_t i = 0; i < pts.size(); ++i)
      best[i] = std::min(best[i], std::hypot(pts[i][0] - p[0], pts[i][1] - p[1]));
  }
  return out;
}

// ---- the lab ------------------------------------------------------------------------------

bool read_text(const std::string& path, std::string* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), {});
  return true;
}

void run(gl::Lab& lab) {
  std::string error;
  cl::SearchPath files;
  std::vector<uint8_t> map_bytes, palette_store;
  std::span<const uint8_t> palette;
  std::string map_name = "sample";
  mc::PlanIntent intent;
  const std::string folder = lab.arg("game");
  if (!folder.empty()) {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("map-overview: " + error, "Pass --game=<basedir>/<game folder>. Skill: map-design.");
      return;
    }
    palette = cl::game_palette(files, palette_store);
    const std::string& m = lab.arg("map");
    std::string text;
    if (m.size() > 4 && m.substr(m.size() - 4) == ".bsp" && read_text(m, &text)) {
      map_bytes.assign(text.begin(), text.end());
      const size_t s = m.find_last_of('/');
      map_name = m.substr(s == std::string::npos ? 0 : s + 1,
                          m.size() - 4 - (s == std::string::npos ? 0 : s + 1));
    } else {
      std::vector<uint8_t> store;
      const std::span<const uint8_t> f = cl::find_file(files, "maps/" + m + ".bsp", store);
      map_bytes.assign(f.begin(), f.end());
      map_name = m;
    }
    if (map_bytes.empty()) {
      lab.fail("map-overview: no map " + m + " in " + folder,
               "Pass --map=<a name under maps/ or a .bsp path>. Skill: map-design.");
      return;
    }
  } else {
    // The sample, compiled with the compiler's patterns over a warm ramp.
    mc::Source src;
    if (!mc::read_plan(kSample, &src, &error, &intent)) {
      lab.fail("map-overview: the sample plan: " + error,
               "Fix the lab's sample. Skill: map-design.");
      return;
    }
    mc::CompileReport rep;
    if (!mc::compile_map(src, {}, &map_bytes, &rep, &error)) {
      lab.fail("map-overview: the sample: " + error, "Fix the lab's sample. Skill: brush.");
      return;
    }
    palette_store.resize(768);
    for (int i = 0; i < 256; ++i) {
      palette_store[size_t(i) * 3] = uint8_t(std::min(255, i + 30));
      palette_store[size_t(i) * 3 + 1] = uint8_t(i * 7 / 8 + 20);
      palette_store[size_t(i) * 3 + 2] = uint8_t(i * 3 / 4 + 14);
    }
    palette = palette_store;
  }
  // The intent: `--intent` (a plan or an .intent file of area, trick and modes lines).
  if (const std::string& ip = lab.arg("intent"); !ip.empty()) {
    std::string text;
    if (!read_text(ip, &text)) {
      lab.fail("map-overview: the intent " + ip + " does not open",
               "Pass --intent=<a .plan or .intent file>. Skill: map-design.");
      return;
    }
    intent = {};
    if (!mc::read_intent(text, &intent, &error)) {
      lab.fail("map-overview: " + ip + ": " + error, "Fix the line it names. Skill: map-design.");
      return;
    }
  }
  ba::Pack pack;
  pack.bytes = map_bytes;
  pack.files.push_back({"maps/" + map_name + ".bsp", 0, uint32_t(map_bytes.size())});
  ba::Scene scene;
  if (!ba::load_scene(pack, pack.files[0].name, palette, &scene, &error)) {
    lab.fail("map-overview: " + error, "Pass a version 29 map. Skill: brush.");
    return;
  }
  // The frame: the floors' bounds (map units), fitted to `--size` px wide.
  Frame fr;
  const double k = 1 / br::kUnitM;
  fr.lo[0] = scene.mins[0] * k, fr.hi[0] = scene.maxs[0] * k;
  fr.lo[1] = scene.mins[2] * k, fr.hi[1] = scene.maxs[2] * k;
  fr.lo[2] = scene.mins[1] * k, fr.hi[2] = scene.maxs[1] * k;
  const double size = std::max(400.0, lab.arg_num("size"));
  fr.scale =
      (size - 2 * fr.pad) / std::max(1.0, std::max(fr.hi[0] - fr.lo[0], fr.hi[1] - fr.lo[1]));
  fr.width = int(std::ceil((fr.hi[0] - fr.lo[0]) * fr.scale)) + 2 * fr.pad;
  fr.height = int(std::ceil((fr.hi[1] - fr.lo[1]) * fr.scale)) + 2 * fr.pad;
  // The jump finder: the map's move world, its nav graph, one proven jump a ledge.
  std::vector<sh::FoundJump> jumps;
  sh::JumpFinderCounts jc;
  if (lab.arg_bool("jumps")) {
    sh::BspWorld bsp;
    if (sh::bsp_world(scene.map, &bsp, &error)) {
      sh::JumpFinderSettings js;
      if (lab.arg_num("reach") > 0) js.reach = float(lab.arg_num("reach"));
      // The walls that are brush models (func_wall) as solids; the lifts (func_plat) as movers
      // the graph rides (built at their top, lowered by their height, 150 units/s), so a jump
      // from a lift's top is tried and a ride up counts as a way there; the teleporters as links.
      auto world = std::make_unique<sh::MoveWorld>();
      sh::map_world(bsp.models[0], *world);
      std::vector<sh::NavMover> movers;
      struct Tele {
        sh::Vec3f lo, hi;
        std::string target;
      };
      std::vector<Tele> teles_at;
      std::map<std::string, sh::Vec3f> dests_at;
      for (const br::Entity& e : scene.map.entities) {
        const std::string& cls = e.get("classname");
        double o[3];
        if ((cls.rfind("item_", 0) == 0 || cls.rfind("weapon_", 0) == 0 ||
             cls.rfind("info_player", 0) == 0) &&
            origin_of(e, o))
          js.places.push_back({float(o[0]), float(o[1]), float(o[2]) + 24});
        if (cls == "info_teleport_destination" && origin_of(e, o))
          dests_at[e.get("targetname")] = {float(o[0]), float(o[1]), float(o[2]) + 27};
        const std::string& model = e.get("model");
        if (model.size() < 2 || model[0] != '*') continue;
        const size_t mi = size_t(std::atoi(model.c_str() + 1));
        if (mi == 0 || mi >= bsp.models.size()) continue;
        const sh::BrushModel& m = bsp.models[mi];
        if (cls == "trigger_teleport") teles_at.push_back({m.mins, m.maxs, e.get("target")});
        if (cls == "func_wall" && world->count < sh::kMaxSolids) {
          sh::Solid& so = world->solids[world->count++];
          so = sh::Solid{};
          so.kind = sh::SolidKind::brush;
          so.point_hull = m.hulls[0];
          so.clip_hull = m.hulls[1];
          so.mins = m.mins;
          so.maxs = m.maxs;
        }
        if (cls == "func_plat") {
          const float rise = e.get("height").empty() ? m.maxs.z - m.mins.z - 8
                                                     : float(std::atof(e.get("height").c_str()));
          sh::NavMover mv;
          mv.ent = int(movers.size());
          mv.model = &m;
          mv.rest = {0, 0, -rise};
          mv.travel = {0, 0, rise};
          mv.absmin = m.mins + mv.rest;
          mv.absmax = m.maxs + mv.rest;
          mv.ride_s = 1 + rise / 150;
          movers.push_back(mv);
        }
      }
      sh::NavGraph g;
      sh::NavSettings ns;
      ns.treads = true;  // the stairs' treads walked: a jump up a stair is no trick
      sh::build_nav(*world, movers, sh::MoveSettings{}, ns, g);
      // A teleporter: from every floor node its trigger touches to the node at its destination.
      std::vector<sh::NavExtraLink> tl;
      for (const Tele& t : teles_at) {
        const auto d = dests_at.find(t.target);
        if (d == dests_at.end()) continue;
        const int32_t to = sh::nav_nearest(g, d->second);
        if (to < 0) continue;
        for (size_t n = 0; n < g.nodes.size(); ++n) {
          const sh::Vec3f q = g.nodes[n].origin;
          // Touched from the floor beside it: a trigger stands in an alcove a grid step (32) and
          // the player's half width (16) from the nearest node.
          if (q.x < t.lo.x - 48 || q.x > t.hi.x + 48 || q.y < t.lo.y - 48 || q.y > t.hi.y + 48 ||
              q.z < t.lo.z - 24 || q.z > t.hi.z + 32)
            continue;
          sh::NavExtraLink x;
          x.from = int32_t(n);
          x.to = to;
          x.teleport = true;
          tl.push_back(x);
        }
      }
      sh::nav_add_links(g, tl);
      lab.note("the floors' graph: " + std::to_string(g.nodes.size()) + " nodes, " +
               std::to_string(g.links.size()) + " links, " + std::to_string(movers.size()) +
               " lifts ridden, " + std::to_string(tl.size()) + " teleporter links");
      if (const std::string& at = lab.arg("from"); !at.empty()) {
        float p[3] = {0, 0, 0};
        if (std::sscanf(at.c_str(), "%f,%f,%f", &p[0], &p[1], &p[2]) == 3) {
          jumps = sh::jumps_from(*world, sh::MoveSettings{}, g, {p[0], p[1], p[2]}, js, &jc);
          const int32_t n = sh::nav_nearest(g, {p[0], p[1], p[2]});
          if (n >= 0) {
            const sh::Vec3f o = g.nodes[size_t(n)].origin;
            lab.note("from the floor at " + std::to_string(int(o.x)) + "," +
                     std::to_string(int(o.y)) + "," + std::to_string(int(o.z)) + ": " +
                     std::to_string(jc.ledges) + " ledges tried");
          }
        }
      } else {
        jumps = sh::find_jumps(*world, sh::MoveSettings{}, g, js, &jc);
      }
    } else {
      lab.note("the jump finder skipped: " + error);
    }
  }
  const Floors floors = draw_floors(scene, fr, 2);
  ui::Image map_img = shade_floors(floors, fr);
  // The areas' washes go straight into the floor picture.
  for (const mc::PlanArea& a : intent.areas)
    wash(map_img, fr, a, hue_of(a.intent), a.intent == "camp");
  // The items' icons, once a kind.
  std::map<const ItemKind*, ui::Image> icons;
  std::vector<Placed> placed;
  struct Spot {
    float x, y;
    double angle;
  };
  std::vector<Spot> spawns;
  std::vector<std::pair<ui::Vec2, std::string>> teles;  // the trigger's middle, its target
  std::map<std::string, ui::Vec2> dests;
  int drawn = 0, badges = 0;
  for (const br::Entity& e : scene.map.entities) {
    const std::string& cls = e.get("classname");
    double o[3] = {0, 0, 0};
    const bool has = origin_of(e, o);
    if (cls == "info_player_deathmatch" && has) {
      spawns.push_back({fr.px(o[0]), fr.py(o[1]), std::atof(e.get("angle").c_str())});
      continue;
    }
    if (cls == "info_teleport_destination" && has) {
      dests[e.get("targetname")] = {fr.px(o[0]), fr.py(o[1])};
      continue;
    }
    if (cls == "trigger_teleport") {
      const std::string& model = e.get("model");
      if (model.size() > 1 && model[0] == '*') {
        const size_t mi = size_t(std::atoi(model.c_str() + 1));
        if (mi < scene.map.models.size()) {
          const auto& md = scene.map.models[mi];
          teles.push_back(
              {{fr.px((md.mins[0] + md.maxs[0]) / 2), fr.py((md.mins[1] + md.maxs[1]) / 2)},
               e.get("target")});
        }
      }
      continue;
    }
    const ItemKind* kind = icons::item_of(e);
    if (!kind || !has) continue;
    if (!icons.count(kind)) {
      icons::Mesh mesh;
      const bool ok = kind->box ? icons::box_mesh(files, palette, kind->model, &mesh)
                                : icons::model_mesh(files, palette, kind->model, kind->skin, &mesh);
      ui::Image img;
      if (ok && !mesh.idx.empty()) {
        img = icons::draw_icon(mesh, icon_size(kind->tier) * 2, kind->box ? -90 : 30,
                               kind->box ? 40 : 32);
        ++drawn;
      } else {
        ++badges;
      }
      icons.emplace(kind, std::move(img));
    }
    // Under a higher floor (a bridge, a ledge over water): the picture shows that floor, not the
    // item's, so it is drawn dim.
    bool under = false;
    {
      const int X = int(double(fr.px(o[0])) * floors.ss), Y = int(double(fr.py(o[1])) * floors.ss);
      if (X >= 0 && Y >= 0 && X < floors.w && Y < floors.h) {
        const float top = floors.top[size_t(Y) * size_t(floors.w) + size_t(X)];
        under = top != kNone && top > o[2] + 48;
      }
    }
    placed.push_back({fr.px(o[0]), fr.py(o[1]), under, kind, &icons[kind]});
  }
  // The picture: the map, a panel beside it.
  const int panel = 400;
  const int W = fr.width + panel, H = std::max(fr.height, 820);
  ui::Image img = ui::make_image(W, H, ui::hex(0x0b0f15));
  for (int y = 0; y < fr.height; ++y)
    std::copy_n(&map_img.rgba[size_t(y) * size_t(fr.width) * 4], size_t(fr.width) * 4,
                &img.rgba[size_t(y) * size_t(W) * 4]);
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& body = ui::ui_font(ui::Weight::medium);
  ui::DrawList dl;
  dl.begin(W, H);
  // Areas: the outline, the number and the name at the box's top left.
  int number = 0;
  struct Key {
    int n;
    const mc::PlanArea* a;
  };
  std::vector<Key> keys;
  for (const mc::PlanArea& a : intent.areas) {
    const ui::Color c = hue_of(a.intent);
    std::vector<ui::Vec2> pts;
    double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
    for (const auto& [x, y] : a.poly) {
      pts.push_back({fr.px(x), fr.py(y)});
      lo[0] = std::min(lo[0], x), hi[0] = std::max(hi[0], x);
      lo[1] = std::min(lo[1], y), hi[1] = std::max(hi[1], y);
    }
    dl.polyline(pts, 9, ui::with_alpha(c, 0.18f), true, 6);
    dl.polyline(pts, 2.2f, c, true);
    ++number;
    if (!a.copy) keys.push_back({number, &a});
    const float x = fr.px(lo[0]) + 6, y = fr.py(hi[1]) + 6;
    ui::Box num;
    num.rect = {x, y, 22, 22};
    num.radius = 11;
    num.fill = ui::solid(c);
    dl.box(num);
    dl.text(bold, 14, x + 11, y + 16, std::to_string(a.copy ? number - 1 : number),
            ui::hex(0x0d1117), ui::Align::center);
    if (!a.copy) pill(dl, x + 26, y, a.name, 14, ui::hex(0x0d1117, 0.82f), ui::hex(0xf2f2f2), bold);
    if (a.copy) --number;
  }
  // Teleporters: a ring, a thin dashed way to where it puts you.
  for (const auto& [at, target] : teles) {
    const auto d = dests.find(target);
    if (d != dests.end()) {
      const std::vector<ui::Vec2> pts = curve(at, d->second, 20);
      for (size_t i = 0; i + 1 < pts.size(); i += 2)
        dl.line(pts[i], pts[i + 1], 2, ui::with_alpha(ui::hex(0xc77dff), 0.8f));
      dl.box(ringed({d->second.x - 6, d->second.y - 6, 12, 12}, 6, ui::hex(0xc77dff, 0.35f), 2,
                    ui::hex(0xc77dff)));
    }
    glow(dl, at.x, at.y, 26, ui::hex(0xc77dff), 0.55f);
    dl.arc(at, 11, 3, 0, 4.5f, ui::solid(ui::hex(0xe0b3ff)), true);
  }
  for (const sh::FoundJump& j : jumps) draw_found(dl, j, fr, bold);
  for (const mc::PlanTrick& t : intent.tricks) arrow(dl, t, fr, bold);
  for (const Spot& s : spawns) draw_spawn(dl, s.x, s.y, s.angle);
  // Items: the small ones first, the powers last and on top.
  std::stable_sort(placed.begin(), placed.end(),
                   [](const Placed& a, const Placed& b) { return a.kind->tier > b.kind->tier; });
  for (const Placed& p : placed) {
    const float s = float(icon_size(p.kind->tier));
    const ui::Color c = ui::hex(p.kind->colour);
    const float dim = p.under ? 0.38f : 1.0f;
    if (p.kind->tier == Tier::power && !p.under) glow(dl, p.x, p.y, s * 0.95f, c, 0.55f);
    ui::Box sh;
    sh.rect = {p.x - s * 0.36f, p.y + s * 0.18f, s * 0.72f, s * 0.2f};
    sh.radius = s * 0.1f;
    sh.fill = ui::solid({0, 0, 0, 0.5f});
    sh.feather = 6;
    sh.opacity = dim;
    dl.box(sh);
    if (!p.icon->empty()) {
      ui::Box b;
      b.rect = {p.x - s / 2, p.y - s / 2 - s * 0.08f, s, s};
      b.image = p.icon;
      b.uv = {0, 0, float(p.icon->width), float(p.icon->height)};
      b.opacity = dim;
      dl.box(b);
    } else {
      ui::Box b;
      const float bs = s * 0.62f;
      b.rect = {p.x - bs / 2, p.y - bs / 2, bs, bs};
      b.radius = bs * 0.22f;
      b.fill = ui::solid(shade(c, 0.85f));
      b.border = 2;
      b.border_color = ui::hex(0x0d1117, 0.9f);
      dl.box(b);
      dl.text(bold, std::max(11.0f, bs * 0.36f), p.x, p.y + bs * 0.14f, p.kind->tag,
              ui::hex(0x0d1117), ui::Align::center);
    }
    if (p.kind->tier != Tier::minor) {
      std::string tag = p.kind->tag;
      if (p.kind->respawn > 0) tag += "  " + std::to_string(p.kind->respawn) + "s";
      const float tw = dl.text(bold, 12, -10000, -10000, tag, {0, 0, 0, 0});
      pill(dl, p.x - (tw + 14) / 2, p.y + s * 0.4f, tag, 12, ui::hex(0x0d1117, 0.85f), c, bold);
    }
  }
  // The panel.
  const float px0 = float(fr.width) + 8;
  dl.rect({px0 - 8, 0, float(panel), float(H)}, ui::hex(0x111722));
  dl.line({px0 - 8, 0}, {px0 - 8, float(H)}, 1, ui::hex(0x263042));
  float y = 58;
  dl.text(bold, 40, px0 + 20, y, map_name, ui::hex(0xf4f1ea));
  y += 26;
  dl.text(body, 16, px0 + 20, y, "Intended gameplay", ui::hex(0x8d97a8));
  y += 18;
  float cx = px0 + 20;
  for (const std::string& m : [&] {
         std::vector<std::string> v;
         std::stringstream ss(intent.modes);
         for (std::string s; std::getline(ss, s, ',');)
           if (!s.empty()) v.push_back(s);
         return v;
       }()) {
    cx += pill(dl, cx, y, m, 13, ui::hex(0x263042), ui::hex(0xdfe6f2), bold) + 6;
  }
  y += 44;
  const auto heading = [&](const std::string& t) {
    dl.text(bold, 15, px0 + 20, y, t, ui::hex(0x8d97a8), ui::Align::left, 1.5f);
    y += 10;
    dl.line({px0 + 20, y}, {float(W) - 24, y}, 1, ui::hex(0x263042));
    y += 22;
  };
  if (!keys.empty()) {
    heading("AREAS");
    for (const Key& kk : keys) {
      const ui::Color c = hue_of(kk.a->intent);
      ui::Box num;
      num.rect = {px0 + 20, y - 15, 20, 20};
      num.radius = 10;
      num.fill = ui::solid(c);
      dl.box(num);
      dl.text(bold, 13, px0 + 30, y, std::to_string(kk.n), ui::hex(0x0d1117), ui::Align::center);
      const float nw = dl.text(bold, 17, px0 + 48, y, kk.a->name, ui::hex(0xf2f2f2));
      std::string up = kk.a->intent;
      for (char& ch : up) ch = char(std::toupper(static_cast<unsigned char>(ch)));
      dl.text(bold, 12, px0 + 56 + nw, y, up, c, ui::Align::left, 1.2f);
      y += 18;
      if (!kk.a->note.empty()) {
        // The note, wrapped to the panel.
        std::string line, word;
        std::stringstream ws(kk.a->note);
        while (ws >> word) {
          const std::string next = line.empty() ? word : line + " " + word;
          if (dl.text(body, 14, -10000, -10000, next, {0, 0, 0, 0}) > float(panel) - 90 &&
              !line.empty()) {
            dl.text(body, 14, px0 + 48, y, line, ui::hex(0xa9b3c4));
            y += 17;
            line = word;
          } else {
            line = next;
          }
        }
        if (!line.empty()) {
          dl.text(body, 14, px0 + 48, y, line, ui::hex(0xa9b3c4));
          y += 17;
        }
      }
      y += 10;
      if (y > float(H) - 260) break;
    }
    y += 6;
  }
  heading("KEY");
  for (const Hue& h : kHues) {
    bool used = intent.areas.empty();
    for (const mc::PlanArea& a : intent.areas) used = used || a.intent == h.intent;
    if (!used) continue;
    dl.rect({px0 + 20, y - 12, 26, 14}, ui::with_alpha(h.c, 0.35f), 3);
    dl.polyline(
        std::vector<ui::Vec2>{
            {px0 + 20, y - 12}, {px0 + 46, y - 12}, {px0 + 46, y + 2}, {px0 + 20, y + 2}},
        1.6f, h.c, true);
    std::string up = h.intent;
    for (char& ch : up) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    const float tw = dl.text(bold, 13, px0 + 56, y, up, h.c, ui::Align::left, 1.2f);
    dl.text(body, 14, px0 + 64 + tw, y, h.word, ui::hex(0xa9b3c4));
    y += 22;
  }
  if (!intent.tricks.empty()) {
    for (int l = 0; l < 3; ++l) {
      dl.line({px0 + 20, y - 5}, {px0 + 46, y - 5}, 3, kLevel[l]);
      dl.text(body, 14, px0 + 56, y, std::string("trick for ") + kLevelWord[l], ui::hex(0xa9b3c4));
      y += 20;
    }
    dl.text(body, 13, px0 + 20, y + 2, "Dashed: through the air (RJ, GJ, HEAD, B-RL, B-2).",
            ui::hex(0x6f7a8c));
    y += 22;
  }
  if (!jumps.empty()) {
    for (int lifted = 0; lifted < 2; ++lifted) {
      const ui::Color c = lifted ? kLifted : kFound;
      for (int d = 0; d < 4; ++d) {
        ui::Box b;
        b.rect = {px0 + 20 + float(d) * 7, y - 7, 4.4f, 4.4f};
        b.radius = 2.2f;
        b.fill = ui::solid(c);
        dl.box(b);
      }
      dl.text(body, 14, px0 + 56, y,
              lifted ? "a found jump a ramp or a rising lift carries"
                     : "a found rocket jump near its reach",
              ui::hex(0xa9b3c4));
      y += 20;
    }
    y += 4;
  }
  draw_spawn(dl, px0 + 33, y - 6, 0);
  dl.text(body, 14, px0 + 56, y, "a start, facing its way", ui::hex(0xa9b3c4));
  y += 28;
  // The scale: a second's run at 320 units a second.
  float run = float(320 * fr.scale);
  std::string run_words = "one second's run (320 units)";
  if (run > 160) run = float(160 * fr.scale), run_words = "half a second's run (160 units)";
  if (run > 160) run = float(64 * fr.scale), run_words = "64 units";
  dl.line({px0 + 20, y}, {px0 + 20 + run, y}, 3, ui::hex(0xdfe6f2));
  dl.line({px0 + 20, y - 6}, {px0 + 20, y + 6}, 2, ui::hex(0xdfe6f2));
  dl.line({px0 + 20 + run, y - 6}, {px0 + 20 + run, y + 6}, 2, ui::hex(0xdfe6f2));
  dl.text(body, 14, px0 + 28 + run, y + 5, run_words, ui::hex(0xa9b3c4));
  ui::render(dl, img);
  // Write: the shot, the bare picture, the photo pass's places.
  gl::Picture pic{img.width, img.height, img.rgba};
  lab.shot(
      {"overview",
       map_name + ": intended gameplay",
       "The map from above as its floors are lit, the intent's areas washed in their colours and "
       "numbered, the tricks as arrows coloured by who can do them, the items as the game's own "
       "models (badges where the pack has none), the starts and the teleporters.",
       "every area where the plan puts it; the powers glowing; nothing hidden under a label",
       {{"map", map_name}, {"intent", lab.arg("intent")}, {"size", lab.arg("size")}}},
      pic, folder.empty() ? gl::Det::deterministic : gl::Det::measured);
  lab.attach("overview.png", gl::encode_png(pic), gl::Det::measured);
  const std::vector<View> views = photo_views(intent, floors, fr);
  std::string vt;
  for (const View& v : views) {
    char line[256];
    std::snprintf(line, sizeof line, "%s %.0f,%.0f,%.0f,%.0f,%.0f\n", v.name.c_str(), v.at[0],
                  v.at[1], v.at[2], v.yaw, v.pitch);
    vt += line;
  }
  lab.attach("views.txt", std::vector<uint8_t>(vt.begin(), vt.end()));
  lab.metric("areas", double(intent.areas.size()), "count");
  lab.metric("tricks", double(intent.tricks.size()), "count");
  lab.metric("items", double(placed.size()), "count");
  lab.metric("icons-drawn", double(drawn), "count");
  lab.metric("icons-as-badges", double(badges), "count");
  lab.metric("starts", double(spawns.size()), "count");
  lab.metric("photo-views", double(views.size()), "count");
  lab.metric("jumps-found", double(jumps.size()), "count");
  lab.metric("jumps-lifted", double(jc.lifted), "count");
  lab.metric("jump-arcs", double(jc.arcs), "count");
  lab.metric("jump-reach", double(jc.reach), "units");
  lab.metric("jumps-walkable", double(jc.walkable), "count");
  for (const sh::FoundJump& j : jumps) {
    char line[240];
    std::snprintf(
        line, sizeof line,
        "jump: %s from %.0f,%.0f,%.0f to %.0f,%.0f,%.0f run %.0f lift %.0f pitch %.0f apex %.0f "
        "health %.0f flight %.2f s walk %.1f s",
        found_tag(j).c_str(), j.from.x, j.from.y, j.from.z, j.to.x, j.to.y, j.to.z, j.speed,
        j.lift_vz, j.pitch, j.apex, j.health, j.time_s, j.walk_s);
    lab.note(line);
  }
}

const gl::LabSpec kSpec{
    "map-overview",
    gl::LabKind::picture,
    "game:shooters_edge",
    "a map's intended gameplay from above (any version 29 map; the intent from a plan or an "
    ".intent file) and the photo pass's places",
    "map-design",
    "Does the picture from above show where each area is and what it is for, how the tricks run, "
    "and every item as the game's own model, so a player sees how the map is meant to play?",
    {
        {"game", "", "a game folder, <basedir>/<game>: the map and the models (empty: the sample)"},
        {"map", "", "with --game: a map under maps/ by name, or a path to a .bsp"},
        {"intent", "", "a plan or an .intent file (area, trick and modes lines) to draw"},
        {"size", "1400", "the map's width in px (the panel is beside it)"},
        {"jumps", "1", "1: find and draw one proven rocket jump a ledge (shooter/jump_finder.hpp)"},
        {"from", "",
         "x,y,z: instead of one jump a ledge, every ledge a rocket jump reaches from there"},
        {"reach", "0",
         "a standing rocket jump's highest ledge, units (0: the finder's, from map-moves)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
