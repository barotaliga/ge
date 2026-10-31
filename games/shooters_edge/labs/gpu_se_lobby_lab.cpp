// LAB `se-lobby-view`: THE LOBBY IN THE WINDOW'S FRAME. The fixture halls (plugins/brush) with four
// deathmatch spots, the baseline mod as a listen server with the lobby on (client/lobby_room.hpp),
// the player and two bots seated; the frame drawn as the window draws it (the world from the
// player's eyes, the weapon in hand, the HUD, then the lobby panel of hud/lobby_panel.hpp over
// it) at three moments: the WARMUP (the player roaming the hall, not ready), the COUNTDOWN after
// his `ready`, and the MATCH's first frame after the map restarted (no panel). The lobby's counts
// are deterministic metrics; the pictures are measured (each adapter rasterises its own way).
//
// ge:summary Lab: the lobby drawn in the window's frame: warmup, countdown, the match's start
// ge:keywords lab se-lobby-view lobby warmup countdown panel frame hud listen-server halls
// ge:skill shooter render labs
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
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
#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace ls = ge::lab::liquid_scene;

ge::brush::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  ge::brush::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// The halls with four deathmatch spots (the hall twice, the side room, the vault) and a rocket
// launcher under the first, in a pack.
bool halls_pack(std::vector<uint8_t>& out, std::string& error) {
  ge::brush::Fixture f = ge::brush::sample_fixture();
  for (const char* at : {"128 192 40", "400 96 40", "700 256 40", "544 700 40"})
    f.entities.push_back(
        entity({{"classname", "info_player_deathmatch"}, {"origin", at}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(f, ge::brush::Family::v29, &bsp, &error)) return false;
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &out, &error);
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
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
  cl::ListenOptions opt;
  opt.lobby = true;
  opt.lobby_settings.countdown_s = 3;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
  if (!map || !cl::listen_start(l, progs, std::move(map), opt, error)) {
    lab.fail(error, "The fixture map should load under the baseline mod. Skill: shooter.");
    return;
  }
  for (const char* skill : {"average", "expert"})
    cl::listen_lobby(l, std::string("addbot ") + skill);

  auto device = ge::gpu::create_device({.label = "se-lobby-view", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
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
    out = ge::gpu::create_target(*device, "se-lobby-view.final", w, h);
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
  constexpr double kTic = 0.013;
  double t = 0;
  const auto play = [&](int frames, sh::MoveCmd cmd) {
    for (int i = 0; i < frames; i++) {
      cl::listen_frame(l, cmd);
      if (l.restarted) {  // the match: what the window keeps of the old server goes
        l.restarted = false;
        shown = {};
        fx_state = {};
        held = {};
      }
      t += kTic;
      cl::frame_view(l, shown, 1, t, kTic);
      cl::step_fx(l, shown, fx_state, t, kTic, fx);
      hand = cl::step_held(l, shown, held, view_settings, weapons, t, kTic);
    }
  };
  const auto picture = [&](const char* id, const char* title, const char* caption,
                           const char* look_for) {
    ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    cl::step_fx(l, shown, fx_state, t, 0, fx);
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
    se::draw_lobby(overlay, cl::lobby_view(*l.room, l.server->sv.time, 0, true),
                   static_cast<int>(w), static_cast<int>(h));
    cl::draw_world(*device, *world, view, fx, &hand, 0, overlay, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    lab.shot({id, title, caption, look_for, {{"map", "halls"}, {"size", lab.arg("size")}}},
             gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
             gl::Det::measured);
  };

  play(80, {});  // the spawn, the fall onto the launcher
  l.impulse = 7;
  sh::MoveCmd walk;
  walk.forward = 300;
  play(60, walk);
  picture("warmup", "THE WARMUP IN THE HALL",
          "The player free in the fixture hall with the launcher he picked up, the HUD along the "
          "bottom, and the lobby over it: WARMUP, 2 of 3 ready (the bots), the keys, and the two "
          "teams' roster on the right with the player's row lit and NOT READY.",
          "the banner at the top centre, the roster clear of the HUD, the world lit behind");
  cl::listen_lobby(l, "ready");
  play(80, {});
  picture("countdown", "THE COUNTDOWN",
          "After `ready` (F1): everyone is ready and the countdown's seconds stand large in the "
          "upper half; the roster's marks are all READY.",
          "the count readable over the world, the banner saying COUNTDOWN");
  const int countdowns = l.room->lobby.countdowns;
  play(260, {});
  picture("match", "THE MATCH'S FIRST MOMENTS",
          "The countdown ran out and the map restarted a second ago: the player respawned on the "
          "launcher's spot with the frags at zero (the bots are already fighting him), and no "
          "lobby over the game.",
          "no banner or roster; the HUD alone");
  lab.metric("lobby.seated", sv::lobby_seated(l.room->lobby), "count");
  lab.metric("lobby.countdowns", countdowns, "count");
  lab.metric("lobby.matches", l.room->lobby.matches, "count");
  if (l.room->lobby.matches != 1)
    lab.fail("the match did not start (" + std::to_string(l.room->lobby.matches) + " matches)",
             "Run tests/shooters_edge_lobby_test.cpp. Skill: shooter.");
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-lobby-view",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/hud/lobby_panel over the window's frame (the listen server's lobby)",
    "shooter",
    "Does the lobby read over the game in the window's frame: the warmup, the countdown, and "
    "nothing once the match is on?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"models", "games/shooters_edge/content/models/view", "the held weapons' folder"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
