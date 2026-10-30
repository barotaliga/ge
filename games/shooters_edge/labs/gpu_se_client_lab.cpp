// LAB `se-client`: THE `shooters_edge` WINDOW'S FRAME WITHOUT A WINDOW. The fixture halls
// (plugins/brush) in a pack on the client's search path, the baseline mod as a listen server, the
// player spawned on a rocket launcher; he takes it, steps into the hall, looks straight down,
// jumps and fires at the floor in the same frame: a ROCKET JUMP. The frame is drawn as the window
// draws it (client/world_draw.hpp, the first-person eyes of client/present.hpp, the effects of
// client/world_fx.hpp, the weapon in hand of client/held.hpp, the HUD of hud/layout.hpp) at three
// moments: standing at the spawn, the
// blast under him (the rocket's trail, the explosion's sparks and its light on the floor, the hit's
// red flash), and the top of the jump looking down at the floor he left. The rise, the health it
// cost and the effects thrown are deterministic metrics; the pictures are measured (each adapter
// rasterises its own way).
//
// With `--game=<folder>` (a game folder of packs, `--map=<name>` its map) the
// lab draws that map instead, for PRIVATE pictures only: the player at the map's start looking
// four ways, each in the classic look and in the modern look with every classic-enhance switch on
// (the enhanced textures when the pack's cache has them); `--cvars="ce_water 0"` sets the modern
// side's cvars as the console would.
//
// ge:summary Lab: the window's frame drawn headless: the halls, a rocket jump, its effects, the HUD
// ge:keywords lab se-client listen-server window frame rocket-jump hud effects trail explosion
// ge:skill shooter render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "console/console.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;
namespace sh = ge::shooter;
namespace ls = ge::lab::liquid_scene;

constexpr const char* kShots[] = {"spawn", "blast", "peak"};

ge::brush::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  ge::brush::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// The halls with a deathmatch spot facing east and a rocket launcher under it, in a pack.
bool halls_pack(std::vector<uint8_t>& out, std::string& error) {
  ge::brush::Fixture f = ge::brush::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(f, ge::brush::Family::v29, &bsp, &error)) return false;
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &out, &error);
}

// A game folder's map from its start, four ways, classic beside enhanced (private pictures).
void run_game(gl::Lab& lab, uint32_t w, uint32_t h) {
  const std::string folder = lab.arg("game");
  const size_t cut = folder.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
  const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
  std::string error;
  cl::SearchPath files;
  ge::progs::Progs progs;
  cl::Listen l;
  if (!cl::open_game_dir(base, game, files, error) || !cl::baseline_progs(progs, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, lab.arg("map"), error);
  if (!map || !cl::listen_start(l, progs, std::move(map), {}, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-client", .validation = 1});
  if (!device) {
    lab.fail("no GPU device", "Install Mesa's Vulkan drivers. Skill: gpu.");
    return;
  }
  shooters_edge::look::Picture looks[2];
  ge::console::Console con[2];
  for (int k = 0; k < 2; ++k) shooters_edge::look::add_cvars(con[k], looks[k]);
  con[0].execute("r_look classic");
  con[1].execute(lab.arg("cvars"));
  cl::WorldDrawPtr world[2];
  for (int k = 0; k < 2; ++k) {
    world[k] = cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                     looks[k], {}, error);
    if (!world[k]) {
      lab.fail(error, "Read the gpu: lines above. Skill: render.");
      return;
    }
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-client.final", w, h);
  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  double t = 0;
  for (int i = 0; i < 80; i++) {
    cl::listen_frame(l, {});
    t += 0.013;
    cl::frame_view(l, shown, 1, t, 0.013);
    cl::step_fx(l, shown, fx_state, t, 0.013, fx);
  }
  // The views: `--views` (x,y,z,yaw,pitch in the map's units and degrees, ';' between), the
  // player stood there; else the start, four ways.
  struct Place {
    bool moved = false;
    float at[3] = {0, 0, 0};
    float yaw = 0, pitch = 0;
  };
  std::vector<Place> places;
  const std::string views = lab.arg("views");
  for (size_t a = 0; a < views.size();) {
    const size_t b = std::min(views.find(';', a), views.size());
    Place p;
    p.moved = std::sscanf(views.substr(a, b - a).c_str(), "%f,%f,%f,%f,%f", &p.at[0], &p.at[1],
                          &p.at[2], &p.yaw, &p.pitch) == 5;
    if (p.moved) places.push_back(p);
    a = b + 1;
  }
  if (places.empty()) {
    for (int turn = 0; turn < 4; ++turn) {
      Place p;
      p.yaw = l.view.y + static_cast<float>(90 * turn);
      p.pitch = l.view.x;
      places.push_back(p);
    }
  }
  const char* kLooks[2] = {"classic", "enhanced"};
  for (size_t v = 0; v < places.size(); ++v) {
    const Place& p = places[v];
    auto& player = l.server->sv.edicts[1];
    if (p.moved) {
      player.origin = {p.at[0], p.at[1], p.at[2]};
      player.velocity = {};
    }
    l.view.x = p.pitch;
    l.view.y = p.yaw;
    for (int i = 0; i < 4; i++) {  // the eyes and the effects settle (the screen's blend, say)
      if (p.moved) player.origin = {p.at[0], p.at[1], p.at[2]};
      player.velocity = {};
      t += 0.013;
      cl::frame_view(l, shown, 1, t, 0.013);
      cl::step_fx(l, shown, fx_state, t, 0.013, fx);
    }
    fx.births.clear();
    ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    for (int k = 0; k < 2; ++k) {
      for (int f = 0; f < 30; ++f)  // the weather and the flames run a moment in
        cl::draw_world(*device, *world[k], view, fx, nullptr, 1.0 / 30, {}, out.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, out);
      const std::string id = std::string(kLooks[k]) + "-" + std::to_string(v);
      lab.shot({id,
                id,
                "View " + std::to_string(v) + " in the " + kLooks[k] + " look.",
                "the map from the player's eyes",
                {{"map", lab.arg("map")}, {"views", views}}},
               gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
               gl::Det::measured);
    }
  }
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  if (!lab.arg("game").empty()) {
    run_game(lab, w, h);
    return;
  }
  std::string error;
  cl::SearchPath files;
  std::vector<uint8_t> pak;
  ge::progs::Progs progs;
  if (!halls_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error) ||
      !cl::baseline_progs(progs, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
  if (!map || !cl::listen_start(l, progs, std::move(map), {}, error)) {
    lab.fail(error, "The fixture map should load under the baseline mod. Skill: shooter.");
    return;
  }

  auto device = ge::gpu::create_device({.label = "se-client", .validation = 1});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  const shooters_edge::look::Picture look;
  std::vector<ge::viewmodel::HeldModel> weapons;
  if (!shooters_edge::view::load_weapon_models(lab.arg("models"), weapons, &error))
    lab.note("no weapon is drawn: " + error);
  cl::WorldDrawPtr world;
  ge::gpu::Target out;
  if (device) {
    world = cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                  look, weapons, error);
    if (!world) {
      lab.fail(error, "Read the gpu: lines above. Skill: render.");
      return;
    }
    out = ge::gpu::create_target(*device, "se-client.final", w, h);
  } else {
    lab.note("No GPU adapter on this machine: no pictures.");
  }

  se::Hud hud = se::make_hud();
  se::HudArt art = se::free_art();
  art.finish();
  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  cl::HeldState held;
  const shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::HandFrame hand;
  shooters_edge::effects::Particles births;  // thrown since the last picture
  double births_total = 0;
  constexpr double kTic = 0.013;
  double t = 0;
  float ground = 0, peak = 0, health_before = 0;
  int shot = 0;
  // `frames` server frames of `cmd`, each drawn into the eyes; a moment's picture after them.
  const auto play = [&](int frames, sh::MoveCmd cmd) {
    for (int i = 0; i < frames; i++) {
      cl::listen_frame(l, cmd);
      cmd.impulse = 0;
      t += kTic;
      peak = std::max(peak, l.server->sv.edicts[1].origin.z);
      cl::frame_view(l, shown, 1, t, kTic);
      cl::step_fx(l, shown, fx_state, t, kTic, fx);
      hand = cl::step_held(l, shown, held, view_settings, weapons, t, kTic);
      births.insert(births.end(), fx.births.begin(), fx.births.end());
      births_total += static_cast<double>(fx.births.size());
    }
  };
  const auto picture = [&](const char* title, const char* caption, const char* look_for) {
    const char* id = kShots[shot++];
    // The view is taken with or without an adapter, so the run's state is the same on every row.
    ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    cl::step_fx(l, shown, fx_state, t, 0, fx);
    fx.births = births;  // every effect thrown since the last picture, flown a moment below
    births.clear();
    if (!device) {
      ls::placeholders(lab, {id});
      return;
    }
    ge::ui::DrawList overlay;
    overlay.begin(static_cast<int>(w), static_cast<int>(h));
    se::fill_hud(hud, cl::status_of(l, t), art);
    hud.layout.draw(overlay,
                    hud.layout.frame(static_cast<int>(w), static_cast<int>(h), se::hud_style()),
                    se::hud_style());
    cl::draw_world(*device, *world, view, fx, &hand, 0.08, {}, out.view);  // births flown a moment
    fx.births.clear();
    cl::draw_world(*device, *world, view, fx, &hand, 0, overlay, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    lab.shot({id, title, caption, look_for, {{"map", "halls"}, {"size", lab.arg("size")}}},
             gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
             gl::Det::measured);
  };

  play(80, {});  // the spawn, the fall onto the launcher, picked up
  l.impulse = 7;
  play(80, {});
  ground = l.server->sv.edicts[1].origin.z;
  picture("AT THE SPAWN",
          "The fixture hall from the player's eyes at the deathmatch spot, facing east, the "
          "rocket launcher in hand at the lower right; the classic HUD along the bottom.",
          "the floor level under the eyes; the HUD's rockets and the launcher lit");
  sh::MoveCmd walk;
  walk.forward = 400;
  play(15, walk);
  play(60, {});
  health_before = l.server->sv.edicts[1].health;
  l.view.x = 89;
  sh::MoveCmd both;
  both.buttons = sh::kButtonJump | sh::kButtonAttack;
  peak = ground;
  int blast_at = -1;
  for (int i = 0; i < 40 && blast_at < 0; i++) {
    play(1, i < 2 ? both : sh::MoveCmd{});
    if (!shown.blasts.empty()) blast_at = i;
  }
  picture("THE BLAST UNDER HIM",
          "Looking straight down the frame the rocket hit the floor: the explosion's sparks and "
          "smoke under the player as he leaves the ground, its light on the floor, the rocket's "
          "short trail, and the screen tinted red by the hit.",
          "the burst of sparks on the floor below the eyes, the floor lit round it, a red tint");
  sh::Vec3f top = l.server->sv.edicts[1].origin;
  for (int i = 0; i < 120; i++) {
    play(1, {});
    const sh::Vec3f now = l.server->sv.edicts[1].origin;
    if (now.z < top.z) break;
    top = now;
  }
  l.view.x = 60;
  picture("THE TOP OF THE JUMP",
          "At the top of the rocket jump, looking down at the hall floor he left; the health the "
          "blast cost on the HUD, the hit's red flash fading.",
          "the floor well below the eyes; the health under 100");
  lab.metric("rocket-jump.rise", peak - ground, "units");
  lab.metric("rocket-jump.blast-frame", blast_at, "frames");
  lab.metric("rocket-jump.health-lost", health_before - l.server->sv.edicts[1].health, "hp");
  lab.metric("effects.births", births_total, "particles");
  lab.metric("effects.explosions", static_cast<double>(fx_state.explosions_seen), "count");
  lab.metric("hand.shots", held.shots, "count");
  if (peak - ground < 100)
    lab.fail("the rocket jump rose only " + std::to_string(peak - ground) + " units",
             "Read combat.qc's push and the rocket's blast (games/shooters_edge/qc). Skill: "
             "shooter.");
  // With no adapter the same two metrics are written as zero, so the digest is one on every row.
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-client",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client (the listen server, the eyes, the effects, the world's frame)",
    "shooter",
    "Does the window's frame show the map from the player's eyes with the HUD, and does a "
    "rocket jump lift him?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"models", "games/shooters_edge/content/models/view", "the held weapons' folder"},
     {"game", "", "a game folder of packs to draw instead of the halls (private pictures only)"},
     {"map", "start", "with --game: the map's name"},
     {"cvars", "", "with --game: the modern side's cvars, as the console takes them"},
     {"views", "", "with --game: x,y,z,yaw,pitch;... where to stand the player (else the start)"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
