// LAB `se-weapon-fx`: EVERY WEAPON'S EFFECTS IN THE GAME'S OWN FRAME, CLASSIC BESIDE ENHANCED. The
// baseline mod as a listen server on a map, the player handed every weapon and its ammunition,
// stood where `--at` says, firing each weapon in turn at the wall ahead; the frame drawn as the
// window draws it (client/world_draw.hpp, the effects of client/world_fx.hpp, the weapon in hand)
// in the classic look and in the modern look with the classic-enhance switches on (the enhanced
// weapon effects of app/weapon_fx.hpp: the layered blast, the rocket's flame and smoke, the bolt,
// the hits), at moments after the first shot (`--times`, ms), from the eye and, with `--side`,
// from a second camera beside the line of fire.
//
// The default map is the fixture halls (plugins/brush's sample, a deathmatch spot facing the far
// wall, two torches), so its pictures may go anywhere. `--game=<folder>` (a game folder of packs,
// `--map=<name>`) draws the player's own maps with the packs' own weapons in hand, for PRIVATE
// pictures only. The births each look
// threw, the explosions and beams drawn are deterministic metrics; the pictures are measured.
//
// ge:summary Lab: every weapon's muzzle, rounds, hits, blasts and bolt in the game's frame
// ge:keywords lab se-weapon-fx weapon-effects explosion rocket-trail lightning bolt beam hits
// ge:keywords shotgun nailgun grenade rocket arc classic-vs-enhanced before-after side-view
// ge:skill render shooter labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "camera/pose.hpp"
#include "camera/to_view.hpp"
#include "console/console.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/labs/staged.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace ls = ge::lab::liquid_scene;

constexpr double kTic = 0.013;  // a server frame, s (77 Hz, as the window's)

ge::brush::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  ge::brush::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// The halls with a deathmatch spot facing the far wall and two wall torches.
bool halls_pack(std::vector<uint8_t>& out, std::string& error) {
  ge::brush::Fixture f = ge::brush::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity(
      {{"classname", "light_torch_small_walltorch"}, {"origin", "24 120 136"}, {"light", "200"}}));
  f.entities.push_back(entity(
      {{"classname", "light_torch_small_walltorch"}, {"origin", "488 264 136"}, {"light", "200"}}));
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(f, ge::brush::Family::v29, &bsp, &error)) return false;
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &out, &error);
}

// A weapon: its name in --weapons, its impulse (its rank) and the frames the trigger is held.
struct Gun {
  const char* name;
  int impulse;
  int hold;
};
constexpr Gun kGuns[] = {{"shotgun", 2, 1},  {"double", 3, 1},  {"spiker", 4, 60}, {"heavy", 5, 60},
                         {"grenades", 6, 1}, {"rockets", 7, 1}, {"arc", 8, 120}};

// x,y,z,yaw,pitch (map units, degrees: pitch positive down) into `out`; false when not five.
bool place_of(const std::string& text, float out[5]) {
  return std::sscanf(text.c_str(), "%f,%f,%f,%f,%f", &out[0], &out[1], &out[2], &out[3], &out[4]) ==
         5;
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  std::string error;
  cl::SearchPath files;
  const std::string folder = lab.arg("game");
  std::string map_name = lab.arg("map");
  if (folder.empty()) {
    std::vector<uint8_t> pak;
    if (!halls_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "Read the reason above. Skill: brush.");
      return;
    }
    map_name = "halls";
  } else {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail(error, "Pass a game folder of packs (--game=<install>/id1). Skill: shooter.");
      return;
    }
  }
  if (!shooters_edge::labs::stage_asked(lab, files)) return;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map || !cl::listen_start(l, progs, std::move(map), {}, error)) {
    lab.fail(error, "The map should load under the baseline mod. Skill: shooter.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-weapon-fx", .validation = 1});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  // The weapons in hand: a game folder's own (its packs' first-person models), else ours.
  std::vector<ge::viewmodel::HeldModel> weapons;
  bool at_eye = false;
  if (!folder.empty())
    at_eye = cl::load_held_set(cl::classic_adapter(), files, {}, weapons);
  else if (!shooters_edge::view::load_weapon_models(lab.arg("models"), weapons, &error))
    lab.note("no weapon is drawn: " + error);
  // The two looks: classic (or --before's cvars) and modern with --cvars on it.
  const char* kLooks[2] = {lab.arg("before").empty() ? "classic" : "before", "enhanced"};
  shooters_edge::look::Picture looks[2];
  ge::console::Console con[2];
  for (int k = 0; k < 2; ++k) shooters_edge::look::add_cvars(con[k], looks[k]);
  con[0].execute(lab.arg("before").empty() ? "r_look classic" : lab.arg("before"));
  con[1].execute(lab.arg("cvars"));
  con[1].execute(lab.cvars());
  cl::WorldDrawPtr world[2];
  ge::gpu::Target out;
  if (device) {
    for (int k = 0; k < 2; ++k) {
      world[k] =
          cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                looks[k], weapons, error, {l.server->models, {}});
      if (!world[k]) {
        lab.fail(error, "Read the gpu: lines above. Skill: render.");
        return;
      }
    }
    out = ge::gpu::create_target(*device, "se-weapon-fx.final", w, h);
  } else {
    lab.note("No GPU adapter on this machine: no pictures.");
  }

  cl::ViewState shown;
  cl::FxState fx_state[2];
  cl::FxFrame fx[2];
  cl::HeldFire held_fire[2];
  for (cl::HeldFire& f : held_fire)
    if (!cl::load_held_fire(f, &error)) lab.note("no muzzle fire: " + error);
  cl::HeldState held;
  held.hand.at_eye = at_eye;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    for (cl::FxState& f : fx_state) f.palette.assign(pal.begin(), pal.end());
  }
  const shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::HandFrame hand;
  double t = 0;
  double births[2] = {0, 0}, blasts[2] = {0, 0}, beams[2] = {0, 0};

  float at[5] = {0, 0, 0, 0, 0}, side[5] = {0, 0, 0, 0, 0};
  // The halls' spot, looking down the hall at the floor 9 m ahead, by default.
  bool placed =
      place_of(lab.arg("at").empty() && folder.empty() ? "128,192,24,0,8" : lab.arg("at"), at);
  bool has_side = place_of(lab.arg("side"), side);
  auto& player = l.server->sv.edicts[1];
  const auto stand = [&] {
    if (!placed) return;
    player.origin = {at[0], at[1], at[2]};
    player.velocity = {};
    l.view.x = at[4];
    l.view.y = at[3];
  };
  // Every weapon and its ammunition (qc/common/defs.qc's W_ bits and the ammo fields).
  const auto arm = [&] {
    cl::set_field_named(l, 1, "items", 1 + 2 + 4 + 8 + 16 + 32 + 64 + 4096);
    cl::set_field_named(l, 1, "ammo_shells", 100);
    cl::set_field_named(l, 1, "ammo_nails", 200);
    cl::set_field_named(l, 1, "ammo_rockets", 100);
    cl::set_field_named(l, 1, "ammo_cells", 200);
  };
  // One server frame with `cmd`, its effects in both looks, each drawn so the particles fly.
  const auto frame = [&](sh::MoveCmd cmd, bool draw) {
    stand();
    arm();
    cl::listen_frame(l, cmd);
    t += kTic;
    cl::frame_view(l, shown, 1, t, kTic);
    hand = cl::step_held(l, shown, held, view_settings, weapons, t, kTic);
    for (int k = 0; k < 2; ++k) {
      fx_state[k].on = cl::fx_switches(looks[k]);
      cl::step_fx(l, shown, fx_state[k], t, kTic, fx[k]);
      if (hand.flashing)
        fx[k].lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
      cl::fire_held(held_fire[k], hand, fx_state[k].on.muzzles, t, fx[k]);
      births[k] += static_cast<double>(fx[k].births.size());
      if (device && draw) {
        const ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
        cl::draw_world(*device, *world[k], view, fx[k], &hand, kTic, {}, out.view);
      }
    }
  };
  // The moment's pictures: each look from the eye, and from the side camera.
  const auto picture = [&](const std::string& id, const std::string& what) {
    for (int k = 0; k < 2; ++k) {
      blasts[k] += static_cast<double>(fx[k].puffs.size());
      beams[k] += static_cast<double>(fx[k].beams.size());
    }
    if (!device) {  // the same ids, so the digest is one on every row
      for (int cam = 0; cam < (has_side ? 2 : 1); ++cam)
        for (const char* look : kLooks) {
          const std::string name = id + (cam == 1 ? "-side-" : "-") + look;
          gl::Picture none =
              gl::create_picture(static_cast<int>(w), static_cast<int>(h), {40, 40, 48});
          gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
          lab.shot({name, name, "", "", {}}, none, gl::Det::measured);
        }
      return;
    }
    for (int cam = 0; cam < (has_side ? 2 : 1); ++cam) {
      ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
      if (cam == 1) {
        ge::camera::Pose pose;
        pose.eye = cl::engine_point({side[0], side[1], side[2]});
        pose.yaw = cl::engine_heading(side[3]);
        pose.pitch = cl::engine_pitch(side[4]);
        pose.fov_y = 1.2;
        ge::camera::to_frame_view(pose, view);
        view.time_s = t;
      }
      for (int k = 0; k < 2; ++k) {
        cl::FxFrame still = fx[k];
        still.births.clear();  // thrown already, flown by the frames above
        cl::draw_world(*device, *world[k], view, still, cam == 0 ? &hand : nullptr, 0, {},
                       out.view);
        const ge::gpu::Image img = ge::gpu::read_now(*device, out);
        const std::string name = id + (cam == 1 ? "-side-" : "-") + kLooks[k];
        lab.shot({name,
                  name,
                  what + " In the " + kLooks[k] + " look" +
                      (cam == 1 ? ", from beside the line of fire." : ", from the eye."),
                  "the weapon's fire, its rounds, what they hit and what they leave",
                  {{"map", map_name}, {"at", lab.arg("at")}}},
                 gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
                 gl::Det::measured);
      }
    }
  };

  for (int i = 0; i < 90; i++) frame({}, true);  // the spawn and the fall to the floor
  births[0] = births[1] = 0;  // the spawn's ash (client/warps.hpp): not the weapons' own
  // No --at: from where the player stands, the way whose wall is nearest kAimAt units off at the
  // eye's height (a target near enough to see the hits, far enough to see the rounds fly); no
  // --side: a camera beside the line of fire, back from its middle, looking at the far end.
  const sh::Hull& hull = l.map->bsp.world.models[0].hulls[0];
  constexpr float kAimAt = 360, kEyeZ = 22;
  if (!placed) {
    const sh::Vec3f eye = {player.origin.x, player.origin.y, player.origin.z + kEyeZ};
    float best = 1e9f, dist = 0;
    for (int a = 0; a < 360; a += 10) {
      const double r = a * ge::core::kPi / 180;
      const sh::Vec3f to = {eye.x + 2048 * static_cast<float>(ge::core::libm::cos(r)),
                            eye.y + 2048 * static_cast<float>(ge::core::libm::sin(r)), eye.z};
      const float d = sh::trace_hull(hull, eye, to).fraction * 2048;
      if (d > 200 && std::abs(d - kAimAt) < best) {
        best = std::abs(d - kAimAt);
        dist = d;
        at[3] = static_cast<float>(a);
      }
    }
    at[0] = player.origin.x, at[1] = player.origin.y, at[2] = player.origin.z, at[4] = 2;
    placed = true;
    lab.note("aimed at yaw " + std::to_string(int(at[3])) + ", the wall " +
             std::to_string(int(dist)) + " units off");
  }
  if (!has_side) {
    const double r = at[3] * ge::core::kPi / 180;
    const float fx_ = static_cast<float>(ge::core::libm::cos(r)),
                fy = static_cast<float>(ge::core::libm::sin(r));
    const sh::Vec3f eye = {at[0], at[1], at[2] + kEyeZ};
    const float reach =
        sh::trace_hull(hull, eye, {eye.x + 2048 * fx_, eye.y + 2048 * fy, eye.z}).fraction * 2048;
    const sh::Vec3f mid = {eye.x + fx_ * reach * 0.35f, eye.y + fy * reach * 0.35f, eye.z + 24};
    // As far to the side as the room lets, up to 112 units, either side.
    sh::Vec3f cam = mid;
    float room = 0;
    for (const float sign : {1.0f, -1.0f}) {
      const sh::Vec3f wide = {mid.x + sign * fy * 112, mid.y - sign * fx_ * 112, mid.z};
      const sh::HullTrace tr = sh::trace_hull(hull, mid, wide);
      const float got = tr.fraction * 112 - 12;
      if (got > room) {
        room = got;
        cam = {mid.x + sign * fy * got, mid.y - sign * fx_ * got, mid.z};
      }
    }
    const sh::Vec3f look = {eye.x + fx_ * reach * 0.9f, eye.y + fy * reach * 0.9f, eye.z - 8};
    const double dx = look.x - cam.x, dy = look.y - cam.y, dz = look.z - cam.z;
    side[0] = cam.x, side[1] = cam.y, side[2] = cam.z;
    side[3] = static_cast<float>(ge::core::libm::atan2(dy, dx) * 180 / ge::core::kPi);
    side[4] = static_cast<float>(-ge::core::libm::atan2(dz, std::sqrt(dx * dx + dy * dy)) * 180 /
                                 ge::core::kPi);
    has_side = room > 24;
  }
  std::vector<double> times;
  for (double v : lab.arg_nums("times")) times.push_back(v);
  if (times.empty()) times = {40, 200, 600};
  std::vector<double> blast_times;
  for (double v : lab.arg_nums("blast-times")) blast_times.push_back(v);
  if (blast_times.empty()) blast_times = {30, 150, 400, 1200};
  const std::string asked = "," + lab.arg("weapons") + ",";
  for (const Gun& gun : kGuns) {
    if (lab.arg("weapons") != "all" &&
        asked.find(std::string(",") + gun.name + ",") == std::string::npos)
      continue;
    l.impulse = gun.impulse;
    for (int i = 0; i < 50; i++) frame({}, true);  // the switch, and the last weapon's smoke gone
    for (int i = 0; i < 120; i++) frame({}, true);
    sh::MoveCmd fire;
    fire.buttons = sh::kButtonAttack;
    // The moments after the first shot; a launcher's after its blast instead (--blast-times).
    const bool blasts_ = gun.impulse == 6 || gun.impulse == 7;
    const std::vector<double>& when = blasts_ ? blast_times : times;
    const size_t blasts_before = l.explosions.size();
    double from_ms = 0;
    bool started = !blasts_;
    size_t next = 0;
    for (int i = 0; next < when.size() && i < 500; i++) {
      frame(i < gun.hold ? fire : sh::MoveCmd{}, true);
      const double ms = (i + 1) * kTic * 1000;
      if (!started && l.explosions.size() > blasts_before) {
        started = true;
        from_ms = ms - kTic * 1000;
      }
      if (started && ms - from_ms + 1e-6 >= when[next]) {
        char label[64];
        std::snprintf(label, sizeof label, "%s-%04d", gun.name, static_cast<int>(when[next]));
        picture(label, std::string("The ") + gun.name + " " + std::to_string(int(when[next])) +
                           (blasts_ ? " ms after its blast." : " ms after its first shot."));
        next++;
      }
    }
  }
  for (int k = 0; k < 2; ++k) {
    lab.metric(std::string(kLooks[k]) + ".births", births[k], "particles");
    lab.metric(std::string(kLooks[k]) + ".blasts-drawn", blasts[k], "count");
    lab.metric(std::string(kLooks[k]) + ".beams-drawn", beams[k], "count");
  }
  lab.metric("explosions", static_cast<double>(l.explosions.size()), "count");
  lab.metric("effects", static_cast<double>(l.effects.size()), "count");
  lab.metric("weapons.pack", at_eye ? 1 : 0, "");
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-weapon-fx",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/app/weapon_fx and client/world_fx (the weapons' effects in the frame)",
    "render",
    "Does each weapon's fire, its rounds, its hits, its blast and its bolt read as the classic "
    "game's and look modern in the enhanced look?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"models", "games/shooters_edge/content/models/view", "the held weapons' folder"},
     {"game", "", "a game folder of packs (private pictures); empty: the fixture halls"},
     {"map", "start", "with --game: the map's name"},
     {"at", "",
      "x,y,z,yaw,pitch: where the player stands (map units; pitch down positive); "
      "empty: the spawn, aimed at the wall nearest 360 units off"},
     {"side", "", "x,y,z,yaw,pitch: a second camera; empty: beside the line of fire"},
     {"weapons", "all",
      "the weapons fired, ',' between: shotgun double spiker heavy grenades "
      "rockets arc"},
     {"times", "40,200,600", "the moments pictured after the first shot, ms"},
     {"blast-times", "30,150,400,1200", "the launchers' moments, after the first blast, ms"},
     {"cvars", "", "the enhanced side's cvars, as the console takes them"},
     {"before", "", "the left side's cvars in the modern look instead of the classic look"}},
    "--size=240,135 --weapons=shotgun,rockets,arc --times=60,400 --blast-times=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
