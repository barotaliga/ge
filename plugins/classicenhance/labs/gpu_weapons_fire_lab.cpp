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
// the load. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: the classic held weapons firing the engine's muzzle fire from their muzzles
// ge:keywords lab gpu classicenhance viewmodel held-weapon muzzle muzzle-fire flame smoke fire
// ge:keywords firing-frames hide-flame first-person shotgun nailgun grenade rocket
// ge:skill classicenhance labs render
// ge:test tests/classicenhance_weapons_test.cpp
#include <algorithm>
#include <cmath>
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
  const gr::MuzzleLook* look = nullptr;
};

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

Moment moment_at(const Gun& g, const ge::camera::Pose& view, double t) {
  Moment m;
  ce::classic_fire_frame(int(g.held.frames.size()), t, &m.a, &m.b, &m.ft);
  m.place = placement_at(t);
  vm::muzzle_to_world(g.held, m.a, m.b, m.ft, m.place, 0, view, m.pose.at, m.pose.dir, m.pose.up);
  return m;
}

ge::camera::Pose pose_of(const gr::FrameView& v) {
  ge::camera::Pose p;
  p.eye = {v.eye[0], v.eye[1], v.eye[2]};
  p.yaw = v.heading;
  p.pitch = v.pitch;
  p.fov_y = v.fov_y;
  return p;
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

// One shot fired at 0 and pictured at each moment: the particles flown at 120 Hz between them,
// each still settled over `settle` frames, the gun drawn over it. `fire` off: the file's own
// flash and no engine fire (the classic look).
std::vector<gl::Picture> shoot(ge::gpu::Device& d, es::Scene& s, const Gun& g, gr::FrameView v0,
                               const std::vector<double>& moments, int settle, bool fire) {
  gr::clear_particles(d, s.particles);
  const ge::camera::Pose view = pose_of(v0);
  const gr::Shot shot{g.look, 3, 0};
  const gr::MuzzlePose at_shot = moment_at(g, view, 0).pose;
  const std::vector<gr::Blast> puff = {gr::smoke_of(shot, at_shot)};
  std::vector<gl::Picture> out;
  out.reserve(moments.size());
  bool thrown = false;
  double t = 0;
  for (const double when : moments) {
    while (fire && t + 1e-9 < when) {
      const double dt = std::min(kStep, when - t);
      t += dt;
      if (!thrown) {
        gr::throw_shot(s.particles, shot, at_shot);
        thrown = true;
      }
      gr::FrameView v = v0;
      v.time_s = t;
      const gr::MuzzleFire f{shot, moment_at(g, view, t).pose};
      es::draw(d, s, v, puff, dt, std::span<const gr::MuzzleFire>(&f, 1));
    }
    gr::FrameView v = v0;
    v.time_s = when;
    const Moment m = moment_at(g, view, when);
    const gr::MuzzleFire f{shot, m.pose};
    gl::Picture pic =
        fire ? es::settled(d, s, v, puff, settle, std::span<const gr::MuzzleFire>(&f, 1))
             : es::settled(d, s, v, {}, settle);
    const gr::Flame flame = gr::flame_at(shot, m.pose, when);
    draw_gun(pic, g, m, view, v.sun_elevation, fire && flame.alive ? flame.fire : 0, fire);
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
    guns.push_back({"fixture", ce::held_from_morph(ce::labs::fixture_weapon(palette), palette),
                    gr::muzzle_look_by_id(ce::classic_fire_of("v_shot")->muzzle)});
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
      guns.push_back({short_name(pw.name), ce::held_from_morph(pw.model, palette),
                      gr::muzzle_look_by_id(f->muzzle)});
    }
    for (const std::string& s : skipped) lab.note("skipped: " + s);
    lab.note(
        "local run on the classic pack: these pictures are derived from it; keep them on this "
        "machine");
  }
  for (const Gun& g : guns) {
    if (!g.look) {
      lab.fail("no muzzle look for " + g.name,
               "next: name a render/muzzle.hpp preset in classicenhance/weapons.cpp's fires.");
      return;
    }
    lab.metric("flame-tris-" + g.name,
               double(std::count(g.held.flame.begin(), g.held.flame.end(), uint8_t{1})), "count");
  }
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
    placeholders(lab, {"fire", "swap", "day"}, w, h);
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
              "each flame on its own barrel's tip and riding it through the firing frames; the "
              "shotguns' short wide flash, the nailguns' quick jet, the grenade launcher's thump, "
              "the rocket launcher's long jet; the gun lit warm while the flame burns; the smoke "
              "drifting on after the frames come back to rest",
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
