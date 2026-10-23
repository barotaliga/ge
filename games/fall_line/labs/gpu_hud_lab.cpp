// LAB `hud`: Fall Line's HUD over the game's own frame, at the three reference viewports (a
// desktop, a phone held upright, a phone on its side). The bot skis a free ride on the seed's
// map; at the run's MOMENTS (cruising, his longest air, the army hunting him, the run's busiest
// news) the chase camera's frame is rendered on the headless device (the software adapter in the
// GPU-less container), read back, and the HUD (games/fall_line/hud/view.hpp) is drawn over it by
// the 2D layer's CPU raster. A last, STAGED moment puts every widget up at once (the prompt, the
// stars searching, the air clock on a best, a full news column): the worst case for the layout.
//
// Every layout is held to `ge::hud::layout_problems` (no two boxes overlapping, none off the
// screen): a problem fails the lab. The snapshot's figures at each moment are metrics, so the
// digest moves when what the HUD reads moves; the pictures are measured (the adapter's pixels).
// Without an adapter it draws the HUD over a painted snowfield and says so; GE_GPU_REQUIRE=1
// fails instead.
//
// ge:summary Lab: Fall Line's HUD over the frame at three viewports, the free ride's moments
// ge:keywords lab hud heads-up-display overlay screenshot viewports portrait landscape phone
// ge:keywords free-ride moments air wanted news prompt layout-problems chase-camera
// ge:skill ui labs
// ge:test tests/fall_line_hud_test.cpp
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/hud/snapshot.hpp"
#include "fall_line/hud/view.hpp"
#include "fall_line/hud/words.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "ui/background.hpp"
#include "ui/raster.hpp"

namespace {

namespace fs = fall_line::sim;
namespace fh = fall_line::hud;
namespace gh = ge::hud;
namespace ui = ge::ui;
using ge::lab::Det;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && std::strcmp(v, "1") == 0;
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "1") == 0;
#endif
}

struct Viewport {
  int w, h;
};

std::vector<Viewport> viewports_of(const std::string& list) {
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
    if (by && he == std::errc{} && hx == end && w > 0 && h > 0 && w <= 4096 && h <= 4096)
      out.push_back({w, h});
    at = comma + 1;
  }
  return out;
}

// One moment of the run: the state's skiers for the frame and what the HUD showed.
struct Moment {
  std::string id;
  std::string title;
  std::string caption;
  std::string look;
  ge::skier::SkierState skier;
  fh::HudSnapshot snap;
  fh::HudRun run;
};

// A snowfield under a winter sky, for a machine with no adapter: the HUD's worst light (white
// snow under white words) is what it must still read over.
ui::Image painted_snow(int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  const ui::Stop sky[2] = {{0, ui::hex(0x2d5f9a)}, {1, ui::hex(0xbcd3ea)}};
  ui::Box b;
  b.rect = {0, 0, float(w), float(h) * 0.42f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, sky, 2);
  dl.box(b);
  const ui::Stop snow[2] = {{0, ui::hex(0xdfe8f2)}, {1, ui::hex(0xf9fbfd)}};
  b.rect = {0, float(h) * 0.42f, float(w), float(h) * 0.58f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, snow, 2);
  dl.box(b);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

// The hunted moment, s after the stars are raised.
constexpr double kHuntedAfter = 3;

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double seconds = lab.arg_num("seconds");
  const int settle = std::max(1, static_cast<int>(lab.arg_num("settle")));
  const std::vector<Viewport> viewports = viewports_of(lab.arg("viewports"));
  if (viewports.empty()) {
    lab.fail("--viewports=" + lab.arg("viewports") + " named no WxH size",
             "run make lab LAB=hud ARGS=--viewports=1280x720,390x844,844x390");
    return;
  }

  const auto level = std::make_shared<const ge::mountain::mapgen::Level>(fs::free_map(seed));
  fs::GameState st = fs::create_free_ride(level);
  const fs::BotPlan plan = fs::plan_free(*level, 0, st.spec);
  const gh::Style style = fh::hud_style();
  const fh::NewsInk ink = fh::news_ink(style);
  fh::HudRun run;
  const double raise_at = seconds * 0.45;

  // THE RUN, every step's snapshot weighed for the moments worth a picture.
  std::vector<Moment> moments;
  auto keep = [&](std::string id, std::string title, std::string caption, std::string look) {
    moments.push_back({std::move(id), std::move(title), std::move(caption), std::move(look),
                       st.racers[0].skier, fh::snapshot(st, run), run});
  };
  std::optional<Moment> longest;
  size_t busiest = 0;
  std::optional<Moment> news;
  bool cruised = false, hunted = false;
  while (st.t < seconds) {
    if (st.t >= raise_at && !st.wanted.active) {
      // Raised between steps (as a developer's key does), so its event is read here: the next
      // step clears the events before the HUD would see them.
      fs::set_wanted(st, 3);
      fh::note_step(run, st, ink);
    }
    const fs::Controls c = fs::bot_controls(st, 0, plan);
    fs::step(st, std::span<const fs::Controls>(&c, 1));
    fh::note_step(run, st, ink);
    const fh::HudSnapshot s = fh::snapshot(st, run);
    if (!cruised && st.t >= seconds * 0.15 && s.air_s == 0) {
      cruised = true;
      keep("cruise", "CRUISING",
           "The bot carving down the first run " + fixed(st.t, 1) +
               " s in: the seed and where he stands top left, the run and the metres skied under "
               "it, the speed with the edge bar over it bottom left.",
           "every figure readable over the snow and the sky; the edge bar's fill on the side he "
           "is turning to; nothing over the skier");
    }
    if (s.air_s > 0 && (!longest || s.air_s > longest->snap.air_s)) {
      longest = Moment{"air",
                       "IN THE AIR",
                       "His longest flight in the run's first " + fixed(seconds, 0) +
                           " s, at its longest: the air clock top centre, red while it beats the "
                           "run's best so far.",
                       "the air clock top centre (under the corners on the upright phone); no "
                       "overlap with the seed's column",
                       st.racers[0].skier,
                       s,
                       run};
    }
    if (!hunted && s.wanted && st.t >= raise_at + kHuntedAfter) {
      hunted = true;
      const bool searching = s.wanted->flashing;
      keep("hunted", searching ? "HUNTED, OUT OF SIGHT" : "HUNTED, IN SIGHT",
           "The wanted level set to three stars by hand " + fixed(raise_at, 0) + " s in, " +
               fixed(kHuntedAfter, 0) + " s on: " +
               (searching ? "the army has lost sight of him and searches where he was last seen, "
                            "so the lit stars blink and the bar under them drains."
                          : "the army has him in sight (its helicopters are out), so the stars "
                            "stand solid.") +
               " The survival timer counts the hunt.",
           "the lit stars top right, the survival timer under them; the WANTED line in the news");
    }
    if (run.news.lines().size() >= busiest && !run.news.lines().empty()) {
      busiest = run.news.lines().size();
      news =
          Moment{"news",
                 "THE NEWS COLUMN",
                 "The moment in the run with the most news up at once (" + std::to_string(busiest) +
                     " lines): what happened lately, newest lowest, each fading out after a "
                     "few seconds.",
                 "the column bottom right, clear of the speed; the newest line lowest; older "
                 "lines fainter",
                 st.racers[0].skier,
                 s,
                 run};
    }
  }
  if (longest) moments.push_back(*longest);
  if (news) moments.push_back(*news);
  // THE STAGED WORST CASE: every widget up at once over the run's last frame.
  {
    Moment m{"staged",
             "EVERY WIDGET UP (STAGED)",
             "Not a moment of the run: the last frame with every widget forced up at once (a "
             "stuck prompt, four stars searching, a best air, a full news column), the layout's "
             "worst case.",
             "nothing overlaps or leaves the screen at any viewport; the prompt clear of the air "
             "clock and the stars",
             st.racers[0].skier,
             fh::snapshot(st, run),
             run};
    m.snap.prompt = fh::Prompt::stuck;
    m.snap.air_s = 2.31, m.snap.air_best = true, m.snap.best_air_s = 1.92;
    fh::WantedTile w;
    w.stars = 4, w.flashing = true, w.search = 0.4, w.survived = 754;
    m.snap.wanted = w;
    m.run.news.clear();
    const double t = m.snap.t;
    m.run.news.push(fh::words::wipeout(ge::skier::Wipeout::across), style.bad, t - 3);
    m.run.news.push(fh::words::landed(2.31, 3.4), style.good, t - 2);
    m.run.news.push(fh::words::kTree, style.bad, t - 1);
    m.run.news.push(fh::words::lift_top(252), style.ink, t - 0.5);
    moments.push_back(std::move(m));
  }
  lab.metric("moments", static_cast<double>(moments.size()), "count");
  for (const Moment& m : moments) {
    lab.metric(m.id + ".t", m.snap.t, "s");
    lab.metric(m.id + ".speed", m.snap.speed_kmh, "km/h");
    lab.metric(m.id + ".air", m.snap.air_s, "s");
    lab.metric(m.id + ".stars", m.snap.wanted ? m.snap.wanted->stars : 0, "count");
    lab.metric(m.id + ".news", static_cast<double>(m.run.news.lines().size()), "count");
  }

  auto device = ge::gpu::create_device({.label = "hud", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE"))
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    lab.note("No GPU adapter on this machine: the HUD is drawn over a painted snowfield.");
  } else {
    lab.note("adapter: " + device->report.name + " / " + device->report.backend +
             (device->report.software ? " (software)" : ""));
  }
  std::unique_ptr<fall_line::app::GameFrame> frame;
  int problems = 0;
  for (const Viewport& v : viewports) {
    const std::string size = std::to_string(v.w) + "x" + std::to_string(v.h);
    std::optional<ge::gpu::Target> target;
    if (device) {
      if (!frame) {
        frame = fall_line::app::create_frame(*device, *level, static_cast<uint32_t>(v.w),
                                             static_cast<uint32_t>(v.h),
                                             wgpu::TextureFormat::RGBA8Unorm, {});
        if (!frame) {
          lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
          return;
        }
      } else {
        fall_line::app::resize(*device, *frame, static_cast<uint32_t>(v.w),
                               static_cast<uint32_t>(v.h));
      }
      target = ge::gpu::create_target(*device, "hud.final", static_cast<uint32_t>(v.w),
                                      static_cast<uint32_t>(v.h));
    }
    const gh::Frame f = gh::frame_for(v.w, v.h, style);
    for (const Moment& m : moments) {
      ui::Image img;
      if (device && frame && target) {
        ge::render::FrameView view;
        fall_line::app::day_view(*level, -1, view);
        fall_line::app::chase_view(*level, m.skier, view);
        const std::vector<ge::skier::SkierState> skiers{m.skier};
        fall_line::app::cut(*frame);
        for (int i = 0; i < settle; ++i) {
          view.frame = static_cast<uint64_t>(i);
          fall_line::app::draw(*device, *frame, view, {skiers}, target->view);
        }
        const ge::gpu::Image shot = ge::gpu::read_now(*device, *target);
        img.width = v.w, img.height = v.h, img.rgba = shot.rgba;
      } else {
        img = painted_snow(v.w, v.h);
      }
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      const std::vector<gh::Placed> placed = fh::draw_hud(dl, f, style, m.snap, m.run);
      for (const std::string& p : gh::layout_problems(f, placed)) {
        ++problems;
        lab.fail(std::string(m.id).append(" at ").append(size).append(": ").append(p),
                 "move or shrink the box in games/fall_line/hud/view.cpp, or shorten its words in "
                 "words.hpp; rerun `make lab LAB=hud`. Skill: ui.");
      }
      ui::render(dl, img);
      ge::lab::Picture pic;
      pic.width = img.width, pic.height = img.height, pic.rgba = std::move(img.rgba);
      lab.shot({m.id + "-" + size,
                m.title + " AT " + size + (v.h > v.w ? " (UPRIGHT)" : ""),
                m.caption,
                m.look,
                {{"seed", fixed(seed, 0)},
                 {"t", fixed(m.snap.t, 2)},
                 {"viewport", size},
                 {"scale", fixed(f.scale, 2)},
                 {"speed", fixed(m.snap.speed_kmh, 0) + " km/h"},
                 {"boxes", std::to_string(placed.size())}}},
               pic, Det::measured);
    }
  }
  lab.metric("layout.problems", problems, "count");
  lab.metric("viewports", static_cast<double>(viewports.size()), "count");
}

const ge::lab::LabSpec kSpec{
    "hud",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/hud (the snapshot, the words, the view) over engine/hud",
    "ui",
    "Does Fall Line's HUD read over the game's own frame, and lay out clean, at a desktop's, an "
    "upright phone's and a phone on its side's size?",
    {
        {"seed", "38", "the free ride's map seed"},
        {"seconds", "60", "how long the bot skis; the hunt is raised at 45% of it"},
        {"viewports", "1280x720,390x844,844x390", "screen sizes, WxH, comma-separated"},
        {"settle", "6", "frames drawn after each moment's cut before the capture"},
    },
    "--seconds=20 --viewports=640x360,360x640 --settle=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
