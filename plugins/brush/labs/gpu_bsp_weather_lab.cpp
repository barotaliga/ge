// LAB `gpu-bsp-weather`: THE WEATHER ON A BSP MAP (plugins/brush/app/bsp_weather.hpp) LOOKED AT:
// each view drawn by the v2 renderer under a ladder of weathers set through the weather's own
// cvars (dry, drizzle, rain, a downpour, a storm caught in a lightning flash, sleet, snow, a
// blizzard), the wind blowing in from the open sky. The open sky and the windows decide where it
// reaches: the rain and the snow fall only where the sky is open along the way the wind drives
// them, so the yard is soaked and snowed on, the hall's floor wet inside its windows (the deeper
// the harder the wind), the store behind its wall dry.
//
// The PROBES are the check that needs no GPU: points on the floors with a ray cast back up the
// fall's way through the map's faces, open when it leaves by a sky face, sheltered when a wall or
// a roof stops it, at each weather's slant. The GPU's own picture of the roofed store is checked
// too: under every weather but a flash it must not move from the dry one outside its door
// (`store.<weather>.changed`). With `--pak=<file>` and `--maps=<a>,<b>` a pack's maps are drawn
// from their starts (and `--view`s); without, the brush plugin's weather fixture (a hall with two
// windows and a door onto an open yard, a store behind it), so CI needs no game data.
//
// ge:summary Lab: a BSP map under a ladder of weathers, the rain and snow kept to the open sky
// ge:keywords lab bsp weather rain snow sleet storm lightning puddles wet wind shelter window
// ge:keywords before-after probes gpu-bsp-weather store-leak roofed indoor
// ge:skill brush render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_liquids.hpp"
#include "brush/app/bsp_sky.hpp"
#include "brush/app/bsp_weather.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/write.hpp"
#include "console/cvar.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "render/bloom.hpp"
#include "render/clouds.hpp"
#include "render/frame.hpp"
#include "render/liquid.hpp"
#include "render/post.hpp"
#include "render/precip.hpp"
#include "render/sky.hpp"
#include "render/water.hpp"
#include "wind/precip.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace ba = ge::brush::app;
namespace ls = ge::lab::liquid_scene;
namespace con = ge::console;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// One weather of the ladder: the cvars it sets.
struct Sky {
  const char* id;
  const char* label;
  double precip, rain, wind, gust;
  bool on;
  bool flash;  // drawn at a moment a stroke lights the sky
};

const Sky kSkies[] = {
    {"dry", "DRY (sv_weather 0)", 0, 1, 3, 0.2, false, false},
    {"drizzle", "DRIZZLE", 0.15, 1, 2, 0.2, true, false},
    {"rain", "RAIN", 0.45, 1, 5, 0.25, true, false},
    {"downpour", "DOWNPOUR", 0.75, 1, 8, 0.3, true, false},
    {"storm", "STORM, A FLASH", 0.95, 1, 18, 0.45, true, true},
    {"sleet", "SLEET", 0.5, 0.5, 6, 0.3, true, false},
    {"snow", "SNOW", 0.45, 0, 3, 0.25, true, false},
    {"blizzard", "BLIZZARD", 0.9, 0, 20, 0.45, true, false},
};
constexpr int kSkyCount = int(sizeof kSkies / sizeof kSkies[0]);

struct View {
  std::string id, title, caption;
  size_t map = 0;
  double eye[3] = {0, 0, 0};
  double heading = 0, pitch = 0;
};

struct Probe {
  const char* name;
  double at[3];  // engine m, on the floor
  bool open;     // what the fixture's design says the fall reaches
};

// The weather fixture's floors (plugins/brush/fixture.cpp: a unit 0.025 m; the yard at +z, 40 m
// long under 5 m walls; the hall's windows 1.6 .. 4.4 m up in its wall onto the yard).
const Probe kProbes[] = {
    {"yard, by the hall", {8, 0.05, 3}, true},
    {"hall, under a window's sill", {4, 0.05, -0.6}, false},
    {"store", {8, 0.05, -13.4}, false},
};

// Where the fall through the west window's middle lands on the hall's floor (it is open there
// by design), or false when it meets the hall's far wall first.
bool window_patch(const double way[3], double out[3]) {
  const double mid[3] = {4, 3.0, -0.2};
  const double t = (mid[1] - 0.05) / -way[1];
  for (int k = 0; k < 3; ++k) out[k] = mid[k] + t * way[k];
  return out[2] > -9.4;
}

// The store's check on the GPU's own picture: the share of a pane's pixels whose luminance moved
// by more than 24 of 255 from the dry pane, outside the columns where the store's door shows the
// hall (a third to over a half of the width, at the store view's heading). The store is roofed
// and walled: no flake, wet face or snow belongs on that share.
constexpr double kStoreDoor[2] = {0.32, 0.58};
constexpr double kStoreLeak = 0.02;

double changed_share(const gl::Picture& a, const gl::Picture& dry) {
  const auto lum = [](const uint8_t* p) { return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]; };
  int seen = 0, moved = 0;
  for (int y = 0; y < a.height; ++y)
    for (int x = 0; x < a.width; ++x) {
      const double u = (x + 0.5) / a.width;
      if (u > kStoreDoor[0] && u < kStoreDoor[1]) continue;
      const size_t at = (size_t(y) * size_t(a.width) + size_t(x)) * 4;
      ++seen;
      if (std::fabs(lum(&a.rgba[at]) - lum(&dry.rgba[at])) > 24) ++moved;
    }
  return seen > 0 ? double(moved) / seen : 0;
}

struct MapState {
  ba::Scene scene;
  ba::BspDraw bsp;
  gr::Water water;
  gr::Liquids liquids;
  ba::LiquidWorld world;
  ba::LiquidRows rows;
  ba::SkyPick sky;
};

struct Frame {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  gr::Sky sky;
  gr::CloudPass clouds;
  gr::Bloom bloom;
  gr::Post post;
  gr::Precip precip;
  gr::FrameRecorder rec;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

// The first face of kind solid or sky a ray from `o` along `d` (unit) meets within `reach` m:
// its kind, or -1 when it meets none.
int first_hit(const ba::Scene& s, const double o[3], const double d[3], double reach) {
  double best = reach;
  int kind = -1;
  for (size_t i = 0; i + 2 < s.indices.size(); i += 3) {
    const ba::DrawVertex& a = s.vertices[s.indices[i]];
    if (a.kind != ba::kSolid && a.kind != ba::kSky) continue;
    const ba::DrawVertex& b = s.vertices[s.indices[i + 1]];
    const ba::DrawVertex& c = s.vertices[s.indices[i + 2]];
    const double e1[3] = {b.pos[0] - a.pos[0], b.pos[1] - a.pos[1], b.pos[2] - a.pos[2]};
    const double e2[3] = {c.pos[0] - a.pos[0], c.pos[1] - a.pos[1], c.pos[2] - a.pos[2]};
    const double p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2],
                         d[0] * e2[1] - d[1] * e2[0]};
    const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (std::fabs(det) < 1e-12) continue;
    const double inv = 1 / det;
    const double t0[3] = {o[0] - a.pos[0], o[1] - a.pos[1], o[2] - a.pos[2]};
    const double u = (t0[0] * p[0] + t0[1] * p[1] + t0[2] * p[2]) * inv;
    if (u < 0 || u > 1) continue;
    const double q[3] = {t0[1] * e1[2] - t0[2] * e1[1], t0[2] * e1[0] - t0[0] * e1[2],
                         t0[0] * e1[1] - t0[1] * e1[0]};
    const double v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
    if (v < 0 || u + v > 1) continue;
    const double t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    if (t > 1e-4 && t < best) {
      best = t;
      kind = int(a.kind);
    }
  }
  return kind;
}

// Whether the fall coming along `way` (unit, down and downwind) reaches `p`: the ray back up it
// leaves by a sky face, or by nothing at all.
bool reached(const ba::Scene& s, const double p[3], const double way[3]) {
  const double back[3] = {-way[0], -way[1], -way[2]};
  return first_hit(s, p, back, 200) != ba::kSolid;
}

// The weather's cvars set the way a console would.
void set_sky(con::Cvars& cvars, const Sky& sky, double from) {
  const auto put = [&](const char* name, double v) {
    cvars.set(name, ls::fixed(v, 3), con::Source::local);
  };
  cvars.set("sv_weather", sky.on ? "1" : "0", con::Source::local);
  put("sv_precip", sky.precip);
  put("sv_rain", sky.rain);
  put("sv_wind", sky.wind);
  put("sv_gust", sky.gust);
  put("sv_winddir", from);
}

// The first moment from `t` a storm's flash is at its brightest above 0.7 (within ten minutes),
// else `t`.
double flash_moment(const ba::WeatherCvars& c, double t) {
  const ge::wind::Precip p{c.precip, c.rain};
  for (int k = 0; k < 60000; ++k) {
    const double at = t + k * 0.01;
    if (ge::wind::flash_at(at, p, c.wind) > 0.7) return at;
  }
  return t;
}

void dress(gr::FrameView& v, const ba::SkyPick& sky, double t) {
  v.style = sky.style;
  v.sun_elevation = sky.sun_elevation * kDeg;
  v.sun_azimuth = 220 * kDeg;
  v.altitude_m = 100;
  v.time_s = t;
  v.fov_y = 1.25;
  v.near_m = 0.05;
}

// One picture: the weather stepped to `t`, the particles flowed for `frames` frames before it.
gl::Picture draw(ge::gpu::Device& d, Frame& f, MapState& m, gr::FrameView v,
                 const ba::WeatherCvars& c, double t, int frames, ba::WeatherFrame* last) {
  ba::WeatherState state;
  const double dt = 1.0 / 30;
  gl::Picture pic;
  for (int k = 0; k < frames; ++k) {
    const double at = t - (frames - 1 - k) * dt;
    v.time_s = at;
    const ba::WeatherFrame w = ba::step_weather(c, state, v.eye, at, 0.2);
    ba::Options o;
    o.sky_light = m.sky.sky_light;
    for (int j = 0; j < 3; ++j) o.sky_tint[j] = m.sky.tint[j];
    gr::FrameView fv = v;
    ba::apply_weather(w, c, fv, o);
    gr::PrecipFrame pf = w.draw;
    pf.dt = k == 0 ? 0 : dt;
    pf.near_m = fv.near_m;
    f.rec.begin_frame(d);
    gr::fill_uniforms(f.staged, fv, f.w, f.h);
    d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
    gr::prepare_sky(f.sky, fv);
    ba::prepare_bsp(d, m.bsp, fv, o);
    const gr::PrecipShelter shelter = ba::bsp_shelter(m.bsp);
    gr::bind_precip_shelter(d, f.precip, w.on ? &shelter : nullptr);
    gr::prepare_precip(d, f.precip, pf);
    ba::light_rooms(m.water, m.liquids, m.rows, o.minlight, o.brightness);
    gr::prepare_water(d, m.water, fv);
    gr::prepare_liquid(d, m.liquids, fv);
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    gr::encode_sky_tables(f.sky, f.pipelines, f.rec, e);
    gr::encode_precip_sim(f.precip, f.pipelines, f.rec, e);
    ba::encode_bsp(m.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_sky(f.sky, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, fv, f.staged);
    ba::encode_bsp_veil(m.bsp, f.pipelines, f.rec, e, f.targets);
    gr::encode_water(m.water, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_liquid(m.liquids, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_precip(f.precip, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
    gr::encode_post(f.post, f.pipelines, f.rec, e, f.frame, f.post_group, f.out.view);
    f.rec.end_frame(e);
    wgpu::CommandBuffer cb = e.Finish();
    d.queue.Submit(1, &cb);
    f.rec.after_submit(d);
    if (k + 1 == frames) {
      const ge::gpu::Image img = ge::gpu::read_now(d, f.out);
      pic = gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
      if (last) *last = w;
    }
  }
  return pic;
}

std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    size_t end = s.find(c, at);
    if (end == std::string::npos) end = s.size();
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

bool parse_view(const std::string& spec, const std::vector<std::string>& names, View* out) {
  const size_t colon = spec.find(':');
  if (colon == std::string::npos) return false;
  double x, y, z, hd, pt;
  if (std::sscanf(spec.c_str() + colon + 1, "%lf,%lf,%lf,%lf,%lf", &x, &y, &z, &hd, &pt) != 5)
    return false;
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i] != spec.substr(0, colon)) continue;
    *out =
        View{names[i] + "-at-" + ls::fixed(x, 0) + "-" + ls::fixed(z, 0),
             names[i] + " at " + ls::fixed(x, 1) + ", " + ls::fixed(y, 1) + ", " + ls::fixed(z, 1),
             "A view picked by hand.",
             i,
             {x, y, z},
             hd * kDeg,
             pt * kDeg};
    return true;
  }
  return false;
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const double t = lab.arg_num("time");
  const double from = lab.arg_num("winddir");
  const int frames = std::max(1, int(lab.arg_num("frames")));

  // ── The maps ──
  ba::Pack pack;
  std::vector<uint8_t> palette;
  std::vector<std::string> paths, names;
  const bool fixture = lab.arg("pak").empty();
  if (!fixture) {
    std::string err;
    if (!ba::read_pack(lab.arg("pak"), &pack, &err)) {
      lab.fail(err,
               "Pass --pak=<a pack file> holding maps/*.bsp and gfx/palette.lmp. Skill: brush.");
      return;
    }
    const auto pal = ba::pack_file(pack, "gfx/palette.lmp");
    palette.assign(pal.begin(), pal.end());
    for (const std::string& m : split(lab.arg("maps"), ',')) {
      paths.push_back("maps/" + m + ".bsp");
      names.push_back(m);
    }
  } else {
    std::vector<uint8_t> bytes;
    std::string err;
    ge::brush::write_bsp(ge::brush::weather_fixture(), ge::brush::Family::v29, &bytes, &err);
    pack.bytes = bytes;
    pack.files.push_back({"maps/weather.bsp", 0, uint32_t(bytes.size())});
    paths.push_back("maps/weather.bsp");
    names.push_back("weather");
  }
  if (palette.size() < 768) {
    // The fixture's textures are palette greys from 16 up; lifted, so its rooms read.
    palette.resize(768);
    for (int i = 0; i < 256; ++i)
      for (int k = 0; k < 3; ++k)
        palette[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(250, 90 + i * 3 / 4));
  }
  std::vector<std::unique_ptr<MapState>> maps;
  for (size_t i = 0; i < paths.size(); ++i) {
    auto m = std::make_unique<MapState>();
    std::string err;
    if (!ba::load_scene(pack, paths[i], palette, &m->scene, &err)) {
      lab.fail(paths[i] + ": " + err, "Check the map's name in the pack (--maps). Skill: brush.");
      return;
    }
    m->sky = ba::pick_sky(m->scene.sky_colours);
    maps.push_back(std::move(m));
  }

  // ── The weathers, through the cvars ──
  con::Cvars cvars;
  ba::WeatherCvars wc;
  ba::add_weather_cvars(cvars, wc);
  gl::LabTable ladder{"The weathers: what each one's cvars make of the air and the ground",
                      {"weather", "tier", "mm/h", "seen m", "wind m/s", "slant deg", "wet",
                       "puddles", "snow", "strokes/min"},
                      {}};
  std::vector<ba::WeatherCvars> set(kSkyCount);
  for (int i = 0; i < kSkyCount; ++i) {
    set_sky(cvars, kSkies[i], from);
    set[size_t(i)] = wc;
    ba::WeatherState st;
    const double eye[3] = {0, 0, 0};
    const ba::WeatherFrame f = ba::step_weather(wc, st, eye, t, 0.2);
    const double slant =
        ge::core::libm::atan2(ge::core::hypot(f.way[0], f.way[2]), -f.way[1]) / kDeg;
    const ge::wind::Precip p{wc.precip, wc.rain};
    const double strokes = wc.on ? ge::wind::strokes_per_minute(p, wc.wind) : 0;
    ladder.rows.push_back({kSkies[i].id, ge::wind::tier_name(f.tier), ls::fixed(f.rate, 2),
                           ls::fixed(f.visibility, 0), ls::fixed(wc.wind, 0), ls::fixed(slant, 1),
                           ls::fixed(f.wet.wet, 2), ls::fixed(f.wet.puddles, 2),
                           ls::fixed(f.cover.cover, 2), ls::fixed(strokes, 2)});
    const std::string id = kSkies[i].id;
    lab.metric(id + ".rate", f.rate, "mm/h");
    lab.metric(id + ".slant", slant, "deg");
    lab.metric(id + ".wet", f.wet.wet, "share");
    lab.metric(id + ".puddles", f.wet.puddles, "share");
    lab.metric(id + ".snow", f.cover.cover, "share");
  }
  lab.table(ladder);

  // ── The probes: what the fall reaches, on the CPU ──
  if (fixture) {
    gl::LabTable probes{
        "The probes: a ray back up the fall from each floor (open: it leaves by "
        "the sky)",
        {"probe", "designed"},
        {}};
    for (int i = 0; i < kSkyCount; ++i)
      if (kSkies[i].on) probes.columns.push_back(kSkies[i].id);
    int wrong = 0;
    std::vector<ba::WeatherFrame> airs(kSkyCount);
    for (int i = 0; i < kSkyCount; ++i) {
      ba::WeatherState st;
      airs[size_t(i)] = ba::step_weather(set[size_t(i)], st, kProbes[0].at, t, 0.2);
    }
    for (const Probe& p : kProbes) {
      std::vector<std::string> row = {p.name, p.open ? "open" : "sheltered"};
      for (int i = 0; i < kSkyCount; ++i) {
        if (!kSkies[i].on) continue;
        const bool open = reached(maps[0]->scene, p.at, airs[size_t(i)].way);
        row.push_back(open ? "open" : "sheltered");
        if (open != p.open) ++wrong;
      }
      probes.rows.push_back(row);
    }
    std::vector<std::string> patch = {"hall, the west window's patch", "open"};
    for (int i = 0; i < kSkyCount; ++i) {
      if (!kSkies[i].on) continue;
      double at[3];
      if (!window_patch(airs[size_t(i)].way, at)) {
        patch.push_back("(the far wall)");
        continue;
      }
      const bool open = reached(maps[0]->scene, at, airs[size_t(i)].way);
      patch.push_back(std::string(open ? "open" : "sheltered") + " " + ls::fixed(-at[2], 1) +
                      " m in");
      if (!open) ++wrong;
    }
    probes.rows.push_back(patch);
    lab.table(probes);
    lab.metric("probes.wrong", wrong, "count");
    if (wrong > 0)
      lab.fail(std::to_string(wrong) +
                   " probe(s) wet where a roof shelters them, or dry under "
                   "the open sky",
               "next: read the probes table; the fixture (plugins/brush/fixture.cpp) or the "
               "fall's way (bsp_weather.cpp's step_weather) moved. Skill: brush.");
  }

  // ── The views ──
  std::vector<View> views;
  if (fixture) {
    views.push_back({"weather-hall",
                     "The hall, toward its windows",
                     "Inside the hall looking out through its two windows and its door to the "
                     "yard.",
                     0,
                     {8, 1.6, -8.6},
                     0,
                     -6 * kDeg});
    views.push_back({"weather-window",
                     "The hall's floor under the west window",
                     "Close by the west window, looking down at the floor the rain blows in on.",
                     0,
                     {5.5, 1.7, -4.2},
                     -20 * kDeg,
                     -26 * kDeg});
    views.push_back({"weather-yard",
                     "The yard, toward the hall",
                     "Out in the open yard, looking at the hall's wall, its windows and its door.",
                     0,
                     {8, 1.7, 12},
                     kPi,
                     -8 * kDeg});
    views.push_back({"weather-store",
                     "The store, behind the hall",
                     "In the closed store behind the hall, looking through its door into the "
                     "hall: dry.",
                     0,
                     {8, 1.6, -15.8},
                     -24 * kDeg,
                     -4 * kDeg});
  } else {
    for (size_t i = 0; i < maps.size(); ++i) {
      const ba::Scene& s = maps[i]->scene;
      views.push_back({names[i] + "-start",
                       names[i] + ": the start",
                       "From where the player starts, looking the way they face.",
                       i,
                       {s.start[0], s.start[1], s.start[2]},
                       s.start_heading,
                       0});
    }
  }
  for (const std::string& spec : split(lab.arg("view"), ';')) {
    View v;
    if (parse_view(spec, names, &v))
      views.push_back(v);
    else
      lab.fail("--view=" + spec + " is not <map>:x,y,z,heading,pitch with the map in --maps",
               "next: write it as e.g. --view=start:20,-1,30,180,-15 and name its map in --maps.");
  }

  auto device = ge::gpu::create_device({.label = "gpu-bsp-weather", .validation = 1});
  if (!device) {
    if (ls::gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: no pictures.");
    for (const View& v : views) ls::placeholders(lab, {v.id.c_str()});
    ls::checked(lab, 0, 0);
    return;
  }
  ge::gpu::Device& d = *device;
  Frame f;
  f.w = w;
  f.h = h;
  f.targets = gr::create_targets(d, "gpu-bsp-weather", w, h);
  f.uniforms = gr::make_buffer(d, "gpu-bsp-weather.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  f.layout = gr::frame_layout(d);
  gr::create_sky(d, f.pipelines, f.sky, f.uniforms, f.layout);
  gr::create_clouds(d, f.pipelines, f.clouds, f.layout);
  gr::BloomSettings bs;
  bs.strength = 0.05;
  gr::create_bloom_pipelines(d, f.pipelines, f.bloom, bs);
  gr::PostSettings ps;
  ps.bloom = 0.05;
  gr::create_post(d, f.pipelines, f.post, f.layout, wgpu::TextureFormat::RGBA8Unorm, ps);
  gr::PrecipSettings precip;
  precip.particles = ba::weather_particles(precip, lab.arg_num("density"));
  gr::create_precip(d, f.pipelines, f.precip, f.layout, precip);
  for (auto& m : maps) {
    ba::create_bsp(d, f.pipelines, m->bsp, m->scene, f.layout);
    gr::create_water(d, f.pipelines, m->water, f.layout);
    gr::create_liquid(d, f.pipelines, m->liquids, f.layout);
    m->world = ba::liquid_world(m->scene, m->rows);
    ba::add_bsp_liquids(d, m->water, m->liquids, m->world, m->rows);
  }
  if (!f.pipelines.finish(d)) {
    lab.fail("the BSP renderer's or the fall's pipelines did not build",
             "Read the gpu: lines above (plugins/brush/app/*.wgsl, engine/render/precip.wgsl). "
             "Skill: render.");
    return;
  }
  ge::gpu::end_load(d);
  f.frame = gr::frame_group(d, f.layout, f.uniforms, f.sky);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, w, h, inputs);
  f.post_group = gr::post_group(d, f.post, f.targets.hdr_view, f.bloom.view);
  gr::bind_precip_targets(d, f.precip, f.targets);
  f.rec.init(d, "gpu-bsp-weather.timer", 32);
  f.out = ge::gpu::create_target(d, "gpu-bsp-weather.final", w, h);
  for (auto& m : maps) {
    ba::create_bsp_targets(d, m->bsp, f.targets);
    gr::create_water_targets(d, m->water, f.targets);
    gr::create_liquid_targets(d, m->liquids, f.targets);
  }

  // ── Each view under every weather, four a row ──
  const int across = 4;
  const int rows = (kSkyCount + across - 1) / across;
  for (const View& v : views) {
    if (!lab.arg("only").empty() && v.id.find(lab.arg("only")) == std::string::npos) continue;
    gr::FrameView fv;
    for (int k = 0; k < 3; ++k) fv.eye[k] = v.eye[k];
    fv.heading = v.heading;
    fv.pitch = v.pitch;
    MapState& m = *maps[v.map];
    dress(fv, m.sky, t);
    gl::Picture sheet = gl::create_picture(int(w) * across + 10 * (across + 1),
                                           (int(h) + 30) * rows + 10, {250, 250, 248});
    const bool store = fixture && v.id == "weather-store";
    gl::Picture dry;
    double leak = 0;
    std::string leaky;
    for (int i = 0; i < kSkyCount; ++i) {
      const ba::WeatherCvars& c = set[size_t(i)];
      const double at = kSkies[i].flash ? flash_moment(c, t) : t;
      ba::WeatherFrame last;
      const gl::Picture p = draw(d, f, m, fv, c, at, frames, &last);
      if (store && !kSkies[i].on) dry = p;
      // A flash lights the rooms a little by design; every other weather leaves the store as dry.
      if (store && kSkies[i].on && !kSkies[i].flash && dry.width == p.width) {
        const double share = changed_share(p, dry);
        lab.metric(std::string("store.") + kSkies[i].id + ".changed", share, "share",
                   Det::measured);
        if (share > leak) {
          leak = share;
          leaky = kSkies[i].id;
        }
      }
      const int ox = 10 + (i % across) * (int(w) + 10);
      const int oy = (i / across) * (int(h) + 30);
      gl::draw_text(sheet, ox, oy + 6, kSkies[i].label, {24, 26, 32});
      ls::paste(sheet, p, ox, oy + 20);
      lab.metric(v.id + "." + kSkies[i].id + ".drawn", double(f.precip.drawn), "count",
                 Det::measured);
      lab.metric(v.id + "." + kSkies[i].id + ".splashed", double(f.precip.splashed), "count",
                 Det::measured);
    }
    lab.shot({v.id,
              v.title,
              v.caption +
                  " The v2 renderer under each weather, set by its cvars (sv_weather, "
                  "sv_precip, sv_rain, sv_wind, sv_winddir, sv_gust), the wind from " +
                  ls::fixed(from, 0) + " degrees.",
              "The rain, the snow, the splashes, the wet, the puddles and the settled snow are "
              "only where the open sky reaches along the wind's slant: the deeper into the hall "
              "the harder the wind, and the store dry but for a strip inside its door when a "
              "gale drives the fall in through the hall's west window.",
              {{"eye", ls::fixed(v.eye[0], 1) + "," + ls::fixed(v.eye[1], 1) + "," +
                           ls::fixed(v.eye[2], 1)},
               {"heading", ls::fixed(v.heading / kDeg, 0)},
               {"pitch", ls::fixed(v.pitch / kDeg, 0)}}},
             sheet, Det::measured);
    if (leak > kStoreLeak)
      lab.fail("the weather reaches into the roofed store: under " + leaky + " " +
                   ls::fixed(leak * 100, 1) + "% of its picture moved from the dry one",
               "next: look at weather-store.png; the fall's shelter test "
               "(engine/render/wgsl/fall_shelter.wgsl) lets it in. Skill: render.");
  }
  ls::checked(lab, int(d.errors.load()), int(d.pipelines_after_load));
}

const gl::LabSpec kSpec{
    "gpu-bsp-weather",
    gl::LabKind::picture,
    "plugin:brush",
    "plugins/brush/app/bsp_weather.hpp",
    "brush",
    "Does the weather on a BSP map (rain, snow, sleet, storms, the wind) fall, splash, wet, pool "
    "and settle only where the open sky and the windows let it in, the way the wind drives it?",
    {{"width", "480", "a pane's width, px (16:9)"},
     {"pak", "", "a pack file holding maps/*.bsp and gfx/palette.lmp (none: the weather fixture)"},
     {"maps", "start", "the maps in the pack, comma separated, without maps/ and .bsp"},
     {"view", "", "more views: <map>:x,y,z,heading,pitch (m, degrees), separated by ;"},
     {"winddir", "0", "the bearing the wind blows from, deg (sv_winddir; 0 the map's +z)"},
     {"time", "40", "the views' time, s (a storm's flash: the first one from then)"},
     {"frames", "16", "the frames the fall flows for before each picture, at 30 a second"},
     {"density", "1", "the particles a full fall draws, a share of the default"},
     {"only", "", "draw only the views whose id holds this"}},
    "--width=128 --frames=3",
    run};
GE_LAB(kSpec);

}  // namespace
