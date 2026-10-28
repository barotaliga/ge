// LAB `gpu-muzzle`: MUZZLE FIRE (render/muzzle.hpp, drawn by render/explosions.hpp) on the
// software adapter, on the explosion labs' yard at dusk and by day. A stand-in gun is held in
// first person (a dark barrel drawn over the picture from the grip to the muzzle; the real
// held weapon is engine/viewmodel's) and fired at each weapon's rate, its muzzle kicked back and
// up by each shot on a spring as a firing animation does, and the flame staged off that pose
// every frame. Named views: every preset's shot in first person through its life at dusk and by
// day; each one from the side at its flame's peak and as its smoke drifts; each one looking down
// the barrel (a shotgun's prongs); a shotgun's flame riding its recoil frame by frame; and with
// `--frames=N` N frames of `--weapon`'s fire at 60 a second for an animation (`--set=knob=v,...`
// turns its knobs, render/muzzle.hpp's `muzzle_knobs` and "smoke.<knob>"). Measured: the
// explosions pass's GPU time for one shot in first person, a nailgun's stream with its puffs,
// and the CPU's staging. It fails on a GPU error or a pipeline made after the load. Without an
// adapter it notes that, with grey cards.
//
// ge:summary Lab: muzzle fire in first person and from the side, each weapon, recoil, its cost
// ge:keywords lab gpu muzzle muzzle-flash flame smoke weapon shotgun nailgun grenade rocket
// ge:keywords recoil firing animation first-person picture software-adapter
// ge:skill render labs
// ge:test tests/render_muzzle_test.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"
#include "render/particles.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

// A weapon as the lab fires it: its fire's look, how often it fires and how hard it kicks.
struct Weapon {
  const char* look;
  const char* name;
  double refire_s;
  double kick_m;    // the muzzle pushed back, m
  double kick_rad;  // and up
};
constexpr std::array<Weapon, 4> kWeapons{{
    {"muzzle.shotgun", "SHOTGUN", 0.55, 0.07, 0.14},
    {"muzzle.nailgun", "NAILGUN", 0.1, 0.01, 0.015},
    {"muzzle.grenade", "GRENADE LAUNCHER", 0.6, 0.05, 0.16},
    {"muzzle.rocket", "ROCKET LAUNCHER", 0.8, 0.06, 0.08},
}};

const Weapon* weapon_by_look(std::string_view id) {
  for (const Weapon& w : kWeapons) {
    if (id == w.look) return &w;
  }
  return nullptr;
}

// THE HELD GUN, in the view (+x right, +y up, +z forward, m): the grip and the muzzle at rest.
constexpr double kGrip[3] = {0.13, -0.2, 0.12};
constexpr double kMuzzle[3] = {0.11, -0.12, 0.6};

// A firing animation's kick: up in a few hundredths of a second, settled in a quarter.
double kick_at(double t) {
  if (t < 0) return 0;
  constexpr double kRise = 0.035;
  return (t / kRise) * std::exp(1 - t / kRise);
}

struct Gun {
  const Weapon* weapon;
  const gr::MuzzleLook* look;
  std::vector<gr::Shot> shots;
};

// The muzzle's view-space tip and direction at `t`, every shot so far kicking it.
void muzzle_in_view(const Gun& g, double t, double at[3], double dir[3]) {
  double back = 0, up = 0;
  for (const gr::Shot& s : g.shots) {
    const double k = kick_at(t - s.born_s);
    back += g.weapon->kick_m * k;
    up += g.weapon->kick_rad * k;
  }
  // The barrel turns up about the grip and is pushed back along itself.
  double d[3];
  for (int i = 0; i < 3; ++i) d[i] = kMuzzle[i] - kGrip[i];
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (double& v : d) v /= len;
  const double c = std::cos(up), sn = std::sin(up);
  const double dy = d[1] * c + d[2] * sn, dz = -d[1] * sn + d[2] * c;
  dir[0] = d[0];
  dir[1] = dy;
  dir[2] = dz;
  for (int i = 0; i < 3; ++i) at[i] = kGrip[i] + dir[i] * (len - back);
}

// The view's frame into the world (heading 0, pitch `pitch`: right +x, forward +z tipped up).
void to_world(const gr::FrameView& v, const double in[3], double out[3], bool point) {
  const double c = std::cos(v.pitch), sn = std::sin(v.pitch);
  const double y = in[1] * c + in[2] * sn, z = -in[1] * sn + in[2] * c;
  out[0] = in[0] + (point ? v.eye[0] : 0);
  out[1] = y + (point ? v.eye[1] : 0);
  out[2] = z + (point ? v.eye[2] : 0);
}

gr::MuzzlePose pose_of(const Gun& g, const gr::FrameView& v, double t) {
  double at[3], dir[3];
  muzzle_in_view(g, t, at, dir);
  gr::MuzzlePose p;
  to_world(v, at, p.at, true);
  to_world(v, dir, p.dir, false);
  // Up: square to the barrel, in the view's up-forward plane.
  const double up_v[3] = {0, dir[2], -dir[1]};
  to_world(v, up_v, p.up, false);
  return p;
}

// The stand-in barrel drawn over the picture: discs from the grip to the muzzle, each the
// barrel's 2.2 cm radius at its depth.
void draw_barrel(gl::Picture& pic, const Gun& g, double t, double fov_y) {
  double at[3], dir[3];
  muzzle_in_view(g, t, at, dir);
  const double f = 0.5 * pic.height / std::tan(0.5 * fov_y);
  for (int k = 0; k <= 60; ++k) {
    const double u = k / 60.0;
    double p[3];
    for (int i = 0; i < 3; ++i) p[i] = kGrip[i] * (1 - u) + (at[i] - dir[i] * 0.01) * u;
    p[2] = std::max(p[2], 0.08);
    const double x = 0.5 * pic.width + f * p[0] / p[2], y = 0.5 * pic.height - f * p[1] / p[2];
    const auto shade = static_cast<uint8_t>(26 + 30 * u);
    fill_disc(pic, x, y, f * 0.022 / p[2], {shade, shade, static_cast<uint8_t>(shade + 4)});
  }
}

// The gun fired at its rate from 0 until `until`; the shots it has fired by now.
void fire_until(Gun& g, double until) {
  g.shots.clear();
  const auto n = static_cast<uint32_t>(until / g.weapon->refire_s + 1e-9) + 1;
  g.shots.reserve(n);
  for (uint32_t i = 0; i < n; ++i) {
    g.shots.push_back({g.look, i + 1, i * g.weapon->refire_s});
  }
}

struct Live {
  std::vector<gr::Blast> puffs;
  std::vector<gr::MuzzleFire> fires;
};

Live live_at(const Gun& g, const gr::FrameView& v, double t) {
  Live l;
  l.puffs.reserve(g.shots.size());
  l.fires.reserve(g.shots.size());
  for (const gr::Shot& s : g.shots) {
    if (s.born_s > t + 1e-9) continue;
    l.puffs.push_back(gr::smoke_of(s, pose_of(g, v, s.born_s)));
    l.fires.push_back({s, pose_of(g, v, t)});
  }
  return l;
}

// A firing run: the particles flown and the sparks thrown frame by frame at 120 Hz, a picture
// at each moment asked for (settled over `settle` frames), with the barrel drawn over it.
std::vector<gl::Picture> run_gun(ge::gpu::Device& d, es::Scene& s, Gun& g, const gr::FrameView& v0,
                                 const std::vector<double>& moments, int settle, bool barrel) {
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
      for (; thrown < g.shots.size() && g.shots[thrown].born_s <= t + 1e-9; ++thrown) {
        gr::throw_shot(s.particles, g.shots[thrown], pose_of(g, v, g.shots[thrown].born_s));
      }
      const Live l = live_at(g, v, t);
      es::draw(d, s, v, l.puffs, dt, l.fires);
    }
    gr::FrameView v = v0;
    v.time_s = m;
    const Live l = live_at(g, v, m);
    gl::Picture pic = es::settled(d, s, v, l.puffs, settle, l.fires);
    if (barrel) draw_barrel(pic, g, m, v.fov_y);
    out.push_back(std::move(pic));
  }
  return out;
}

gr::FrameView first_person(double sun_el) {
  return es::eye_at(es::kMid, 1.6, 100, 0.0, sun_el, 0, 0);
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const auto h = static_cast<uint32_t>(lab.arg_num("height"));
  const int settle = std::clamp(static_cast<int>(lab.arg_num("settle")), 1, 16);
  const int frames = std::clamp(static_cast<int>(lab.arg_num("frames")), 0, 600);
  if (w < 64 || h < 36 || w > 4096 || h > 4096) {
    lab.fail("--width must be 64..4096 and --height 36..4096", "next: pass --width=640.");
    return;
  }
  const Weapon* chosen_weapon = weapon_by_look(lab.arg("weapon"));
  const gr::MuzzleLook* preset = gr::muzzle_look_by_id(lab.arg("weapon"));
  if (!chosen_weapon || !preset) {
    lab.fail("no weapon \"" + lab.arg("weapon") + "\"",
             "next: pass --weapon=muzzle.shotgun (or .nailgun, .grenade, .rocket).");
    return;
  }
  gr::MuzzleLook chosen = *preset;
  for (std::string_view rest = lab.arg("set"); !rest.empty();) {
    const size_t comma = rest.find(',');
    const std::string_view one = rest.substr(0, comma);
    rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    const size_t eq = one.find('=');
    if (eq == std::string_view::npos ||
        !gr::set_muzzle_knob(chosen, one.substr(0, eq),
                             std::strtod(std::string(one.substr(eq + 1)).c_str(), nullptr))) {
      lab.fail("--set: \"" + std::string(one) + "\" is not knob=value for a known knob",
               "next: name a knob from render/muzzle.hpp's muzzle_knobs, e.g. --set=length=0.5.");
      return;
    }
  }
  // The CPU's half: a nailgun's stream of 12 shots and their puffs staged, many times over.
  {
    gr::Explosions ex;
    ex.staged.assign(gr::kMaxExplosions, gr::GpuBlast{});
    ex.order.reserve(256);
    Gun g{&kWeapons[1], gr::muzzle_look_by_id(kWeapons[1].look), {}};
    fire_until(g, 1.1);
    const gr::FrameView v = first_person(0.6);
    const Live l = live_at(g, v, 1.1);
    const auto t0 = std::chrono::steady_clock::now();
    constexpr int kRounds = 2000;
    for (int k = 0; k < kRounds; ++k) gr::stage_explosions(ex, l.puffs, v, w, h, l.fires);
    lab.metric(
        "cpu.stage-stream",
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() /
            kRounds,
        "us", Det::measured);
  }
  auto device = ge::gpu::create_device({.label = "gpu-muzzle", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"fire-dusk", "fire-day", "side", "front", "recoil"}, w, h);
    return;
  }
  es::Scene s;
  if (!es::make_scene(*device, s, w, h, 10)) {
    lab.fail("the explosions' pipelines did not build",
             "Read the gpu: lines above. Skill: render.");
    return;
  }

  // Every weapon's shot through its life, first person.
  const auto through_life = [&](double sun_el, const std::vector<double>& at, const char* id,
                                const char* title, const char* what, const char* look_for) {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(kWeapons.size() * at.size());
    names.reserve(kWeapons.size() * at.size());
    for (const Weapon& wp : kWeapons) {
      Gun g{&wp, gr::muzzle_look_by_id(wp.look), {}};
      g.shots = {{g.look, 3, 0}};
      for (gl::Picture& p : run_gun(*device, s, g, first_person(sun_el), at, settle, true)) {
        pics.push_back(std::move(p));
      }
      for (const double m : at) names.push_back(std::string(wp.name) + " " + fixed(m, 3) + " S");
    }
    lab.shot({id, title, what, look_for, {}},
             es::sheet_of(pics, names, static_cast<int>(at.size())), Det::measured);
  };
  through_life(-0.06, {0.008, 0.017, 0.03, 0.05, 0.1, 0.35, 1.0}, "fire-dusk",
               "ONE SHOT OF EACH, AT DUSK",
               "Each weapon fired once in first person at dusk (a stand-in barrel), at seven "
               "moments from 8 ms to 1 s.",
               "the shotgun's wide five-petalled flash and a fat grey puff; the nailgun's small "
               "quick jet and a wisp; the grenade launcher's dull orange thump and a brown "
               "cloud; the rocket launcher's long white-hot jet, fast sparks and white smoke "
               "racing off; every flame gone within a tenth of a second, the smoke drifting");
  through_life(0.6, {0.01, 0.03, 0.1, 0.6}, "fire-day", "ONE SHOT OF EACH, BY DAY",
               "As above under a sun 34 degrees up, at four moments.",
               "the same flames dimmer against the day, the smoke lit on its sunward side");

  // From the side: the barrel's line across the view, 1.4 m off.
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(2 * kWeapons.size());
    names.reserve(2 * kWeapons.size());
    gr::FrameView side = es::eye_at(es::kMid, 1.5, 100, 0.0, -0.06, 0, 0);
    for (const Weapon& wp : kWeapons) {
      const gr::MuzzleLook* look = gr::muzzle_look_by_id(wp.look);
      const gr::Shot shot{look, 5, 0};
      gr::MuzzlePose p;
      p.at[0] = es::kMid - 0.5;
      p.at[1] = 1.45;
      p.at[2] = 101.4;
      p.dir[0] = 1;
      p.dir[2] = 0;
      for (const double m : {0.35 * look->life_s, 0.5}) {
        gr::clear_particles(*device, s.particles);
        side.time_s = m;
        const std::vector<gr::Blast> puff = {gr::smoke_of(shot, p)};
        const gr::MuzzleFire fire{shot, p};
        pics.push_back(
            es::settled(*device, s, side, puff, settle, std::span<const gr::MuzzleFire>(&fire, 1)));
        names.push_back(std::string(wp.name) + " " + fixed(m, 3) + " S");
      }
    }
    lab.shot({"side",
              "FROM THE SIDE",
              "Each weapon's flame 1.4 m off, its barrel pointing right across the view, at "
              "the flame's peak and at 0.5 s.",
              "a flame stretched along the barrel's line, hottest at its base, its length "
              "each weapon's; then only smoke carried on down the line and slowing",
              {}},
             es::sheet_of(pics, names, 2), Det::measured);
  }

  // Down the barrel: the flame's face, 1.6 m ahead of it.
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(kWeapons.size());
    names.reserve(kWeapons.size());
    gr::FrameView front = es::eye_at(es::kMid, 1.5, 101.6, 0.0, -0.06, 0, 0);
    front.heading = 3.14159265358979;  // looking back at the gun
    for (const Weapon& wp : kWeapons) {
      const gr::MuzzleLook* look = gr::muzzle_look_by_id(wp.look);
      const gr::Shot shot{look, 5, 0};
      gr::MuzzlePose p;
      p.at[0] = es::kMid;
      p.at[1] = 1.5;
      p.at[2] = 100;
      gr::clear_particles(*device, s.particles);
      front.time_s = 0.35 * look->life_s;
      const gr::MuzzleFire fire{shot, p};
      pics.push_back(
          es::settled(*device, s, front, {}, settle, std::span<const gr::MuzzleFire>(&fire, 1)));
      names.push_back(wp.name);
    }
    lab.shot({"front",
              "DOWN THE BARREL",
              "Each weapon's flame at its peak seen from 1.6 m in front of the muzzle.",
              "the shotgun's star of five petals; the others round, the rocket's a bright disc",
              {}},
             es::sheet_of(pics, names, static_cast<int>(kWeapons.size())), Det::measured);
  }

  // The flame riding the recoil: a shotgun's shot frame by frame at 120 Hz.
  {
    Gun g{&kWeapons[0], gr::muzzle_look_by_id(kWeapons[0].look), {}};
    g.shots = {{g.look, 3, 0}};
    std::vector<double> at;
    std::vector<std::string> names;
    at.reserve(8);
    names.reserve(8);
    for (int k = 0; k < 8; ++k) {
      at.push_back((k + 1) / 120.0);
      names.push_back("FRAME " + std::to_string(k + 1));
    }
    lab.shot({"recoil",
              "THE FLAME RIDES THE RECOIL",
              "A shotgun's shot frame by frame at 120 Hz: the barrel kicked back and up by "
              "the firing animation, the flame staged off the muzzle's pose of each frame.",
              "the flame staying on the barrel's tip as it kicks up, never left behind",
              {}},
             es::sheet_of(run_gun(*device, s, g, first_person(-0.06), at, settle, true), names, 4),
             Det::measured);
  }

  // THE COST: the explosions pass with one shotgun shot at its peak, and a nailgun's stream.
  {
    const auto median_ms = [&](Gun& g, double t) {
      const gr::FrameView v0 = first_person(-0.06);
      gr::FrameView v = v0;
      v.time_s = t;
      const Live l = live_at(g, v, t);
      std::vector<double> ts;
      ts.reserve(7);
      for (int k = 0; k < 3; ++k) es::draw(*device, s, v, l.puffs, 0, l.fires);
      for (int k = 0; k < 7; ++k) ts.push_back(es::draw(*device, s, v, l.puffs, 0, l.fires).ms);
      std::sort(ts.begin(), ts.end());
      return ts[3];
    };
    gr::clear_particles(*device, s.particles);
    Gun none{&kWeapons[0], gr::muzzle_look_by_id(kWeapons[0].look), {}};
    lab.metric("pass.none", median_ms(none, 0.02), "ms", Det::measured);
    Gun one{&kWeapons[0], gr::muzzle_look_by_id(kWeapons[0].look), {{nullptr, 3, 0}}};
    one.shots[0].look = one.look;
    lab.metric("pass.shotgun", median_ms(one, 0.02), "ms", Det::measured);
    Gun stream{&kWeapons[1], gr::muzzle_look_by_id(kWeapons[1].look), {}};
    fire_until(stream, 1.1);
    lab.metric("pass.nailgun-stream", median_ms(stream, 1.11), "ms", Det::measured);
    lab.metric("drawn.nailgun-stream", s.explosions.drawn, "count", Det::measured);
    Gun rocket{&kWeapons[3], gr::muzzle_look_by_id(kWeapons[3].look), {}};
    rocket.shots = {{rocket.look, 3, 0}};
    lab.metric("pass.rocket", median_ms(rocket, 0.04), "ms", Det::measured);
    lab.metric("frame.gpu", es::draw(*device, s, first_person(-0.06), {}, 0).frame_ms, "ms",
               Det::measured);
  }

  // The animation's frames: `--weapon` fired at its rate, 60 frames a second.
  if (frames > 0) {
    Gun g{chosen_weapon, &chosen, {}};
    fire_until(g, (frames - 1) / 60.0);
    std::vector<double> ts;
    ts.reserve(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) ts.push_back((i + 1) / 60.0);
    const std::vector<gl::Picture> pics =
        run_gun(*device, s, g, first_person(-0.06), ts, settle, true);
    for (size_t i = 0; i < pics.size(); ++i) {
      char id[32];
      std::snprintf(id, sizeof id, "anim-%03zu", i);
      lab.shot({id, "FRAME", "The weapon fired at its rate, 60 frames a second.", "", {}}, pics[i],
               Det::measured);
    }
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
    "gpu-muzzle",
    gl::LabKind::picture,
    "engine",
    "engine/render/muzzle.hpp",
    "render",
    "Does each weapon's muzzle fire read as its own, flame and smoke, does it stay on the barrel "
    "as the gun kicks, and how little does it cost?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
        {"settle", "4", "frames the march's jitter is averaged over for a still"},
        {"frames", "0", "also N frames of --weapon's fire at 60 a second (an animation)"},
        {"weapon", "muzzle.shotgun", "the preset the animation fires"},
        {"set", "", "knob=value,... turned on that preset (render/muzzle.hpp's muzzle_knobs)"},
    },
    "--width=128 --height=72 --settle=1",
    run,
};

GE_LAB(kSpec);

}  // namespace
