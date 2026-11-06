// LAB `gpu-particle-effects`: PARTICLE EFFECTS WRITTEN AS TEXT (engine/particles/effect.hpp)
// thrown into the render pool (render/spark.hpp) on the explosion labs' yard at dusk, on the
// software adapter. A sample book of four effects, each a few lines: a fountain of bright drops
// that bounce off the yard, embers that rise and cool, a puff of smoke that spreads, and a trail
// laid along a flight across the view. Each is pictured at three moments. A table shows the
// faults a bad file is refused with (file, line, the fix). Measured: the specks each effect
// throws, the CPU's throw, the particles' GPU time at the busiest moment. It fails on a GPU
// error or a pipeline made after the load. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: particle effects written as text, thrown on the yard, pictured; faults; cost
// ge:keywords lab gpu particles particle-effects effect-text data-driven-particles fountain embers
// ge:keywords smoke trail fault book throw_effect throw_trail picture software-adapter
// ge:skill render labs
// ge:test tests/particles_effect_test.cpp
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "particles/effect.hpp"
#include "render/particles.hpp"
#include "render/spark.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace pt = ge::particles;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

// The sample book, in metres: what a game or a mod would ship as a file.
constexpr const char* kSample = R"(# The lab's sample effects, lengths in metres.
effect fountain           # bright drops thrown up, bouncing off the yard
  burst 300
    ball 0.1
    along 3 6
    speed 0 1.2
    life 1.5 2.5
    size 0.04 0.06
    colour 0.4 0.7 1 0.9 0.95 1
    shine 2
    bounce 0.4

effect embers             # sparks that rise and cool from orange to grey
  burst 200
    box 0.2
    speed 0.5 2.5
    rise 0.5 1.5
    life 1 2
    size 0.03 0.05
    colour 1 0.45 0.1 1 0.8 0.3
    shine 3
    cool 0.6 0.1
    fall -0.5

effect puff               # a puff of smoke that spreads as it rises
  burst 80
    ball 0.3
    rise 0.3 0.8
    speed 0 0.3
    life 2 3
    size 0.3 0.6
    smoke 0.5 0.35
    grow 0.5
    fall 0
    drag 1.5

effect trail              # a flight's trail, a speck every 5 cm
  trail 0.05
    box 0.02
    speed 0 0.3
    life 0.6 1
    size 0.05 0.08
    colour 1 0.6 0.2 0.8 0.8 0.8
    shine 2
    cool 0.3 0.3
    fall -1
)";

// Bad lines and the fault each is refused with.
constexpr const char* kFaults[] = {
    "effect spray\n  burst 10\n  speed 4\n",
    "effect spray\n  burst 10\n  spin 3 4\n",
    "effect Spray\n  burst 10\n",
    "effect spray\n  life 1 2\n",
    "effect spray\n  burst 10\n  palette 250 300\n",
};

constexpr double kAt[3] = {es::kMid, 0.05, 134};  // where a burst is thrown
constexpr double kFlightS = 0.5;                  // the trail's flight across the view
constexpr double kFrom[3] = {es::kMid - 3, 1.0, 134}, kTo[3] = {es::kMid + 3, 2.0, 134};

// The effect `e` thrown at `kAt` at 0 s (a trail flown from `kFrom` to `kTo` over kFlightS), the
// pool flown at 120 Hz, a picture at each moment.
std::vector<gl::Picture> run_effect(ge::gpu::Device& d, es::Scene& s, const pt::Effect& e,
                                    const gr::FrameView& v0, const std::vector<double>& moments,
                                    int settle, size_t& thrown) {
  gr::clear_particles(d, s.particles);
  std::vector<pt::Spark> sparks;
  pt::Throw th;
  for (int k = 0; k < 3; ++k) th.at[k] = kAt[k];
  th.floor_y = 0;
  th.seed = 11;
  pt::throw_effect(e, th, sparks);
  gr::add_sparks(sparks, s.particles.births);
  thrown = sparks.size();
  std::vector<gl::Picture> out;
  double t = 0;
  for (const double m : moments) {
    while (t + 1e-9 < m) {
      const double dt = std::min(1.0 / 120, m - t);
      // The trail's stretch flown this step.
      if (t < kFlightS) {
        const double a = t / kFlightS, b = std::min(1.0, (t + dt) / kFlightS);
        double p0[3], p1[3];
        for (int k = 0; k < 3; ++k) {
          p0[k] = kFrom[k] + (kTo[k] - kFrom[k]) * a;
          p1[k] = kFrom[k] + (kTo[k] - kFrom[k]) * b;
        }
        sparks.clear();
        th.seed++;
        pt::throw_trail(e, p0, p1, th, sparks);
        gr::add_sparks(sparks, s.particles.births);
        thrown += sparks.size();
      }
      t += dt;
      gr::FrameView v = v0;
      v.time_s = t;
      es::draw(d, s, v, {}, dt);
    }
    gr::FrameView v = v0;
    v.time_s = m;
    out.push_back(es::settled(d, s, v, {}, settle));
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
  pt::Book book;
  std::string error;
  if (!pt::parse_effects(kSample, "lab/sample.fx", book, error)) {
    lab.fail("the lab's own sample did not read: " + error, "Fix kSample. Skill: render.");
    return;
  }
  // The faults a bad file is refused with.
  {
    gl::LabTable t{"A BAD FILE, REFUSED", {"text", "fault"}, {}};
    for (const char* f : kFaults) {
      pt::Book scratch;
      std::string why;
      if (pt::parse_effects(f, "mod/particles/spray.fx", scratch, why)) {
        lab.fail(std::string("a bad text was read: ") + f,
                 "next: refuse it in parse_effects (engine/particles/effect.cpp). Skill: render.");
        continue;
      }
      std::string text = f;
      std::replace(text.begin(), text.end(), '\n', '/');
      t.rows.push_back({text, why});
    }
    lab.table(t);
  }
  // The CPU's throw: every burst of the book, many times over.
  {
    std::vector<pt::Spark> sparks;
    sparks.reserve(1024);
    size_t total = 0;
    constexpr int kRounds = 200;
    const auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < kRounds; ++k)
      for (const pt::Effect& e : book.effects) {
        sparks.clear();
        pt::Throw th;
        th.seed = static_cast<uint32_t>(k + 1);
        pt::throw_effect(e, th, sparks);
        total += sparks.size();
      }
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    lab.metric("cpu.throw-per-speck", us / static_cast<double>(std::max<size_t>(total, 1)) * 1000,
               "ns", Det::measured);
  }
  const std::vector<std::string> ids = {"fountain", "embers", "puff", "trail"};
  auto device = ge::gpu::create_device({.label = "gpu-particle-effects", .validation = 1});
  if (!device) {
    if (gpu_required())
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    placeholders(lab, {"effects"}, w, h);
    return;
  }
  es::Scene s;
  if (!es::make_scene(*device, s, w, h, 10)) {
    lab.fail("the scene's pipelines did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const gr::FrameView eye = es::eye_at(es::kMid, 1.4, 129.5, 0.08, -0.06, 0, 0);
  const std::vector<double> moments = {0.15, 0.6, 1.5};
  std::vector<gl::Picture> pics;
  std::vector<std::string> names;
  for (const std::string& id : ids) {
    const pt::Effect* e = book.find(id);
    size_t thrown = 0;
    for (gl::Picture& p : run_effect(*device, s, *e, eye, moments, settle, thrown))
      pics.push_back(std::move(p));
    for (const double m : moments) names.push_back(id + " " + fixed(m, 2) + " S");
    lab.metric("thrown." + id, static_cast<double>(thrown), "count");
  }
  lab.shot({"effects",
            "FOUR EFFECTS WRITTEN AS TEXT",
            "The lab's sample book (a few lines each, in metres) thrown on the yard at dusk: a "
            "fountain, embers, a puff of smoke, a trail along a flight across the view; each at "
            "0.15, 0.6 and 1.5 s.",
            "the fountain's blue-white drops rising, falling and hopping on the dirt; the embers "
            "rising orange and greying; the smoke spreading as it rises; the trail drawn left to "
            "right and fading from its start",
            {}},
           es::sheet_of(pics, names, static_cast<int>(moments.size())), Det::measured);
  // The particles' GPU time at the fountain's busiest.
  {
    gr::clear_particles(*device, s.particles);
    std::vector<pt::Spark> sparks;
    pt::Throw th;
    for (int k = 0; k < 3; ++k) th.at[k] = kAt[k];
    pt::throw_effect(*book.find("fountain"), th, sparks);
    gr::add_sparks(sparks, s.particles.births);
    es::draw(*device, s, eye, {}, 1.0 / 120);
    lab.metric("frame.gpu", es::draw(*device, s, eye, {}, 1.0 / 120).frame_ms, "ms", Det::measured);
  }
  const uint32_t errors = device->errors.load();
  const uint32_t late = device->pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0)
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  if (late != 0)
    lab.fail("a pipeline was made after the load",
             "Add it to its pass's manifest. Skill: gpu, § Pipelines.");
}

const gl::LabSpec kSpec{
    "gpu-particle-effects",
    gl::LabKind::picture,
    "engine",
    "engine/particles/effect.hpp",
    "render",
    "Does an effect written as a few lines of text throw what it says (where, how fast, how long, "
    "what colour, smoke or shine), and is a bad file refused with its line and the fix?",
    {
        {"width", "320", "each view's width, px"},
        {"height", "180", "each view's height, px"},
        {"settle", "2", "frames the jitter is averaged over for a still"},
    },
    "--width=96 --height=54 --settle=1",
    run,
};

GE_LAB(kSpec);

}  // namespace
