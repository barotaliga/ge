// LAB `camera-world`: what each rung of Fall Line's camera ladder (games/fall_line/camera) SHOWS
// over a run. The bot skis seed --seed's map and six cameras follow him at 60 frames a second,
// one on each rung (tips, helmet, chase, far, high, orbit), every one with the whole ladder kept
// warm as the game keeps it; two more make the camera key's hand-overs at --switch s (chase to
// helmet, blended in his frame; high to chase, inertialized).
//
// Every frame of every camera is checked against the mountain: how far the eye is over the snow
// under it, and how near it comes to the bark of any trunk. A lens inside a trunk or under the
// snow fails.
//
// THE MOMENTS, picked off the run itself: the START (a second in), a CARVE (the hardest
// sideways load on the snow), a JUMP (the most air under him), its LANDING (the frame he is
// back on the snow), and the TREES (his closest pass to a trunk). The piste keeps the bot well
// clear of the woods, so THE WOODS are a run of their own: a skier let run straight down the fall
// line through the densest wood, every rung following him, shot half way to the trunk he meets
// and at his closest pass before it. Each is a sheet of the six
// rungs' pictures, the game's own frame drawn headless with the tracks he laid up to it; each
// hand-over is a strip of six frames through it. Without an adapter the pictures are
// placeholders and every number above is still measured, so the digest is the same on every
// row. Built only with GE_GPU.
//
// SEEING HIM THROUGH THE WOODS: every picture is drawn with the trees between the lens and him
// seen through (render/instances.hpp § Seeing through; --see-through=0 draws them whole, the
// before). Each woods moment counts the trees each lens sees through (the CPU twin), and each
// sheet measures how much of him each outside lens SEES: points down his middle projected into
// the picture, each seen when the nearest surface there is no more than half a metre before it.
//
// ge:summary Lab: every camera rung at a run's moments, and no lens inside a trunk or the snow
// ge:keywords lab camera camera-world ladder rung tips helmet chase far high orbit hand-over
// ge:keywords moments carve jump landing trees clearance trunk snow fall-line contact-sheet
// ge:skill camera labs
// ge:test tests/fall_line_camera_test.cpp
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "camera/to_view.hpp"
#include "core/libm/libm.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/scene.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/camera/rungs.hpp"
#include "fall_line/camera/subject.hpp"
#include "fall_line/camera/world.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/hiz.hpp"
#include "render/instances.hpp"
#include "skier/skier.hpp"
#include "skier/trees.hpp"

namespace {

namespace gl = ge::lab;
namespace cam = ge::camera;
namespace fc = fall_line::camera;
namespace libm = ge::core::libm;
using gl::Det;

constexpr double kHz = 60;
constexpr int kEdits = 2;
constexpr int kStrip = 6;
constexpr double kStripEvery = 0.15;  // s between a strip's frames
constexpr double kSnowFloor = 0.05;   // m: a lens nearer the snow than a near plane is in it

struct Edit {
  const char* id;
  fc::Rung from, to;
};
constexpr Edit kEditRows[kEdits] = {{"chase-to-helmet", fc::Rung::chase, fc::Rung::helmet},
                                    {"high-to-chase", fc::Rung::high, fc::Rung::chase}};

// One display frame of the run: the skier and every camera's pose.
struct Frame {
  double t = 0;
  ge::skier::SkierState skier;
  std::array<cam::Pose, fc::kRungs> rung{};
  std::array<cam::Pose, kEdits> edit{};
};

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && v[0] == '1';
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v && v[0] == '1';
#endif
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// How near `p` is to the bark of a trunk it is beside (negative: inside), m; a large number when
// no trunk stands at its height near it.
double bark_gap(const ge::skier::World& w, const cam::Vec3& p) {
  double gap = 1e9;
  ge::skier::for_each_tree_near(*w.trees, p.x, p.z, [&](int32_t i) {
    const auto& t = w.level->trees[static_cast<size_t>(i)];
    if (p.y < t.y || p.y > t.y + t.height) return;
    const double d = std::sqrt((p.x - t.x) * (p.x - t.x) + (p.z - t.z) * (p.z - t.z)) - t.radius;
    gap = d < gap ? d : gap;
  });
  return gap;
}

// The worst a lens did over the run.
struct Worst {
  double gap = 1e9;   // the least bark gap
  double over = 1e9;  // the least height over the snow
};

void judge(Worst& w, const ge::skier::World& sw, const cam::Pose& p) {
  const double gap = bark_gap(sw, p.eye);
  const double over = p.eye.y - sw.level->ground_at(p.eye.x, p.eye.z);
  w.gap = gap < w.gap ? gap : w.gap;
  w.over = over < w.over ? over : w.over;
}

// The tree with the most others within 15 m of it (every fifth tree asked): the densest wood.
int32_t densest(const ge::mountain::mapgen::Level& level) {
  int32_t best = -1;
  int most = 0;
  for (size_t i = 0; i < level.trees.size(); i += 5) {
    const auto& t = level.trees[i];
    int near = 0;
    for (const auto& u : level.trees) {
      if (std::fabs(u.x - t.x) < 15 && std::fabs(u.z - t.z) < 15) ++near;
    }
    if (near > most) {
      most = near;
      best = static_cast<int32_t>(i);
    }
  }
  return best;
}

// The trees lens `p` sees through to him in frame `f` (render/instances.hpp's CPU twin).
int seen_through(const std::vector<ge::render::Instance>& trees, const Frame& f,
                 const cam::Pose& p) {
  ge::render::SeeThrough see;
  see.on = true;
  const cam::Vec3 at = fc::framed_point(f.skier);
  see.subject[0] = at.x;
  see.subject[1] = at.y;
  see.subject[2] = at.z;
  const double eye[3] = {p.eye.x, p.eye.y, p.eye.z};
  int n = 0;
  for (const auto& t : trees) {
    if (std::fabs(t.position[0] - eye[0]) > 60 || std::fabs(t.position[2] - eye[2]) > 60) continue;
    if (ge::render::see_through_keep(see, eye, t) < 1) ++n;
  }
  return n;
}

// The worn lenses look out from him: how much of him they see is not asked.
bool outside(int rung) {
  return rung != static_cast<int>(fc::Rung::tips) && rung != static_cast<int>(fc::Rung::helmet);
}

// A moment to shoot: its name, what it is, and the frame.
struct Moment {
  std::string id, what;
  size_t frame;
};

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double hour = lab.arg_num("hour");
  const double seconds = lab.arg_num("seconds");
  const double switch_at = lab.arg_num("switch");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int settle = static_cast<int>(lab.arg_num("settle"));
  const bool see_through = lab.arg_num("see-through") != 0;
  if (width < 32 || height < 18 || width > 1920 || height > 1080 || settle < 1) {
    lab.fail("--width and --height must be 32..1920 and 18..1080, --settle at least 1",
             "next: pass a size such as --width=320 --height=180.");
    return;
  }

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const auto shared = std::make_shared<const ge::mountain::mapgen::Level>(
      ge::mountain::mapgen::generate_level(seed, opts));
  const ge::mountain::mapgen::Level& level = *shared;

  // THE RUN: every camera framed every display frame, two steps of the run apart.
  fall_line::sim::Driver drive(shared, 0);
  const ge::skier::World sw = drive.state().world();
  const cam::World world = fc::camera_world(sw);
  std::array<fc::RunCamera, fc::kRungs> cams;
  for (int i = 0; i < fc::kRungs; ++i) {
    cams[static_cast<size_t>(i)] = fc::make_skier_camera(static_cast<fc::Rung>(i), 7);
  }
  std::array<fc::RunCamera, kEdits> edits;
  for (int e = 0; e < kEdits; ++e) {
    edits[static_cast<size_t>(e)] = fc::make_skier_camera(kEditRows[e].from, 7);
  }
  std::array<Worst, fc::kRungs> worst{};
  std::array<Worst, kEdits> edit_worst{};
  std::vector<Frame> frames;
  long switch_frame = -1;
  while (!drive.over() && (seconds <= 0 || drive.state().t < seconds)) {
    for (int k = 0; k < 2 && !drive.over(); ++k) {
      drive.step(fall_line::sim::bot_controls(drive.state(), 0, drive.plan()));
    }
    Frame f;
    f.t = drive.state().t;
    f.skier = drive.state().racers[0].skier;
    for (size_t i = 0; i < cams.size(); ++i) {
      f.rung[i] = fc::frame_camera(cams[i], f.skier, sw, world, 1 / kHz);
      judge(worst[i], sw, f.rung[i]);
    }
    if (switch_frame < 0 && f.t >= switch_at) {
      switch_frame = static_cast<long>(frames.size());
      for (int e = 0; e < kEdits; ++e) {
        fc::change_rung(edits[static_cast<size_t>(e)].cam, kEditRows[e].to);
      }
    }
    for (size_t e = 0; e < edits.size(); ++e) {
      f.edit[e] = fc::frame_camera(edits[e], f.skier, sw, world, 1 / kHz);
      judge(edit_worst[e], sw, f.edit[e]);
    }
    frames.push_back(f);
  }
  lab.metric("frames", static_cast<double>(frames.size()), "count");
  if (frames.empty()) {
    lab.fail("the run took no frames", "next: check --seconds and the map's start.");
    return;
  }

  // THE WOODS: the piste keeps the bot clear of the trees, so a skier is let run straight down
  // the fall line through the densest wood, from --woods-run m above its heart, for --woods s,
  // every rung following him: the push-off off the trunks at work.
  std::vector<Frame> woods;
  double woods_gap = 1e9;
  size_t woods_frame = 0, woods_mid = 0;
  if (const int32_t heart = densest(level); heart >= 0 && lab.arg_num("woods") > 0) {
    const auto& t = level.trees[static_cast<size_t>(heart)];
    ge::mountain::mapgen::Vec3 n{};
    level.normal_at(t.x, t.z, n);
    const double h = libm::atan2(n.x, n.z);  // the fall line: down the ground's tilt
    const double back = lab.arg_num("woods-run");
    ge::skier::SkierState s =
        ge::skier::stand_skier(sw, t.x - back * libm::sin(h), t.z - back * libm::cos(h), h, 6);
    std::array<fc::RunCamera, fc::kRungs> wc;
    for (int i = 0; i < fc::kRungs; ++i) {
      wc[static_cast<size_t>(i)] = fc::make_skier_camera(static_cast<fc::Rung>(i), 7);
    }
    const ge::skier::SkierInput in{0, 0.3, 0, false};
    const ge::skier::SkiSpec& spec = drive.state().spec;
    for (int k = 0; k < static_cast<int>(lab.arg_num("woods") * kHz); ++k) {
      for (int j = 0; j < 2; ++j) {
        ge::skier::SkierEvents ev;
        ge::skier::step_skier(s, in, sw, spec, 0.5 / kHz, ev);
      }
      Frame f;
      f.t = k / kHz;
      f.skier = s;
      for (size_t i = 0; i < wc.size(); ++i) {
        f.rung[i] = fc::frame_camera(wc[i], s, sw, world, 1 / kHz);
        judge(worst[i], sw, f.rung[i]);
      }
      // His closest pass on his feet (a trunk he meets puts him down).
      const double g = s.down ? 1e9 : bark_gap(sw, cam::Vec3{s.x, s.y + 1, s.z});
      if (g < woods_gap) {
        woods_gap = g;
        woods_frame = woods.size();
      }
      woods.push_back(f);
    }
    lab.metric("woods.frames", static_cast<double>(woods.size()), "count");
    lab.metric("woods.closest-trunk", woods_gap > 1e8 ? -1 : woods_gap, "m");
    size_t fell = 0;
    while (fell < woods.size() && !woods[fell].skier.down) ++fell;
    lab.metric("woods.down-at", fell < woods.size() ? woods[fell].t : -1, "s");
    woods_mid = fell / 2;
    const std::vector<ge::render::Instance> trees = fall_line::app::tree_instances(level);
    for (int i = 0; i < fc::kRungs; ++i) {
      const std::string id(fc::rung_name(static_cast<fc::Rung>(i)));
      const auto r = static_cast<size_t>(i);
      lab.metric("woods." + id + ".seen-through",
                 seen_through(trees, woods[woods_mid], woods[woods_mid].rung[r]), "count");
      lab.metric("woods-trunk." + id + ".seen-through",
                 seen_through(trees, woods[woods_frame], woods[woods_frame].rung[r]), "count");
    }
  }

  // Every lens's worst over the run.
  std::vector<std::vector<std::string>> rows;
  auto report = [&](const std::string& id, const Worst& w) {
    lab.metric(id + ".least-over-snow", w.over, "m");
    lab.metric(id + ".least-bark-gap", w.gap > 1e8 ? -1 : w.gap, "m");
    rows.push_back({id, fixed(w.over, 2), w.gap > 1e8 ? "none near" : fixed(w.gap, 2)});
    if (w.gap < 0) {
      lab.fail(id + ": a lens stood " + fixed(-w.gap, 2) + " m inside a trunk",
               "Read the push-off in engine/camera/boom.cpp and the trunks games/fall_line/camera/"
               "world.cpp answers (skill: camera).");
    }
    if (w.over < kSnowFloor) {
      lab.fail(id + ": a lens came " + fixed(w.over, 2) + " m over the snow (under it or in it)",
               "Read the clearance in engine/camera/boom.cpp, or the worn eyes in games/fall_line/"
               "camera/subject.cpp (skill: camera).");
    }
  };
  for (int i = 0; i < fc::kRungs; ++i) {
    report(std::string(fc::rung_name(static_cast<fc::Rung>(i))), worst[static_cast<size_t>(i)]);
  }
  for (int e = 0; e < kEdits; ++e) report(kEditRows[e].id, edit_worst[static_cast<size_t>(e)]);
  lab.table({"every lens's worst frame over the run",
             {"lens", "least-over-snow-m", "least-bark-gap-m"},
             rows});

  // THE MOMENTS.
  std::vector<Moment> moments;
  auto at_time = [&](double t) {
    size_t i = 0;
    while (i + 1 < frames.size() && frames[i].t < t) ++i;
    return i;
  };
  moments.push_back({"start", "a second after the start", at_time(1.0)});
  {
    size_t best = 0;
    double most = -1;
    for (size_t i = 0; i < frames.size(); ++i) {
      const auto& s = frames[i].skier;
      if (s.on_snow && !s.down && s.speed > 8 && std::fabs(s.g_lat) > most) {
        most = std::fabs(s.g_lat);
        best = i;
      }
    }
    if (most >= 0) {
      moments.push_back({"carve", "the hardest carve, " + fixed(most, 2) + " g sideways", best});
      lab.metric("carve-g", most, "g");
    }
  }
  {
    size_t best = 0;
    double most = 0.5;
    for (size_t i = 0; i < frames.size(); ++i) {
      const auto& s = frames[i].skier;
      if (s.on_snow) continue;
      const double over = s.y - level.ground_at(s.x, s.z);
      if (over > most) {
        most = over;
        best = i;
      }
    }
    if (most > 0.5) {
      moments.push_back({"jump", "the most air under him, " + fixed(most, 1) + " m", best});
      lab.metric("jump-air", most, "m");
      size_t land = best;
      while (land + 1 < frames.size() && !frames[land].skier.on_snow) ++land;
      if (frames[land].skier.on_snow) {
        moments.push_back({"landing", "back on the snow after that flight", land});
      }
    } else {
      lab.note("No flight over half a metre in this run: no jump and no landing sheet.");
    }
  }
  {
    size_t best = 0;
    double least = 1e9;
    for (size_t i = 0; i < frames.size(); ++i) {
      const auto& s = frames[i].skier;
      if (s.speed < 3 || s.down) continue;
      const double g = bark_gap(sw, cam::Vec3{s.x, s.y + 1, s.z});
      if (g < least) {
        least = g;
        best = i;
      }
    }
    if (least < 1e8) {
      moments.push_back({"trees", "his closest pass to a trunk, " + fixed(least, 1) + " m", best});
      lab.metric("trees-gap", least, "m");
    }
  }
  for (const Moment& m : moments) lab.metric(m.id + ".t", frames[m.frame].t, "s");
  std::stable_sort(moments.begin(), moments.end(),
                   [](const Moment& a, const Moment& b) { return a.frame < b.frame; });

  // The strips' frames through each hand-over.
  std::array<size_t, kStrip> strip{};
  const bool have_strip = switch_frame >= 0;
  if (have_strip) {
    for (int k = 0; k < kStrip; ++k) {
      const auto at = static_cast<size_t>(switch_frame) +
                      static_cast<size_t>(std::lround((k - 1) * kStripEvery * kHz));
      strip[static_cast<size_t>(k)] = std::min(at, frames.size() - 1);
    }
    lab.metric("switch.t", frames[static_cast<size_t>(switch_frame)].t, "s");
  } else {
    lab.note("The run ended before --switch: no hand-over strips.");
  }

  // THE PICTURES.
  const int label_h = 14;
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const Moment woods_moment{"woods",
                            "a skier let run straight through the densest wood, half way to where "
                            "he meets a trunk",
                            woods_mid};
  const Moment trunk_moment{"woods-trunk",
                            "the same run at his closest pass to a trunk on his feet (" +
                                fixed(woods_gap, 1) + " m), the frame before he meets it",
                            woods_frame};
  auto sheet_shot = [&](const Moment& m, const Frame& f) {
    std::string upper = m.id;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return gl::Shot{"moment-" + m.id,
                    "SEED " + seed_s + ": EVERY RUNG AT THE " + upper,
                    (&m == &woods_moment || &m == &trunk_moment ? std::string("The woods run ")
                                                                : std::string("The bot's run ")) +
                        fixed(f.t, 1) + " s in, " + m.what +
                        ", seen from each rung of the ladder (tips, helmet, chase on the top row; "
                        "far, high, orbit under), each camera following him all run.",
                    "the skier in every outside lens, placed a little under the middle of the "
                    "frame; the tips and helmet looking down his way with the horizon tipped by "
                    "his lean; no picture of bark, the inside of a crown or the underside of the "
                    "snow",
                    {{"seed", seed_s}, {"moment", m.id}}};
  };
  auto strip_shot = [&](int e) {
    return gl::Shot{std::string("hand-over-") + kEditRows[e].id,
                    std::string("HAND-OVER: ") + kEditRows[e].id,
                    std::string("The camera key pressed at ") + fixed(switch_at, 1) +
                        " s: from the frame before it, every 0.15 s through the 0.6 s change.",
                    "a move, never a jump between two frames; the skier in every frame of a "
                    "change between two outside lenses",
                    {{"seed", seed_s}, {"edit", kEditRows[e].id}}};
  };

  auto device = ge::gpu::create_device({.label = "camera-world", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    gl::Picture none = gl::create_picture(width, height, {40, 40, 48});
    gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
    // In the order the device path shoots them (a shot's id is hashed): the run's moments with
    // the strips where their frames fall, then the woods.
    size_t shots = moments.size() + (have_strip ? kEdits : 0);
    bool stripped = !have_strip;
    for (const Moment& m : moments) {
      if (!stripped && strip[0] <= m.frame) {
        for (int e = 0; e < kEdits; ++e) lab.shot(strip_shot(e), none, Det::measured);
        stripped = true;
      }
      lab.shot(sheet_shot(m, frames[m.frame]), none, Det::measured);
    }
    if (!stripped) {
      for (int e = 0; e < kEdits; ++e) lab.shot(strip_shot(e), none, Det::measured);
    }
    if (!woods.empty()) {
      lab.shot(sheet_shot(woods_moment, woods[woods_mid]), none, Det::measured);
      lab.shot(sheet_shot(trunk_moment, woods[woods_frame]), none, Det::measured);
      shots += 2;
    }
    lab.metric("shots", static_cast<double>(shots), "count");
    lab.metric("gpu-errors", 0, "count", Det::measured);
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
      *device, "camera-world.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

  // The tracks he laid, up to frame `to`, every other frame (four steps).
  size_t laid = 0;
  auto lay_to = [&](size_t to) {
    for (; laid < to; laid += 2) {
      fall_line::app::track(*frame,
                            {std::span<const ge::skier::SkierState>(&frames[laid].skier, 1)});
    }
  };
  auto picture = [&](const Frame& f, const cam::Pose& pose) {
    ge::render::FrameView v;
    fall_line::app::day_view(level, hour, v);
    cam::to_frame_view(pose, v);
    v.time_s = f.t;
    fall_line::app::cut(*frame);
    fall_line::app::FrameScene scene{std::span<const ge::skier::SkierState>(&f.skier, 1)};
    if (see_through) {
      const cam::Vec3 at = fc::framed_point(f.skier);
      scene.subject = std::array<double, 3>{at.x, at.y, at.z};
    }
    for (int i = 0; i < settle; ++i) {
      v.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, v, scene, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    if (img.rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      return gl::create_picture(width, height, {255, 0, 255});
    }
    return gl::Picture{width, height, img.rgba};
  };
  // The share of the points down his middle the last picture shows (the nearest surface at each
  // no more than half a metre before it), from the eye at `pose`.
  auto seen_share = [&](const Frame& f, const cam::Pose& pose) {
    const std::vector<float> depth = ge::render::read_depth(*device, frame->targets);
    const auto rw = static_cast<int>(frame->targets.width),
               rh = static_cast<int>(frame->targets.height);
    if (depth.size() != static_cast<size_t>(rw) * static_cast<size_t>(rh)) return -1.0;
    const float* m = frame->staged.view_proj;
    auto depth_of = [&](double x, double y, double z, double& px, double& py) {
      const double cx = m[0] * x + m[4] * y + m[8] * z + m[12];
      const double cy = m[1] * x + m[5] * y + m[9] * z + m[13];
      const double cz = m[2] * x + m[6] * y + m[10] * z + m[14];
      const double cw = m[3] * x + m[7] * y + m[11] * z + m[15];
      if (cw <= 1e-6) return -1.0;
      px = (cx / cw * 0.5 + 0.5) * rw;
      py = (0.5 - cy / cw * 0.5) * rh;
      return cz / cw;
    };
    const cam::Vec3 mid = fc::framed_point(f.skier);
    const cam::Vec3 fwd = cam::look_forward(pose.yaw, pose.pitch);
    const double fl = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
    const double rx = fl > 1e-6 ? fwd.z / fl : 1, rz = fl > 1e-6 ? -fwd.x / fl : 0;
    int seen = 0, asked = 0;
    for (int up = -2; up <= 2; ++up) {
      for (int side = -1; side <= 1; ++side) {
        const double h = f.skier.down ? 0.12 * up : 0.3 * up;
        double x = mid.x + 0.2 * side * rx - pose.eye.x, y = mid.y + h - pose.eye.y,
               z = mid.z + 0.2 * side * rz - pose.eye.z;
        const double d = std::sqrt(x * x + y * y + z * z);
        if (d < 0.6) continue;
        double px = 0, py = 0;
        const double k = (d - 0.5) / d;  // half a metre toward the eye
        const double before = depth_of(x * k, y * k, z * k, px, py);
        if (depth_of(x, y, z, px, py) < 0 || before < 0) continue;
        if (px < 0 || py < 0 || px >= rw || py >= rh) continue;
        ++asked;
        const float at = depth[static_cast<size_t>(static_cast<int>(py)) * static_cast<size_t>(rw) +
                               static_cast<size_t>(static_cast<int>(px))];
        if (at <= before) ++seen;  // reversed-z: a larger depth is nearer
      }
    }
    return asked ? static_cast<double>(seen) / asked : -1.0;
  };
  auto labelled = [&](gl::Picture& sheet, const gl::Picture& p, int col, int row,
                      const std::string& label) {
    const int x = col * width, y = row * (height + label_h);
    gl::blit(sheet, p, x, y + label_h);
    gl::draw_text(sheet, x + 4, y + 3, label, {235, 235, 235});
  };

  // In the run's order, so the tracks are laid forward only.
  size_t next_strip = have_strip ? strip[0] : frames.size();
  bool strips_done = !have_strip;
  auto shoot_strips = [&]() {
    for (int e = 0; e < kEdits; ++e) {
      gl::Picture sheet = gl::create_picture(kStrip * width, height + label_h, {20, 22, 26});
      for (int k = 0; k < kStrip; ++k) {
        const size_t fi = strip[static_cast<size_t>(k)];
        lay_to(fi);
        const double since = frames[fi].t - frames[static_cast<size_t>(switch_frame)].t;
        labelled(sheet, picture(frames[fi], frames[fi].edit[static_cast<size_t>(e)]), k, 0,
                 (since < 0 ? std::string("BEFORE") : "+" + fixed(since, 2) + " S"));
      }
      lab.shot(strip_shot(e), sheet, Det::measured);
    }
    strips_done = true;
  };
  size_t shots = 0;
  auto shoot_sheet = [&](const Moment& m, const Frame& f) {
    gl::Picture sheet = gl::create_picture(3 * width, 2 * (height + label_h), {20, 22, 26});
    for (int i = 0; i < fc::kRungs; ++i) {
      std::string label(fc::rung_name(static_cast<fc::Rung>(i)));
      for (char& c : label) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      const cam::Pose& pose = f.rung[static_cast<size_t>(i)];
      labelled(sheet, picture(f, pose), i % 3, i / 3, label);
      if (outside(i)) {
        lab.metric(m.id + "." + std::string(fc::rung_name(static_cast<fc::Rung>(i))) + ".seen",
                   seen_share(f, pose), "share", Det::measured);
      }
    }
    lab.shot(sheet_shot(m, f), sheet, Det::measured);
    ++shots;
  };
  for (const Moment& m : moments) {
    if (!strips_done && next_strip <= m.frame) shoot_strips();
    lay_to(m.frame);
    shoot_sheet(m, frames[m.frame]);
  }
  if (!strips_done) shoot_strips();
  if (have_strip) shots += kEdits;
  if (!woods.empty()) {
    shoot_sheet(woods_moment, woods[woods_mid]);
    shoot_sheet(trunk_moment, woods[woods_frame]);
  }
  lab.metric("shots", static_cast<double>(shots), "count");
  lab.metric("gpu-errors", device->errors.load(), "count", Det::measured);
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "camera-world",
    gl::LabKind::picture,
    "game:fall_line",
    "games/fall_line/camera (the ladder, the subject, the mountain's answers) over engine/camera",
    "camera",
    "What does each rung of Fall Line's camera ladder show at the moments of a run, and does any "
    "lens stand inside a trunk or the snow?",
    {
        {"seed", "38", "the map's seed"},
        {"hour", "-1", "the solar hour to light the map at; negative: the map's own"},
        {"seconds", "0", "how much of the run to follow, s; 0: all of it"},
        {"switch", "12", "when the camera key is pressed for the hand-over strips, s"},
        {"woods", "8", "how long the skier runs straight through the densest wood, s; 0: no woods"},
        {"woods-run", "40", "how far above the wood's heart he starts, m"},
        {"width", "320", "each rung's picture's width, px"},
        {"height", "180", "each rung's picture's height, px"},
        {"settle", "6", "frames drawn after each picture's cut before the capture"},
        {"see-through", "1", "the trees between a lens and him seen through; 0: drawn whole"},
    },
    "--seconds=16 --switch=4 --woods=3 --width=64 --height=36 --settle=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
