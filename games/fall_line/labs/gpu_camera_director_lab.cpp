// LAB `camera-director`: THE DIRECTOR'S LENSES DRAWN (games/fall_line/camera/film.hpp over
// engine/camera/plant.hpp; docs/camera.md § Directors). The pilot races seed 38 with a field
// through a live recorder, the player's moments are filed as marks and the director cuts them
// (as the `replay-cut` lab does). Then:
//
//   - THE FILM: the cut's film plan is made once off the player's path (every planted lens placed
//     where it sees most of its shot), and the reel is played at `--fps` through `film_pose`, a
//     pure function of the plan and the step. Each shot is pictured as a strip across it; every
//     frame is measured: the most the lens turned in one frame inside a shot (a jump there is a
//     broken shot; the cut between shots is meant), the share of frames with him inside the
//     frame and with a clear line to him, and the lens's least height over the snow.
//   - THE LENSES: the first shot's stretch planned through every lens Fall Line has (side, front,
//     aerial, impact, chase), pictured side by side at its mark, each with the share of the
//     stretch its place sees.
//   - THE BROADCAST: the trackside lenses stood down the piste, the live race followed through
//     the broadcast's cut rules; the first cuts pictured before and after, every cut counted
//     (and those across his line), the least a lens was held, and the share of frames he was in
//     sight.
//
// The same race always makes the same cut, the same plan and the same broadcast: the numbers are
// deterministic; the pictures are the GPU's.
//
//   make lab LAB=camera-director ARGS="--seed=38 --rivals=3"
//
// ge:summary Lab: the replay's film camera through the director's cut, every lens, the broadcast
// ge:keywords lab camera director film replay cut shot lens planted trackside broadcast strip
// ge:keywords in-frame sight continuity 180-degree-rule
// ge:skill camera replay labs
// ge:test tests/fall_line_film_test.cpp
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

#include "camera/plant.hpp"
#include "camera/to_view.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/camera/film.hpp"
#include "fall_line/camera/subject.hpp"
#include "fall_line/camera/world.hpp"
#include "fall_line/sim/moments.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/state.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "replay/director.hpp"
#include "replay/live.hpp"

namespace {

namespace gl = ge::lab;
namespace cam = ge::camera;
namespace fc = fall_line::camera;
namespace fs = fall_line::sim;
namespace gp = ge::play;
namespace gr = ge::replay;
using gl::Det;

constexpr int kStrip = 5;            // pictures across a shot
constexpr double kTurnJump = 0.12;   // rad: a lens turning more than this in one frame jumps
constexpr double kSnowFloor = 0.05;  // m
constexpr double kInFrame = 0.9;     // the least share of a shot's frames with him in frame
constexpr double kAspect = 16.0 / 9.0;

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

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

// Whether a point lies inside the pose's frame (a 16:9 picture).
bool in_frame(const cam::Pose& p, const cam::Vec3& at) {
  namespace lm = ge::core::libm;
  const cam::Vec3 f = cam::look_forward(p.yaw, p.pitch);
  const double dx = at.x - p.eye.x, dy = at.y - p.eye.y, dz = at.z - p.eye.z;
  const double ahead = dx * f.x + dy * f.y + dz * f.z;
  if (ahead <= 0) return false;
  // Right is (cos yaw, 0, -sin yaw); up is right x forward (its sign does not matter here).
  const double rx = lm::cos(p.yaw), rz = -lm::sin(p.yaw);
  const double side = dx * rx + dz * rz;
  const double ux = -rz * f.y, uy = rz * f.x - rx * f.z, uz = rx * f.y;
  const double up = dx * ux + dy * uy + dz * uz;
  const double th = lm::tan(p.fov_y * 0.5);
  return std::fabs(side) <= ahead * th * kAspect && std::fabs(up) <= ahead * th;
}

double angle_step(const cam::Pose& a, const cam::Pose& b) {
  const double dy = ge::core::angle_diff(a.yaw, b.yaw), dp = b.pitch - a.pitch;
  return ge::core::hypot(dy, dp);
}

ge::lab::Rgba colour_of(fc::FilmLens l) {
  switch (l) {
    case fc::FilmLens::side:
      return {30, 90, 200, 255};
    case fc::FilmLens::front:
      return {150, 60, 190, 255};
    case fc::FilmLens::aerial:
      return {40, 160, 60, 255};
    case fc::FilmLens::impact:
      return {220, 30, 30, 255};
    case fc::FilmLens::chase:
      break;
  }
  return {230, 140, 20, 255};
}

// The race as two bars: the film's shots coloured by lens over the run they show, and the
// broadcast's live lens (alternating shades, a tick at every cut).
gl::Picture timeline(const fc::FilmPlan& plan, const std::vector<int>& live, int64_t length,
                     double hz, double every) {
  constexpr int kW = 1200, kH = 200, kX0 = 40, kX1 = kW - 40;
  gl::Picture p = gl::create_picture(kW, kH, {250, 250, 250, 255});
  const auto x_of = [&](double s) {
    return kX0 + (kX1 - kX0) * s / static_cast<double>(length ? length : 1);
  };
  gl::draw_text(p, kX0, 10,
                "FILM: THE CUT'S SHOTS BY LENS (BLUE SIDE  PURPLE FRONT  GREEN AERIAL  RED IMPACT  "
                "ORANGE CHASE)    BROADCAST: THE LIVE TRACKSIDE LENS, A TICK AT EVERY CUT",
                {40, 40, 40, 255});
  gl::draw_text(p, kX0, 36, "FILM", {60, 60, 60, 255});
  gl::fill_rect(p, kX0, 50, kX1 - kX0, 40, {225, 225, 225, 255});
  for (const fc::FilmShot& s : plan.shots) {
    const double a = x_of(static_cast<double>(s.from)), b = x_of(static_cast<double>(s.to));
    gl::fill_rect(p, a, 50, std::max(2.0, b - a), 40, colour_of(s.lens));
  }
  gl::draw_text(p, kX0, 104, "BROADCAST", {60, 60, 60, 255});
  for (size_t i = 0; i < live.size(); ++i) {
    const double s = static_cast<double>(i) * every;
    const double a = x_of(s), b = x_of(s + every);
    const uint8_t c = live[i] % 2 ? 110 : 170;
    gl::fill_rect(p, a, 118, std::max(1.0, b - a), 30, {c, c, static_cast<uint8_t>(c + 40), 255});
    if (i > 0 && live[i] != live[i - 1]) gl::fill_rect(p, a - 1, 112, 2, 42, {20, 20, 20, 255});
  }
  for (double t = 0; t * hz <= static_cast<double>(length); t += 10) {
    const double x = x_of(t * hz);
    gl::draw_line(p, x, 160, x, 166, {90, 90, 90, 255}, 1);
    gl::draw_text(p, x - 6, 172, fixed(t, 0), {60, 60, 60, 255});
  }
  return p;
}

struct ShotStats {
  double turn = 0;      // the most the lens turned in one frame inside the shot, rad
  double in_frame = 0;  // share of frames
  double sight = 0;     // share of frames with a clear line to him
  double over = 1e9;    // the lens's least height over the snow, m
  int frames = 0;
};

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(fs::kGame, seed, {{"rivals", lab.arg("rivals")}, {"hands", "human"}}, setup,
                      error)) {
    lab.fail(error, "Pass --rivals as 0..7 (skill: sim).");
    return;
  }
  gr::Live live(fs::kGame, setup);
  if (!live.ok()) {
    lab.fail(live.error(), "Pick another --seed (skill: mountain).");
    return;
  }
  const double hz = fs::kGame.hz;
  const auto most = static_cast<int64_t>(lab.arg_num("seconds") * hz);
  fs::Moments moments;
  std::vector<std::vector<ge::skier::SkierState>> racers;  // every racer, a step
  std::vector<cam::PathPoint> path;                        // the player, a step
  auto keep = [&](const fs::GameState& s) {
    std::vector<ge::skier::SkierState> all;
    all.reserve(s.racers.size());
    for (const auto& r : s.racers) all.push_back(r.skier);
    racers.push_back(std::move(all));
    path.push_back(fc::path_point(s.racers[0].skier));
  };
  if (const fs::GameState* s = fs::state_of(live.run())) keep(*s);
  std::vector<double> input(fs::kGame.axes.size(), 0);
  while (!live.done() && (most <= 0 || live.steps() < most)) {
    live.run().pilot(input);
    live.step(input);
    if (const fs::GameState* s = fs::state_of(live.run())) {
      moments.file(*s, live);
      keep(*s);
    }
  }
  const fs::GameState* end = fs::state_of(live.run());
  if (!end || path.empty()) {
    lab.fail("the race left no state", "Read games/fall_line/sim/play.cpp (skill: sim).");
    return;
  }
  const ge::skier::World sw = end->world();
  const cam::World cw = fc::camera_world(sw);
  const auto& level = *end->level;
  const std::vector<gr::Mark>& marks = live.marks();
  const int64_t length = static_cast<int64_t>(path.size()) - 1;
  const gr::Cut cut = gr::plan_cut(marks, fs::shot_rules(), fs::director_policy(), hz, length);
  if (cut.shots.empty()) {
    lab.fail("the director cut no shots", "Run the `replay-cut` lab (skill: replay).");
    return;
  }
  const fc::FilmPlan plan = fc::plan_film(cut, marks, path, cw, hz);

  // THE FILM: the reel frame by frame through the plan.
  const double fps = lab.arg_num("fps");
  std::vector<ShotStats> stats(plan.shots.size());
  {
    cam::Pose last{};
    size_t last_shot = SIZE_MAX;
    for (int64_t f = 0;; ++f) {
      const gr::ReelAt at = gr::reel_at(cut, hz, static_cast<double>(f) / fps);
      const cam::Pose p = fc::film_pose(plan, cw, at.shot, at.step);
      ShotStats& st = stats[at.shot];
      const auto i = static_cast<size_t>(std::clamp(at.step, 0.0, static_cast<double>(length)));
      const cam::Vec3 him = fc::framed_point(racers[i][0]);
      st.frames++;
      st.in_frame += in_frame(p, him) ? 1 : 0;
      st.sight += cam::sight_share(cw, p.eye, him) >= 0.999 ? 1 : 0;
      st.over = std::min(st.over, p.eye.y - cam::ground_at(cw, p.eye.x, p.eye.z));
      if (at.shot == last_shot) st.turn = std::max(st.turn, angle_step(last, p));
      last = p;
      last_shot = at.shot;
      if (at.over) break;
    }
  }
  std::vector<std::vector<std::string>> rows;
  for (size_t k = 0; k < plan.shots.size(); ++k) {
    const fc::FilmShot& s = plan.shots[k];
    ShotStats& st = stats[k];
    const double n = st.frames ? st.frames : 1;
    st.in_frame /= n;
    st.sight /= n;
    const std::string id = "shot" + std::to_string(k + 1) + "." + fc::film_lens_name(s.lens);
    lab.metric(id + ".planted-sees", s.plant.candidate < 0 ? -1 : s.plant.seen, "share");
    lab.metric(id + ".in-frame", st.in_frame, "share");
    lab.metric(id + ".in-sight", st.sight, "share");
    lab.metric(id + ".most-turn", st.turn, "rad/frame");
    lab.metric(id + ".least-over-snow", st.over, "m");
    rows.push_back({std::to_string(k + 1), fc::film_lens_name(s.lens),
                    fixed(static_cast<double>(s.from) / hz, 2),
                    fixed(static_cast<double>(s.to) / hz, 2),
                    s.plant.candidate < 0 ? "-" : fixed(s.plant.seen, 2), fixed(st.in_frame, 2),
                    fixed(st.sight, 2), fixed(st.turn, 3), fixed(st.over, 2)});
    if (st.turn > kTurnJump) {
      lab.fail(id + ": the lens turned " + fixed(st.turn, 3) + " rad in one frame inside a shot",
               "Read film_pose in games/fall_line/camera/film.cpp: a shot is one continuous move "
               "(skill: camera).");
    }
    if (st.in_frame < kInFrame) {
      lab.fail(id + ": he was in frame " + fixed(st.in_frame * 100, 0) + " % of the shot",
               "Read the lens's row in games/fall_line/camera/film.cpp (its hold and place) and "
               "plant_beside in engine/camera/plant.cpp (skill: camera).");
    }
    if (st.over < kSnowFloor) {
      lab.fail(id + ": the lens stood " + fixed(st.over, 2) + " m over the snow",
               "Read the lens's height in games/fall_line/camera/film.cpp (skill: camera).");
    }
  }
  lab.metric("shots", static_cast<double>(plan.shots.size()), "count");
  lab.table(
      {"the film: every shot of the cut through its lens (planted-sees: the share of its "
       "stretch its place sees; most-turn: rad in one frame inside the shot)",
       {"shot", "lens", "from-s", "to-s", "planted-sees", "in-frame", "in-sight", "most-turn",
        "least-over-snow-m"},
       rows});

  // THE LENSES: the first shot's stretch through every lens.
  gr::Cut every_lens;
  for (const char* name : {"side", "front", "aerial", "impact", "chase"}) {
    gr::Shot s = cut.shots[0];
    s.lens = name;
    every_lens.shots.push_back(s);
  }
  const fc::FilmPlan lenses = fc::plan_film(every_lens, marks, path, cw, hz);
  for (const fc::FilmShot& s : lenses.shots) {
    lab.metric(std::string("lens.") + fc::film_lens_name(s.lens) + ".planted-sees",
               s.plant.candidate < 0 ? -1 : s.plant.seen, "share");
  }

  // THE BROADCAST: the trackside lenses and the live race through the cut rules.
  fc::Broadcast b;
  b.lenses = fc::trackside_lenses(level, cw);
  const int every = std::max(1, static_cast<int>(std::lround(hz / fps)));
  std::vector<int> on_air;
  std::vector<cam::Pose> broadcast;
  std::vector<int64_t> cut_steps;
  double least_hold = 1e9, held = 0;
  int seen = 0, framed = 0;
  for (int64_t i = 0; i <= length; i += every) {
    const ge::skier::SkierState& sk = racers[static_cast<size_t>(i)][0];
    cam::Subject s;
    s.pos = {sk.x, sk.y, sk.z};
    s.vel = {sk.vx, sk.vy, sk.vz};
    const cam::Pose p = fc::broadcast_pose(b, level, s, cw, static_cast<double>(every) / hz);
    const cam::Vec3 him = fc::framed_point(sk);
    seen += cam::sight_share(cw, p.eye, him) >= 0.999 ? 1 : 0;
    framed += in_frame(p, him) ? 1 : 0;
    if (b.state.cut && i > 0) {
      least_hold = std::min(least_hold, held);
      held = 0;
      cut_steps.push_back(i);
    }
    held += static_cast<double>(every) / hz;
    on_air.push_back(b.state.live);
    broadcast.push_back(p);
  }
  const double frames = on_air.empty() ? 1 : static_cast<double>(on_air.size());
  lab.metric("broadcast.lenses", static_cast<double>(b.lenses.size()), "count");
  lab.metric("broadcast.cuts", b.state.cuts, "count");
  lab.metric("broadcast.crossed", b.state.crossed, "count");
  lab.metric("broadcast.least-hold", least_hold > 1e8 ? -1 : least_hold, "s");
  lab.metric("broadcast.in-sight", seen / frames, "share");
  lab.metric("broadcast.in-frame", framed / frames, "share");
  if (b.lenses.empty()) {
    lab.fail("no trackside lens could be planted",
             "Read trackside_lenses in games/fall_line/camera/film.cpp (skill: camera).");
  }

  // THE PICTURES.
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int settle = static_cast<int>(lab.arg_num("settle"));
  const double hour = lab.arg_num("hour");
  const std::string sd = fixed(seed, 0);
  std::vector<gl::Shot> specs;
  for (size_t k = 0; k < plan.shots.size(); ++k) {
    const fc::FilmShot& s = plan.shots[k];
    const gr::Shot& c = cut.shots[k];
    specs.push_back(
        {"shot-" + std::to_string(k + 1),
         "SHOT " + std::to_string(k + 1) + ": " + upper(c.kind) + " THROUGH THE " +
             upper(fc::film_lens_name(s.lens)) + " LENS",
         "The cut's shot " + std::to_string(k + 1) + " (a " + c.kind + " at " +
             fixed(static_cast<double>(s.mark) / hz, 1) + " s, played at " + fixed(c.rate, 2) +
             "x), " + std::to_string(kStrip) + " frames across it through the film camera's " +
             fc::film_lens_name(s.lens) + " lens" +
             (s.plant.candidate < 0 ? ""
                                    : ", planted where it sees " + fixed(s.plant.seen * 100, 0) +
                                          " % of the shot's stretch") +
             ".",
         "Him in every frame, the lens still (a planted one) or moving with him (the "
         "chase), and the moment the mark names in the middle frames.",
         {{"seed", sd}, {"shot", std::to_string(k + 1)}}});
  }
  specs.push_back({"lenses",
                   "THE FIRST SHOT THROUGH EVERY LENS",
                   "The cut's first shot at its mark through each of Fall Line's film lenses "
                   "(side, front, aerial, impact, chase), each planted for that stretch.",
                   "Five distinct framings of the same moment, him in each.",
                   {{"seed", sd}}});
  specs.push_back({"broadcast-cuts",
                   "THE BROADCAST'S FIRST CUTS",
                   "The live race through the trackside lenses: for each of the first cuts, the "
                   "frame a third of a second before it (top) and the cut's frame (bottom).",
                   "Each cut to a lens he is running toward, on the same side of his line as the "
                   "last where the rules could keep it.",
                   {{"seed", sd}}});
  specs.push_back({"timeline",
                   "SEED " + sd + ": THE FILM AND THE BROADCAST",
                   "The race as two bars: the film's shots coloured by lens over the run each "
                   "shows, and the broadcast's live lens with a tick at every cut.",
                   "Shots where the moments are. The broadcast's holds no shorter than its least "
                   "hold.",
                   {{"seed", sd}}});
  lab.shot(specs.back(), timeline(plan, on_air, length, hz, every), Det::deterministic);
  specs.pop_back();

  auto device = ge::gpu::create_device({.label = "camera-director", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    gl::Picture none = gl::create_picture(width, height, {40, 40, 48});
    gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
    for (const gl::Shot& s : specs) lab.shot(s, none, Det::measured);
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
  const ge::gpu::Target target =
      ge::gpu::create_target(*device, "camera-director.final", static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height));
  auto picture = [&](int64_t step, const cam::Pose& pose) {
    const auto i = static_cast<size_t>(std::clamp<int64_t>(step, 0, length));
    ge::render::FrameView v;
    fall_line::app::day_view(level, hour, v);
    cam::to_frame_view(pose, v);
    v.time_s = static_cast<double>(i) / hz;
    fall_line::app::cut(*frame);
    fall_line::app::FrameScene scene{std::span<const ge::skier::SkierState>(racers[i])};
    const cam::Vec3 at = fc::framed_point(racers[i][0]);
    scene.subject = std::array<double, 3>{at.x, at.y, at.z};
    for (int k = 0; k < settle; ++k) {
      v.frame = static_cast<uint64_t>(k);
      fall_line::app::draw(*device, *frame, v, scene, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    if (img.rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      return gl::create_picture(width, height, {255, 0, 255});
    }
    return gl::Picture{width, height, img.rgba};
  };
  const int label_h = 14;
  auto labelled = [&](gl::Picture& sheet, const gl::Picture& p, int col, int row,
                      const std::string& label) {
    const int x = col * width, y = row * (height + label_h);
    gl::blit(sheet, p, x, y + label_h);
    gl::draw_text(sheet, x + 4, y + 3, label, {235, 235, 235});
  };
  size_t spec = 0;
  for (size_t k = 0; k < plan.shots.size(); ++k, ++spec) {
    const fc::FilmShot& s = plan.shots[k];
    gl::Picture sheet = gl::create_picture(kStrip * width, height + label_h, {20, 22, 26});
    for (int c = 0; c < kStrip; ++c) {
      const double step =
          static_cast<double>(s.from) + static_cast<double>(s.to - s.from) * c / (kStrip - 1);
      const auto st = static_cast<int64_t>(step);
      labelled(sheet, picture(st, fc::film_pose(plan, cw, k, step)), c, 0,
               upper(fc::film_lens_name(s.lens)) + " " + fixed(step / hz, 2) + " S");
    }
    lab.shot(specs[spec], sheet, Det::measured);
  }
  {
    gl::Picture sheet = gl::create_picture(static_cast<int>(lenses.shots.size()) * width,
                                           height + label_h, {20, 22, 26});
    for (size_t k = 0; k < lenses.shots.size(); ++k) {
      const fc::FilmShot& s = lenses.shots[k];
      const double step = static_cast<double>(s.mark);
      labelled(sheet, picture(s.mark, fc::film_pose(lenses, cw, k, step)), static_cast<int>(k), 0,
               upper(fc::film_lens_name(s.lens)) +
                   (s.plant.candidate < 0 ? "" : " SEES " + fixed(s.plant.seen * 100, 0) + "%"));
    }
    lab.shot(specs[spec++], sheet, Det::measured);
  }
  {
    const int cols = std::max(1, std::min(5, static_cast<int>(cut_steps.size())));
    gl::Picture sheet = gl::create_picture(cols * width, 2 * (height + label_h), {20, 22, 26});
    for (int c = 0; c < cols && c < static_cast<int>(cut_steps.size()); ++c) {
      const int64_t at = cut_steps[static_cast<size_t>(c)];
      const auto fi = static_cast<size_t>(at / every);
      const size_t before =
          fi >= static_cast<size_t>(fps / 3) ? fi - static_cast<size_t>(fps / 3) : 0;
      labelled(sheet, picture(static_cast<int64_t>(before) * every, broadcast[before]), c, 0,
               "BEFORE " + fixed(static_cast<double>(at) / hz, 1) + " S");
      labelled(sheet, picture(at, broadcast[fi]), c, 1,
               "CUT TO LENS " + std::to_string(on_air[fi] + 1));
    }
    lab.shot(specs[spec++], sheet, Det::measured);
  }
  lab.metric("gpu-errors", device->errors.load(), "count", Det::measured);
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "camera-director",
    gl::LabKind::picture,
    "game:fall_line",
    "games/fall_line/camera/film (the replay's film camera, the trackside broadcast) over "
    "engine/camera/plant and engine/replay/director",
    "camera",
    "Does the film camera draw the director's cut with him in every frame, each shot one "
    "continuous move and each cut a clean one, is every lens planted where it sees its moment, "
    "and does the broadcast cut between trackside lenses by its rules?",
    {
        {"seed", "38", "the race map's seed"},
        {"rivals", "3", "bots on the grid beside the pilot (0..7)"},
        {"seconds", "0", "the race cut to its first this many seconds (0: all of it)"},
        {"fps", "60", "the reel's and the broadcast's frames a second"},
        {"hour", "-1", "the solar hour to light the map at; negative: the map's own"},
        {"width", "320", "each picture's width, px"},
        {"height", "180", "each picture's height, px"},
        {"settle", "6", "frames drawn after each picture's cut before the capture"},
    },
    "--rivals=1 --seconds=60 --width=64 --height=36 --settle=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
