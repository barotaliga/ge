// LAB `gpu-plunge`: A FIRST-PERSON PLAYER JUMPS INTO A DIRTY POND, through the classic player
// move (shooter/pmove.hpp) over clip hulls built from brushes (a bank, a pit 16 m square and
// 3.5 m deep, a water brush in it), drawn on the software adapter with the engine's water: the
// player runs off the bank and jumps; his feet going through the surface set off the water's
// answer to a 90 kg, 0.088 m^3 body at his speed (water/entry.hpp: the crater, the crown of
// drops, the jet as it closes, the rings); his eyes going under switch the view to the murky
// water's (render/underwater.hpp: the fog, the specks, the waterline split on the way); he swims
// down, looks round, swims up, and his eyes coming out leave the lens wet (render/wet_lens.hpp:
// the sheet draining, the drops sliding and drying). A second camera on the bank watches the
// same run from 2 m up. Then four bodies of different mass and size dropped from 2 m, side by side
// in time, show the water's answer scaling with the body.
//
// Shots: the bank; the plunge in eight frames from his eyes; the bank's view of the splash in
// six; the view under (looking ahead, down and up); surfacing in six frames; the four bodies;
// a chart of his eyes and feet against the surface. Metrics (deterministic: the move, the water
// and the lens are CPU and exact): the entry speed and energy, the crater, the drops, the wave at
// 2 m, the lens's drops after surfacing, each body's crater and wave. Without an adapter the
// pictures are placeholders and the numbers are the same.
//
// ge:summary Lab: a first-person jump into a dirty pond: the splash, rings, the murk, the wet lens
// ge:keywords lab gpu plunge jump splash crater crown jet rings ripples dive underwater murky
// ge:keywords waterline wet-lens drops surfacing first-person shooter pmove mass size entry
// ge:skill shooter water labs
// ge:test tests/water_entry_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "gpu/device.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/plot.hpp"
#include "lab/raster.hpp"
#include "render/underwater.hpp"
#include "render/water_stream.hpp"
#include "render/wet_lens.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/pmove.hpp"
#include "shooter/recording.hpp"
#include "water/entry.hpp"
#include "water/world.hpp"
#include "wind/field.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace gw = ge::water;
namespace sh = ge::shooter;
namespace lm = ge::core::libm;
namespace ws = ge::lab::water_scene;
using gl::Det;
using ws::fixed;
constexpr double kPi = 3.14159265358979323846;

// THE POND, in metres (y up): the bank at 101 m, the water at 100 m, the pit 16 m square round
// (64, 64) with its bed at 96.5 m; hills past 30 m. The move's units are 32 a metre, z up:
// (x, y, z) in metres is (32 x, 32 z, 32 y) in units.
constexpr double kUnits = 32;
constexpr double kBank = 101, kLevel = 100, kBed = 96.5, kMid = 64, kHalf = 8;
constexpr double kStep = 0.014;  // s: a command's 14 ms
constexpr double kEye = 22;      // units over the origin

bool in_pit(double x, double z) {
  return std::fabs(x - kMid) < kHalf && std::fabs(z - kMid) < kHalf;
}

float pond_ground(double x, double z) {
  if (in_pit(x, z)) {
    return static_cast<float>(kBed + 0.15 * lm::sin(x * 0.9) * lm::cos(z * 0.7));
  }
  const double r = ge::core::hypot(x - kMid, z - kMid);
  const double hills = r > 30 ? (r - 30) * 0.45 + 2.0 * lm::sin(x / 9) * lm::cos(z / 13) : 0;
  return static_cast<float>(kBank + 0.05 * lm::sin(x * 1.3 + z) + std::max(hills, 0.0));
}

// The bank a trodden muddy grass, the pit's walls and bed dark silt.
void paint(ws::Ground& g) {
  const ge::core::Heightfield& f = g.field;
  for (int r = 0; r < f.rows; ++r) {
    for (int c = 0; c < f.cols; ++c) {
      const double x = c * f.cell, z = r * f.cell;
      const size_t i =
          (static_cast<size_t>(r) * static_cast<size_t>(f.cols) + static_cast<size_t>(c)) * 4;
      const bool pit = in_pit(x, z);
      const double n = 0.5 + 0.5 * lm::sin(x * 2.1) * lm::cos(z * 1.7);
      const double rim = std::max(std::fabs(x - kMid), std::fabs(z - kMid)) - kHalf;
      const bool mud = !pit && rim < 1.5;
      g.albedo[i] = static_cast<uint8_t>(pit ? 62 + 10 * n : mud ? 96 + 14 * n : 78 + 20 * n);
      g.albedo[i + 1] = static_cast<uint8_t>(pit ? 56 + 8 * n : mud ? 82 + 10 * n : 96 + 22 * n);
      g.albedo[i + 2] = static_cast<uint8_t>(pit ? 40 + 6 * n : mud ? 58 + 8 * n : 52 + 12 * n);
    }
  }
}

sh::Brush box(int contents, double x0, double y0, double z0, double x1, double y1, double z1) {
  sh::Brush b;
  b.contents = contents;
  const auto f = [](double v) { return static_cast<float>(v); };
  b.planes = {{{1, 0, 0}, f(x1)},   {{-1, 0, 0}, f(-x0)}, {{0, 1, 0}, f(y1)},
              {{0, -1, 0}, f(-y0)}, {{0, 0, 1}, f(z1)},   {{0, 0, -1}, f(-z0)}};
  return b;
}

// The move's world: the bank round the pit, the pit's bed, the water in it.
sh::RecordedScene pond_scene() {
  const double u = kUnits, lo = 0, hi = 128 * u, a = (kMid - kHalf) * u, b = (kMid + kHalf) * u;
  const double base = 94 * u, top = kBank * u, bed = kBed * u, water = kLevel * u;
  sh::RecordedScene s;
  s.name = "pond";
  s.brushes = {box(sh::kContentsSolid, lo, lo, base, hi, a, top),
               box(sh::kContentsSolid, lo, b, base, hi, hi, top),
               box(sh::kContentsSolid, lo, a, base, a, b, top),
               box(sh::kContentsSolid, b, a, base, hi, b, top),
               box(sh::kContentsSolid, a, a, base, b, b, bed),
               box(sh::kContentsWater, a, a, bed, b, b, water)};
  return s;
}

gw::World pond_world(const std::string& look) {
  ge::wind::Field field;
  field.mean = 1;
  field.from = 0.3 * kPi;
  gw::World world(field);
  const std::vector<gw::Point> shore = {{kMid - kHalf, kMid - kHalf},
                                        {kMid + kHalf, kMid - kHalf},
                                        {kMid + kHalf, kMid + kHalf},
                                        {kMid - kHalf, kMid + kHalf}};
  gw::add_lake(world, shore, kLevel, {.look = look}, {}, kLevel - kBed);
  return world;
}

// THE PLAYER, as the engine's eye sees him.
struct Pose {
  double t = 0;
  double eye[3]{}, feet[3]{}, v[3]{};  // m, m/s
  double heading = 0, pitch = 0;       // the engine's: radians, pitch up positive
};

Pose pose_of(const sh::MoveState& st, double t) {
  Pose p;
  p.t = t;
  p.eye[0] = st.origin.x / kUnits;
  p.eye[1] = (st.origin.z + kEye) / kUnits;
  p.eye[2] = st.origin.y / kUnits;
  p.feet[0] = p.eye[0];
  p.feet[1] = (st.origin.z - 24) / kUnits;
  p.feet[2] = p.eye[2];
  p.v[0] = st.velocity.x / kUnits;
  p.v[1] = st.velocity.z / kUnits;
  p.v[2] = st.velocity.y / kUnits;
  p.heading = (90.0 - st.angles.y) * kPi / 180;
  p.pitch = -st.angles.x * kPi / 180;
  return p;
}

// THE SCRIPT: stand, run at the pit, jump off its edge, sink looking down, swim down and ahead,
// look round, look up and swim up, then tread water at the surface turning back to where he
// went in. `in` is when his feet went in (negative before).
sh::MoveCmd script(double t, const sh::MoveState& st, double in, double out) {
  sh::MoveCmd c;
  c.msec = 14;
  c.angles = {12, 90, 0};
  if (in < 0) {
    if (t < 0.4) return c;
    c.forward = 400;
    c.angles.x = 8;
    // The jump off the last stride before the edge.
    if (st.ground >= 0 && st.origin.y >= (kMid - kHalf - 0.9) * kUnits) c.buttons = sh::kButtonJump;
    return c;
  }
  const double s = t - in;
  if (out < 0) {
    if (s < 0.7) {
      c.angles.x = 30;
    } else if (s < 2.3) {
      c.angles.x = 40;
      c.forward = 250;
    } else if (s < 3.1) {
      c.angles.x = 5;
    } else if ((st.origin.z + kEye) / kUnits < kLevel - 0.8) {
      c.angles.x = -70;
      c.forward = 320;
    } else {
      // The last of the way up looking ahead, so his eyes come out level.
      c.angles.x = 5;
      c.up = 340;
    }
    return c;
  }
  // At the surface: tread water, turning back toward the splash.
  const double since = t - out;
  c.up = 200;
  c.angles.x = 8;
  c.angles.y = static_cast<float>(90 + std::min(180.0, since * 75));
  return c;
}

// One run of the move with the water's answers. `on_step` sees every step.
struct Events {
  double jump = -1, in = -1, under = -1, out = -1;
  gw::Entry entry;  // the feet's
  double jet = -1;  // when the jet goes up
  double head_speed = 0;
  double under_at = -1, out_at = -1;  // when the eyes crossed the surface, within a step
};

template <class F>
void run_plunge(gw::World& world, gr::WetLensState& lens, Events& ev, double until, F&& on_step) {
  sh::RecordedScene scene = pond_scene();
  sh::SceneWorld sw;
  sh::build_scene_world(scene, {}, sw);
  sh::MoveSettings settings;
  sh::MoveState st;
  st.origin = {static_cast<float>(kMid * kUnits), static_cast<float>(50 * kUnits),
               static_cast<float>(kBank * kUnits + 24)};
  st.angles = {12, 90, 0};
  gw::Trail trail;
  const int steps = static_cast<int>(until / kStep);
  for (int i = 0; i < steps; ++i) {
    const double t = (i + 1) * kStep;
    const Pose before = pose_of(st, t - kStep);
    const sh::MoveCmd cmd = script(t, st, ev.in, ev.out);
    if (cmd.buttons & sh::kButtonJump && ev.jump < 0 && ev.in < 0) ev.jump = t;
    sh::run_command(st, cmd, settings, sw.world);
    const Pose now = pose_of(st, t);
    const gw::Sample feet0 = gw::sample(world, before.feet[0], before.feet[1], before.feet[2], t);
    const gw::Sample feet1 = gw::sample(world, now.feet[0], now.feet[1], now.feet[2], t);
    const gw::Sample eye1 = gw::sample(world, now.eye[0], now.eye[1], now.eye[2], t);
    const gw::Sample eye0 = gw::sample(world, before.eye[0], before.eye[1], before.eye[2], t);
    gw::Entry fresh;
    bool entered = false, left = false;
    // His feet through the surface: the water's answer to all of him at his speed.
    if (ev.in < 0 && feet1.body >= 0 && feet0.depth < 0 && feet1.depth >= 0) {
      ev.in = t;
      ev.entry = gw::entry_of({.mass = 90,
                               .volume = 0.088,
                               .footprint = 0.15,
                               .x = now.feet[0],
                               .y = feet1.surface,
                               .z = now.feet[2],
                               .vx = now.v[0],
                               .vy = now.v[1],
                               .vz = now.v[2]},
                              0, t);
      gw::emit_ring(world.wakes, ev.entry.ring);
      ev.jet = t + ev.entry.jet_at;
      fresh = ev.entry;
      entered = true;
    }
    // His eyes under, and out again.
    const bool under = eye1.body >= 0 && eye1.depth > 0;
    // Where in the step the eyes crossed, from the depths at its two ends.
    const auto cross = [&] { return t - kStep * (1 - eye0.depth / (eye0.depth - eye1.depth)); };
    if (under && ev.under < 0 && ev.in >= 0) {
      ev.under = t;
      ev.under_at = cross();
    }
    if (ev.under >= 0 && ev.out < 0 && eye0.depth > 0 && !under) {
      ev.out = t;
      ev.out_at = cross();
      ev.head_speed = now.v[1];
      fresh = gw::entry_of({.mass = 5,
                            .volume = 0.006,
                            .footprint = 0.03,
                            .x = now.eye[0],
                            .y = eye1.surface,
                            .z = now.eye[2],
                            .vx = now.v[0],
                            .vy = now.v[1],
                            .vz = now.v[2],
                            .leaving = true},
                           0, t);
      gw::emit_ring(world.wakes, fresh.ring);
      left = true;
    }
    gr::step_wet_lens(lens, under, kStep);
    // Treading water: his strokes' wake.
    if (ev.out >= 0 && st.waterlevel >= 1) {
      gw::wake_trail(world.wakes, trail, 0, now.feet[0], now.feet[2], now.v[0], now.v[2], 1.8, 0.5,
                     0.3, t);
    }
    on_step(t, before, now, eye1, entered || left ? &fresh : nullptr);
  }
}

gr::Waterline line_at(const gw::Sample& s) {
  gr::Waterline l;
  l.body = 0;
  l.surface = s.surface;
  l.dx = -s.nx / std::max(s.ny, 1e-6);
  l.dz = -s.nz / std::max(s.ny, 1e-6);
  return l;
}

gr::FrameView eye_view(const Pose& p) {
  gr::FrameView v = ws::view_at(p.eye[0], p.eye[1], p.eye[2], p.heading, p.pitch, 0.62, 2.2, p.t);
  v.fov_y = 1.02;  // ninety degrees across a wide screen
  return v;
}

// The bank's camera on the pit's west edge, 2 m up, 6 m from the point it looks at.
gr::FrameView bank_view(double x, double z, double t) {
  const double ex = kMid - kHalf - 0.5, ez = z - 3.5;
  const double heading = lm::atan2(x - ex, z - ez);
  const double run = ge::core::hypot(x - ex, z - ez);
  gr::FrameView v = ws::view_at(ex, kBank + 2.0, ez, heading, -lm::atan2(kBank + 2.0 - kLevel, run),
                                0.62, 2.2, t);
  v.fov_y = 0.75;
  return v;
}

// A sheet of `n` frames in a row (or rows of `per`), each with a label over it.
struct Sheet {
  gl::Picture pic;
  int tw = 0, th = 0, per = 0;
  Sheet(int n, int per_row, int w, int h) : tw(w), th(h), per(per_row) {
    const int rows = (n + per_row - 1) / per_row;
    pic = gl::create_picture(per_row * (w + 10) + 10, rows * (h + 16) + 6, {250, 250, 248});
  }
  void put(int i, const gl::Picture& p, const std::string& label) {
    const int ox = 10 + (i % per) * (tw + 10), oy = 4 + (i / per) * (th + 16);
    gl::draw_text(pic, ox, oy, label, {24, 26, 32});
    ws::paste(pic, p, ox, oy + 12);
  }
};

gl::Picture shrink(const gl::Picture& p, int k) {
  if (k <= 1) return p;
  gl::Picture out = gl::create_picture(p.width / k, p.height / k, {0, 0, 0});
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      for (int c = 0; c < 4; ++c) {
        int sum = 0;
        for (int j = 0; j < k * k; ++j) {
          const int sx = k * x + j % k, sy = k * y + j / k;
          sum += p.rgba[(static_cast<size_t>(sy) * static_cast<size_t>(p.width) +
                         static_cast<size_t>(sx)) *
                            4 +
                        static_cast<size_t>(c)];
        }
        out.rgba[(static_cast<size_t>(y) * static_cast<size_t>(out.width) +
                  static_cast<size_t>(x)) *
                     4 +
                 static_cast<size_t>(c)] = static_cast<uint8_t>(sum / (k * k));
      }
    }
  }
  return out;
}

// The wave a ring set: the highest crest it raises at `r` m over `span` s.
double crest_at(const gw::Wakes& wakes, const gw::Ring& ring, double r, double span) {
  double most = 0;
  for (int i = 5; i <= static_cast<int>(span * 100); ++i) {
    const double tau = i * 0.01;
    most = std::max(most, gw::ring_height(wakes, ring, r, tau));
  }
  return most;
}

// THE FOUR BODIES, dropped from 2 m.
struct Dropped {
  const char* name;
  double mass, volume, footprint;
};
constexpr Dropped kBodies[] = {{"pebble 50 g", 0.05, 2e-5, 0.0008},
                               {"crate 20 kg", 20, 0.05, 0.13},
                               {"player 90 kg", 90, 0.088, 0.15},
                               {"boulder 400 kg", 400, 0.15, 0.28}};

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const auto h = static_cast<uint32_t>(lab.arg_num("height"));
  const std::string look = lab.arg("look");
  const std::string preset = lab.arg("under");
  if (w < 64 || h < 36 || w > 4096 || h > 4096) {
    lab.fail("--width must be 64..4096 and --height 36..4096", "next: pass --width=640.");
    return;
  }
  if (!gw::find_look(look)) {
    lab.fail("no water look named '" + look + "'",
             "next: pass --look= one of water/optics.hpp's looks(); skill: water.");
    return;
  }
  gr::UnderwaterLook under_look;
  if (!gr::underwater_look(preset, under_look)) {
    lab.fail("no under-water preset named '" + preset + "'",
             "next: pass --under=eyes-open, mask, murky or classic; skill: water.");
    return;
  }
  lab.metric("look.visibility", gw::optics(*gw::find_look(look)).visibility, "m");

  // PASS ONE, the CPU's alone: when everything happens, and the numbers.
  Events ev;
  gl::Trace eye_tr{"eyes", {}, {}}, feet_tr{"feet", {}, {}},
      surf_tr{"surface over the eyes", {}, {}};
  int drops_1s = -1, drops_3s = -1;
  {
    gw::World world = pond_world(look);
    gr::WetLensState lens;
    run_plunge(
        world, lens, ev, 11.0,
        [&](double t, const Pose&, const Pose& p, const gw::Sample& at_eye, const gw::Entry*) {
          eye_tr.x.push_back(t);
          eye_tr.y.push_back(p.eye[1] - kLevel);
          feet_tr.x.push_back(t);
          feet_tr.y.push_back(p.feet[1] - kLevel);
          surf_tr.x.push_back(t);
          surf_tr.y.push_back(at_eye.body >= 0 ? at_eye.surface - kLevel : 0);
          if (ev.out >= 0 && drops_1s < 0 && t >= ev.out + 1)
            drops_1s = static_cast<int>(lens.drops.size());
          if (ev.out >= 0 && drops_3s < 0 && t >= ev.out + 3)
            drops_3s = static_cast<int>(lens.drops.size());
        });
  }
  if (ev.jump < 0 || ev.in < 0 || ev.under < 0 || ev.out < 0) {
    lab.fail(
        "the script did not jump in, go under and come out (jump " + fixed(ev.jump, 2) + ", in " +
            fixed(ev.in, 2) + ", under " + fixed(ev.under, 2) + ", out " + fixed(ev.out, 2) + " s)",
        "next: check the script's timings against the pond in gpu_plunge_lab.cpp; skill: shooter.");
    return;
  }
  const gw::Entry& e = ev.entry;
  const double entry_speed = std::sqrt(2 * e.energy / 90);
  lab.metric("jump", ev.jump, "s");
  lab.metric("feet-in", ev.in, "s");
  lab.metric("eyes-under", ev.under, "s");
  lab.metric("eyes-out", ev.out, "s");
  lab.metric("entry.speed", entry_speed, "m/s");
  lab.metric("entry.energy", e.energy, "J");
  lab.metric("crater.radius", e.radius, "m");
  lab.metric("crater.depth", e.ring.depth, "m");
  lab.metric("crater.closes", e.jet_at, "s");
  lab.metric("crown.drops", e.drops, "count");
  lab.metric("crown.speed", e.speed, "m/s");
  gw::Wakes still;
  lab.metric("wave.at-2m", crest_at(still, e.ring, 2, 4), "m");
  lab.metric("wave.at-5m", crest_at(still, e.ring, 5, 6), "m");
  lab.metric("lens.drops-1s", drops_1s, "count");
  lab.metric("lens.drops-3s", drops_3s, "count");
  {
    gl::LineChart lc;
    lc.frame = {"HIS EYES AND FEET AGAINST THE SURFACE", "look " + look + ", the classic move",
                "t (s)", "height over the level (m)"};
    lc.traces = {eye_tr, feet_tr, surf_tr};
    lc.marks = {
        {ev.jump, "jump"}, {ev.in, "feet in"}, {ev.under, "eyes under"}, {ev.out, "eyes out"}};
    lc.rules = {0};
    lc.spikes = false;
    const gl::Chart chart = gl::line_chart(lc);
    lab.shot({"heights",
              chart.title,
              "The classic move's player run off the bank, jumping, falling into the pit's water, "
              "sinking and swimming down, then up; his eyes and feet over the still level, and "
              "the surface over his eyes with the crater and the rings in it.",
              "the jump's arc, the feet in about a tenth of a second before the eyes, the "
              "surface's dip as the crater opens over him, the eyes out and bobbing",
              {{"look", look}}},
             gl::to_picture(chart.plot));
  }
  {
    gl::LabTable t;
    t.title = "Four bodies dropped from 2 m (6.3 m/s) into still water";
    t.columns = {"body",    "kg",    "m^3",         "energy J",   "crater m",
                 "depth m", "drops", "wave at 2 m", "wave at 5 m"};
    for (const Dropped& b : kBodies) {
      const gw::Entry d = gw::entry_of({.mass = b.mass,
                                        .volume = b.volume,
                                        .footprint = b.footprint,
                                        .vy = -std::sqrt(2 * 9.80665 * 2)},
                                       0, 0);
      t.rows.push_back({b.name, fixed(b.mass, 2), fixed(b.volume, 5), fixed(d.energy, 1),
                        fixed(d.radius, 3), fixed(d.ring.depth, 3), std::to_string(d.drops),
                        fixed(crest_at(still, d.ring, 2, 4) * 100, 2) + " cm",
                        fixed(crest_at(still, d.ring, 5, 6) * 100, 2) + " cm"});
    }
    lab.table(t);
  }

  // PASS TWO, drawn.
  auto device = ge::gpu::create_device({.label = "gpu-plunge", .validation = 1});
  const std::vector<std::string> ids = {"bank", "plunge", "splash", "under", "surfacing", "bodies"};
  if (!device) {
    if (ws::gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    ws::placeholders(lab, ids, w, h);
    return;
  }
  ws::Scene s;
  if (!ws::make_scene(*device, s, pond_ground, kLevel, 0.5, w, h, paint)) {
    lab.fail("the water's pipelines did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  gw::World world = pond_world(look);
  gr::WaterDraw draw;
  draw.base_spacing = 0.08;
  draw.levels = 10;
  gr::add_water_body(*device, s.water, world.bodies[0], draw);
  s.under.look = under_look;
  const int tw = static_cast<int>(w / 2), th = static_cast<int>(h / 2);

  // The frames to take, by when.
  struct Take {
    double t;
    int sheet;  // 0 bank, 1 plunge, 2 splash, 3 under, 4 surfacing
    int slot;
    std::string label;
  };
  const double in = ev.in, out = ev.out;
  std::vector<Take> takes = {{0.2, 0, 0, ""},
                             {ev.jump - 0.15, 1, 0, "RUN"},
                             {ev.jump + 0.25, 1, 1, "JUMP"},
                             {in - 0.12, 1, 2, "FALLING"},
                             {in + 0.03, 1, 3, "FEET IN"},
                             {ev.under_at, 1, 4, "WATERLINE"},
                             {ev.under + 0.1, 1, 5, "UNDER"},
                             {in + 1.0, 1, 6, "SINKING, 1 S"},
                             {in + 2.0, 1, 7, "SWIMMING DOWN"},
                             {in + 2.9, 3, 0, "AHEAD"},
                             {in + 1.6, 3, 1, "DOWN"},
                             {ev.out_at - 0.3, 3, 2, "UP"},
                             {ev.out_at, 4, 0, "WATERLINE"},
                             {out + 0.12, 4, 1, "+0.1 S: THE SHEET"},
                             {out + 0.35, 4, 2, "+0.35 S"},
                             {out + 0.8, 4, 3, "+0.8 S"},
                             {out + 1.6, 4, 4, "+1.6 S"},
                             {out + 3.5, 4, 5, "+3.5 S"},
                             {in + 0.08, 2, 0, "+0.08 S"},
                             {in + 0.25, 2, 1, "+0.25 S: THE CRATER"},
                             {in + e.jet_at + 0.12, 2, 2, "THE JET"},
                             {in + 1.2, 2, 3, "+1.2 S"},
                             {in + 2.5, 2, 4, "+2.5 S"},
                             {in + 5.0, 2, 5, "+5 S"}};
  std::stable_sort(takes.begin(), takes.end(),
                   [](const Take& a, const Take& b) { return a.t < b.t; });
  Sheet plunge(8, 4, tw, th), splash(6, 3, tw, th), under(3, 3, tw, th), surfacing(6, 3, tw, th);
  gl::Picture bank;
  size_t next = 0;
  ge::core::Rng spray_rng = ge::core::make_rng(11);
  std::vector<gr::Particle> births;
  double last_birth = -10;
  bool jet_done = false;
  gr::WetLensState& lens = s.lens.state;
  Events drawn;  // the same run again, its events found again on the way
  run_plunge(world, lens, drawn, out + 3.6,
             [&](double t, const Pose& before, const Pose& now, const gw::Sample&,
                 const gw::Entry* fresh) {
               if (fresh) {
                 gr::entry_spray(*fresh, spray_rng, births);
                 gr::set_water_wakes(*device, s.water, 0, world.wakes, 0);
               }
               if (!jet_done && drawn.jet > 0 && t >= drawn.jet) {
                 jet_done = true;
                 gr::jet_spray(drawn.entry, spray_rng, births);
               }
               if (!births.empty()) last_birth = t;
               if (t - last_birth < 2.5) {
                 ws::fly(*device, s, births, kStep);
                 births.clear();
               }
               // The trail's rings, uploaded as they come.
               if (drawn.out >= 0 && static_cast<int>(t / kStep) % 8 == 0) {
                 gr::set_water_wakes(*device, s.water, 0, world.wakes, 0);
               }
               while (next < takes.size() && takes[next].t <= t + 1e-9) {
                 const Take& k = takes[next++];
                 // The pose at the take's own moment, between the step's two ends.
                 const double f = std::clamp((k.t - before.t) / kStep, 0.0, 1.0);
                 Pose p = now;
                 p.t = k.t;
                 for (int i = 0; i < 3; ++i)
                   p.eye[i] = before.eye[i] + f * (now.eye[i] - before.eye[i]);
                 p.heading = before.heading + f * (now.heading - before.heading);
                 p.pitch = before.pitch + f * (now.pitch - before.pitch);
                 const gr::Waterline line =
                     line_at(gw::sample(world, p.eye[0], p.eye[1], p.eye[2], k.t));
                 if (k.sheet == 2) {
                   const gr::FrameView v = bank_view(ev.entry.ring.x, ev.entry.ring.z, k.t);
                   gr::WetLensState kept = lens;
                   lens = {};  // the bank's camera is dry
                   splash.put(k.slot, shrink(ws::draw(*device, s, v), 2), k.label);
                   lens = kept;
                   continue;
                 }
                 const gl::Picture pic = ws::draw(*device, s, eye_view(p), true, &line);
                 if (k.sheet == 0) bank = pic;
                 if (k.sheet == 1) plunge.put(k.slot, shrink(pic, 2), k.label);
                 if (k.sheet == 3) under.put(k.slot, shrink(pic, 2), k.label);
                 if (k.sheet == 4) surfacing.put(k.slot, shrink(pic, 2), k.label);
               }
             });
  const std::vector<std::pair<std::string, std::string>> inputs = {{"look", look},
                                                                   {"under", preset}};
  lab.shot({"bank", "ON THE BANK",
            "His eyes on the bank 6 m from a dirty pond in a pit 16 m square, its water at the "
            "bank's foot a metre down: the classic player before he runs.",
            "a still, opaque brown-green surface mirroring the sky, the pit's dark walls", inputs},
           bank, Det::measured);
  lab.shot({"plunge", "THE PLUNGE FROM HIS EYES",
            "The run, the jump, the fall, his feet through the surface (the crater opening and "
            "the crown thrown up round him), his eyes at the waterline (the picture split by the "
            "lens's plate), just under, sinking and swimming down into the murk.",
            "the surface rushing up; the crown's drops; the split at the waterline with its dark "
            "meniscus; the murk closing in to a metre or two with specks drifting in it",
            inputs},
           plunge.pic, Det::measured);
  lab.shot(
      {"splash", "THE SPLASH FROM THE BANK",
       "A camera on the bank 2 m up and about 6 m off watching the same run: the water's answer to "
       "a 90 kg "
       "body at " +
           fixed(entry_speed, 1) + " m/s (water/entry.hpp): a crater " + fixed(2 * e.radius, 2) +
           " m across and " + fixed(e.ring.depth, 2) + " m deep with its crown of " +
           std::to_string(e.drops) + " drops, the jet as it closes, then the rings running out.",
       "the crater and crown at a quarter second, the jet up the middle, then rings, the "
       "longest first, spreading to the pit's walls; nothing past the pit moves",
       inputs},
      splash.pic, Det::measured);
  lab.shot({"under", "UNDER THE DIRTY WATER",
            "Looking ahead, down at the bed and up at the surface from under the pond, in the '" +
                preset +
                "' preset: the water's own colour closing in, the specks suspended in "
                "it, Snell's window over him.",
            "barely anything past a metre or two; specks near the eye; the bed only as a "
            "darker murk; the window a dim bright patch overhead",
            inputs},
           under.pic, Det::measured);
  lab.shot({"surfacing", "COMING UP: THE WATERLINE AND THE WET LENS",
            "His eyes through the surface: the split at the waterline, then the sheet of water "
            "draining off the lens (render/wet_lens.hpp) and the drops it leaves sliding and "
            "drying while he treads water, turning back to where he went in.",
            "the split; a rippled sheet with a bright edge running down; big drops showing the "
            "view upside down and sliding with beads behind them; fewer, smaller drops by 3.5 s",
            inputs},
           surfacing.pic, Det::measured);

  // THE FOUR BODIES, each into still water from 2 m, watched from the bank.
  {
    Sheet bodies(12, 3, tw, th);
    ws::fly(*device, s, {}, 5.0);  // the run's spray gone
    int row = 0;
    for (const Dropped& b : kBodies) {
      gw::Wakes wakes;
      const double t0 = 200.0 + 20 * row;
      const gw::Entry d = gw::entry_of({.mass = b.mass,
                                        .volume = b.volume,
                                        .footprint = b.footprint,
                                        .x = kMid,
                                        .y = kLevel,
                                        .z = kMid,
                                        .vy = -std::sqrt(2 * 9.80665 * 2)},
                                       0, t0);
      gw::emit_ring(wakes, d.ring);
      gr::set_water_wakes(*device, s.water, 0, wakes, 0);
      std::vector<gr::Particle> spray;
      ge::core::Rng rng = ge::core::make_rng(5);
      gr::entry_spray(d, rng, spray);
      const double at[3] = {0.25, d.jet_at + 0.12, 2.0};
      double now = t0;
      ws::fly(*device, s, spray, 0.0);
      bool jet = false;
      for (int k = 0; k < 3; ++k) {
        while (now < t0 + at[k] - 1e-9) {
          std::vector<gr::Particle> more;
          if (!jet && now >= t0 + d.jet_at) {
            jet = true;
            gr::jet_spray(d, rng, more);
          }
          ws::fly(*device, s, more, kStep);
          now += kStep;
        }
        const gr::FrameView v = bank_view(kMid, kMid, now);
        bodies.put(row * 3 + k, shrink(ws::draw(*device, s, v), 2),
                   std::string(b.name) + "  +" + fixed(at[k], 2) + " S");
      }
      ws::fly(*device, s, {}, 5.0);
      ++row;
    }
    lab.shot({"bodies", "THE WATER'S ANSWER SCALES WITH THE BODY",
              "A 50 g pebble, a 20 kg crate (it floats), the 90 kg player and a 400 kg boulder, "
              "each dropped from 2 m into the still pond: its crater, its crown and its jet, "
              "then its rings two seconds on, all off its mass, volume and footprint "
              "(water/entry.hpp; the numbers in the table).",
              "a pebble's few drops and short ripples; the crate's shallow crater; the player's "
              "and the boulder's wide craters, tall crowns and long, high rings",
              inputs},
             bodies.pic, Det::measured);
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
             "Add it to the scene's manifest (lab/labs/water_scene.hpp). Skill: gpu.");
  }
}

const gl::LabSpec kSpec{
    "gpu-plunge",
    gl::LabKind::picture,
    "shooter",
    "engine/water/entry.hpp",
    "water",
    "Does a first-person jump into dirty water read: the splash sized by the body, the rings, "
    "the murk under the surface, the waterline split and the wet lens coming out?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
        {"look", "algal-pond", "the pond's water (water/optics.hpp's looks)"},
        {"under", "eyes-open", "the view under water's preset (render/underwater.hpp)"},
    },
    "--width=128 --height=72",
    run,
};
GE_LAB(kSpec);

}  // namespace
