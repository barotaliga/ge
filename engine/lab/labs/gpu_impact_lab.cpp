// LAB `gpu-impact`: ROUNDS HITTING SURFACES (render/impact.hpp, drawn by render/explosions.hpp
// and the particle pool) on the software adapter, on the explosion labs' yard. Each surface class
// is hit where it would be met: stone, metal, wood and tile on the wall, dirt, grass, sand and
// water on the ground before it, the round coming in from the shooter's right. Named views: one
// nail into each surface through the burst's life by day; the same at dusk, where the sparks
// show; a shotgun's spread of pellets into each; a nailgun's stream into stone and into metal.
// A table lists what one hit of each look throws. Measured: the explosions pass's GPU time for a
// shotgun's spread of dust, and the CPU's throw. It fails on a GPU error or a pipeline made after
// the load. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: nails and pellets hitting each surface class, by day and dusk, and its cost
// ge:keywords lab gpu impact hit bullet-hit nail pellet shotgun nailgun surface material stone
// ge:keywords metal wood dirt sand tile water dust sparks chips picture software-adapter
// ge:skill render labs
// ge:test tests/render_impact_test.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/impact.hpp"
#include "render/particles.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

// Each surface and where it is met: on the wall (the yard's step at z 139..140, rising 4 m in
// one, so its face leans back) or on the ground before it.
struct Target {
  const char* surface;
  const char* name;
  bool wall;
};
constexpr std::array<Target, 8> kTargets{{
    {"stone", "STONE", true},
    {"metal", "METAL", true},
    {"wood", "WOOD", true},
    {"tile", "TILE", true},
    {"dirt", "DIRT", false},
    {"grass", "GRASS", false},
    {"sand", "SAND", false},
    {"liquid", "WATER", false},
}};

constexpr double kShooter[3] = {es::kMid + 1.4, 1.5, 135.5};  // where the rounds come from

// The point on the target `dx`, `dy` m off its middle, its normal, and the round's way to it.
gr::Impact aim(const Target& t, double dx, double dy, const gr::ImpactRound* round, uint32_t seed,
               double born_s) {
  gr::Impact h;
  if (t.wall) {
    const double y = 1.3 + dy;
    h.at[0] = es::kMid + dx;
    h.at[1] = y;
    h.at[2] = 139 + y / es::kWallH;
    const double n = std::sqrt(1 + es::kWallH * es::kWallH);
    h.normal[0] = 0;
    h.normal[1] = 1 / n;
    h.normal[2] = -es::kWallH / n;
  } else {
    h.at[0] = es::kMid + dx;
    h.at[1] = 0;
    h.at[2] = 138.2 + dy;
  }
  double d[3], l = 0;
  for (int i = 0; i < 3; ++i) {
    d[i] = h.at[i] - kShooter[i];
    l += d[i] * d[i];
  }
  l = std::sqrt(l);
  for (int i = 0; i < 3; ++i) h.dir[i] = d[i] / l;
  h.look = gr::impact_look_for(t.surface);
  h.round = round;
  h.seed = seed;
  h.born_s = born_s;
  return h;
}

// The eye on a target: square to the wall, or looking down onto the ground before it.
gr::FrameView eye_on(const Target& t, double sun_el) {
  return t.wall ? es::eye_at(es::kMid, 1.4, 138.2, 0.0, sun_el, 0, 0)
                : es::eye_at(es::kMid, 0.8, 137.0, -0.5, sun_el, 0, 0);
}

// A shotgun's spread: `n` pellets round the aim within `r` m, each its own seed.
std::vector<gr::Impact> spread(const Target& t, int n, double r, uint32_t seed, double born_s) {
  ge::core::Rng rng = ge::core::make_rng(seed);
  std::vector<gr::Impact> out;
  out.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    const double a = ge::core::range(rng, 0, 6.283185307179586);
    const double d = r * std::sqrt(ge::core::next(rng));
    out.push_back(aim(t, d * ge::core::libm::cos(a), d * ge::core::libm::sin(a),
                      gr::impact_round_by_id("round.pellet"), seed * 31 + static_cast<uint32_t>(i),
                      born_s));
  }
  return out;
}

std::vector<gr::Blast> dust_at(const std::vector<gr::Impact>& hits, double t) {
  std::vector<gr::Blast> out;
  out.reserve(hits.size());
  for (const gr::Impact& h : hits) {
    gr::Blast b;
    if (h.born_s <= t + 1e-9 && gr::dust_of(h, b)) out.push_back(b);
  }
  return out;
}

// The hits flown frame by frame at 120 Hz, each thrown into the pool the frame it lands, and a
// picture at each moment asked for (settled over `settle` frames).
std::vector<gl::Picture> run_hits(ge::gpu::Device& d, es::Scene& s, std::vector<gr::Impact> hits,
                                  const gr::FrameView& v0, const std::vector<double>& moments,
                                  int settle) {
  std::sort(hits.begin(), hits.end(),
            [](const gr::Impact& a, const gr::Impact& b) { return a.born_s < b.born_s; });
  gr::clear_particles(d, s.particles);
  std::vector<gl::Picture> out;
  out.reserve(moments.size());
  size_t thrown = 0;
  double t = 0;
  for (const double m : moments) {
    while (t + 1e-9 < m) {
      const double dt = std::min(1.0 / 120, m - t);
      t += dt;
      gr::FrameView v = v0;
      v.time_s = t;
      for (; thrown < hits.size() && hits[thrown].born_s <= t + 1e-9; ++thrown) {
        gr::throw_impact(s.particles, hits[thrown]);
      }
      es::draw(d, s, v, dust_at(hits, t), dt);
    }
    gr::FrameView v = v0;
    v.time_s = m;
    out.push_back(es::settled(d, s, v, dust_at(hits, m), settle));
  }
  return out;
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const auto h = static_cast<uint32_t>(lab.arg_num("height"));
  const int settle = std::clamp(static_cast<int>(lab.arg_num("settle")), 1, 16);
  if (w < 64 || h < 36 || w > 4096 || h > 4096) {
    lab.fail("--width must be 64..4096 and --height 36..4096", "next: pass --width=320.");
    return;
  }
  // What one hit of each look throws, nail and pellet.
  {
    gl::LabTable t{"WHAT ONE HIT THROWS",
                   {"surface", "round", "sparks", "chips", "grit", "drops", "dust"},
                   {}};
    for (const Target& tg : kTargets) {
      for (const gr::ImpactRound& r : gr::impact_rounds()) {
        const gr::Impact hit = aim(tg, 0, 0, &r, 1, 0);
        std::vector<gr::Spark> sp(gr::impact_births_max(gr::look_of(hit), r));
        sp.resize(gr::impact_births(hit, sp));
        int n[5] = {0, 0, 0, 0, 0};
        for (const gr::Spark& x : sp) ++n[static_cast<int>(x.kind)];
        gr::Blast b;
        t.rows.push_back({tg.surface, std::string(r.id), std::to_string(n[0]), std::to_string(n[2]),
                          std::to_string(n[3]), std::to_string(n[4]),
                          gr::dust_of(hit, b) ? fixed(gr::look_of(hit).dust.smoke, 2) : "none"});
        lab.metric(std::string("births.") + tg.surface + "." + std::string(r.id.substr(6)),
                   static_cast<double>(sp.size()), "count");
      }
    }
    lab.table(t);
  }
  // The CPU's half: a shotgun's spread of 12 pellets thrown, many times over.
  {
    const std::vector<gr::Impact> hits = spread(kTargets[0], 12, 0.25, 5, 0);
    std::array<gr::Spark, 512> sp{};
    size_t total = 0;
    const auto t0 = std::chrono::steady_clock::now();
    constexpr int kRounds = 2000;
    for (int k = 0; k < kRounds; ++k) {
      for (const gr::Impact& x : hits) total += gr::impact_births(x, sp);
    }
    lab.metric(
        "cpu.throw-spread",
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() /
            kRounds,
        "us", Det::measured);
    lab.metric("thrown.spread", static_cast<double>(total) / kRounds, "count");
  }
  auto device = ge::gpu::create_device({.label = "gpu-impact", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"nail-day", "nail-dusk", "shotgun", "stream"}, w, h);
    return;
  }
  es::Scene s;
  if (!es::make_scene(*device, s, w, h, 10)) {
    lab.fail("the explosions' pipelines did not build",
             "Read the gpu: lines above. Skill: render.");
    return;
  }

  const gr::ImpactRound* nail = gr::impact_round_by_id("round.nail");
  const auto by_surface = [&](double sun_el, const std::vector<double>& at, const char* id,
                              const char* title, const char* what, const char* look_for,
                              bool shotgun) {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(kTargets.size() * at.size());
    names.reserve(kTargets.size() * at.size());
    for (const Target& tg : kTargets) {
      const std::vector<gr::Impact> hits =
          shotgun ? spread(tg, 9, 0.22, 3, 0.001)
                  : std::vector<gr::Impact>{aim(tg, 0, 0, nail, 3, 0.001)};
      for (gl::Picture& p : run_hits(*device, s, hits, eye_on(tg, sun_el), at, settle)) {
        pics.push_back(std::move(p));
      }
      for (const double m : at) names.push_back(std::string(tg.name) + " " + fixed(m, 2) + " S");
    }
    lab.shot({id, title, what, look_for, {}},
             es::sheet_of(pics, names, static_cast<int>(at.size())), Det::measured);
  };
  by_surface(0.6, {0.02, 0.06, 0.15, 0.4, 1.0}, "nail-day", "ONE NAIL INTO EACH SURFACE, BY DAY",
             "A nail into each surface class, from the shooter's right, at five moments from "
             "20 ms to 1 s: stone, metal, wood and tile on the wall, dirt, grass, sand and water "
             "on the ground.",
             "stone: a grey puff and flakes; metal: sparks glancing off to the left and hardly "
             "any dust; wood: a tan puff and pale splinters; tile: white shards; dirt and grass: "
             "a dark plume kicked straight up and clods falling back; sand: a pale fan of fines; "
             "water: a column of bright beads and a white mist",
             false);
  by_surface(-0.06, {0.03, 0.15}, "nail-dusk", "ONE NAIL INTO EACH SURFACE, AT DUSK",
             "As above at dusk, at two moments.",
             "the sparks off metal (and the few off stone and tile) glowing against the dusk",
             false);
  by_surface(0.6, {0.05, 0.3, 1.0}, "shotgun", "A SHOTGUN'S SPREAD INTO EACH SURFACE",
             "Nine pellets in a 22 cm spread into each surface by day, at three moments.",
             "nine small bursts, each smaller than a nail's, the metal's sparks fewer (soft "
             "lead); the dust running together into one cloud",
             true);
  // A nailgun's stream: ten a second for a second, walking across the wall.
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(4);
    names.reserve(4);
    for (const Target& tg : {kTargets[0], kTargets[1]}) {
      std::vector<gr::Impact> hits;
      hits.reserve(10);
      for (int i = 0; i < 10; ++i) {
        hits.push_back(aim(tg, -0.45 + 0.1 * i, 0.05 * ((i * 7) % 3 - 1), nail,
                           100 + static_cast<uint32_t>(i), 0.001 + 0.1 * i));
      }
      for (const double sun : {0.6, -0.06}) {
        pics.push_back(run_hits(*device, s, hits, eye_on(tg, sun), {0.95}, settle)[0]);
        names.push_back(std::string(tg.name) + (sun > 0 ? " DAY" : " DUSK"));
      }
    }
    lab.shot({"stream",
              "A NAILGUN'S STREAM",
              "Ten nails a second for a second walked across the wall, stone and metal, by day "
              "and at dusk, seen as the last one lands.",
              "a trail of puffs fading from the oldest; on metal a fresh spray of sparks at each "
              "new hit",
              {}},
             es::sheet_of(pics, names, 2), Det::measured);
  }

  // THE COST: the pass with nothing, and with a shotgun's spread of dust at its thickest.
  {
    const auto median_ms = [&](const std::vector<gr::Impact>& hits, double t) {
      gr::FrameView v = eye_on(kTargets[0], 0.6);
      v.time_s = t;
      const std::vector<gr::Blast> dust = dust_at(hits, t);
      std::vector<double> ts;
      ts.reserve(7);
      for (int k = 0; k < 3; ++k) es::draw(*device, s, v, dust, 0);
      for (int k = 0; k < 7; ++k) ts.push_back(es::draw(*device, s, v, dust, 0).ms);
      std::sort(ts.begin(), ts.end());
      return ts[3];
    };
    gr::clear_particles(*device, s.particles);
    lab.metric("pass.none", median_ms({}, 0.1), "ms", Det::measured);
    lab.metric("pass.spread", median_ms(spread(kTargets[0], 9, 0.22, 3, 0), 0.1), "ms",
               Det::measured);
    lab.metric("frame.gpu", es::draw(*device, s, eye_on(kTargets[0], 0.6), {}, 0).frame_ms, "ms",
               Det::measured);
  }

  const uint32_t errors = device->errors.load();
  const uint32_t late = device->pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (late != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_explosions' manifest. Skill: gpu, § Pipelines.");
  }
}

const gl::LabSpec kSpec{
    "gpu-impact",
    gl::LabKind::picture,
    "engine",
    "engine/render/impact.hpp",
    "render",
    "Does a nail or a pellet hitting each kind of surface read as that surface (stone, metal, "
    "wood, tile, dirt, grass, sand, water), and how little does it cost?",
    {
        {"width", "320", "each view's width, px"},
        {"height", "180", "each view's height, px"},
        {"settle", "4", "frames the march's jitter is averaged over for a still"},
    },
    "--width=96 --height=54 --settle=1",
    run,
};

GE_LAB(kSpec);

}  // namespace
