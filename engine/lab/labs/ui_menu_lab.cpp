// LAB `menu`: a game's front door drawn by the 2D layer (engine/ui) on the CPU, LOOKED AT. One
// menu (a root page with a prominent PLAY, an options page with every item kind) over every kind
// of backdrop the engine offers (a colour, a gradient, a glow with a vignette, a slowly drifting
// picture, the live scene softened behind a scrim), at the three reference viewports (a desktop,
// a phone held upright, a phone on its side), then driven: the focus moved and the highlight
// caught gliding, a page opened mid-fade, and the picture's drift at its two ends.
//
// The picture behind is painted here from value noise (a winter range under a clear sky), so the
// lab needs no asset and no GPU. Every layout is held to `layout_problems`: an item's words wider
// than its slab, a slab out of the safe area or two overlapping fail the lab.
//
// ge:summary Lab: the menu and its backdrops pictured at three viewports, driven, layout-checked
// ge:keywords lab menu main-menu front-door backdrop background gradient vignette scrim drift
// ge:keywords scene viewport portrait landscape phone desktop focus glide ui screenshot
// ge:skill ui labs
// ge:test tests/ui_menu_test.cpp
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "core/math.hpp"
#include "core/noise.hpp"
#include "lab/lab.hpp"
#include "ui/background.hpp"
#include "ui/menu_view.hpp"
#include "ui/raster.hpp"

namespace {

using namespace ge::ui;

// A winter range under a clear sky, from value noise: the sky's gradient, three ridgelines (ridged
// noise over four octaves, so the peaks come to points), each hazier with distance, snow down to
// a line that wanders, the faces lit from the left by the ridge's slope, dark woods low on the
// nearest.
double ridge_height(double x, double seed) {
  double h = 0, amp = 1, scale = 420, sum = 0;
  for (int o = 0; o < 4; o++) {
    const double n = ge::core::value_noise(x, 0, scale, seed + o * 17);
    h += (1 - std::fabs(2 * n - 1)) * amp;  // ridged: a crease at every crossing of 0.5
    sum += amp;
    amp *= 0.48, scale *= 0.45;
  }
  return h / sum;
}

Image painted_range(int w, int h, double seed) {
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

Menu front_door() {
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

MenuStyle front_style() {
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

ge::lab::Picture to_picture(Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height;
  p.rgba = std::move(img.rgba);
  return p;
}

struct Backdrop {
  const char* id;
  const char* what;
  Background bg;
  bool scene = false;
};

}  // namespace

namespace {

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const Image range = painted_range(1600, 900, seed);
  const Image soft = blurred(range, 18);
  Drift drift;
  drift.period_s = 24;
  drift.zoom_from = 1.0f, drift.zoom_to = 1.18f;
  drift.focus_from_x = 0.62f, drift.focus_to_x = 0.4f;
  drift.focus_from_y = drift.focus_to_y = 0.45f;

  std::vector<Backdrop> backdrops;
  backdrops.push_back({"color", "a flat colour", Background::color(hex(0x101b27)), false});
  {
    const Stop s[3] = {{0, hex(0x0e2a47)}, {0.6f, hex(0x14385e)}, {1, hex(0x5d86b0)}};
    backdrops.push_back({"gradient", "a three-stop gradient, top to bottom",
                         Background::gradient(180, s, 3), false});
  }
  {
    Background bg = Background::gradient(160, hex(0x0b1524), hex(0x1d3550));
    bg.glow(0.72f, 0.3f, 0.55f, hex(0xffc58a, 0.55f)).vignette(0.7f);
    backdrops.push_back({"glow", "a gradient with a warm glow and a vignette", bg, false});
  }
  {
    Background bg = Background::image(range, Fit::cover, drift);
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

  std::vector<Viewport> viewports;
  {
    const std::string& list = lab.arg("viewports");
    size_t at = 0;
    while (at < list.size()) {
      const size_t comma = std::min(list.find(',', at), list.size());
      const std::string one = list.substr(at, comma - at);
      int w = 0, h = 0;
      if (std::sscanf(one.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
        viewports.push_back({w, h, h > w ? "portrait" : "landscape"});
      at = comma + 1;
    }
  }
  if (viewports.empty()) {
    lab.fail("--viewports=" + lab.arg("viewports") + " named no WxH size",
             "run make lab LAB=menu ARGS=--viewports=1280x720,390x844");
    return;
  }

  const MenuStyle st = front_style();
  int problems_total = 0;
  double raster_ms = 0;
  auto shoot = [&](const std::string& id, const std::string& title, const std::string& caption,
                   const std::string& look, Menu& m, MenuView& view, const Backdrop& b, int w,
                   int h, double t) {
    const MenuLayout L = layout_menu(m, w, h, st);
    for (const std::string& p : layout_problems(m, L, st)) {
      problems_total++;
      std::string what = id;
      what.append(": ").append(p);
      lab.fail(what,
               "fix the layout in engine/ui/menu_view.cpp or the menu's words; "
               "rerun `make lab LAB=menu`. Skill: ui.");
    }
    DrawList dl;
    dl.begin(w, h);
    draw_background(dl, b.bg, {0, 0, float(w), float(h)}, t, b.scene ? &soft : nullptr);
    view.draw(dl, m, L, st);
    Image img = make_image(w, h, hex(0x000000));
    const auto t0 = std::chrono::steady_clock::now();
    render(dl, img);
    raster_ms +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    lab.shot({id,
              title,
              caption,
              look,
              {{"viewport", std::to_string(w) + "x" + std::to_string(h)},
               {"backdrop", b.id},
               {"page", m.top().id},
               {"t", std::to_string(t)},
               {"scale", std::to_string(L.scale).substr(0, 4)}}},
             to_picture(std::move(img)));
  };

  // 1. The front door over every backdrop, at the first viewport.
  const Viewport& main = viewports.front();
  for (const Backdrop& b : backdrops) {
    Menu m = front_door();
    m.input({});
    MenuView view;
    view.settle(m, layout_menu(m, main.w, main.h, st));
    shoot("door-" + std::string(b.id), "FRONT DOOR OVER " + std::string(b.id),
          "The root page (the game's name, PLAY lit as the focus, a disabled REPLAYS) over " +
              std::string(b.what) + ".",
          "every word readable over the backdrop; PLAY's slab lit red and bigger than the rest; "
          "REPLAYS dimmed; the help bottom left and the stamp bottom right",
          m, view, b, main.w, main.h, 0);
  }

  // 2. Both pages at every viewport, over the drifting picture.
  const Backdrop& image = backdrops[3];
  for (const Viewport& v : viewports) {
    for (const char* page : {"root", "options"}) {
      Menu m = front_door();
      if (std::string(page) == "options") m.open("options");
      m.input({});
      if (std::string(page) == "options") m.focus("volume");
      MenuView view;
      view.settle(m, layout_menu(m, v.w, v.h, st));
      const std::string size = std::to_string(v.w) + "x" + std::to_string(v.h);
      shoot(std::string(page) + "-" + size,
            std::string(page) + " PAGE AT " + size + " (" + v.name + ")",
            std::string("The ") + page + " page laid out for a " + v.name + " screen of " + size +
                (v.h > v.w ? ": the column runs the width, anchored low, the picture above it."
                           : ": the column stands left, the picture's subject to its right."),
            "nothing clipped or overlapping; the words fit their slabs; the column clear of the "
            "edges by its margin; the focused item's hint under the column",
            m, view, image, v.w, v.h, 0);
    }
  }

  // 3. Driven: the focus moved down twice, the highlight caught 2 and 5 frames into its glide,
  // then OPTIONS opened and caught mid-fade.
  {
    Menu m = front_door();
    m.input({});
    MenuLayout L = layout_menu(m, main.w, main.h, st);
    MenuView view;
    view.settle(m, L);
    MenuInput down;
    down.down = true;
    m.input(down);
    m.input(down);
    int settled = -1;
    const Rect target = L.items[static_cast<size_t>(m.top().focus)];
    for (int frames = 0; frames < 120; frames++) {
      view.frame(m, L, 1.0 / 60);
      if (settled < 0 && std::fabs(view.highlight().y - target.y) < 0.5f) settled = frames + 1;
      if (frames == 1 || frames == 4) {
        shoot("glide-" + std::to_string(frames + 1),
              "HIGHLIGHT GLIDING, FRAME " + std::to_string(frames + 1),
              "PLAY to OPTIONS in two presses at 60 Hz; the highlight eases to the focus rather "
              "than jumping, " +
                  std::to_string(frames + 1) + " frames in.",
              "the red slab between PLAY and OPTIONS, nearer OPTIONS on the later frame; the words "
              "stay on their own slabs",
              m, view, backdrops[1], main.w, main.h, 0);
      }
    }
    lab.metric("glide.frames-to-settle", settled, "count");
    if (settled < 0 || settled > 15)
      lab.fail(
          "the highlight took " + std::to_string(settled) + " frames at 60 Hz to reach its item",
          "it should settle within a quarter second: check MenuView::frame's rate in "
          "engine/ui/menu_view.cpp. Skill: ui.");
    MenuInput accept;
    accept.accept = true;
    const auto ev = m.input(accept);
    lab.metric("drive.events-on-open", static_cast<double>(ev.size()), "count");
    L = layout_menu(m, main.w, main.h, st);
    view.frame(m, L, 1.0 / 60);
    view.frame(m, L, 1.0 / 60);
    view.frame(m, L, 1.0 / 60);
    shoot("open-fading", "OPTIONS OPENING, 3 FRAMES IN",
          "OPTIONS pressed: the page fades up and slides in from the left over about 0.2 s; caught "
          "three frames in.",
          "the page part-transparent and a little left of its place; the backdrop unchanged", m,
          view, backdrops[1], main.w, main.h, 0);
  }

  // 4. The drift at both ends of its period.
  for (const double t : {0.0, drift.period_s / 2.0}) {
    Menu m = front_door();
    m.input({});
    MenuView view;
    view.settle(m, layout_menu(m, main.w, main.h, st));
    const std::string at = t == 0 ? "start" : "half";
    shoot("drift-" + at, "DRIFT AT ITS " + std::string(t == 0 ? "START" : "TURN"),
          "The drifting picture at t = " + std::to_string(static_cast<int>(t)) +
              " s of a 24 s period: it zooms from 1.0 to 1.18 and pans left, then back.",
          "the later frame closer in and shifted; the menu exactly where it was", m, view, image,
          main.w, main.h, t);
  }

  lab.metric("layout.problems", problems_total, "count");
  lab.metric("viewports", static_cast<double>(viewports.size()), "count");
  lab.metric("raster.ms", raster_ms, "ms", ge::lab::Det::measured);
}

const ge::lab::LabSpec kSpec{
    "menu",
    ge::lab::LabKind::picture,
    "engine",
    "engine/ui/menu.hpp, engine/ui/menu_view.hpp, engine/ui/background.hpp, engine/ui/raster.hpp",
    "ui",
    "Does a menu read over every kind of backdrop at a desktop's, an upright phone's and a phone "
    "on its side's size, and does it move smoothly?",
    {
        {"viewports", "1280x720,390x844,844x390",
         "screen sizes, WxH, comma-separated; the first "
         "is the one every backdrop is shown at"},
        {"seed", "38", "the painted range's noise seed"},
    },
    "--viewports=640x360,390x844,844x390",
    run,
};
GE_LAB(kSpec);

}  // namespace
