// LAB `se-blast-kinds`: THE ENHANCED BLASTS' KINDS AND THE TINTED HITS, on the software adapter in
// the explosion labs' yard (a dirt floor, a wall 40 m ahead). Every kind of app/weapon_fx.hpp's
// `blast` (common, big, huge, dark, bare: the kinds Quake II's mission packs' explosions are drawn
// as under classic enhance, client/q2_enhance_fx.hpp) goes off on the floor 16 m ahead and is
// pictured at three moments of its life, its embers and sparks flown in the particles between
// them; then each pack hit's sparks (`q2_pack_hit_of`: the green bolt, the heat beam's sparks and
// steam, the electric sparks, a flechette) thrown off the floor 3 m ahead at dusk.
//
// Counted (deterministic): what each kind throws and its fireball's scale. It fails on a GPU error
// or a pipeline made after the load. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: the enhanced blast kinds (big, huge, dark, bare) and the packs' tinted hits
// ge:keywords lab gpu se-blast-kinds blast-kind blastof big-blast nuke tracker dark-blast
// ge:keywords tinted-hit q2_pack_hit_of mission-pack classic-enhance ce_explosions ce_hits
// ge:skill render shooter labs
// ge:test tests/shooters_edge_weapon_fx_test.cpp
#include <algorithm>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "render/explosions.hpp"
#include "render/particles.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/q2_enhance_fx.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace fx = shooters_edge::effects;
namespace cl = shooters_edge::client;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

struct Kind {
  fx::BlastOf of;
  const char* name;
};
constexpr Kind kKinds[] = {{fx::BlastOf::common, "common"},
                           {fx::BlastOf::big, "big"},
                           {fx::BlastOf::huge, "huge"},
                           {fx::BlastOf::dark, "dark"},
                           {fx::BlastOf::bare, "bare"}};
constexpr int kHitCodes[] = {30, 43, 44, 46, 55};
constexpr const char* kHitNames[] = {"green bolt", "heat sparks", "heat steam", "electric",
                                     "flechette"};
constexpr double kDt = 1.0 / 30;

// The particles flown from 0 to `m` in kDt steps after `births` went in, then the still at `m`.
gl::Picture at_moment(ge::gpu::Device& d, es::Scene& s, const std::vector<gr::Blast>& blasts,
                      const fx::Particles& births, gr::FrameView v, double m, int settle) {
  gr::clear_particles(d, s.particles);
  gr::emit_particles(s.particles, births);
  for (double t = 0; t + kDt < m; t += kDt) {
    v.time_s = t;
    es::draw(d, s, v, blasts, kDt);
  }
  v.time_s = m;
  es::draw(d, s, v, blasts, m - static_cast<int>(m / kDt) * kDt);
  return es::settled(d, s, v, blasts, settle);
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const int settle = std::clamp(static_cast<int>(lab.arg_num("settle")), 1, 16);
  if (w < 64 || w > 2048) {
    lab.fail("--width must be 64..2048", "next: pass --width=480.");
    return;
  }
  const double floor_at[3] = {es::kMid, 0, 116}, up[3] = {0, 1, 0};
  for (const Kind& k : kKinds) {
    std::vector<gr::Blast> blasts;
    fx::Particles births;
    fx::blast(floor_at, up, 7, 0.0, &blasts, &births, k.of);
    lab.metric(std::string("thrown.") + k.name, static_cast<double>(births.size()), "count");
    lab.metric(std::string("scale.") + k.name, blasts[0].scale, "x");
  }
  auto device = ge::gpu::create_device({.label = "se-blast-kinds", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"kinds", "hits"}, w, h);
    return;
  }
  es::Scene s;
  if (!es::make_scene(*device, s, w, h, 10)) {
    lab.fail("the explosions' pipelines did not build",
             "Read the gpu: lines above. Skill: render.");
    return;
  }
  {
    const double moments[] = {0.08, 0.3, 0.9};
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const Kind& k : kKinds) {
      std::vector<gr::Blast> blasts;
      fx::Particles births;
      fx::blast(floor_at, up, 7, 0.0, &blasts, &births, k.of);
      for (const double m : moments) {
        pics.push_back(at_moment(*device, s, blasts, births,
                                 es::eye_at(es::kMid, 2.2, 100, 0.12, -0.05, 0, 0), m, settle));
        names.push_back(std::string(k.name) + " " + fixed(m, 2) + " S");
      }
    }
    lab.shot(
        {"kinds",
         "THE BLAST'S KINDS",
         "Each kind of the enhanced blast on the floor 16 m ahead at dusk, at three moments: "
         "common (a rocket's), big (explosion1's big one, 1.8 times), huge (the nuke's, 3.2 "
         "times, white-hot), dark (the tracker's) and bare (the ball alone).",
         "common: the orange ball and its embers; big: the same, larger, more embers; huge: "
         "a white ball filling the view and lighting the yard and wall, the dusk sky gone dark "
         "while it burns (the eye adapted to a flash many times the common one's, as a "
         "camera's exposure would), pale dust after; "
         "dark: a black sooty cloud with black specks rushing in, no glow, gone by 0.9 s; "
         "bare: the ball with no embers or sparks",
         {}},
        es::sheet_of(pics, names, 3), Det::measured);
  }
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    const double hit_at[3] = {es::kMid, 0, 103};
    for (size_t i = 0; i < std::size(kHitCodes); ++i) {
      cl::Q2PackHit hit;
      if (!cl::q2_pack_hit_of(kHitCodes[i], &hit)) continue;
      fx::Particles births;
      for (int n = 0; n < 3; ++n) fx::tinted_hit(hit_at, up, hit.hit, 40 + n, &births);
      pics.push_back(at_moment(*device, s, {}, births,
                               es::eye_at(es::kMid, 1.2, 100, -0.35, -0.05, 0, 0), 0.1, settle));
      names.push_back(std::to_string(kHitCodes[i]) + " " + kHitNames[i]);
    }
    lab.shot({"hits",
              "THE PACKS' HITS IN THEIR COLOURS",
              "Three of each mission-pack hit thrown off the floor 3 m ahead at dusk, 0.1 s on.",
              "green specks; hot orange sparks; pale grey steam rising; fast blue-white sparks; "
              "a few steel-blue glints",
              {}},
             es::sheet_of(pics, names, 5), Det::measured);
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
    "se-blast-kinds",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/app/weapon_fx (blast kinds, tinted hits) over the explosions pass",
    "render",
    "Does each kind of enhanced blast read as its own (larger, white-hot, dark, bare) beside the "
    "common one, and does each mission-pack hit shine in its own colour?",
    {{"width", "480", "a panel's width, px (its height is 9/16 of it)"},
     {"settle", "3", "frames averaged a still (the march's jitter)"}},
    "--width=128 --settle=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
