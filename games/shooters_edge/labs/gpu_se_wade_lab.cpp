// LAB `se-wade`: DOES LAVA AND SLIME MOVE, THICK, ROUND A PLAYER WADING THROUGH THEM? Two halves.
//
// - THE CPU HALF (the smoke run, deterministic): render/liquid_wade.hpp's surface worked out by its
//   CPU twin for a body walking 4 m through a pool at 2.5 m/s and stepping out, in water (a
//   viscosity of 1), slime (2) and lava (4): the surfaces from above as it walks and 1.5 s after it
//   is out (the furrow shaded, lava's torn crust orange); the furrow's depth at a point it walked
//   through against the time since; and the table of what each viscosity makes of the waves.
// - ON A REAL MAP (`--game=<base>/<folder>`, `--lava`, `--slime`: a map of the folder each, for
//   PRIVATE pictures only): the window (client/world_draw.hpp, the modern look) draws a player
//   walking through the map's lava pool and its slime pool and stopping, as it draws another
//   player, from over the bank: pictures at `--moments` with ce_wade 0 (the still surface) and 1,
//   the same moments, an animated film of each, and the liquid pass's GPU time with the wading off
//   and on (`--frames` each).
//
// ge:summary Lab: a player wading through lava and slime on real maps; the furrow, bow and rings
// ge:keywords lab se-wade wade wading lava slime viscosity furrow bow rings crust-torn ce_wade
// ge:skill render shooter labs
// ge:test tests/render_liquid_wade_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "render/liquid.hpp"
#include "render/liquid_wade.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace br = ge::brush;
namespace gr = ge::render;
namespace num = ge::core::num;
namespace libm = ge::core::libm;

constexpr double kDt = 1.0 / 30;  // the frames drawn
constexpr double kPi = 3.14159265358979323846;
constexpr int kSolid = -2, kEmpty = -1, kSlime = -4, kLava = -5;

std::string fixed(double v, int places) { return num::to_fixed(v, places); }

// ---- THE CPU HALF ---------------------------------------------------------------------------

struct Liquid {
  const char* name;
  double viscosity;
  bool crust;
  gl::Rgba base, lit;
};
const Liquid kLiquids[] = {
    {"water", 1, false, {20, 45, 70, 255}, {150, 190, 220, 255}},
    {"slime", 2, false, {30, 70, 15, 255}, {150, 210, 90, 255}},
    {"lava", 4, true, {28, 22, 20, 255}, {120, 100, 90, 255}},
};

constexpr double kWalkMps = 2.5;
constexpr double kFromM = -2.5, kToM = 1.5;  // the walk along x, in a pool 10 m across

// The stamps of a body that walked from kFromM to kToM along z = 0 and stepped out, at `t_s`
// (wade_liquids on a pool of `look` with its level at 1 m and the body's feet 0.6 m under it).
std::vector<gr::WadeStamp> walk(const gr::WadeLook& look, bool crust, double t_s) {
  gr::Liquids liquids;
  gr::Liquids::Slot slot;
  slot.staged.info[0] = 1;
  slot.staged.box[0] = slot.staged.box[1] = -5;
  slot.staged.box[2] = slot.staged.box[3] = 5;
  slot.wade = look;
  slot.staged.wade2[1] = static_cast<float>(gr::wade_waves(look, crust).life_s);
  liquids.bodies.push_back(slot);
  gr::LiquidWading wading;
  const double walk_s = (kToM - kFromM) / kWalkMps;
  for (int i = 0; i * kDt <= t_s + 1e-9; ++i) {
    const double t = i * kDt;
    gr::LiquidWader w;
    w.id = 2;
    w.feet[0] = kFromM + kWalkMps * std::min(t, walk_s);
    w.feet[1] = t <= walk_s ? 0.4 : 2;  // out of it once there
    w.feet[2] = 0;
    gr::wade_liquids(liquids, wading, {&w, 1}, t, true);
  }
  return wading.stamps[0];
}

// The surface from above, `size_m` across round (cx, 0), `px` a side: shaded by its slope under a
// light from the upper left, the torn crust glowing.
gl::Picture surface(const Liquid& q, std::span<const gr::WadeStamp> stamps, double t_s, double cx,
                    double size_m, int px) {
  gr::WadeLook look;
  look.viscosity = q.viscosity;
  gl::Picture p = gl::create_picture(px, px, q.base);
  const double cell = size_m / px;
  std::vector<double> h(size_t(px + 1) * size_t(px + 1)), open(h.size());
  for (int j = 0; j <= px; ++j)
    for (int i = 0; i <= px; ++i) {
      const double x = cx - size_m / 2 + i * cell, z = size_m / 2 - j * cell;
      const gr::WadeAt a = gr::wade_height(stamps, look, q.crust, x, z, t_s);
      h[size_t(j) * size_t(px + 1) + size_t(i)] = a.height_m;
      open[size_t(j) * size_t(px + 1) + size_t(i)] = a.open;
    }
  for (int j = 0; j < px; ++j)
    for (int i = 0; i < px; ++i) {
      const size_t k = size_t(j) * size_t(px + 1) + size_t(i);
      const double gx = (h[k + 1] - h[k]) / cell, gz = (h[k + size_t(px + 1)] - h[k]) / cell;
      // A light from the upper left at 45 degrees: the slope's share toward it, steepened.
      const double shade = std::clamp(0.5 + 6 * (-gx * 0.7 + gz * 0.7), 0.0, 1.0);
      const auto mixc = [&](uint8_t a, uint8_t b) {
        return static_cast<uint8_t>(a + (b - a) * shade);
      };
      gl::Rgba c{mixc(q.base.r, q.lit.r), mixc(q.base.g, q.lit.g), mixc(q.base.b, q.lit.b), 255};
      const double torn = std::clamp((open[k] - 0.2) / 0.35, 0.0, 1.0);
      if (torn > 0) {
        c.r = static_cast<uint8_t>(c.r + (255 - c.r) * torn);
        c.g = static_cast<uint8_t>(c.g + (150 - c.g) * torn);
        c.b = static_cast<uint8_t>(c.b + (30 - c.b) * torn);
      }
      gl::set_pixel(p, i, j, c);
    }
  return p;
}

void cpu_half(gl::Lab& lab) {
  const double walk_s = (kToM - kFromM) / kWalkMps;
  const int px = 200;
  // The surfaces: a row as it walks, a row 1.5 s after it is out.
  gl::Picture sheet =
      gl::create_picture(3 * px + 4 * 8, 2 * px + 3 * 8 + 2 * 14, {16, 16, 18, 255});
  for (int col = 0; col < 3; ++col) {
    const Liquid& q = kLiquids[col];
    gr::WadeLook look;
    look.viscosity = q.viscosity;
    for (int row = 0; row < 2; ++row) {
      const double t = row == 0 ? 0.8 * walk_s : walk_s + 1.5;
      const std::vector<gr::WadeStamp> stamps = walk(look, q.crust, t);
      const gl::Picture s = surface(q, stamps, t, 0, 5, px);
      const int x0 = 8 + col * (px + 8), y0 = 8 + row * (px + 8 + 14) + 14;
      gl::blit(sheet, s, x0, y0);
      gl::draw_text(sheet, x0, y0 - 12,
                    std::string(q.name) + " x" + fixed(q.viscosity, 0) +
                        (row == 0 ? "  walking" : "  1.5 s out"),
                    {230, 230, 230, 255});
    }
  }
  lab.shot({"surfaces",
            "WATER, SLIME AND LAVA WADED THROUGH, FROM ABOVE",
            "render/liquid_wade.hpp's CPU twin: a body walking 4 m along the middle at 2.5 m/s, "
            "5 m of pool across each panel; top row 1.3 s in, bottom 1.5 s after it stepped out. "
            "Shaded by the slope; lava's torn crust orange.",
            "water's rings run far and its furrow is gone; slime's furrow lingers, its rings "
            "shorter; lava's furrow and its torn crust stay open longest, its folds slow and "
            "broad",
            {{"walk", "2.5 m/s"}, {"viscosity", "1, 2, 4"}}},
           sheet);
  // The furrow's depth where it walked through x = -1, against the time since.
  const double t_at = (-1 - kFromM) / kWalkMps;
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"THE FURROW CLOSING BEHIND A WADER",
              "the surface's height on the track, 1 m into the walk, as the body walks on",
              "t since it passed (s)", "height (cm)"};
  gl::LabTable table{
      "what each viscosity makes of the surface (render/liquid_wade.hpp)",
      {"liquid", "viscosity", "ring speed (m/s)", "wavelength (m)", "ring life (s)",
       "furrow closes (s)", "crust heals (s)", "furrow depth (cm)", "depth after 1 s"},
      {}};
  for (const Liquid& q : kLiquids) {
    gr::WadeLook look;
    look.viscosity = q.viscosity;
    const std::vector<gr::WadeStamp> stamps = walk(look, q.crust, walk_s + 4);
    gl::Trace tr{std::string(q.name) + " x" + fixed(q.viscosity, 0), {}, {}};
    double after1 = 0;
    for (int i = 0; i <= 120; ++i) {
      const double dt = i * 0.05;
      const double h = gr::wade_height(stamps, look, q.crust, -1, 0, t_at + dt, false).height_m;
      tr.x.push_back(dt);
      tr.y.push_back(h * 100);
      if (i == 20) after1 = h;
    }
    ch.traces.push_back(tr);
    const gr::WadeWaves w = gr::wade_waves(look, q.crust);
    table.rows.push_back({q.name, fixed(q.viscosity, 0), fixed(w.speed_mps, 2),
                          fixed(w.wavelength_m, 2), fixed(w.ring_s, 2), fixed(w.furrow_s, 2),
                          q.crust ? fixed(w.heal_s, 1) : "-", fixed(w.depth_m * 100, 1),
                          fixed(after1 * 100, 2) + " cm"});
    lab.metric(std::string("furrow.") + q.name + ".after-1s", after1 * 100, "cm");
    lab.metric(std::string("rings.") + q.name + ".reach", w.speed_mps * w.ring_s, "m");
  }
  lab.table(table);
  lab.shot({"furrow-closing",
            ch.frame.title,
            "The height of the surface on the body's track 1 m into its walk, from the moment it "
            "passed (the bow over it first, then the trough), for each viscosity.",
            "water fills back within a second; slime in about two; lava stays down three or more",
            {{"walk", "2.5 m/s"}}},
           gl::to_picture(gl::line_chart(ch).plot));
}

// ---- ON A REAL MAP --------------------------------------------------------------------------

int contents(const br::Map& m, float x, float y, float z) {
  const float q[3] = {x, y, z};
  return br::hull_contents(m, q, 0, 0);
}

// THE WALK: the longest straight line through the map's liquid `c` a body can wade along waist
// deep (feet 24 units under the surface, the liquid there all along and 40 units either side, air
// 24 units over it), up to
// 320 units long, searched round the points of its pools on a 32-unit lattice in eight ways. `a`
// and `b` its ends (z the surface). False when there is none 96 units long.
bool find_walk(const br::Map& m, int c, float a[3], float b[3]) {
  float best = 0;
  for (const br::Leaf& leaf : m.leaves) {
    if (leaf.contents != c) continue;
    const int nx = static_cast<int>((leaf.maxs[0] - leaf.mins[0]) / 32);
    const int ny = static_cast<int>((leaf.maxs[1] - leaf.mins[1]) / 32);
    for (int i = 0; i < nx; ++i)
      for (int j = 0; j < ny; ++j) {
        const float x = leaf.mins[0] + 16 + 32.f * static_cast<float>(i);
        const float y = leaf.mins[1] + 16 + 32.f * static_cast<float>(j);
        // The surface over this point of the leaf.
        float z = leaf.maxs[2] - 1;
        if (contents(m, x, y, z) != c) {
          z = leaf.mins[2] + 1;
          if (contents(m, x, y, z) != c) continue;
        }
        while (contents(m, x, y, z + 2) == c) z += 2;
        if (contents(m, x, y, z + 2) != kEmpty) continue;
        for (int k = 0; k < 8; ++k) {
          const double ang = k * kPi / 4;
          const float dx = static_cast<float>(libm::cos(ang)),
                      dy = static_cast<float>(libm::sin(ang));
          float len = 0;
          for (int step = 1; step <= 40; ++step) {
            const float s = 8.f * static_cast<float>(step);
            const float px = x + dx * s, py = y + dy * s;
            // Liquid a body's width and more either side, so the furrow lies in open liquid.
            const float sx = -dy * 40, sy = dx * 40;
            if (contents(m, px, py, z - 22) != c || contents(m, px, py, z - 2) != c ||
                contents(m, px, py, z + 24) != kEmpty ||
                contents(m, px + sx, py + sy, z - 2) != c ||
                contents(m, px - sx, py - sy, z - 2) != c)
              break;
            len = s;
          }
          if (len > best) {
            best = len;
            a[0] = x, a[1] = y, a[2] = z;
            b[0] = x + dx * len, b[1] = y + dy * len, b[2] = z;
          }
        }
      }
  }
  return best >= 96;
}

// Whether the segment from `a` to `b` (units) passes no solid.
bool clear(const br::Map& m, const float a[3], const float b[3]) {
  const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
  const int steps = std::max(1, static_cast<int>(std::sqrt(dx * dx + dy * dy + dz * dz) / 8));
  for (int i = 0; i <= steps; i++) {
    const float f = static_cast<float>(i) / static_cast<float>(steps);
    if (contents(m, a[0] + dx * f, a[1] + dy * f, a[2] + dz * f) == kSolid) return false;
  }
  return true;
}

// AN EYE on the walk's middle `target`: `lift` units over it, `near` to `far` units off, beside
// the walk rather than along it, with clear lines to both its ends and the middle. With `margin`
// units of room round it, the eye that sees the most of the liquid round `target` instead.
struct Eye {
  float at[3] = {0, 0, 0};
  float yaw = 0, pitch = 0;
};
bool find_eye(const br::Map& m, const float target[3], const float a[3], const float b[3], int near,
              int far, float lift, Eye& out, int margin = 0) {
  const double along = libm::atan2(b[1] - a[1], b[0] - a[0]);
  int best = -1;
  // Beside the walk first, then round it; the nearer the better.
  for (const bool both_ends : {true, false})
    for (int d = near; d <= far; d += 16)
      for (int k = 0; k < 16; ++k) {
        const double off = (k % 2 == 0 ? 1 : -1) * (0.5 - 0.03125 * static_cast<double>(k - k % 2));
        const double ang = along + off * kPi;
        for (const float rise : {lift, lift * 0.6f, lift * 1.5f}) {
          const float e[3] = {target[0] + static_cast<float>(d * libm::cos(ang)),
                              target[1] + static_cast<float>(d * libm::sin(ang)), target[2] + rise};
          if (contents(m, e[0], e[1], e[2]) == kSolid) continue;
          // Room round the eye: no wall within `margin` units either way (an eye against a wall
          // sees half its picture black).
          bool room = true;
          for (int ax = 0; ax < 3 && margin > 0; ++ax)
            for (const int sg : {-1, 1}) {
              float q[3] = {e[0], e[1], e[2]};
              q[ax] += static_cast<float>(sg * margin);
              if (!clear(m, e, q)) room = false;
            }
          if (!room) continue;
          if (!clear(m, e, target)) continue;
          if (both_ends && (!clear(m, e, a) || !clear(m, e, b))) continue;
          // With a margin, every eye is weighed by how much of the liquid round the target it
          // sees (a grid of points 16 units under the level, 160 units either way), the best kept.
          int score = 0;
          if (margin > 0)
            for (int gx = -160; gx <= 160; gx += 32)
              for (int gy = -160; gy <= 160; gy += 32) {
                const float g[3] = {target[0] + static_cast<float>(gx),
                                    target[1] + static_cast<float>(gy), target[2] - 16};
                const float over[3] = {g[0], g[1], target[2] + 4};
                if (contents(m, g[0], g[1], g[2]) < kSolid && clear(m, e, over)) score++;
              }
          if (margin > 0 && score <= best) continue;
          best = score;
          for (int i = 0; i < 3; i++) out.at[i] = e[i];
          out.yaw = static_cast<float>(libm::atan2(target[1] - e[1], target[0] - e[0]) * 180 / kPi);
          const float dx = target[0] - e[0], dy = target[1] - e[1];
          out.pitch = static_cast<float>(
              libm::atan2(e[2] - target[2], std::sqrt(dx * dx + dy * dy)) * 180 / kPi);
          if (margin == 0) return true;
        }
      }
  return best >= 0;
}

double median(std::vector<double> v) {
  if (v.empty()) return std::nan("");
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

// The share of pixels that differ by more than 2 in any channel.
double changed_share(const gl::Picture& a, const gl::Picture& b) {
  size_t n = 0;
  const size_t px = static_cast<size_t>(a.width) * static_cast<size_t>(a.height);
  for (size_t i = 0; i < px; i++)
    for (int k = 0; k < 3; k++)
      if (std::abs(int(a.rgba[i * 4 + k]) - int(b.rgba[i * 4 + k])) > 2) {
        n++;
        break;
      }
  return px ? double(n) / double(px) : 0;
}

struct Cost {
  std::string what;
  double liquid = 0, span = 0;
};

void run_pool(gl::Lab& lab, const cl::SearchPath& files, const std::string& map_name, int kind,
              std::vector<Cost>& costs) {
  const char* name = kind == kLava ? "lava" : "slime";
  std::string error;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-wade: the baseline mod: " + error, "Read the reason above. Skill: progs.");
    return;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-wade: " + error,
             std::string("Pass --") + name + "=<a map of the folder>. Skill: shooter.");
    return;
  }
  float a[3], b[3];
  if (!find_walk(map->bsp.map, kind, a, b)) {
    lab.fail("se-wade: " + map_name + " has no " + name + " pool a body wades 96 units through",
             std::string("Pass --") + name + "=<a map with one>. Skill: shooter.");
    return;
  }
  // Looked at three quarters along: the body near its end and the track behind it in view.
  const float mid[3] = {a[0] + (b[0] - a[0]) * 0.7f, a[1] + (b[1] - a[1]) * 0.7f, a[2]};
  Eye eye;
  if (!find_eye(map->bsp.map, mid, a, b, 128, 480, 80, eye)) {
    lab.fail("se-wade: " + map_name + "'s " + name + " pool has no eye in the clear over it",
             "Pass another map. Skill: shooter.");
    return;
  }
  lab.note(std::string(name) + " on " + map_name + ": the walk " + fixed(a[0], 0) + "," +
           fixed(a[1], 0) + " to " + fixed(b[0], 0) + "," + fixed(b[1], 0) + " at " +
           fixed(a[2], 0) + "; the eye " + fixed(eye.at[0], 0) + "," + fixed(eye.at[1], 0) + "," +
           fixed(eye.at[2], 0));
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), lo, error)) {
    lab.fail("se-wade: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
  const int player_model = l.server->sv.edicts[1].modelindex;

  auto device = ge::gpu::create_device({.label = "se-wade", .validation = 1});
  if (!device) {
    lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
    return;
  }
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width")),
                 h = static_cast<uint32_t>(lab.arg_num("height"));
  shooters_edge::look::Picture look;
  ge::console::Console con;
  shooters_edge::look::add_cvars(con, look);
  con.execute(lab.cvars());
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {},
                            error, {l.server->models, {}});
  if (!world) {
    lab.fail("se-wade: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-wade.final", w, h);
  cl::ViewState shown;
  gr::FrameView view = cl::frame_view(l, shown, 0, 0, 0);
  const ge::camera::Vec3 e = cl::engine_point({eye.at[0], eye.at[1], eye.at[2]});
  view.eye[0] = e.x, view.eye[1] = e.y, view.eye[2] = e.z;
  view.heading = cl::engine_heading(eye.yaw);
  view.pitch = cl::engine_pitch(eye.pitch);
  cl::FxFrame fx;
  double t = 10;
  auto draw = [&](cl::PassTimes* times) {
    view.time_s = t;
    cl::draw_world(*device, *world, view, fx, nullptr, kDt, {}, out.view);
    ge::gpu::wait(*device, device->queue.OnSubmittedWorkDone(
                               wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {}));
    if (times != nullptr) cl::record_span(*world, *times);
    t += kDt;
  };
  auto grab = [&]() {
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
  };
  const std::vector<std::pair<std::string, std::string>> inputs = {{"map", map_name},
                                                                   {"liquid", name}};
  // The walk: a player at a run's pace in it (3 m/s) from one end to the other, then stood still
  // there for as long again; his feet 24 units under the surface (waist deep).
  const double len = std::sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
  const double speed_ups = 3 / 0.025;
  const double walk_s = len / speed_ups, total_s = walk_s + std::max(2.0, walk_s);
  const float yaw = static_cast<float>(libm::atan2(b[1] - a[1], b[0] - a[0]) * 180 / kPi);
  cl::Placed body;
  body.entity = 2;
  body.modelindex = player_model;
  body.angles = {0, yaw, 0};
  body.top = 4;
  body.bottom = 12;
  auto place = [&](double rt) {
    const double f = std::min(rt / walk_s, 1.0);
    body.origin = {static_cast<float>(a[0] + (b[0] - a[0]) * f),
                   static_cast<float>(a[1] + (b[1] - a[1]) * f), a[2]};
    // The run cycle (frames 6..11) while he walks, standing (12) once there.
    body.frame = rt < walk_s ? 6 + static_cast<int>(rt / 0.1) % 6 : 12;
    fx.models = {body};
  };
  const int moments = std::max(2, static_cast<int>(lab.arg_num("moments")));
  std::vector<double> at_s;
  at_s.reserve(static_cast<size_t>(moments));
  for (int k = 0; k < moments; k++)
    at_s.push_back(walk_s * 0.6 + (total_s - walk_s * 0.6) * k / std::max(1, moments - 1) * 0.9);
  const int frames = static_cast<int>(total_s / kDt);
  // Each run from the same start: the light settled, then the walk, the pictures at the moments.
  auto run = [&](bool on, std::vector<gl::Picture>* shots, std::vector<gl::Picture>* film,
                 cl::PassTimes* times) {
    con.execute(on ? "ce_wade 1" : "ce_wade 0");
    t = 10;
    fx.models.clear();
    for (int i = 0; i < 20; i++) draw(nullptr);  // a jump back clears the tracks; the light settles
    size_t next = 0;
    for (int i = 0; i < frames; i++) {
      const double rt = (i + 1) * kDt;
      place(rt);
      draw(times);
      const bool moment = next < at_s.size() && rt + 1e-9 >= at_s[next];
      if (film != nullptr && i % 3 == 0) film->push_back(gl::shrink(grab(), 360));
      if (moment) {
        if (shots != nullptr) shots->push_back(grab());
        next++;
      }
    }
  };
  std::vector<gl::Picture> still, waded, film;
  cl::PassTimes off_t, on_t;
  run(false, &still, nullptr, &off_t);
  run(true, &waded, &film, &on_t);
  for (size_t k = 0; k < waded.size() && k < still.size(); ++k) {
    const double rt = at_s[k];
    const std::string when = rt < walk_s ? fixed(rt, 1) + " s into his walk"
                                         : fixed(rt - walk_s, 1) + " s after he stopped";
    const double share = changed_share(still[k], waded[k]);
    lab.metric(std::string(name) + ".changed-share." + std::to_string(k), share, "ratio",
               gl::Det::measured);
    const std::string where = std::string(name).append(" on ").append(map_name);
    lab.shot({std::string(name) + "-" + std::to_string(k) + "-still",
              std::string(where).append(", ce_wade 0: ").append(when),
              std::string("A player walking waist deep through the ")
                  .append(name)
                  .append(" at 3 m/s, then standing; ce_wade 0, the surface still round him."),
              "the surface as if nobody were in it", inputs},
             still[k], gl::Det::measured);
    lab.shot({std::string(name) + "-" + std::to_string(k) + "-waded",
              std::string(where).append(", ce_wade 1: ").append(when),
              std::string("The same moment with ce_wade 1: the furrow behind him, the bow ahead, "
                          "the rings (changes ")
                  .append(fixed(share * 100, 1))
                  .append("% of the picture)."),
              kind == kLava ? "the crust torn open behind him, glowing, closing slowly over "
                              "seconds; slow broad folds round his legs"
                            : "a furrow parting the slime behind him, its lips raised, closing "
                              "over a second or two; short rings",
              inputs},
             waded[k], gl::Det::measured);
  }
  // What the wading changed: the two pictures' difference, four times over, on black.
  if (!waded.empty() && !still.empty()) {
    const size_t k = waded.size() / 2;
    gl::Picture d = waded[k];
    for (size_t i = 0; i + 3 < d.rgba.size(); i += 4)
      for (size_t c = 0; c < 3; ++c)
        d.rgba[i + c] = static_cast<uint8_t>(
            std::min(255, 4 * std::abs(int(waded[k].rgba[i + c]) - int(still[k].rgba[i + c]))));
    lab.shot({std::string(name) + "-difference",
              std::string(name) + " on " + map_name + ": what the wading changed",
              "The difference between ce_wade 1 and 0 at the middle moment, four times over: "
              "every pixel the furrow, the bow and the rings moved.",
              "a track behind the body, rings round it; black elsewhere", inputs},
             d, gl::Det::measured);
  }
  lab.attach(std::string(name) + "-film.gif", gl::encode_gif_frames(film, 10), gl::Det::measured);
  for (const auto& [what, p] :
       {std::pair<const char*, cl::PassTimes*>{"wade-off", &off_t}, {"wade-on", &on_t}}) {
    Cost c;
    c.what = std::string(name) + ": " + what;
    c.span = median(p->span_ms);
    for (const auto& [pass, ms] : p->pass_ms)
      if (pass == "liquid") c.liquid = median(ms);
    lab.metric(std::string(name) + "." + what + ".liquid", c.liquid, "ms", gl::Det::measured);
    lab.metric(std::string(name) + "." + what + ".span", c.span, "ms", gl::Det::measured);
    costs.push_back(c);
  }
}

// A BALL OF LAVA hurled out of a map's fire spout (its first `misc_fireball`: straight up at its
// `speed`, 1000 units a second when it has none, under the classic gravity of 800) and falling
// back: drawn with ce_wade off and on at moments round its leaving and its coming down.
void run_fireball(gl::Lab& lab, const cl::SearchPath& files, const std::string& map_name,
                  std::vector<Cost>& costs) {
  std::string error;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-wade: the baseline mod: " + error, "Read the reason above. Skill: progs.");
    return;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-wade: " + error, "Pass --fireball=<a map of the folder>. Skill: shooter.");
    return;
  }
  const br::Map& m = map->bsp.map;
  float spout[3] = {0, 0, 0};
  double speed = 1000;
  bool found = false;
  for (const br::Entity& e : m.entities) {
    if (e.get("classname") != "misc_fireball") continue;
    if (std::sscanf(e.get("origin").c_str(), "%f %f %f", &spout[0], &spout[1], &spout[2]) != 3)
      continue;
    if (!e.get("speed").empty()) speed = std::atof(e.get("speed").c_str());
    found = true;
    break;
  }
  // The lava's surface over the spout (or under it, a spout over the lava).
  float level = spout[2];
  bool lava = false;
  if (found) {
    for (int dz = -128; dz <= 128 && !lava; dz += 2)
      if (contents(m, spout[0], spout[1], spout[2] + static_cast<float>(dz)) == kLava &&
          contents(m, spout[0], spout[1], spout[2] + static_cast<float>(dz + 2)) != kLava) {
        level = spout[2] + static_cast<float>(dz) + 2;
        lava = true;
      }
  }
  if (!lava) {
    lab.fail("se-wade: " + map_name + " has no fire spout in lava (misc_fireball)",
             "Pass --fireball=<a map with one>. Skill: shooter.");
    return;
  }
  const float target[3] = {spout[0], spout[1], level};
  Eye eye;
  if (!find_eye(m, target, target, target, 160, 480, 140, eye, 48)) {
    lab.fail("se-wade: " + map_name + "'s fire spout has no eye in the clear over it",
             "Pass another map. Skill: shooter.");
    return;
  }
  lab.note("fireball on " + map_name + ": the spout " + fixed(spout[0], 0) + "," +
           fixed(spout[1], 0) + "," + fixed(spout[2], 0) + ", the lava at " + fixed(level, 0) +
           ", speed " + fixed(speed, 0) + "; the eye " + fixed(eye.at[0], 0) + "," +
           fixed(eye.at[1], 0) + "," + fixed(eye.at[2], 0));
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), lo, error)) {
    lab.fail("se-wade: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
  std::vector<std::string> names = l.server->models;
  const int ball_model = static_cast<int>(names.size());
  names.emplace_back("progs/lavaball.mdl");
  auto device = ge::gpu::create_device({.label = "se-wade", .validation = 1});
  if (!device) {
    lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
    return;
  }
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width")),
                 h = static_cast<uint32_t>(lab.arg_num("height"));
  shooters_edge::look::Picture look;
  ge::console::Console con;
  shooters_edge::look::add_cvars(con, look);
  con.execute(lab.cvars());
  cl::WorldDrawPtr world = cl::create_world_draw(
      *device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {}, error, {names, {}});
  if (!world) {
    lab.fail("se-wade: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-wade.final", w, h);
  cl::ViewState shown;
  gr::FrameView view = cl::frame_view(l, shown, 0, 0, 0);
  const ge::camera::Vec3 e = cl::engine_point({eye.at[0], eye.at[1], eye.at[2]});
  view.eye[0] = e.x, view.eye[1] = e.y, view.eye[2] = e.z;
  view.heading = cl::engine_heading(eye.yaw);
  view.pitch = cl::engine_pitch(eye.pitch);
  cl::FxFrame fx;
  double t = 10;
  auto draw = [&](cl::PassTimes* times) {
    view.time_s = t;
    cl::draw_world(*device, *world, view, fx, nullptr, kDt, {}, out.view);
    ge::gpu::wait(*device, device->queue.OnSubmittedWorkDone(
                               wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {}));
    if (times != nullptr) cl::record_span(*world, *times);
    t += kDt;
  };
  auto grab = [&]() {
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
  };
  // Its flight: born at the spout (under the level, or a little under it for a spout over it), up
  // at `speed`, drifting 20 units a second; drawn from its birth, so a frame sees it in the lava.
  constexpr double kGravityUps = 800;
  const double z0 = std::min<double>(spout[2], level - 8);
  const double down_s = (speed + std::sqrt(speed * speed + 2 * kGravityUps * (z0 - level))) /
                        kGravityUps;  // back at the level
  const double up_s = (level - z0) / speed;
  const double total_s = down_s + 2.0;
  cl::Placed ball;
  ball.entity = 300;
  ball.modelindex = ball_model;
  auto place = [&](double rt) {
    const double z = z0 + speed * rt - 0.5 * kGravityUps * rt * rt;
    ball.origin = {static_cast<float>(spout[0] + 20 * rt), spout[1], static_cast<float>(z)};
    ball.angles = {static_cast<float>(200 * rt), static_cast<float>(100 * rt), 0};
    // Gone a while after it sinks (the spout's own lasts until it lands on the floor).
    fx.models.clear();
    if (z > level - 96) fx.models.push_back(ball);
  };
  const std::vector<std::pair<double, std::string>> moments = {
      {up_s + 0.1, "0.1 s after it left the lava"},
      {up_s + 0.6, "0.6 s after it left"},
      {down_s + 0.1, "0.1 s after it came down"},
      {down_s + 0.3, "0.3 s after it came down"},
      {down_s + 1.5, "1.5 s after it came down"}};
  const std::vector<std::pair<std::string, std::string>> inputs = {{"map", map_name},
                                                                   {"liquid", "lava"}};
  const int frames = static_cast<int>(total_s / kDt);
  auto run = [&](bool on, std::vector<gl::Picture>* shots, std::vector<gl::Picture>* film,
                 cl::PassTimes* times) {
    con.execute(on ? "ce_wade 1" : "ce_wade 0");
    t = 10;
    fx.models.clear();
    for (int i = 0; i < 20; i++) draw(nullptr);
    size_t next = 0;
    for (int i = 0; i < frames; i++) {
      const double rt = i * kDt;
      place(rt);
      draw(times);
      if (film != nullptr && (i % 2 == 0)) film->push_back(gl::shrink(grab(), 360));
      if (next < moments.size() && rt + 1e-9 >= moments[next].first) {
        if (shots != nullptr) shots->push_back(grab());
        next++;
      }
    }
  };
  std::vector<gl::Picture> still, hit, film;
  cl::PassTimes off_t, on_t;
  run(false, &still, nullptr, &off_t);
  run(true, &hit, &film, &on_t);
  for (size_t k = 0; k < hit.size() && k < still.size(); ++k) {
    const std::string when = moments[k].second;
    const double share = changed_share(still[k], hit[k]);
    lab.metric("fireball.changed-share." + std::to_string(k), share, "ratio", gl::Det::measured);
    lab.shot({"fireball-" + std::to_string(k) + "-still",
              std::string("fireball on ").append(map_name).append(", ce_wade 0: ").append(when),
              "A ball of lava hurled out of the fire spout and falling back; ce_wade 0.",
              "the lava still where the ball leaves and lands", inputs},
             still[k], gl::Det::measured);
    lab.shot({"fireball-" + std::to_string(k) + "-plunged",
              std::string("fireball on ").append(map_name).append(", ce_wade 1: ").append(when),
              std::string("The same moment with ce_wade 1 (changes ")
                  .append(fixed(share * 100, 1))
                  .append("% of the picture)."),
              k < 2 ? "a mound heaved where it left, slow broad rings running out"
                    : "a crater torn through the crust, a few glowing drops thrown a little way "
                      "up, filling back in over seconds",
              inputs},
             hit[k], gl::Det::measured);
  }
  if (hit.size() > 3 && still.size() > 3) {
    for (const size_t k : {size_t{1}, size_t{3}}) {
      gl::Picture d = hit[k];
      for (size_t i = 0; i + 3 < d.rgba.size(); i += 4)
        for (size_t c = 0; c < 3; ++c)
          d.rgba[i + c] = static_cast<uint8_t>(
              std::min(255, 4 * std::abs(int(hit[k].rgba[i + c]) - int(still[k].rgba[i + c]))));
      lab.shot({"fireball-difference-" + std::to_string(k),
                "fireball on " + map_name + ": what the plunge changed, " + moments[k].second,
                "The difference between ce_wade 1 and 0, four times over.",
                "rings round where it left or landed; black elsewhere", inputs},
               d, gl::Det::measured);
    }
  }
  lab.attach("fireball-film.gif", gl::encode_gif_frames(film, 15), gl::Det::measured);
  for (const auto& [what, p] :
       {std::pair<const char*, cl::PassTimes*>{"wade-off", &off_t}, {"wade-on", &on_t}}) {
    Cost c;
    c.what = std::string("fireball: ") + what;
    c.span = median(p->span_ms);
    for (const auto& [pass, ms] : p->pass_ms)
      if (pass == "liquid") c.liquid = median(ms);
    costs.push_back(c);
  }
}

void run(gl::Lab& lab) {
  cpu_half(lab);
  const std::string folder = lab.arg("game");
  if (folder.empty()) return;
  std::string error;
  cl::SearchPath files;
  const size_t cut = folder.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
  const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
  if (!cl::open_game_dir(base, game, files, error)) {
    lab.fail("se-wade: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
    return;
  }
  std::vector<Cost> costs;
  if (!lab.arg("lava").empty()) run_pool(lab, files, lab.arg("lava"), kLava, costs);
  if (!lab.arg("slime").empty()) run_pool(lab, files, lab.arg("slime"), kSlime, costs);
  if (!lab.arg("fireball").empty()) run_fireball(lab, files, lab.arg("fireball"), costs);
  if (costs.empty()) return;
  gl::LabTable table{"the GPU's time a frame through the walk (medians, ms, " + lab.arg("width") +
                         "x" + lab.arg("height") + ")",
                     {"what", "liquid (surfaces)", "whole frame"},
                     {}};
  for (const Cost& c : costs) table.rows.push_back({c.what, fixed(c.liquid, 2), fixed(c.span, 1)});
  lab.table(table, gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-wade",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "lava and slime waded through (render/liquid_wade.hpp: the furrow, the bow, the rings, the "
    "torn crust, by viscosity), drawn by client/world_draw.hpp",
    "render",
    "Does lava and slime part round a player wading through them and close slowly behind him, "
    "lava thicker than slime, and does a ball of lava hurled out heave it and splash coming down? "
    "On real maps' pools, with the liquid pass's GPU cost.",
    {
        {"game", "", "a game folder (<base>/<folder>); empty: the CPU half alone"},
        {"lava", "start", "with --game: a map with a lava pool"},
        {"slime", "e1m1", "with --game: a map with a slime pool"},
        {"fireball", "start", "with --game: a map with a fire spout in lava (misc_fireball)"},
        {"moments", "3", "with --game: pictures through the walk and the stand after"},
        {"width", "800", "with --game: the frame's width, px"},
        {"height", "500", "with --game: the frame's height, px"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
