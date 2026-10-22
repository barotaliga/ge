// LAB `trails`: the tracks (plugins/mountain/app/trails.hpp, laid by skier/app/tracks.hpp) in
// Fall Line's frame. The bot skis a seed's map with --rivals beside him for --until seconds, every
// racer laying his tracks at 60 frames a second, and three places are shot and measured:
//
// - TURN: looking down on the player's track where he was most on his edges. The outside ski
//   carries more of him, so its groove must be the deeper: read back from the fine trail map
//   across his line there (the depth of each groove at its deepest, and its width at half that).
// - STRAIGHT: the same where he was flattest: two grooves of a depth.
// - RIVAL: the chase camera on a racer where he first comes within 4 m of a line another laid
//   a second or more before him: that track must still be in the fine map there (laid when it
//   may have lain outside the window, it is redrawn from the history when the window reaches it).
//
// What must hold on every row is deterministic (the run, the places, every print's numbers off
// the CPU's arithmetic); the pictures and the depths read back from the map are measured, and the
// map must agree with the arithmetic (the outside groove the deeper, the rival's track there).
// Without an adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the tracks through a turn and on a straight, and a rival's track still there
// ge:keywords lab trails tracks grooves outside-ski turn transect rival history fine-window
// ge:keywords fall-line picture readback
// ge:skill mountain render labs
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/app/trails.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/app/tracks.hpp"

namespace {

using ge::lab::Det;
using ge::skier::SkierState;

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

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// Where the ski on `side` (0 left, 1 right) of `s` touches, on the plan.
void foot(const SkierState& s, int side, double* x, double* z) {
  const double m = side == 0 ? -1.0 : 1.0;
  *x = s.x + m * ge::skier::kTrackStance * ge::core::libm::cos(s.heading);
  *z = s.z - m * ge::skier::kTrackStance * ge::core::libm::sin(s.heading);
}

// The groove depth the fine map holds at (x, z), m (bilinear over the read-back texels).
double depth_at(const ge::gpu::Image& map, const ge::mountain::app::TrailRect& r, double x,
                double z) {
  const double u = (x - r.x0) / r.span * map.width - 0.5;
  const double v = (z - r.z0) / r.span * map.height - 0.5;
  const int c = static_cast<int>(std::floor(u)), w = static_cast<int>(std::floor(v));
  auto at = [&](int cc, int rr) {
    if (cc < 0 || rr < 0 || cc >= static_cast<int>(map.width) || rr >= static_cast<int>(map.height))
      return 0.0;
    const double e =
        map.rgba[(static_cast<size_t>(rr) * map.width + static_cast<size_t>(cc)) * 4] / 255.0;
    return e * e * ge::mountain::app::kTrailDepth;
  };
  const double fu = u - c, fv = v - w;
  return (at(c, w) * (1 - fu) + at(c + 1, w) * fu) * (1 - fv) +
         (at(c, w + 1) * (1 - fu) + at(c + 1, w + 1) * fu) * fv;
}

// A groove across the skier's line through one foot: its deepest (m) within 8 cm of the foot, and
// its width (m) where it is at least half that, sampled each centimetre.
struct Groove {
  double depth = 0, width = 0;
};

Groove groove_at(const ge::gpu::Image& map, const ge::mountain::app::TrailRect& r,
                 const SkierState& s, int side) {
  double fx, fz;
  foot(s, side, &fx, &fz);
  const double ax = ge::core::libm::cos(s.heading), az = -ge::core::libm::sin(s.heading);
  std::vector<double> d;
  for (int i = -8; i <= 8; ++i)
    d.push_back(depth_at(map, r, fx + ax * i * 0.01, fz + az * i * 0.01));
  Groove g;
  g.depth = *std::max_element(d.begin(), d.end());
  for (double v : d) {
    if (g.depth > 0 && v >= g.depth / 2) g.width += 0.01;
  }
  return g;
}

struct Place {
  bool found = false;
  size_t sample = 0;    // the player's sample
  size_t laid = 0;      // RIVAL: the rival's sample whose line it is
  size_t who = 0;       // RIVAL: whose line it is
  size_t follower = 0;  // RIVAL: who comes down it
};

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const int rivals = static_cast<int>(lab.arg_num("rivals"));
  const double until = lab.arg_num("until");
  const double hour = lab.arg_num("hour");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int settle = static_cast<int>(lab.arg_num("settle"));
  if (rivals < 1 || until <= 2 || width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
      settle < 1) {
    lab.fail(
        "--rivals must be at least 1, --until over 2 s, --width and --height 1..4096 and "
        "--settle at least 1",
        "next: pass --rivals=3 --until=20 --width=640 --height=360.");
    return;
  }
  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const auto shared = std::make_shared<const ge::mountain::mapgen::Level>(
      ge::mountain::mapgen::generate_level(seed, opts));
  const ge::mountain::mapgen::Level& level = *shared;
  const std::string seed_s = std::to_string(static_cast<long long>(seed));

  // THE RUN, every racer every second step (60 a second), and the prints the CPU lays off it.
  std::vector<std::vector<SkierState>> samples;
  {
    fall_line::sim::Driver run(shared, rivals);
    int steps = 0;
    while (run.state().t < until && !run.over()) {
      run.step(fall_line::sim::bot_controls(run.state(), 0, run.plan()));
      if (++steps % 2 != 0) continue;
      samples.emplace_back();
      for (const auto& racer : run.state().racers) samples.back().push_back(racer.skier);
    }
  }
  std::vector<ge::skier::TrackPen> pens(samples.empty() ? 0 : samples[0].size());
  // The player's prints at each sample (none in the air or the first).
  std::vector<std::array<ge::skier::SkiPrint, 2>> prints(samples.size());
  std::vector<int> printed(samples.size(), 0);
  size_t laid = 0;
  for (size_t i = 0; i < samples.size(); ++i) {
    for (size_t k = 0; k < samples[i].size(); ++k) {
      ge::skier::SkiPrint p[2]{};
      const int n = ge::skier::ski_prints(samples[i][k], ge::skier::kReferenceSkis, pens[k], p);
      laid += static_cast<size_t>(n);
      if (k == 0) {
        printed[i] = n;
        prints[i] = {p[0], p[1]};
      }
    }
  }
  lab.metric("samples", static_cast<double>(samples.size()), "count");
  lab.metric("prints", static_cast<double>(laid), "count");

  // THE PLACES. A carving sample is on the snow, up, at a pace, two seconds or more into the run
  // and a second or more before its end.
  auto skiing = [&](size_t i) {
    const SkierState& s = samples[i][0];
    return printed[i] == 2 && i + 60 < samples.size() && printed[i + 1] == 2 && s.speed > 5 &&
           i > 120;
  };
  Place turn, straight, rival;
  for (size_t i = 0; i < samples.size(); ++i) {
    if (!skiing(i)) continue;
    const double e = std::fabs(samples[i][0].edge);
    if (!turn.found || e > std::fabs(samples[turn.sample][0].edge)) turn = {true, i, 0, 0, 0};
    if (!straight.found || e < std::fabs(samples[straight.sample][0].edge))
      straight = {true, i, 0, 0, 0};
  }
  // Any racer coming down another's line: the first sample, five seconds or more into the run
  // (clear of the start, where they stand abreast), where one at a pace is within 4 m of where
  // another was a second or more before.
  for (size_t j = 0; j < samples.size() && !rival.found; ++j) {
    for (size_t f = 0; f < samples[j].size() && !rival.found; ++f) {
      const SkierState& p = samples[j][f];
      if (j < 300 || !p.on_snow || p.down || p.speed < 5) continue;
      for (size_t i = 0; i + 60 <= j && !rival.found; ++i) {
        for (size_t k = 0; k < samples[i].size(); ++k) {
          const SkierState& r = samples[i][k];
          if (k != f && r.on_snow && !r.down && ge::core::hypot(r.x - p.x, r.z - p.z) < 4.0) {
            rival = {true, j, i, k, f};
            break;
          }
        }
      }
    }
  }
  auto place_metrics = [&](const char* name, const Place& at) {
    lab.metric(std::string(name) + ".found", at.found ? 1 : 0, "count");
    if (!at.found) return;
    const SkierState& s = samples[at.sample][at.follower];
    lab.metric(std::string(name) + ".t", (static_cast<double>(at.sample) + 1) / 60, "s");
    lab.metric(std::string(name) + ".edge", std::fabs(s.edge) * 57.29577951308232, "deg");
    lab.metric(std::string(name) + ".speed", s.speed, "m/s");
    lab.metric(std::string(name) + ".packed", s.packed, "share");
    lab.metric(std::string(name) + ".sink", s.sink * 1000, "mm");
  };
  place_metrics("turn", turn);
  place_metrics("straight", straight);
  place_metrics("rival", rival);
  if (rival.found) {
    lab.metric("rival.gap", static_cast<double>(rival.sample - rival.laid) / 60, "s");
  }
  // What the arithmetic laid at the turn and the straight: each ski's print (outside first).
  auto print_metrics = [&](const char* name, const Place& at) {
    if (!at.found) return;
    const SkierState& s = samples[at.sample][0];
    const int out = s.edge > 0 ? 0 : 1;
    const auto& p = prints[at.sample];
    lab.metric(std::string(name) + ".print-outside-depth", p[out].depth * 1000, "mm");
    lab.metric(std::string(name) + ".print-inside-depth", p[1 - out].depth * 1000, "mm");
    lab.metric(std::string(name) + ".print-width", p[out].half_width * 200, "cm");
  };
  print_metrics("turn", turn);
  print_metrics("straight", straight);
  if (!turn.found || !straight.found) {
    lab.fail("the run never carved on the snow at a pace (no TURN or STRAIGHT place)",
             "next: run longer (--until) or another --seed.");
    return;
  }
  if (!rival.found) {
    lab.note(
        "No racer came within 4 m of another's line laid a second before him: no RIVAL shot "
        "(another --seed, more --rivals or a longer --until finds one).");
  }

  // THE SHOTS.
  struct Shot {
    std::string id;
    const Place* place;
    bool chase;
  };
  std::vector<Shot> shots;
  if (rival.found) shots.push_back({"rival", &rival, true});
  shots.push_back({"turn", &turn, false});
  shots.push_back({"straight", &straight, false});
  auto shot_of = [&](const Shot& sh) {
    const SkierState& s = samples[sh.place->sample][sh.place->follower];
    const std::string t = fixed((static_cast<double>(sh.place->sample) + 1) / 60, 2);
    std::string title = "SEED " + seed_s + ", " + sh.id + " AT " + t + " S";
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    std::string caption, look;
    if (sh.id == "rival") {
      caption = "The chase camera on racer " + std::to_string(sh.place->follower) + ", " + t +
                " s into the run, where a racer first comes within 4 m of a line another (racer " +
                std::to_string(sh.place->who) + ") laid " +
                fixed(static_cast<double>(sh.place->sample - sh.place->laid) / 60, 1) +
                " s before him.";
      look =
          "the other racer's two grooves on the snow beside and ahead of him, as sharp as his "
          "own; no seam where the fine window meets the coarse map";
    } else {
      caption = "Looking back down the player's track from just past where he was " +
                std::string(sh.id == "turn" ? "most" : "least") + " on his edges (" +
                fixed(std::fabs(s.edge) * 57.29577951308232, 0) + " degrees at " +
                fixed(s.speed, 1) + " m/s), after the run has passed; the racers are not drawn.";
      look = sh.id == "turn" ? "two thin grooves a stance apart, the outside one (the left on a "
                               "right turn) deeper and darker; the grooves curve with the turn, "
                               "no gaps or beads along them"
                             : "two grooves of a depth a stance apart, straight and even";
    }
    return ge::lab::Shot{sh.id,
                         title,
                         caption,
                         look,
                         {{"seed", seed_s},
                          {"place", sh.id},
                          {"t", t},
                          {"hour", fixed(hour, 2)},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "trails", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Shot& sh : shots) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(sh), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  fall_line::app::FrameOptions options;
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "trails.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  auto shoot = [&](const Shot& sh, const std::vector<SkierState>& racers) {
    const SkierState& s = samples[sh.place->sample][sh.place->follower];
    ge::render::FrameView view;
    fall_line::app::day_view(level, hour, view);
    if (sh.chase) {
      fall_line::app::chase_view(level, s, view);
    } else {
      // Over the track, a metre and a half on along it and four and a half up, looking back down
      // the way he came.
      const double fx = ge::core::libm::sin(s.heading), fz = ge::core::libm::cos(s.heading);
      const double x = s.x + 1.5 * fx, z = s.z + 1.5 * fz;
      view.eye[0] = x;
      view.eye[1] = ge::core::sample_field(level.ground, x, z) + 4.5;
      view.eye[2] = z;
      view.heading = s.heading + 3.141592653589793;
      view.pitch = -1.15;
    }
    fall_line::app::cut(*frame);
    for (int i = 0; i < settle; ++i) {
      view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, view, {racers}, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    lab.shot(shot_of(sh), ge::lab::Picture{width, height, img.rgba}, Det::measured);
    return ge::gpu::read_now(*device, frame->trails.fine);
  };
  // Lay the run's tracks, shooting the RIVAL place when the run reaches it.
  for (size_t i = 0; i < samples.size(); ++i) {
    fall_line::app::track(*frame, {samples[i]});
    if (!rival.found || i != rival.sample) continue;
    const ge::gpu::Image map = shoot(shots[0], samples[i]);
    const ge::mountain::app::TrailRect rect = ge::mountain::app::fine_rect(frame->trails);
    double deepest = 0;
    for (int side = 0; side < 2; ++side) {
      deepest = std::max(deepest, groove_at(map, rect, samples[rival.laid][rival.who], side).depth);
    }
    lab.metric("rival.track-depth", deepest * 1000, "mm", Det::measured);
    if (deepest <= 0) {
      lab.fail("the rival's track is not in the fine map where the player comes down it",
               "Check the redraw from the history in plugins/mountain/app/trails.cpp "
               "(prepare_trails, the buckets). Skill: mountain.");
    }
  }
  for (const Shot& sh : shots) {
    if (sh.chase) continue;
    const ge::gpu::Image map = shoot(sh, {});
    const ge::mountain::app::TrailRect rect = ge::mountain::app::fine_rect(frame->trails);
    const SkierState& s = samples[sh.place->sample][0];
    const int out = s.edge > 0 ? 0 : 1;
    const Groove o = groove_at(map, rect, s, out), in = groove_at(map, rect, s, 1 - out);
    lab.metric(sh.id + ".outside-depth", o.depth * 1000, "mm", Det::measured);
    lab.metric(sh.id + ".inside-depth", in.depth * 1000, "mm", Det::measured);
    lab.metric(sh.id + ".outside-width", o.width * 100, "cm", Det::measured);
    lab.metric(sh.id + ".inside-width", in.width * 100, "cm", Det::measured);
    if (sh.id == "turn" && !(o.depth > in.depth)) {
      lab.fail("the outside groove is not the deeper through the turn (" +
                   fixed(o.depth * 1000, 1) + " mm against " + fixed(in.depth * 1000, 1) + ")",
               "Check ski_load and ski_prints in plugins/skier/app/tracks.cpp, then the stamp's "
               "cross-section in plugins/mountain/app/trails.wgsl. Skill: skier.");
    }
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  lab.metric("pipelines-after-load", device->pipelines_after_load, "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (device->pipelines_after_load != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_frame's manifest. Skill: gpu, § Pipelines.");
  }
}

const ge::lab::LabSpec kSpec{
    "trails",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "plugins/mountain/app/trails.hpp (the trail map) and plugins/skier/app/tracks.hpp (the prints) "
    "in Fall Line's frame",
    "mountain",
    "Do the tracks open and deepen on the outside ski through a turn, and is a rival's track still "
    "there when the player comes down it?",
    {
        {"seed", "38", "the map's seed"},
        {"rivals", "3", "the bots racing beside him (at least 1)"},
        {"until", "20", "seconds of the run to ski"},
        {"hour", "11", "the solar hour to light the map at"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"settle", "8", "frames drawn after each shot's cut before the capture"},
    },
    "--until=12 --width=160 --height=90 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
