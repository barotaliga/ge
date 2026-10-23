// LAB `crowd-look`: the free ride's crowd (sim/crowd.hpp) drawn in Fall Line's frame as one
// palette family (render/skinning.hpp: the skier model, an amateur an instance, each posed in the
// vertex stage by his own palette; scene.hpp's `crowd_instance`). `--seed`'s free ride is dealt
// `--count` amateurs, run through the real `step` for `--at` seconds with the player stood
// still, then drawn for `--frames` frames at 60 a second (the run stepped between them), so each
// figure's lags and the TAA have settled on the moment shot:
//
// - QUEUE: the lift with the most in its queue, from 14 m down its line and 6 m up: the queue's
//   rows and the riders boarding.
// - BUSY: the amateur with the most others within 40 m, from 16 m behind and 6 m over him: a run
//   full of people turning their own turns.
// - GROUP: the biggest group skiing together, from 9 m ahead of its leader looking back: a
//   family or a class following him, each in his own pose and size.
// - DOWN: an amateur lying in the snow, from 5 m: the figure on its side (a blank when none is).
//
// Each look is filmed twice from the same moment, with the crowd and with none drawn (the
// before): the share of the picture the crowd changes, and over the frames the CPU's ms a frame
// and the GPU's for the instances' colour pass and the figures' shadow atlas, each side, all
// measured. How many of the crowd are drawn (on the snow, not on a lift) is beside them. Without an
// adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the free ride's crowd drawn as one palette family, at a queue, a busy run, a
// group
// ge:keywords lab crowd crowd-look palette-family vertex-skinning amateurs queue group picture
// ge:keywords fall-line pose-cost
// ge:skill render sim labs
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/crowd.hpp"
#include "fall_line/sim/crowd_lift.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/stats.hpp"
#include "skier/skier.hpp"

namespace {

using ge::lab::Det;
namespace fs = fall_line::sim;
namespace r = ge::render;

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

constexpr double kFrame = 1.0 / 60;

struct Look {
  const char* id;
  const char* title;
  const char* caption;
  const char* look_for;
};
constexpr Look kLooks[] = {
    {"queue", "THE BUSIEST LIFT'S QUEUE",
     "From 14 m down the lift's line and 6 m up, looking at its load point: the queue in its rows "
     "behind it, the riders taken onto the carriers.",
     "rows of figures standing in the queue facing up the line; none floating or sunk; each in "
     "his own jacket"},
    {"busy", "THE BUSIEST STRETCH OF RUN",
     "From 16 m behind and 6 m over the amateur with the most others within 40 m.",
     "figures turning their own turns, leaning into them, at different sizes; their skis on the "
     "snow; no two in lockstep"},
    {"group", "A GROUP SKIING TOGETHER",
     "From 9 m ahead of the leader of the biggest group on the snow, looking back up at it.",
     "the followers behind and beside the leader, each in his own pose; children smaller"},
    {"down", "ONE OF THEM DOWN", "From 5 m behind an amateur lying in the snow after a fall.",
     "the figure lying on its side on the snow, its skis with it; nothing through the snow"},
};

// The free ride stepped `seconds` with the player stood where he started.
void run_for(fs::GameState& st, double seconds) {
  const ge::skier::SkierState stood = st.racers[0].skier;
  const fs::Controls none{};
  const double until = st.t + seconds;
  while (st.t < until) {
    st.racers[0].skier = stood;
    fs::step(st, std::span<const fs::Controls>(&none, 1));
  }
}

// The camera `back` m behind and `up` m over the point (x, y, z), looking along `heading` at it.
void look_at(const ge::mountain::mapgen::Level& level, double x, double y, double z, double heading,
             double back, double up, r::FrameView& v) {
  const double ex = x - back * ge::core::libm::sin(heading),
               ez = z - back * ge::core::libm::cos(heading);
  v.eye[0] = ex;
  v.eye[2] = ez;
  v.eye[1] = std::max(y + up, level.ground_at(ex, ez) + 1.5);
  const double tx = x - ex, tz = z - ez, ty = y + 1.0 - v.eye[1];
  v.heading = ge::core::libm::atan2(tx, tz);
  v.pitch = ge::core::libm::atan2(ty, ge::core::hypot(tx, tz));
}

// Where each look stands its camera on the crowd as it is; false when it has nothing to show.
bool aim(const fs::GameState& st, const char* id, r::FrameView& v) {
  const fs::CrowdState& c = st.crowd;
  const ge::mountain::mapgen::Level& level = *st.level;
  if (std::strcmp(id, "queue") == 0) {
    int best = -1, most = 0;
    for (size_t k = 0; k < c.queues.size(); ++k) {
      const int n = c.queues[k].issued - c.queues[k].served;
      if (n > most) {
        most = n;
        best = static_cast<int>(k);
      }
    }
    if (best < 0) return false;
    const ge::mountain::lifts::Place p =
        fs::crowd::load_place((*st.lifts)[static_cast<size_t>(best)]);
    look_at(level, p.x, level.ground_at(p.x, p.z), p.z, p.heading, 14, 6, v);
    return true;
  }
  if (std::strcmp(id, "busy") == 0 || std::strcmp(id, "group") == 0) {
    const bool busy = id[0] == 'b';
    int best = -1;
    size_t most = 0;
    for (size_t i = 0; i < c.amateurs.size(); ++i) {
      const fs::Amateur& a = c.amateurs[i];
      if (a.mode != fs::AmateurMode::ski) continue;
      size_t n = 0;
      if (busy) {
        for (const fs::Amateur& b : c.amateurs) {
          if (&b != &a && b.mode == fs::AmateurMode::ski &&
              ge::core::hypot(b.x - a.x, b.z - a.z) < 40) {
            ++n;
          }
        }
      } else if (a.rank == 0) {
        for (int32_t m : c.groups[static_cast<size_t>(a.group)].members) {
          n += c.amateurs[static_cast<size_t>(m)].mode == fs::AmateurMode::ski ? 1 : 0;
        }
      }
      if (n > most) {
        most = n;
        best = static_cast<int>(i);
      }
    }
    if (best < 0) return false;
    const fs::Amateur& a = c.amateurs[static_cast<size_t>(best)];
    // The busy stretch from behind; the group from 9 m ahead of its leader, looking back at it.
    if (busy) {
      look_at(level, a.x, a.y, a.z, a.heading, 16, 6, v);
    } else {
      look_at(level, a.x, a.y, a.z, a.heading + 3.14159265358979323846, 9, 2.5, v);
    }
    return true;
  }
  for (const fs::Amateur& a : c.amateurs) {
    if (a.mode == fs::AmateurMode::down && !a.rising) {
      look_at(level, a.x, a.y, a.z, a.heading, 5, 2, v);
      return true;
    }
  }
  return false;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const int count = static_cast<int>(lab.arg_num("count"));
  const double at = lab.arg_num("at");
  const int frames = std::max(1, static_cast<int>(lab.arg_num("frames")));
  const int px = static_cast<int>(lab.arg_num("px"));
  const int width = px, height = px * 9 / 16;

  const auto level = std::make_shared<const ge::mountain::mapgen::Level>(
      fs::free_map(seed, static_cast<int>(lab.arg_num("version"))));
  fs::GameState st = fs::create_free_ride(level, 0);
  fs::deal_crowd(st, count);
  run_for(st, at);
  int drawn = 0;
  for (const fs::Amateur& a : st.crowd.amateurs) drawn += fall_line::app::crowd_drawn(a) ? 1 : 0;
  lab.metric("amateurs", static_cast<double>(st.crowd.amateurs.size()), "count");
  lab.metric("drawn", drawn, "count");

  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  auto shot_of = [&](const Look& l) {
    return ge::lab::Shot{
        l.id,
        std::string(l.title) + ", SEED " + seed_s + ", " + std::to_string(count) + " AMATEURS",
        l.caption,
        l.look_for,
        {{"seed", seed_s}, {"count", std::to_string(count)}}};
  };

  auto device = ge::gpu::create_device({.label = "crowd-look", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Look& l : kLooks) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(l), none, Det::measured);
    }
    lab.metric("gpu-errors", 0, "count");
    return;
  }
  fall_line::app::FrameOptions options;
  auto frame = fall_line::app::create_frame(*device, *level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "crowd.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

  // Each look filmed twice from the same moment: with the crowd (the shot), and with none drawn
  // (the before): the frame's CPU ms, the instances' and the figures' shadows' GPU ms.
  struct Film {
    ge::gpu::Image image;
    std::vector<double> cpu;
    double instances = 0, shadows = 0;
  };
  const int steps = static_cast<int>(std::lround(kFrame / fs::kDt));
  auto film = [&](const fs::GameState& from, r::FrameView v, bool crowd) {
    Film out;
    fs::GameState shot = from;
    fall_line::app::cut(*frame);
    for (int f = 0; f < frames; ++f) {
      const ge::skier::SkierState skier = shot.racers[0].skier;
      fall_line::app::FrameScene scene{
          std::span<const ge::skier::SkierState>(&skier, 1), f == 0 ? 0.0 : kFrame, f == 0, {}};
      if (crowd) scene.crowd = shot.crowd.amateurs;
      v.frame = static_cast<uint64_t>(f);
      v.time_s = shot.t;
      const auto t0 = std::chrono::steady_clock::now();
      fall_line::app::draw(*device, *frame, v, scene, target.view);
      out.cpu.push_back(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
      if (f + 1 < frames) run_for(shot, steps * fs::kDt);
    }
    out.image = ge::gpu::read_now(*device, target);
    frame->recorder.wait(*device);
    for (const r::PassStat& p : frame->recorder.stats.passes) {
      if (p.name == "instances") out.instances = p.gpu_ms;
      if (p.name == "shadows.figures") out.shadows = p.gpu_ms;
    }
    return out;
  };
  std::vector<double> cpu[2], instances[2], shadows[2];
  for (const Look& l : kLooks) {
    r::FrameView v;
    fall_line::app::day_view(*level, -1, v);
    if (!aim(st, l.id, v)) {
      // A shot still, so the digest is the same whatever the moment shows.
      lab.note(std::string(l.id) + ": nothing to show at " + std::to_string(at) + " s");
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NOTHING TO SHOW", {230, 230, 230});
      lab.shot(shot_of(l), none, Det::measured);
      continue;
    }
    const Film with = film(st, v, true), without = film(st, v, false);
    size_t covered = 0;
    for (size_t p = 0; p + 3 < with.image.rgba.size(); p += 4) {
      const int d = std::abs(with.image.rgba[p] - without.image.rgba[p]) +
                    std::abs(with.image.rgba[p + 1] - without.image.rgba[p + 1]) +
                    std::abs(with.image.rgba[p + 2] - without.image.rgba[p + 2]);
      covered += d > 24 ? 1 : 0;
    }
    lab.metric(std::string(l.id) + ".covered",
               100.0 * static_cast<double>(covered) / (static_cast<double>(width) * height), "%",
               Det::measured);
    const Film* both[2] = {&without, &with};
    for (int k = 0; k < 2; ++k) {
      cpu[k].insert(cpu[k].end(), both[k]->cpu.begin() + 1, both[k]->cpu.end());
      instances[k].push_back(both[k]->instances);
      shadows[k].push_back(both[k]->shadows);
    }
    lab.shot(shot_of(l), ge::lab::Picture{width, height, with.image.rgba}, Det::measured);
  }
  const char* side[2] = {"none", "crowd"};
  for (int k = 0; k < 2; ++k) {
    if (cpu[k].empty()) continue;
    const std::string at_k = side[k];
    lab.metric(at_k + ".frame-cpu.median", ge::lab::median(cpu[k]), "ms", Det::measured);
    lab.metric(at_k + ".instances-gpu.median", ge::lab::median(instances[k]), "ms", Det::measured);
    lab.metric(at_k + ".shadows-gpu.median", ge::lab::median(shadows[k]), "ms", Det::measured);
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing the crowd",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "crowd-look",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/app/scene.hpp's crowd as engine/render/skinning.hpp's palette family",
    "render",
    "Does the free ride's crowd read as people on the snow, each in his own pose, and what does "
    "it cost a frame?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator version (the ski area's)"},
        {"count", "400", "how many amateurs are dealt"},
        {"at", "90", "s of the free ride run before the shots"},
        {"frames", "12", "frames drawn into each shot at 60 a second"},
        {"px", "768", "the pictures' width (16:9)"},
    },
    "--px=256 --count=120 --at=30 --frames=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
