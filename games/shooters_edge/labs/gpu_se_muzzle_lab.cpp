// LAB `se-muzzle`: Shooter's Edge's own held weapons FIRING the engine's muzzle fire, on the
// software adapter, exactly as the window stages it: each weapon held through the game's hand
// (view/hand.hpp, the player's default cvars: centred), fired once, its fire made from its table
// row (view/weapons.hpp's preset and knobs, scaled with the gun by app/muzzle.hpp's `fire_looks`)
// and stepped by `step_muzzle` at 120 Hz: the flame on THIS frame's muzzle (the kick and the
// `fire` clip carry it), the puff left where the shot went off, the sparks into the particles.
// The world is the explosion labs' yard (a dirt floor, a wall 40 m ahead); the gun is drawn over
// it on the CPU (the view-model pass's reference), lit by the yard's light and its own flash.
//
// Named views: every firing weapon through a shot at dusk, from 8 ms to 0.9 s; each on its shot's
// frame and a moment later by day. It fails on a GPU error or a pipeline made after the load.
// Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: Shooter's Edge's weapons firing the engine's muzzle fire, flame, smoke, sparks
// ge:keywords lab gpu se-muzzle shooters-edge muzzle muzzle-fire flame smoke sparks first-person
// ge:keywords step_muzzle fire_looks held-weapon dusk day
// ge:skill render shooter labs
// ge:test tests/shooters_edge_muzzle_test.cpp
#include <algorithm>
#include <span>
#include <string>
#include <vector>

#include "camera/pose.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"
#include "render/particles.hpp"
#include "shooters_edge/app/muzzle.hpp"
#include "shooters_edge/view/hand.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace vm = ge::viewmodel;
namespace se = shooters_edge::view;
namespace fx = shooters_edge::effects;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

constexpr double kEyeY = 1.6;  // the eye over the yard's floor, m
constexpr double kEyeZ = 100;  // where the eye stands, the wall 40 m ahead
constexpr double kStep = 1.0 / 120;
constexpr double kSettleS = 0.5;  // at rest before the shot

ge::camera::Pose pose_of(const gr::FrameView& v) {
  ge::camera::Pose p;
  p.eye = {v.eye[0], v.eye[1], v.eye[2]};
  p.yaw = v.heading;
  p.pitch = v.pitch;
  p.fov_y = v.fov_y;
  p.near_m = v.near_m;
  return p;
}

gr::MuzzlePose muzzle_pose(const se::HandFrame& f) {
  gr::MuzzlePose p;
  for (int k = 0; k < 3; ++k) {
    p.at[k] = f.muzzle_at[k];
    p.dir[k] = f.muzzle_dir[k];
    p.up[k] = f.muzzle_up[k];
  }
  return p;
}

// The gun over `pic` on hand frame `f`, lit by the yard's light and, while it burns, its flame.
void draw_gun(gl::Picture& pic, const vm::HeldModel& gun, const se::HandFrame& f,
              const ge::camera::Pose& view, double sun_el, std::span<const gr::MuzzleFire> flames,
              double now_s, bool hands) {
  vm::Probe probe;
  const auto sky = static_cast<float>(sun_el > 0 ? 0.55 : 0.12);
  probe.ambient[0] = probe.ambient[1] = probe.ambient[2] = sky;
  if (sun_el > 0) probe.direct[0] = probe.direct[1] = probe.direct[2] = 0.5f;
  std::vector<vm::PointLight> lights;
  if (f.flashing) lights.push_back(f.flash);
  double burning = 0;
  for (const gr::MuzzleFire& m : flames)
    burning = std::max(burning, gr::flame_at(m.shot, m.pose, now_s).fire);
  for (vm::PointLight& l : lights) l.intensity *= static_cast<float>(0.4 + burning);
  std::vector<float> gp, gn;
  vm::place_held(gun, f.placement, f.frame_a, f.frame_b, f.frame_t, gp, gn);
  const float grip[3] = {static_cast<float>(f.placement.m[3]), static_cast<float>(f.placement.m[7]),
                         static_cast<float>(f.placement.m[11])};
  const vm::Lighting lit = vm::light_held(probe, lights, view, grip);
  const float black[3] = {0, 0, 0};
  vm::ViewTarget t = vm::make_target(pic.width, pic.height, 2, black);
  std::vector<uint8_t> hidden;
  vm::hidden_triangles(gun, true, !hands, hidden);
  vm::DrawLit d;
  d.positions = gp;
  d.normals = gn;
  d.indices = gun.indices;
  d.colour = gun.colour;
  d.uv = gun.uv;
  d.skin = gun.skin.image.rgba.empty() ? nullptr : &gun.skin;
  d.fov_y = f.placement.fov_y > 0 ? f.placement.fov_y : view.fov_y;
  d.near_m = 0.01;
  d.hidden = hidden;
  vm::draw_lit(t, d, lit);
  pic.rgba = vm::resolve_over(t, pic.rgba);
}

// Weapon `w` fired once at kSettleS and pictured `moments` s after the shot through `v0`: the
// hand and the fire stepped at 120 Hz, the particles flown between the moments, each still
// settled over `settle` frames, the gun drawn over it.
std::vector<gl::Picture> shoot(ge::gpu::Device& d, es::Scene& s,
                               std::span<const vm::HeldModel> models, se::Weapon w,
                               const gr::MuzzleLook& look, gr::FrameView v0,
                               const std::vector<double>& moments, int settle,
                               const se::ViewSettings& settings, int* flames_seen) {
  gr::clear_particles(d, s.particles);
  const vm::HeldModel& gun = models[static_cast<size_t>(w)];
  const ge::camera::Pose view = pose_of(v0);
  se::Hand hand;
  fx::MuzzleShots shots;
  se::HandInput in;
  in.weapon = w;
  std::vector<gl::Picture> out;
  double t = 0;
  bool fired = false;
  for (const double m : moments) {
    const double until = kSettleS + m;
    se::HandFrame f;
    std::vector<gr::Blast> puffs;
    std::vector<gr::MuzzleFire> flames;
    while (t + 1e-9 < until) {
      t += kStep;
      in.fired = !fired && t + 1e-9 >= kSettleS;
      fired = fired || in.fired;
      f = se::step_hand(hand, in, settings, models, view, kStep);
      std::vector<gr::Particle> births;
      puffs.clear();
      flames.clear();
      fx::step_muzzle(shots, &look, in.fired, muzzle_pose(f), t, &births, &puffs, &flames);
      gr::emit_particles(s.particles, births);
      if (t > kSettleS - 2 * kStep) {
        gr::FrameView v = v0;
        v.time_s = t;
        es::draw(d, s, v, puffs, kStep, flames);
      }
    }
    *flames_seen += static_cast<int>(flames.size());
    gr::FrameView v = v0;
    v.time_s = t;
    gl::Picture pic = es::settled(d, s, v, puffs, settle, flames);
    draw_gun(pic, gun, f, view, v0.sun_elevation, flames, t, settings.hands);
    out.push_back(std::move(pic));
  }
  return out;
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const int settle = std::clamp(static_cast<int>(lab.arg_num("settle")), 1, 16);
  if (w < 64 || w > 1920) {
    lab.fail("--width must be 64..1920", "next: pass --width=480.");
    return;
  }
  std::vector<vm::HeldModel> models;
  std::string error;
  if (!se::load_weapon_models(lab.arg("models"), models, &error)) {
    lab.fail(error, "next: rerun from the repository's root, or pass --models=<folder>.");
    return;
  }
  std::vector<fx::FireRow> rows;
  for (const se::WeaponLook& l : se::weapon_looks()) rows.push_back({l.muzzle, l.muzzle_knobs});
  std::vector<gr::MuzzleLook> looks;
  if (!fx::fire_looks(rows, se::kBuiltScale, looks, &error)) {
    lab.fail(error, "next: fix the weapon's fire in games/shooters_edge/view/weapons.cpp.");
    return;
  }
  std::vector<se::Weapon> firing;
  gl::LabTable table{"each weapon's fire", {"weapon", "preset", "flame m", "smoke", "sparks"}, {}};
  for (const se::WeaponLook& l : se::weapon_looks()) {
    const gr::MuzzleLook& k = looks[static_cast<size_t>(l.weapon)];
    if (!fx::fires(k)) continue;
    firing.push_back(l.weapon);
    table.rows.push_back({std::string(l.name), std::string(l.muzzle), fixed(k.length_m, 3),
                          fixed(k.smoke.smoke, 2), std::to_string(k.sparks)});
  }
  lab.table(table);
  lab.metric("firing", static_cast<double>(firing.size()), "count");

  auto device = ge::gpu::create_device({.label = "se-muzzle", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"fire", "day"}, w, h);  // the shots' ids and the two GPU counts at 0
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
    v.fov_y = ge::camera::fov_y_of(90.0 * ge::core::kPi / 180.0, 16.0 / 9.0);
    v.near_m = 0.05;
    return v;
  };
  const gr::FrameView dusk = view_at(-0.06), day = view_at(0.6);
  const se::ViewSettings game;
  int flames_seen = 0;
  {
    const std::vector<double> at = {0.008, 0.02, 0.05, 0.3, 0.9};
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const se::Weapon wp : firing) {
      const auto i = static_cast<size_t>(wp);
      for (gl::Picture& p :
           shoot(*device, s, models, wp, looks[i], dusk, at, settle, game, &flames_seen))
        pics.push_back(std::move(p));
      for (const double m : at)
        names.push_back(std::string(se::weapon_look(wp).name) + " " + fixed(m, 3) + " S");
    }
    lab.shot({"fire",
              "EVERY WEAPON'S SHOT, AT DUSK",
              "Each of Shooter's Edge's firing weapons held centred (the default) in the yard at "
              "dusk, fired once, at five moments from 8 ms to 0.9 s: the engine's muzzle fire "
              "from its table row, scaled with the gun, on the muzzle the hand puts in the world.",
              "a brief small flash on the barrel's tip, white-hot at the mouth and orange where it "
              "frays, gone within a few hundredths of a second; the shotguns a short star, the "
              "rocket launcher a jet, the railgun a faint puff and no flame; then grey smoke "
              "drifting forward and up from where the shot went off; a few sparks",
              {}},
             es::sheet_of(pics, names, static_cast<int>(at.size())), Det::measured);
  }
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (const se::Weapon wp : firing) {
      const auto i = static_cast<size_t>(wp);
      for (gl::Picture& p :
           shoot(*device, s, models, wp, looks[i], day, {0.008, 0.25}, settle, game, &flames_seen))
        pics.push_back(std::move(p));
      names.push_back(std::string(se::weapon_look(wp).name) + " SHOT");
      names.push_back(std::string(se::weapon_look(wp).name) + " 0.25 S");
    }
    lab.shot({"day",
              "EVERY WEAPON'S SHOT, BY DAY",
              "The same shots by day, on the shot's frame and a quarter second on.",
              "by day the flash is hardly seen and the smoke is what shows, as a real gun's",
              {}},
             es::sheet_of(pics, names, 4), Det::measured);
  }
  lab.metric("flames-seen", flames_seen, "count", Det::measured);  // drawn only with a GPU
  if (flames_seen == 0) {
    lab.fail("no shot's flame was staged at any moment",
             "next: check step_muzzle in games/shooters_edge/app/muzzle.cpp and the looks' life.");
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
    "se-muzzle",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/app/muzzle (the weapons' fire) over render/muzzle and the explosions "
    "pass",
    "render",
    "Does each of Shooter's Edge's weapons fire a believable flame, smoke and sparks from its own "
    "muzzle, sized to the gun, as the window stages it?",
    {{"models", "games/shooters_edge/content/models/view", "the folder of the view models"},
     {"width", "480", "a panel's width, px (its height is 9/16 of it)"},
     {"settle", "4", "frames averaged a still (the march's jitter)"}},
    "--width=160 --settle=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
