// LAB `hud-kit`: the engine's HUD kit (engine/hud) LOOKED AT, in more than one genre. Three mock
// HUDs, each made only of the kit's widgets and each held to the layout check at the three
// reference viewports (a desktop, a phone held upright, a phone on its side):
//
//   - a RACER's, in the built-in TrueType face: the lap and the place, the speed with its unit,
//     a centred bar, a news feed, a prompt;
//   - a SHOOTER's, in the square charset: health and armour as big numbers over bars, the
//     ammunition as a number over its reserve, hearts for lives, a kill feed, a crosshair, the
//     death banner;
//   - every CHARSET as a HUD face: one readout a font, so a game sees each look before it picks.
//
// The backdrops are painted here (a bright snowfield and a dark corridor), the two lights a HUD's
// words must read over, so the lab needs no GPU and its pictures are the same bits on every row.
//
// ge:summary Lab: the HUD kit's widgets in a racer's and a shooter's HUD, every charset as a face
// ge:keywords lab hud hud-kit widgets racer shooter health ammo armour lives kill-feed crosshair
// ge:keywords charset faces truetype viewports layout-problems screenshot
// ge:skill ui labs
// ge:test tests/hud_hud_test.cpp
#include <chrono>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "hud/feed.hpp"
#include "hud/hud.hpp"
#include "lab/lab.hpp"
#include "ui/raster.hpp"

namespace {

using namespace ge::hud;
namespace ui = ge::ui;
namespace cs = ge::charset;

struct Viewport {
  int w, h;
  std::string name() const { return std::to_string(w) + "x" + std::to_string(h); }
};

// Two lights a HUD must read over: daylight on snow (white words on white) and a dim corridor.
ui::Image backdrop(int w, int h, bool bright) {
  ui::DrawList dl;
  dl.begin(w, h);
  ui::Box b;
  if (bright) {
    const ui::Stop sky[2] = {{0, ui::hex(0x2d5f9a)}, {1, ui::hex(0xbcd3ea)}};
    b.rect = {0, 0, float(w), float(h) * 0.4f};
    b.fill = ui::linear(0, 0, 0, b.rect.h, sky, 2);
    dl.box(b);
    const ui::Stop snow[2] = {{0, ui::hex(0xdfe8f2)}, {1, ui::hex(0xfafcfe)}};
    b.rect = {0, float(h) * 0.4f, float(w), float(h) * 0.6f};
    b.fill = ui::linear(0, 0, 0, b.rect.h, snow, 2);
    dl.box(b);
  } else {
    const ui::Stop wall[3] = {
        {0, ui::hex(0x1b1712)}, {0.5f, ui::hex(0x4a3b2a)}, {1, ui::hex(0x15110d)}};
    b.rect = {0, 0, float(w), float(h)};
    b.fill = ui::linear(0, 0, float(w), 0, wall, 3);
    dl.box(b);
    ui::Box glow;
    glow.rect = {float(w) * 0.3f, float(h) * 0.2f, float(w) * 0.4f, float(h) * 0.5f};
    const ui::Stop g[2] = {{0, ui::hex(0xffb35a, 0.5f)}, {1, ui::hex(0xffb35a, 0)}};
    glow.fill = ui::radial(glow.rect.w / 2, glow.rect.h / 2, glow.rect.w / 2, g, 2);
    dl.box(glow);
  }
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

// Measures a widget, places it on the stack and draws it, recording its box.
template <typename W>
void stacked(ui::DrawList& dl, const Frame& f, const Style& st, Stack& s, const W& w,
             const char* name, std::vector<Placed>& placed) {
  const Size z = measure(f, st, w);
  const ui::Rect r = s.next(z.w, z.h);
  draw(dl, f, st, w, r, ui::Align::left);
  placed.push_back({name, r});
}

std::vector<Placed> racer(ui::DrawList& dl, const Frame& f, const Style& st, const Feed& feed,
                          double t) {
  std::vector<Placed> placed;
  Stack tl(f, Anchor::top_left, f.px(st.gap));
  stacked(dl, f, st, tl, Readout{"2 / 3", "LAP", st.ink}, "lap", placed);
  stacked(dl, f, st, tl, Readout{"1:42.07", "TIME", st.ink}, "time", placed);
  Stack tr(f, Anchor::top_right, f.px(st.gap));
  {
    const Readout pos{"3RD", "OF 8", st.ink};
    const Size z = measure(f, st, pos);
    const ui::Rect r = tr.next(z.w, z.h);
    draw(dl, f, st, pos, r, ui::Align::right);
    placed.push_back({"place", r});
  }
  Stack bl(f, Anchor::bottom_left, f.px(st.gap));
  stacked(dl, f, st, bl, Big{"184", "KM/H", "", st.ink}, "speed", placed);
  {
    Meter m;
    m.value = 0.72f, m.ticks = 10, m.fill = st.accent, m.w = 220, m.h = 9;
    const Size z = measure(f, m);
    const ui::Rect r = bl.next(z.w, z.h);
    draw(dl, f, st, m, r);
    placed.push_back({"boost", r});
  }
  placed.push_back({"feed", draw_feed(dl, f, st, feed, t, Anchor::bottom_right, 20, 300)});
  const Banner b{"WRONG WAY", "TURN AROUND", st.bad};
  const Size z = measure(f, st, b);
  const ui::Rect r = place(f, Anchor::center, z.w, z.h, 0, -f.safe.h * 0.22f);
  draw(dl, f, st, b, r);
  placed.push_back({"banner", r});
  return placed;
}

std::vector<Placed> shooter(ui::DrawList& dl, const Frame& f, const Style& st, const Feed& feed,
                            double t) {
  std::vector<Placed> placed;
  const float gap = f.px(st.gap);
  Stack bl(f, Anchor::bottom_left, gap);
  stacked(dl, f, st, bl, Big{"72", "", "HEALTH", st.ink}, "health", placed);
  {
    Meter m;
    m.value = 0.72f, m.fill = st.good, m.w = 170, m.h = 8;
    const Size z = measure(f, m);
    const ui::Rect r = bl.next(z.w, z.h);
    draw(dl, f, st, m, r);
    placed.push_back({"health-bar", r});
  }
  {
    Pips lives;
    lives.count = 3, lives.lit = 2, lives.symbol = cs::sym(cs::Sym::heart);
    lives.font = st.value_face.charset, lives.color = st.bad, lives.size = 40;
    const Size z = measure(f, lives);
    const ui::Rect r = bl.next(z.w, z.h);
    draw(dl, f, st, lives, r);
    placed.push_back({"lives", r});
  }
  Stack br(f, Anchor::bottom_right, gap);
  {
    const Big ammo{"24", "/ 90", "AMMO", st.ink};
    const Size z = measure(f, st, ammo);
    const ui::Rect r = br.next(z.w, z.h);
    draw(dl, f, st, ammo, r, ui::Align::right);
    placed.push_back({"ammo", r});
  }
  {
    const Readout armour{"150", "ARMOUR", st.caption};
    const Size z = measure(f, st, armour);
    const ui::Rect r = br.next(z.w, z.h);
    draw(dl, f, st, armour, r, ui::Align::right);
    placed.push_back({"armour", r});
  }
  placed.push_back({"kills", draw_feed(dl, f, st, feed, t, Anchor::top_right, 18, 340)});
  // The crosshair: four ticks round the centre of the screen.
  const float cx = float(f.width) / 2, cy = float(f.height) / 2, a = f.px(6), b = f.px(2);
  for (const auto& [dx, dy, w, h] : {std::tuple{-3 * a, -b / 2, 2 * a, b},
                                     {a, -b / 2, 2 * a, b},
                                     {-b / 2, -3 * a, b, 2 * a},
                                     {-b / 2, a, b, 2 * a}})
    dl.rect({cx + dx, cy + dy, w, h}, st.ink);
  return placed;
}

std::vector<Placed> faces(ui::DrawList& dl, const Frame& f, const Style& base) {
  std::vector<Placed> placed;
  Stack left(f, Anchor::top_left, f.px(base.gap));
  Stack right(f, Anchor::top_right, f.px(base.gap));
  int i = 0;
  for (const cs::FontInfo& info : cs::fonts()) {
    Style st = base;
    st.value_face = st.caption_face = Face::bitmap(info.id);
    std::string name = info.name;
    for (char& c : name) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    const Readout r{"1:42.07", name, st.ink};
    const Size z = measure(f, st, r);
    Stack& s = i++ % 2 == 0 ? left : right;
    const ui::Rect at = s.next(z.w, z.h);
    draw(dl, f, st, r, at, &s == &left ? ui::Align::left : ui::Align::right);
    placed.push_back({name, at});
  }
  return placed;
}

void run(ge::lab::Lab& lab) {
  const std::vector<Viewport> viewports{{1280, 720}, {390, 844}, {844, 390}};
  const double t = 10;
  Style race;
  race.value_face = race.caption_face = race.big_face = Face::truetype(ui::ui_font());
  Feed race_feed(4);
  race_feed.push("FASTEST LAP 1:41.88", race.good, t - 3);
  race_feed.push("3RD PLACE", race.ink, t - 2);
  race_feed.push("BOOST READY", race.ink, t - 1);
  Style shoot;
  shoot.value_face = shoot.caption_face = shoot.big_face = Face::bitmap(cs::FontId::square);
  shoot.accent = ui::hex(0xffb020);
  Feed kills(4);
  kills.push("RAVEN > MOTH", shoot.ink, t - 3);
  kills.push("YOU > RAVEN", shoot.good, t - 2);
  kills.push("LAVA > OWL", shoot.bad, t - 1);
  Style gallery;
  gallery.value_size = 30;

  int problems = 0;
  double raster_ms = 0;
  auto shoot_one = [&](const std::string& id, const std::string& title, const std::string& caption,
                       const std::string& look, const Viewport& v, bool bright, auto&& draw_it,
                       const Style& st) {
    const Frame f = frame_for(v.w, v.h, st);
    ui::DrawList dl;
    dl.begin(v.w, v.h);
    const std::vector<Placed> placed = draw_it(dl, f);
    for (const std::string& p : layout_problems(f, placed)) {
      problems++;
      lab.fail(std::string(id).append(": ").append(p),
               "move or shrink the box in engine/lab/labs/hud_kit_lab.cpp, or fix the widget's "
               "measure in engine/hud/hud.cpp; rerun `make lab LAB=hud-kit`. Skill: ui.");
    }
    ui::Image img = backdrop(v.w, v.h, bright);
    const auto t0 = std::chrono::steady_clock::now();
    ui::render(dl, img);
    raster_ms +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ge::lab::Picture pic;
    pic.width = img.width, pic.height = img.height, pic.rgba = std::move(img.rgba);
    lab.shot({id,
              title,
              caption,
              look,
              {{"viewport", v.name()},
               {"scale", std::to_string(f.scale).substr(0, 4)},
               {"boxes", std::to_string(placed.size())}}},
             pic);
  };
  for (const Viewport& v : viewports) {
    shoot_one(
        "racer-" + v.name(), "A RACER'S HUD AT " + v.name(),
        "The kit in the built-in TrueType face over daylit snow: the lap and the time top left, "
        "the "
        "place top right, the speed and a ticked bar bottom left, a news feed bottom right, a "
        "prompt on a plate.",
        "white words readable over white snow by their shadow alone; nothing overlapping", v, true,
        [&](ui::DrawList& dl, const Frame& f) { return racer(dl, f, race, race_feed, t); }, race);
    shoot_one(
        "shooter-" + v.name(), "A SHOOTER'S HUD AT " + v.name(),
        "The same kit in the square charset over a dim corridor: health over its bar and two of "
        "three hearts bottom left, ammunition over its reserve and the armour bottom right, the "
        "kill feed top right, a crosshair.",
        "the hearts red, the third dark; the crosshair centred; the feed's newest line highest", v,
        false, [&](ui::DrawList& dl, const Frame& f) { return shooter(dl, f, shoot, kills, t); },
        shoot);
  }
  shoot_one(
      "faces", "EVERY CHARSET AS A HUD FACE",
      "One readout in each of the engine's ten bitmap charsets, its name as the caption, over "
      "daylit snow.",
      "every face legible at HUD size; the pixel faces hard-edged", viewports[0], true,
      [&](ui::DrawList& dl, const Frame& f) { return faces(dl, f, gallery); }, gallery);
  lab.metric("layout.problems", problems, "count");
  lab.metric("raster.ms", raster_ms, "ms", ge::lab::Det::measured);
}

const ge::lab::LabSpec kSpec{
    "hud-kit",
    ge::lab::LabKind::picture,
    "engine",
    "engine/hud/hud.hpp, engine/hud/feed.hpp, engine/hud/face.hpp",
    "ui",
    "Do the HUD kit's widgets build a racer's and a shooter's HUD that read over bright and dark "
    "scenes and lay out clean at a desktop's and a phone's sizes, in TrueType and in every "
    "charset?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
