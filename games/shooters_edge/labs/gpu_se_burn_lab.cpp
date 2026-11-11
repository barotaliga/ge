// LAB `se-burn`: THE AIR OVER LAVA AND SLIME, AND A BODY BURNING IN THEM. On a real map's lava pool
// and a real map's slime pool (`--game=<base>/<folder>`, `--lava`, `--slime`: a map of the folder
// each, for PRIVATE pictures only) the window draws (client/world_draw.hpp, the modern look):
//
// - THE AIR with nobody in it: across the pool from its bank, with ce_heathaze and ce_fumes off,
//   then on (render/liquid.hpp's shimmer and fumes), and the share of the picture they change;
//   the pool again a tenth of a second later (the shimmer wavers, the wisps drift).
// - A BODY IN IT: a player stood waist deep in the pool for `--inside` seconds, then on its bank
// for
//   `--after` seconds, drawn as the window draws another player (client/burning.hpp, ce_burning):
//   in lava the body smokes, chars and burns in small flames, the crust breaks round it and a
//   plume of hot air and smoke rises over it; in slime fumes rise off it and its skin is etched;
//   once out it wears off over a few seconds. Pictures at `--moments` through it, and a film (an
//   animated GIF) of the whole run.
// - THE COST: the GPU time of the passes it reaches (`liquid`: the surfaces; `liquid.air`: the
//   heat haze and the gas in one box; `particles`: the smoke and the flames) as medians over
//   `--frames` frames each, with the air off, on, the haze alone, the gas alone, and a body burning
//   in it; a table.
//
// Without `--game` (the smoke run) it is the CPU half alone: a body's scorch (render/body_burn.hpp)
// stepped through the same run, waist deep in lava and then in slime, its levels at each moment
// and the births it throws, a table and a chart (deterministic).
//
// ge:summary Lab: lava's heat haze, slime's fumes and a player burning in them on real maps
// ge:keywords lab se-burn heat-haze shimmer fumes smoke burning charred scorch lava slime acid
// ge:keywords ce_heathaze ce_fumes ce_burning liquid-cost body-in-lava
// ge:skill render shooter labs
// ge:test tests/render_body_burn_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "render/body_burn.hpp"
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
namespace sh = ge::shooter;
namespace gr = ge::render;
namespace num = ge::core::num;

constexpr double kDt = 1.0 / 30;  // the frames drawn
constexpr int kSolid = -2, kEmpty = -1, kSlime = -4, kLava = -5;

std::string fixed(double v, int places) { return num::to_fixed(v, places); }

int contents(const br::Map& m, float x, float y, float z) {
  const float q[3] = {x, y, z};
  return br::hull_contents(m, q, 0, 0);
}

// THE POOL: the point of the map's liquid `c` with the most of it round it, a point under its
// surface with air within 24 units over it (a body waist deep), on a 16-unit lattice through every
// leaf of it. `at` its surface's point (z the surface, found to 2 units). False when there is none.
bool find_pool(const br::Map& m, int c, float at[3]) {
  int best = -1;
  for (const br::Leaf& leaf : m.leaves) {
    if (leaf.contents != c) continue;
    const int nx = static_cast<int>((leaf.maxs[0] - leaf.mins[0]) / 16);
    const int ny = static_cast<int>((leaf.maxs[1] - leaf.mins[1]) / 16);
    const int nz = static_cast<int>((leaf.maxs[2] - leaf.mins[2]) / 16);
    for (int i = 0; i < nx; i++)
      for (int j = 0; j < ny; j++)
        for (int k = 0; k < nz; k++) {
          const float x = leaf.mins[0] + 8 + 16.f * static_cast<float>(i);
          const float y = leaf.mins[1] + 8 + 16.f * static_cast<float>(j);
          const float z = leaf.mins[2] + 8 + 16.f * static_cast<float>(k);
          if (contents(m, x, y, z) != c || contents(m, x, y, z + 24) != kEmpty) continue;
          int n = 0;
          for (int d = 16; d <= 192; d += 16) {
            const float s = static_cast<float>(d);
            n += int(contents(m, x + s, y, z) == c) + int(contents(m, x - s, y, z) == c) +
                 int(contents(m, x, y + s, z) == c) + int(contents(m, x, y - s, z) == c);
          }
          if (n > best) {
            best = n;
            at[0] = x, at[1] = y, at[2] = z;
          }
        }
  }
  if (best < 0) return false;
  while (contents(m, at[0], at[1], at[2] + 2) == c) at[2] += 2;
  at[2] += 1;
  return true;
}

// Whether the segment from `a` to `b` (units) passes no solid.
bool clear(const br::Map& m, const float a[3], const float b[3]) {
  const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
  const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
  const int steps = std::max(1, static_cast<int>(len / 8));
  for (int i = 0; i <= steps; i++) {
    const float f = static_cast<float>(i) / static_cast<float>(steps);
    if (contents(m, a[0] + dx * f, a[1] + dy * f, a[2] + dz * f) == kSolid) return false;
  }
  return true;
}

// THE BANK: the nearest dry floor at the pool's level or over it, along eight ways from `at`; a
// player's origin standing on it (24 units over the floor). False when none is within 320 units.
bool find_bank(const br::Map& m, const float at[3], float bank[3]) {
  for (int d = 48; d <= 320; d += 16)
    for (int k = 0; k < 8; k++) {
      const double a = k * 3.14159265358979 / 4;
      const float x = at[0] + static_cast<float>(d * ge::core::libm::cos(a));
      const float y = at[1] + static_cast<float>(d * ge::core::libm::sin(a));
      for (const float rise : {48.f, 96.f, 144.f}) {
        float z = at[2] + rise;
        if (contents(m, x, y, z) != kEmpty) continue;
        while (z > at[2] - 4 && contents(m, x, y, z - 2) == kEmpty) z -= 2;
        if (contents(m, x, y, z - 2) != kSolid || z < at[2]) continue;
        const float stand[3] = {x, y, z + 24}, over[3] = {at[0], at[1], at[2] + 24};
        if (contents(m, x, y, z + 50) != kEmpty || !clear(m, stand, over)) continue;
        bank[0] = x, bank[1] = y, bank[2] = z + 24;
        return true;
      }
    }
  return false;
}

// AN EYE on `target` (units): `lift` units over it, `near` to `far` units off round it, the first
// spot with clear lines to the target and a body's width either side of it; the way it looks
// (yaw, pitch, degrees) at the target.
struct Eye {
  float at[3] = {0, 0, 0};
  float yaw = 0, pitch = 0;
};
bool find_eye(const br::Map& m, const float target[3], int near, int far, float lift, Eye& out) {
  for (int d = near; d <= far; d += 16)
    for (int k = 0; k < 16; k++) {
      const double a = k * 3.14159265358979 / 8;
      const float c = static_cast<float>(ge::core::libm::cos(a)),
                  s = static_cast<float>(ge::core::libm::sin(a));
      const float e[3] = {target[0] + static_cast<float>(d) * c,
                          target[1] + static_cast<float>(d) * s, target[2] + lift};
      if (contents(m, e[0], e[1], e[2]) == kSolid) continue;
      bool ok = true;
      for (const float side : {-24.f, 0.f, 24.f}) {
        const float q[3] = {target[0] - s * side, target[1] + c * side, target[2]};
        ok = ok && clear(m, e, q);
      }
      if (!ok) continue;
      for (int i = 0; i < 3; i++) out.at[i] = e[i];
      out.yaw = static_cast<float>(ge::core::libm::atan2(target[1] - e[1], target[0] - e[0]) * 180 /
                                   3.14159265358979);
      const float dx = target[0] - e[0], dy = target[1] - e[1];
      out.pitch =
          static_cast<float>(ge::core::libm::atan2(e[2] - target[2], std::sqrt(dx * dx + dy * dy)) *
                             180 / 3.14159265358979);
      return true;
    }
  return false;
}

// THE CPU HALF: a body's scorch through the run (in for `in_s`, out for `out_s`) in `hot` and
// `acid`, a row every `every` s.
struct Step {
  double t = 0;
  float heat = 0, charred = 0, acid = 0, etched = 0;
  size_t flames = 0, smoke = 0;
};
std::vector<Step> scorch_run(double hot, double acid, double in_s, double out_s) {
  std::vector<Step> rows;
  gr::BodyBurn s = gr::make_body_burn(2u * 2654435761u);
  gr::BurnShape body;
  body.level_y = body.feet[1] + 0.7;
  std::vector<gr::Particle> out;
  const int frames = static_cast<int>((in_s + out_s) / kDt);
  for (int i = 0; i < frames; i++) {
    const double t = (i + 1) * kDt;
    const bool in = t <= in_s;
    gr::step_body_burn(s, gr::body_burn_look(), in ? hot : 0, in ? acid : 0, kDt);
    out.clear();
    gr::body_burn_births(s, gr::body_burn_look(), body, kDt, out);
    Step r;
    r.t = t;
    r.heat = s.heat, r.charred = s.charred, r.acid = s.acid, r.etched = s.etched;
    for (const gr::Particle& p : out) (p.kelvin > 0 ? r.flames : r.smoke)++;
    rows.push_back(r);
  }
  return rows;
}

void cpu_half(gl::Lab& lab, double in_s, double out_s) {
  for (const auto& [name, hot, acid] :
       {std::tuple{"lava", 2.0 / 3, 0.0}, std::tuple{"slime", 0.0, 2.0 / 3}}) {
    const std::vector<Step> rows = scorch_run(hot, acid, in_s, out_s);
    gl::LabTable table{
        std::string("a body waist deep in ") + name + " for " + fixed(in_s, 1) + " s, then out",
        {"t (s)", "where", "heat", "charred", "acid", "etched", "flames", "smoke"},
        {}};
    size_t births = 0;
    double gone = -1;
    for (size_t i = 0; i < rows.size(); i++) {
      const Step& r = rows[i];
      births += r.flames + r.smoke;
      if (gone < 0 && r.t > in_s && r.heat == 0 && r.charred == 0 && r.acid == 0 && r.etched == 0)
        gone = r.t - in_s;
      if ((i + 1) % 15 != 0) continue;
      table.rows.push_back({fixed(r.t, 2), r.t <= in_s ? "in" : "out", fixed(r.heat, 2),
                            fixed(r.charred, 2), fixed(r.acid, 2), fixed(r.etched, 2),
                            std::to_string(r.flames), std::to_string(r.smoke)});
    }
    lab.table(table);
    const std::string key = std::string("scorch.") + name;
    lab.metric(key + ".births", static_cast<double>(births), "count");
    lab.metric(key + ".gone-after", gone, "s");
    gl::LineChart ch;
    ch.spikes = false;
    ch.frame = {std::string("A BODY IN ") + (name[0] == 'l' ? "LAVA" : "SLIME") + ", THEN OUT",
                "waist deep for " + fixed(in_s, 1) + " s, then on the bank (render/body_burn.hpp)",
                "t (s)", "level 0..1"};
    gl::Trace a{name[0] == 'l' ? "heat" : "acid", {}, {}};
    gl::Trace b{name[0] == 'l' ? "charred" : "etched", {}, {}};
    for (const Step& r : rows) {
      a.x.push_back(r.t);
      a.y.push_back(name[0] == 'l' ? r.heat : r.acid);
      b.x.push_back(r.t);
      b.y.push_back(name[0] == 'l' ? r.charred : r.etched);
    }
    ch.traces = {a, b};
    lab.shot({std::string("levels-") + name,
              ch.frame.title,
              "The body's levels through the run, a frame at a time: up while it is in, worn off "
              "once it is out.",
              "up within a second in; back to nothing a few seconds after it is out",
              {{"inside", fixed(in_s, 1)}, {"after", fixed(out_s, 1)}}},
             gl::to_picture(gl::line_chart(ch).plot));
  }
}

// The mean luminance (0..255) of the middle of a picture: a box a fifth across and a third down.
double middle_luma(const gl::Picture& p, double cx, double cy) {
  const int x0 = static_cast<int>((cx - 0.1) * p.width),
            x1 = static_cast<int>((cx + 0.1) * p.width);
  const int y0 = static_cast<int>((cy - 0.17) * p.height),
            y1 = static_cast<int>((cy + 0.17) * p.height);
  double sum = 0;
  int n = 0;
  for (int y = std::max(y0, 0); y < std::min(y1, p.height); y++)
    for (int x = std::max(x0, 0); x < std::min(x1, p.width); x++) {
      const uint8_t* c = &p.rgba[(static_cast<size_t>(y) * p.width + x) * 4];
      sum += 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
      n++;
    }
  return n ? sum / n : 0;
}

// The share of pixels that differ by more than 2 in any channel.
double changed_share(const gl::Picture& a, const gl::Picture& b) {
  size_t n = 0;
  const size_t px = static_cast<size_t>(a.width) * a.height;
  for (size_t i = 0; i < px; i++)
    for (int k = 0; k < 3; k++)
      if (std::abs(int(a.rgba[i * 4 + k]) - int(b.rgba[i * 4 + k])) > 2) {
        n++;
        break;
      }
  return px ? double(n) / double(px) : 0;
}

double median(std::vector<double> v) {
  if (v.empty()) return std::nan("");
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

// One pool: its air, a body in it and out, the cost.
struct Costs {
  std::string what;
  double liquid = 0, air = 0, particles = 0, span = 0;
};

void run_pool(gl::Lab& lab, const cl::SearchPath& files, const std::string& map_name, int kind,
              std::vector<Costs>& costs) {
  const char* name = kind == kLava ? "lava" : "slime";
  std::string error;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-burn: the baseline mod: " + error, "Read the reason above. Skill: progs.");
    return;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-burn: " + error,
             std::string("Pass --") + name + "=<a map of the folder>. Skill: shooter.");
    return;
  }
  float pool[3], bank[3];
  if (!find_pool(map->bsp.map, kind, pool)) {
    lab.fail("se-burn: " + map_name + " has no " + name + " pool a body stands waist deep in",
             std::string("Pass --") + name + "=<a map with one>. Skill: shooter.");
    return;
  }
  // The eyes: the pool's air from 6 to 9 m off, the body in it and on its bank from 2 to 5 m.
  Eye air_eye, in_eye, out_eye;
  const float chest_in[3] = {pool[0], pool[1], pool[2] + 8};
  if (!find_bank(map->bsp.map, pool, bank) ||
      !find_eye(map->bsp.map, pool, 224, 352, 64, air_eye) ||
      !find_eye(map->bsp.map, chest_in, 80, 208, 24, in_eye)) {
    lab.fail("se-burn: " + map_name + "'s " + name + " pool has no bank and eye in the clear",
             "Pass another map. Skill: shooter.");
    return;
  }
  const float chest_out[3] = {bank[0], bank[1], bank[2] + 8};
  if (!find_eye(map->bsp.map, chest_out, 80, 208, 24, out_eye)) out_eye = in_eye;
  lab.note(std::string(name) + " on " + map_name + ": pool " + fixed(pool[0], 0) + "," +
           fixed(pool[1], 0) + "," + fixed(pool[2], 0) + "; bank " + fixed(bank[0], 0) + "," +
           fixed(bank[1], 0) + "," + fixed(bank[2], 0));
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), lo, error)) {
    lab.fail("se-burn: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
  const int player_model = l.server->sv.edicts[1].modelindex;

  auto device = ge::gpu::create_device({.label = "se-burn", .validation = 1});
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
    lab.fail("se-burn: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-burn.final", w, h);
  cl::ViewState shown;
  gr::FrameView view = cl::frame_view(l, shown, 0, 0, 0);
  auto look_from = [&](const Eye& eye) {
    const ge::camera::Vec3 e = cl::engine_point({eye.at[0], eye.at[1], eye.at[2]});
    view.eye[0] = e.x, view.eye[1] = e.y, view.eye[2] = e.z;
    view.heading = cl::engine_heading(eye.yaw);
    view.pitch = cl::engine_pitch(eye.pitch);
  };
  look_from(air_eye);
  cl::FxFrame fx;
  cl::Placed body;
  body.entity = 2;
  body.modelindex = player_model;
  body.frame = 12;                         // standing
  body.angles = {0, in_eye.yaw + 180, 0};  // facing the eye
  body.top = 4;
  body.bottom = 12;
  const sh::Vec3f in_at{pool[0], pool[1], pool[2] - 6}, out_at{bank[0], bank[1], bank[2]};
  double t = 10;
  const int air_draws = std::max(1, static_cast<int>(lab.arg_num("air-draws")));
  auto draw = [&](bool timed, cl::PassTimes* times) {
    view.time_s = t;
    cl::set_air_draws(*world, timed ? air_draws : 1);  // the pictures draw it once
    cl::draw_world(*device, *world, view, fx, nullptr, kDt, {}, out.view);
    // The frame done before the next (its timestamps read back as it lands).
    ge::gpu::wait(*device, device->queue.OnSubmittedWorkDone(
                               wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {}));
    if (timed && times != nullptr) cl::record_span(*world, *times);
    t += kDt;
  };
  auto grab = [&]() {
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
  };
  const int frames = std::max(4, static_cast<int>(lab.arg_num("frames")));
  auto cost = [&](const std::string& what, const cl::PassTimes& p) {
    Costs c;
    c.what = std::string(name) + ": " + what;
    c.span = median(p.span_ms);
    for (const auto& [pass, ms] : p.pass_ms) {
      if (pass == "liquid") c.liquid = median(ms);
      if (pass == "liquid.air") c.air = median(ms);
      if (pass == "particles") c.particles = median(ms);
    }
    costs.push_back(c);
    lab.metric(std::string(name) + "." + what + ".liquid", c.liquid, "ms", gl::Det::measured);
    lab.metric(std::string(name) + "." + what + ".air", c.air, "ms", gl::Det::measured);
    lab.metric(std::string(name) + "." + what + ".particles", c.particles, "ms", gl::Det::measured);
  };
  const std::vector<std::pair<std::string, std::string>> inputs = {{"map", map_name},
                                                                   {"liquid", name}};

  // THE AIR, nobody in it: off, then on, timed; and the same view a tenth of a second on.
  con.execute("ce_heathaze 0; ce_fumes 0; ce_burning 1");
  for (int i = 0; i < 30; i++) draw(false, nullptr);  // the light settles, the timers start
  cl::PassTimes off;
  for (int i = 0; i < frames; i++) draw(true, &off);
  const double t_shot = t;
  draw(false, nullptr);
  const gl::Picture bare = grab();
  cost("air-off", off);
  con.execute("ce_heathaze 1; ce_fumes 1");
  t = t_shot;
  draw(false, nullptr);
  const gl::Picture air = grab();
  cl::PassTimes on;
  for (int i = 0; i < frames; i++) draw(true, &on);
  cost("air-on", on);
  const double span_off = costs[costs.size() - 2].span;
  // Each half alone: what the heat haze and the fumes cost apiece.
  for (const auto& [what, cvars] : {std::pair<const char*, const char*>{"haze-only", "ce_fumes 0"},
                                    {"fumes-only", "ce_heathaze 0; ce_fumes 1"}}) {
    con.execute(cvars);
    cl::PassTimes one;
    for (int i = 0; i < frames; i++) draw(true, &one);
    cost(what, one);
  }
  // The air's cost a draw, from the whole frame's (a pass's own timestamps on the software adapter
  // do not hold its raster work).
  for (size_t i = costs.size() - 3; i < costs.size(); ++i) {
    const double per = (costs[i].span - span_off) / air_draws;
    const std::string what = costs[i].what.substr(costs[i].what.find(": ") + 2);
    lab.metric(std::string(name) + "." + what + ".per-draw", per, "ms", gl::Det::measured);
    lab.note(costs[i].what + ": " + fixed(per, 2) + " ms a draw of the air (" +
             std::to_string(air_draws) + " a frame)");
  }
  con.execute("ce_heathaze 1; ce_fumes 1");
  const double share = changed_share(bare, air);
  lab.metric(std::string(name) + ".air.changed-share", share, "ratio", gl::Det::measured);
  t = t_shot + 0.1;
  draw(false, nullptr);
  const gl::Picture later = grab();
  lab.metric(std::string(name) + ".air.wavers-share", changed_share(air, later), "ratio",
             gl::Det::measured);
  const std::string air_what = kind == kLava ? "the heat haze over the melt and its thin soot"
                                             : "the acid fumes hanging over it";
  lab.shot({std::string(name) + "-air-off", std::string(name) + " on " + map_name + ", still air",
            "Across the pool from 1.6 m over it, 6 to 9 m off, ce_heathaze 0 and ce_fumes 0: the "
            "liquid alone.",
            "the pool and the walls past it sharp and still", inputs},
           bare, gl::Det::measured);
  lab.shot({std::string(name) + "-air-on", std::string(name) + " on " + map_name + ", its air",
            "The same with ce_heathaze 1 and ce_fumes 1: " + air_what + ". It changes " +
                fixed(share * 100, 1) + "% of the picture.",
            kind == kLava ? "the walls across the lava wavering, most just over it; a faint dark "
                            "haze lit orange from below"
                          : "a pale yellow-green vapour thickest at the surface, in drifting wisps",
            inputs},
           air, gl::Det::measured);
  lab.shot({std::string(name) + "-air-later", std::string(name) + " a tenth of a second on",
            "The same view 0.1 s later: the shimmer's cells have risen and the wisps drifted.",
            "the wavering moved, not frozen", inputs},
           later, gl::Det::measured);

  // A BODY IN IT, then out on the bank: drawn every frame, pictured at the moments, filmed.
  const double in_s = lab.arg_num("inside"), out_s = lab.arg_num("after");
  const int moments = std::max(2, static_cast<int>(lab.arg_num("moments")));
  std::vector<double> at_s;
  for (int k = 0; k < moments; k++) {
    // Half the moments in it, half out, the first early and the last late.
    const bool in = k < moments / 2;
    const int kk = in ? k : k - moments / 2, n = in ? moments / 2 : moments - moments / 2;
    const double span = in ? in_s : out_s;
    at_s.push_back((in ? 0 : in_s) + span * (0.15 + 0.8 * kk / std::max(1, n - 1)));
  }
  fx.models = {body};
  std::vector<gl::Picture> film;
  gl::LabTable rows{std::string("the body in the ") + name + " and out",
                    {"t (s)", "where", kind == kLava ? "heat" : "acid",
                     kind == kLava ? "charred" : "etched", "middle's luminance"},
                    {}};
  cl::PassTimes burning;
  gl::Trace lvl{kind == kLava ? "heat" : "acid", {}, {}},
      mark{kind == kLava ? "charred" : "etched", {}, {}};
  size_t next = 0;
  const int total = static_cast<int>((in_s + out_s) / kDt);
  const double start = t;
  for (int i = 0; i < total; i++) {
    const double rt = (i + 1) * kDt;
    fx.models[0].origin = rt <= in_s ? in_at : out_at;
    fx.models[0].angles = {0, (rt <= in_s ? in_eye.yaw : out_eye.yaw) + 180, 0};
    look_from(rt <= in_s ? in_eye : out_eye);
    draw(rt <= in_s && rt > 0.5, &burning);
    const gr::MorphBurn b = cl::world_burn(*world, 2);
    lvl.x.push_back(rt);
    lvl.y.push_back(kind == kLava ? b.embers : b.etched);
    mark.x.push_back(rt);
    mark.y.push_back(kind == kLava ? b.charred : b.etched);
    if (i % 3 == 0 || (next < at_s.size() && rt + 1e-9 >= at_s[next])) {
      gl::Picture p = grab();
      if (i % 3 == 0) film.push_back(gl::shrink(p, 320));
      if (next < at_s.size() && rt + 1e-9 >= at_s[next]) {
        const bool in = rt <= in_s;
        const std::string when = in ? fixed(rt, 1) + " s in the " + std::string(name)
                                    : fixed(rt - in_s, 1) + " s out of it";
        rows.rows.push_back(
            {fixed(rt, 2), in ? "in" : "out", fixed(kind == kLava ? b.embers : b.etched, 2),
             fixed(kind == kLava ? b.charred : b.etched, 2), fixed(middle_luma(p, 0.5, 0.5), 1)});
        lab.shot(
            {std::string(name) + "-body-" + std::to_string(next), "a player " + when,
             "A player stood waist deep in the " + std::string(name) + " for " + fixed(in_s, 1) +
                 " s, then on its bank, drawn as the window draws another "
                 "player: " +
                 when + ".",
             kind == kLava ? (in ? "smoke pouring off the body, small flames here and there, "
                                   "the skin charring black with glowing cracks, the air "
                                   "over it wavering"
                                 : "the flames out, the smoke thinning, the char wearing "
                                   "off over a few seconds")
                           : (in ? "fumes rising thick round the body, the skin bleached "
                                   "and pitted"
                                 : "the fumes thinning off it, the etch fading"),
             inputs},
            p, gl::Det::measured);
        next++;
      }
    }
  }
  (void)start;
  cost("body-in", burning);
  lab.table(rows, gl::Det::measured);
  lab.attach(std::string(name) + "-film.gif", gl::encode_gif_frames(film, 10), gl::Det::measured);
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {std::string("THE PLAYER IN THE ") + (kind == kLava ? "LAVA" : "SLIME") +
                  " AS DRAWN, THEN OUT",
              map_name + ": in for " + fixed(in_s, 1) + " s, then on the bank", "t (s)",
              "level 0..1"};
  ch.traces = {lvl, mark};
  lab.shot({std::string(name) + "-levels", ch.frame.title,
            "What the window drew on the body each frame (client/burning.hpp through world_draw).",
            "up within a second in; worn off a few seconds after it is out", inputs},
           gl::to_picture(gl::line_chart(ch).plot), gl::Det::measured);
}

void run(gl::Lab& lab) {
  const double in_s = lab.arg_num("inside"), out_s = lab.arg_num("after");
  cpu_half(lab, in_s, out_s);
  const std::string folder = lab.arg("game");
  if (folder.empty()) return;
  std::string error;
  cl::SearchPath files;
  const size_t cut = folder.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
  const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
  if (!cl::open_game_dir(base, game, files, error)) {
    lab.fail("se-burn: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
    return;
  }
  std::vector<Costs> costs;
  if (!lab.arg("lava").empty()) run_pool(lab, files, lab.arg("lava"), kLava, costs);
  if (!lab.arg("slime").empty()) run_pool(lab, files, lab.arg("slime"), kSlime, costs);
  if (costs.empty()) return;
  gl::LabTable table{
      "the GPU's time a frame (medians, ms, " + lab.arg("width") + "x" + lab.arg("height") + ")",
      {"what", "liquid (surfaces)", "liquid.air", "particles", "whole frame"},
      {}};
  for (const Costs& c : costs)
    table.rows.push_back(
        {c.what, fixed(c.liquid, 2), fixed(c.air, 2), fixed(c.particles, 2), fixed(c.span, 1)});
  lab.table(table, gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-burn",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the air over lava and slime (render/liquid.hpp: the shimmer, the fumes, the stirs) and a body "
    "burning in them (render/body_burn.hpp, client/burning.hpp), drawn by client/world_draw.hpp",
    "render",
    "Does lava's air waver and slime's fume, and does a player in them smoke, char and burn, and "
    "wear it off once out? On real maps' pools, with the passes' GPU cost.",
    {
        {"game", "", "a game folder (<base>/<folder>); empty: the CPU half alone"},
        {"lava", "start", "with --game: a map with a lava pool"},
        {"slime", "e1m1", "with --game: a map with a slime pool"},
        {"inside", "2", "seconds the body stands in the pool"},
        {"after", "5", "seconds on the bank after"},
        {"moments", "6", "with --game: pictures through the run (half in, half out)"},
        {"frames", "20", "with --game: frames timed for each cost"},
        {"air-draws", "1",
         "with --game: the air drawn this many times a timed frame, so its cost a draw stands out "
         "of the frame's noise on the software adapter (the pictures draw it once)"},
        {"width", "800", "with --game: the frame's width, px"},
        {"height", "500", "with --game: the frame's height, px"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
