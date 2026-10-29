// LAB `classic-weapons-fire`: the classic held weapons FIRING in the enhanced mode, on the
// software adapter. Each weapon that throws a flame fires the engine's muzzle fire it is given
// (classicenhance/weapons.hpp's `classic_fire_of`, a render/muzzle.hpp preset) from its real
// model's muzzle: the shot plays the file's firing frames at ten a second, the placement kicks,
// and the flame is staged every frame off the muzzle carried into the world through the gun's
// lens (viewmodel's `muzzle_to_world`), so it rides the barrel. The flash the file draws is
// hidden (`DrawLit::hidden` = the model's `flame`) and the flame lights the gun while it burns.
// The world is the explosion labs' yard (a dirt floor and a wall 40 m ahead), at dusk and by
// day; the gun is drawn over it on the CPU (the view-model pass is M48c).
//
// With `--source` it runs on the player's own pack: the pictures are derived from it and stay on
// the machine (a private page at most). Without it the labs' box weapon stands in, firing the
// shotgun's look, so the pictures are the same everywhere and nothing of the pack is in the tree.
// Named views: every firing weapon through a shot at dusk; the file's flash beside the engine's
// fire on the first firing frame; each by day. It fails on a GPU error or a pipeline made after
// the load. Without an adapter it notes that, with grey cards. A weapon of several barrels (the
// nailguns) is also held firing: each shot leaves from the barrel its file flashes on that frame.
//
// ge:summary Lab: the classic held weapons firing the engine's muzzle fire from their muzzles
// ge:keywords lab gpu classicenhance viewmodel held-weapon muzzle muzzle-fire flame smoke fire
// ge:keywords firing-frames hide-flame first-person shotgun nailgun grenade rocket
// ge:skill classicenhance labs render
// ge:test tests/classicenhance_weapons_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

#include "camera/pose.hpp"
#include "classicenhance/labs/fixtures.hpp"
#include "classicenhance/source.hpp"
#include "classicenhance/weapons.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"
#include "render/particles.hpp"
#include "vfs/pak.hpp"
#include "viewmodel/raster.hpp"

namespace {

namespace ce = ge::classicenhance;
namespace gl = ge::lab;
namespace gr = ge::render;
namespace vm = ge::viewmodel;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

constexpr double kFovH = 90;   // the classic lens, degrees across a 4:3 picture
constexpr double kEyeY = 1.6;  // the eye over the yard's floor, m
constexpr double kEyeZ = 100;  // where the eye stands, the wall 40 m ahead
constexpr double kStep = 1.0 / 120;

struct Gun {
  std::string name;  // the file's name, `v_shot`
  vm::HeldModel held;
  gr::MuzzleLook look;    // the preset with the weapon's own knobs turned
  gr::MuzzleLook preset;  // the engine's preset alone
  double refire_s = 0.5;
};

// The weapon's fire: its preset with its knobs turned; false for a preset or a knob not known.
bool look_of(const ce::ClassicFire& f, gr::MuzzleLook* out) {
  const gr::MuzzleLook* preset = gr::muzzle_look_by_id(f.muzzle);
  if (!preset) return false;
  *out = *preset;
  for (std::string_view rest = f.knobs; !rest.empty();) {
    const size_t comma = rest.find(',');
    const std::string_view one = rest.substr(0, comma);
    rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    const size_t eq = one.find('=');
    if (eq == std::string_view::npos ||
        !gr::set_muzzle_knob(*out, one.substr(0, eq),
                             std::strtod(std::string(one.substr(eq + 1)).c_str(), nullptr))) {
      return false;
    }
  }
  return true;
}

std::string short_name(const std::string& path) {
  std::string s = path.substr(path.rfind('/') + 1);
  if (s.size() > 4 && s.substr(s.size() - 4) == ".mdl") s.resize(s.size() - 4);
  return s;
}

// The classic placement `t` s after a shot fired at 0: the row stepped at 120 Hz, kicked once.
vm::Placement placement_at(double t) {
  const vm::PlaceRow row = ce::classic_weapon_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  in.shot = 1;
  vm::Placement p = vm::frame_viewmodel(row, in, st, kStep);
  in.shot = 0;
  for (double at = kStep; at + 1e-9 < t; at += kStep) p = vm::frame_viewmodel(row, in, st, kStep);
  return p;
}

struct Moment {
  int a = 0, b = 0;
  double ft = 0;
  vm::Placement place;
  gr::MuzzlePose pose;
};

// The gun `t` s after its first shot: one shot's frames, or held firing (`stream`) the firing
// frames round and round until `until`, then the rest.
Moment moment_at(const Gun& g, double t, bool stream, double until) {
  Moment m;
  const int frames = int(g.held.frames.size());
  if (stream && t <= until) {
    ce::classic_stream_frame(frames, t, &m.a, &m.b, &m.ft);
  } else {
    // The last shot's frames play out.
    const double last = stream ? std::floor(until / g.refire_s + 1e-9) * g.refire_s : 0;
    ce::classic_fire_frame(frames, t - last, &m.a, &m.b, &m.ft);
  }
  m.place = placement_at(t);
  return m;
}

// Where shot `k` (fired on frame `shot_frame`) leaves the gun on moment `m`: its barrel's tip.
gr::MuzzlePose pose_at(const Gun& g, const ge::camera::Pose& view, const Moment& m,
                       int shot_frame) {
  gr::MuzzlePose p;
  vm::muzzle_to_world(g.held, m.a, m.b, m.ft, m.place, 0, view, p.at, p.dir, p.up,
                      ce::classic_shot_barrel(g.held, shot_frame));
  return p;
}

ge::camera::Pose pose_of(const gr::FrameView& v) {
  ge::camera::Pose p;
  p.eye = {v.eye[0], v.eye[1], v.eye[2]};
  p.yaw = v.heading;
  p.pitch = v.pitch;
  p.fov_y = v.fov_y;
  return p;
}

// A VARIANT of every weapon's fire for side-by-side judging: each knob of the weapon's own look
// scaled. A real flash is a few milliseconds of glowing gas, a small bright core at the mouth and
// a larger, dimmer burn a few calibres out where the gas meets the air; by day it is hardly seen
// and the smoke is what shows; a shotgun's can be a red belch at dusk; a launcher's is little
// more than a puff. The variants run from the weapon's look turned down to that.
struct Variant {
  const char* name;
  double life, size, glow, flash, sparks, smoke;
};
constexpr Variant kVariants[] = {
    {"FAINTER", 1.0, 0.8, 0.5, 0.5, 0.5, 0.6},
    {"ONE FRAME", 0.45, 0.9, 0.7, 0.6, 0.4, 0.5},
    {"REAL", 0.3, 0.6, 0.35, 0.4, 0.2, 0.45},
};

gr::MuzzleLook varied(const gr::MuzzleLook& in, const Variant& v) {
  gr::MuzzleLook out = in;
  const auto set = [&](std::string_view knob, double value) {
    gr::set_muzzle_knob(out, knob, value);
  };
  set("life", in.life_s * v.life);
  set("length", in.length_m * v.size);
  set("width", in.width_m * v.size);
  set("glow", in.glow * v.glow);
  set("flash", in.flash * v.flash);
  set("sparks", std::floor(in.sparks * v.sparks + 0.5));
  set("light_range", in.light_range_m * v.flash);
  set("smoke.smoke", in.smoke.smoke * v.smoke);
  return out;
}

// The gun over `pic` on moment `m`, lit by the yard's light and (while it burns) its own flame.
void draw_gun(gl::Picture& pic, const Gun& g, const Moment& m, const ge::camera::Pose& view,
              double sun_el, double burning, bool hide_flame) {
  vm::Probe probe;
  const auto sky = float(sun_el > 0 ? 0.55 : 0.2);
  probe.ambient[0] = probe.ambient[1] = probe.ambient[2] = sky;
  if (sun_el > 0) probe.direct[0] = probe.direct[1] = probe.direct[2] = 0.5f;
  std::vector<vm::PointLight> lights;
  if (burning > 0) {
    vm::PointLight l = ce::muzzle_flash_light(m.pose.at);
    l.intensity *= float(burning);
    lights.push_back(l);
  }
  std::vector<float> gp, gn;
  vm::place_held(g.held, m.place, m.a, m.b, m.ft, gp, gn);
  const float grip[3] = {float(m.place.m[3]), float(m.place.m[7]), float(m.place.m[11] + 0.4)};
  vm::Lighting lit = vm::light_held(probe, lights, view, grip, ce::kClassicFloor);
  lit.gloss = 0.25f;
  const float black[3] = {0, 0, 0};
  vm::ViewTarget t = vm::make_target(pic.width, pic.height, 2, black);
  vm::DrawLit d;
  d.positions = gp;
  d.normals = gn;
  d.indices = g.held.indices;
  d.colour = g.held.colour;
  d.uv = g.held.uv;
  d.skin = g.held.skin.image.rgba.empty() ? nullptr : &g.held.skin;
  d.fov_y = view.fov_y;
  d.near_m = 0.01;
  if (hide_flame) d.hidden = g.held.flame;
  vm::draw_lit(t, d, lit);
  pic.rgba = vm::resolve_over(t, pic.rgba);
}

// The gun fired at 0 and pictured at each moment: one shot, or (`stream`) held firing at its rate
// until `until`. The particles flown at 120 Hz between the moments, each still settled over
// `settle` frames, the gun drawn over it. `fire` off: the file's own flash and no engine fire.
std::vector<gl::Picture> shoot(ge::gpu::Device& d, es::Scene& s, const Gun& g, gr::FrameView v0,
                               const std::vector<double>& moments, int settle, bool fire,
                               bool stream = false, double until = 0) {
  gr::clear_particles(d, s.particles);
  const ge::camera::Pose view = pose_of(v0);
  // The shots, each with the firing frame it went off on (its barrel) and its puff.
  struct Fired {
    gr::Shot shot;
    int frame;
    gr::MuzzlePose at_shot;
  };
  std::vector<Fired> fired;
  const int n = stream ? int(until / g.refire_s + 1e-9) + 1 : 1;
  for (int k = 0; k < n; ++k) {
    const double born = k * g.refire_s;
    const Moment m = moment_at(g, born, stream, until);
    const int frame = stream ? m.a : 1;
    fired.push_back({{&g.look, uint32_t(3 + k), born}, frame, pose_at(g, view, m, frame)});
  }
  std::vector<gl::Picture> out;
  out.reserve(moments.size());
  size_t thrown = 0;
  double t = 0;
  const auto live = [&](double now, const Moment& m, std::vector<gr::Blast>& puffs,
                        std::vector<gr::MuzzleFire>& fires) {
    for (const Fired& f : fired) {
      if (f.shot.born_s > now + 1e-9) continue;
      puffs.push_back(gr::smoke_of(f.shot, f.at_shot));
      fires.push_back({f.shot, pose_at(g, view, m, f.frame)});
    }
  };
  for (const double when : moments) {
    while (fire && t + 1e-9 < when) {
      const double dt = std::min(kStep, when - t);
      t += dt;
      for (; thrown < fired.size() && fired[thrown].shot.born_s <= t + 1e-9; ++thrown) {
        gr::throw_shot(s.particles, fired[thrown].shot, fired[thrown].at_shot);
      }
      gr::FrameView v = v0;
      v.time_s = t;
      std::vector<gr::Blast> puffs;
      std::vector<gr::MuzzleFire> fires;
      live(t, moment_at(g, t, stream, until), puffs, fires);
      es::draw(d, s, v, puffs, dt, fires);
    }
    gr::FrameView v = v0;
    v.time_s = when;
    const Moment m = moment_at(g, when, stream, until);
    std::vector<gr::Blast> puffs;
    std::vector<gr::MuzzleFire> fires;
    live(when, m, puffs, fires);
    gl::Picture pic =
        fire ? es::settled(d, s, v, puffs, settle, fires) : es::settled(d, s, v, {}, settle);
    // The gun lit by the brightest flame still burning (a nailgun's puff throws no light).
    double burning = 0;
    for (const gr::MuzzleFire& f : fires) {
      const gr::Flame flame = gr::flame_at(f.shot, f.pose, when);
      if (flame.alive && g.look.flash > 0) burning = std::max(burning, flame.fire);
    }
    draw_gun(pic, g, m, view, v.sun_elevation, fire ? burning : 0, fire);
    out.push_back(std::move(pic));
  }
  return out;
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 3 / 4;
  const int settle = std::clamp(static_cast<int>(lab.arg_num("settle")), 1, 16);
  if (w < 64 || w > 1920) {
    lab.fail("--width must be 64..1920", "next: pass --width=480.");
    return;
  }
  // The weapons: the pack's that throw a flame, or the labs' box weapon firing a shotgun's.
  std::vector<Gun> guns;
  const std::string path = lab.arg("source");
  std::string error;
  if (path.empty()) {
    const ce::Palette palette = ce::labs::fixture_palette();
    Gun g{"fixture", ce::held_from_morph(ce::labs::fixture_weapon(palette), palette), {}, {}, 0.5};
    const ce::ClassicFire* f = ce::classic_fire_of("v_shot");
    look_of(*f, &g.look);
    g.preset = *gr::muzzle_look_by_id(f->muzzle);
    g.refire_s = f->refire_s;
    guns.push_back(std::move(g));
  } else {
    ce::Source src;
    ge::vfs::Pak pak;
    if (!ce::open_source(path, &src, &error) || !ge::vfs::read_pak(src.pack, &pak, &error)) {
      lab.fail(error,
               "next: give --source the shareware release zip, an installed folder or its pack.");
      return;
    }
    const ge::vfs::PakEntry* pal = ge::vfs::pak_find(pak, "gfx/palette.lmp");
    if (!pal || pal->size != 768) {
      lab.fail("the pack has no gfx/palette.lmp", "next: give --source the classic pack.");
      return;
    }
    ce::Palette palette;
    const auto pb = ge::vfs::pak_file(src.pack, *pal);
    std::copy(pb.begin(), pb.end(), palette.rgb.begin());
    std::vector<std::string> skipped;
    for (ce::PackWeapon& pw : ce::pack_weapons(src.pack, &skipped)) {
      const ce::ClassicFire* f = ce::classic_fire_of(pw.name);
      if (!f || f->muzzle.empty()) continue;
      Gun g{short_name(pw.name), ce::held_from_morph(pw.model, palette), {}, {}, f->refire_s};
      if (look_of(*f, &g.look)) g.preset = *gr::muzzle_look_by_id(f->muzzle);
      if (!look_of(*f, &g.look)) {
        lab.fail(
            "no muzzle look for " + g.name + " (\"" + std::string(f->muzzle) + "\" " +
                std::string(f->knobs) + ")",
            "next: name a render/muzzle.hpp preset and its knobs in classicenhance/weapons.cpp.");
        return;
      }
      guns.push_back(std::move(g));
    }
    for (const std::string& s : skipped) lab.note("skipped: " + s);
    lab.note(
        "local run on the classic pack: these pictures are derived from it; keep them on this "
        "machine");
  }
  gl::LabTable table{"each weapon's fire",
                     {"weapon", "look", "every s", "flame tris", "barrels", "barrel a frame"},
                     {}};
  for (const Gun& g : guns) {
    const auto flames = double(std::count(g.held.flame.begin(), g.held.flame.end(), uint8_t{1}));
    lab.metric("flame-tris-" + g.name, flames, "count");
    lab.metric("barrels-" + g.name, double(g.held.barrels.size()), "count");
    std::string order;
    for (int16_t b : g.held.frame_barrel) order += b < 0 ? "-" : std::to_string(b);
    table.rows.push_back({g.name, std::string(g.look.id) + (g.look.flash > 0 ? "" : " (puff)"),
                          fixed(g.refire_s, 2), fixed(flames, 0),
                          std::to_string(g.held.barrels.size()), order.empty() ? "-" : order});
  }
  lab.table(table);
  if (guns.empty()) {
    lab.fail("the pack has no held weapon that fires", "next: give --source the classic pack.");
    return;
  }

  auto device = ge::gpu::create_device({.label = "classic-weapons-fire", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"fire", "swap", "stream", "variants", "day"}, w, h);
    return;
  }
  es::Scene s;
  if (!es::make_scene(*device, s, w, h, 10)) {
    lab.fail("the explosions' pipelines did not build",
             "Read the gpu: lines above. Skill: render.");
    return;
  }
  const auto view_at = [&](double sun_el) {
    gr::FrameView v = es::eye_at(es::kMid, kEyeY, kEyeZ, 0.0, sun_el, 0, 0);
    v.fov_y = ge::camera::fov_y_of(kFovH * ge::core::kPi / 180);
    v.near_m = 0.05;
    return v;
  };
  const gr::FrameView dusk = view_at(-0.06), day = view_at(0.6);

  {
    const std::vector<double> at = {0.01, 0.025, 0.05, 0.1, 0.3, 0.8};
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const Gun& g : guns) {
      for (gl::Picture& p : shoot(*device, s, g, dusk, at, settle, true))
        pics.push_back(std::move(p));
      for (const double m : at) names.push_back(g.name + " " + fixed(m, 3) + " S");
    }
    lab.shot({"fire",
              "EVERY WEAPON'S SHOT, AT DUSK",
              "Each held weapon that throws a flame fired once in the yard at dusk, its own "
              "muzzle fire on its real model, at six moments from 10 ms to 0.8 s; the file's "
              "flash hidden.",
              "each flame on its own barrel's tip and riding it through the firing frames: a "
              "brief small flash as a real gun's, gone within a few hundredths of a second, then "
              "the smoke drifting on; the nailguns' faint puff and no flame",
              {}},
             es::sheet_of(pics, names, static_cast<int>(at.size())), Det::measured);
  }
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const Gun& g : guns) {
      for (const bool fire : {false, true}) {
        std::vector<gl::Picture> one = shoot(*device, s, g, dusk, {0.02}, settle, fire);
        pics.push_back(std::move(one[0]));
        names.push_back(g.name + (fire ? " engine fire" : " file flash"));
      }
    }
    lab.shot({"swap",
              "THE FILE'S FLASH, THEN THE ENGINE'S FIRE",
              "Each weapon 20 ms into a shot: as its file draws the flash, then with that flash "
              "hidden and the engine's fire in its place.",
              "the engine's flame where the file's flash was, nothing of the file's left over",
              {}},
             es::sheet_of(pics, names, 4), Det::measured);
  }
  {
    // Held firing: every quick-firing weapon of several barrels, a picture every 50 ms for 0.4 s.
    std::vector<double> at;
    for (int k = 1; k <= 8; ++k) at.push_back(0.05 * k - 0.02);
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const Gun& g : guns) {
      if (g.held.barrels.size() < 2 || g.refire_s > 0.15) continue;  // the nailguns
      for (gl::Picture& p : shoot(*device, s, g, dusk, at, settle, true, true, 0.4)) {
        pics.push_back(std::move(p));
      }
      for (const double m : at) names.push_back(g.name + " " + fixed(m, 2) + " S");
    }
    if (!pics.empty()) {
      lab.shot({"stream",
                "HELD FIRING, BARREL BY BARREL",
                "Each weapon of several barrels held firing at its rate for 0.4 s, a picture "
                "every 50 ms: each shot leaves from the barrel its file shows the flash on for "
                "that frame.",
                "the nailgun's puffs from its two barrels in turn; the super nailgun's from one "
                "barrel at a time round its cluster; each puff on its barrel's mouth",
                {}},
               es::sheet_of(pics, names, static_cast<int>(at.size())), Det::measured);
    }
  }
  {
    // Every weapon's look and three variants of it, at dusk, 8 ms and 25 ms into a shot.
    const std::vector<double> at = {0.008, 0.025};
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const Gun& g : guns) {
      for (int k = -1; k < static_cast<int>(std::size(kVariants)); ++k) {
        Gun v = g;
        v.look = k < 0 ? g.preset : varied(g.preset, kVariants[k]);
        for (gl::Picture& p : shoot(*device, s, v, dusk, at, settle, true)) {
          pics.push_back(std::move(p));
        }
        for (const double m : at) {
          names.push_back(g.name + " " + (k < 0 ? "PRESET" : kVariants[k].name) + " " +
                          fixed(m * 1000, 0) + " MS");
        }
      }
    }
    lab.shot({"variants",
              "EACH WEAPON'S PRESET AND THREE VARIANTS",
              "Each weapon's engine preset, then fainter, then a one-frame flash, then as a real "
              "flash looks (a few milliseconds, small, dim, mostly smoke: the look the classic "
              "weapons fire with), at dusk 8 ms and 25 ms into a shot.",
              "which reads best at speed: the brief ones gone by 25 ms, the smoke left",
              {}},
             es::sheet_of(pics, names, static_cast<int>(2 * (std::size(kVariants) + 1))),
             Det::measured);
  }
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const Gun& g : guns) {
      std::vector<gl::Picture> one = shoot(*device, s, g, day, {0.03}, settle, true);
      pics.push_back(std::move(one[0]));
      names.push_back(g.name);
    }
    lab.shot({"day",
              "EACH SHOT BY DAY",
              "Each weapon 30 ms into a shot under a sun 34 degrees up.",
              "the same flames dimmer against the day, the smoke lit on its sunward side",
              {}},
             es::sheet_of(pics, names, std::min<int>(3, static_cast<int>(pics.size()))),
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
    "classic-weapons-fire",
    gl::LabKind::picture,
    "plugin:classicenhance",
    "plugins/classicenhance/weapons",
    "classicenhance",
    "Does each classic weapon fire its own muzzle fire from its own barrel, riding the firing "
    "frames, with the file's flash gone?",
    {
        {"source", "", "the classic pack (zip, folder or pack); empty: the labs' box weapon"},
        {"width", "480", "a panel's width, px (its height is 3/4 of it)"},
        {"settle", "4", "frames the march's jitter is averaged over for a still"},
    },
    "--width=128 --settle=1",
    run,
};

GE_LAB(kSpec);

}  // namespace
