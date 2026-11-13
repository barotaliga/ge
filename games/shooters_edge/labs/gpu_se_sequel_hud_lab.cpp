// LAB `se-sequel-hud`: THE SEQUEL'S STATUS BAR. The sequel's rules write their own bar program,
// each player's stats and the pictures and item names they point at (server/sequel_bar.hpp); the
// window runs the program (hud/sequel_layout.hpp) and draws it in the game's own pictures
// (client/sequel_bar.hpp). Five moments are staged on the player: a fresh start (health, the
// blaster selected), armed (a shotgun just picked up, its shells, combat armour), hurt (low health
// flashing, the blow's background behind the numbers), the quad running (its timer, the help icon
// flashing) and a deathmatch (the frags in the corner, power armour's cells).
//
// Without `--game` the moments are staged on a hall built here and each is pictured as a plan of
// what the program draws (a box per picture, its name, the text), with the stats as deterministic
// metrics, so CI needs no game data. With `--game=<folder>` (the folder holding the sequel's base
// folder) they are staged on `--map` played on the window's listen server and each is drawn over
// the player's view as the window draws it, in the game's pictures: PRIVATE pictures only.
//
// ge:summary Lab: the sequel's status bar: stats, program and pictures over the player's view
// ge:keywords lab se-sequel-hud sequel status-bar stats layout pickup timer flash deathmatch
// ge:keywords power-shield sequel-hud screenshot
// ge:skill ui labs
// ge:test tests/shooters_edge_sequel_bar_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/sequel_bar.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/sequel_layout.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_bar.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "ui/draw.hpp"
#include "ui/raster.hpp"

namespace {

namespace br = ge::brush;
namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace hud = shooters_edge::hud;
namespace ls = ge::lab::liquid_scene;
namespace pf = shooters_edge::profiles;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

struct Moment {
  const char* id;
  const char* title;
  const char* look_for;
};
constexpr Moment kMoments[] = {
    {"start", "A fresh start",
     "the health (100) and its cross bottom left of centre, the blaster's icon at the right end"},
    {"armed", "A shotgun picked up",
     "the shells' count and icon, the combat armour's 50 and icon, and above them the shotgun's "
     "icon with its name"},
    {"hurt", "Hurt",
     "the health low in the coloured digits, the blow's background behind health and armour"},
    {"quad", "The quad running",
     "the quad's seconds left and its icon at the right of the upper row, the help icon in the "
     "middle"},
    {"deathmatch", "A deathmatch",
     "the frags in the top right corner, the power shield's icon and the cells in the armour's "
     "place"},
};

// The moment `m` staged on slot 0's player, then the stats written (deathmatch's program too).
void stage(sv::RulesRunner& r, sv::SequelServer& s, int m) {
  sv::SequelPlayer& p = s.players[0];
  const auto give = [&](const char* classname) {
    const int row = sv::sq_item(classname);
    if (row >= 0) p.body.counts[row] = 1;
  };
  const auto weapon = [&](sv::SqWeapon w) {
    p.hand.weapon = static_cast<int>(w);
    p.hand.frame = 0;
  };
  switch (m) {
    case 1:
      give("weapon_shotgun");
      weapon(sv::SqWeapon::shotgun);
      p.body.ammo[static_cast<int>(sv::SqAmmo::shells)] = 25;
      p.body.armour = sv::SqArmour::combat;
      p.body.armour_count = 50;
      p.picked = sv::sq_item_number(sv::sq_item("weapon_shotgun"));
      p.picked_at = s.level_time;
      p.body.selected = p.picked;
      break;
    case 2:
      p.health = 18;
      p.body.armour = sv::SqArmour::jacket;
      p.body.armour_count = 7;
      p.hit_frame = s.frame_number;
      p.hit_bits = 3;
      break;
    case 3:
      p.quad_until = s.level_time + 23.5;
      p.body.selected = 23;
      p.help_unread = 1;
      s.frame_number |= 8;  // the help icon's on half
      break;
    case 4:
      r.console("deathmatch 1");
      sv::bar_start(s);
      p.frags = 12;
      p.power_on = true;
      p.body.ammo[static_cast<int>(sv::SqAmmo::cells)] = 80;
      give("item_power_shield");
      break;
    default:
      break;
  }
  sv::bar_rows(s);
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A floored hall and the player's start (the moments without game data).
std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-512, -256, -16}, hi[3] = {512, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

// The program's drawing as a plan: a box a picture (24 px, a digit 16 x 24), its name, the text.
gl::Picture plan(const std::vector<hud::LayoutOp>& ops, int w, int h) {
  gl::Picture p = gl::create_picture(w, h, {24, 26, 32});
  for (const hud::LayoutOp& op : ops) {
    if (op.kind == hud::LayoutOp::Kind::text) {
      gl::draw_text(p, op.x, op.y, op.text,
                    op.alt ? gl::Rgba{240, 170, 60} : gl::Rgba{230, 230, 230});
      continue;
    }
    const bool digit = op.name.rfind("num_", 0) == 0 || op.name.rfind("anum_", 0) == 0;
    const int bw = digit ? 16 : 24;
    const gl::Rgba c = op.name == "field_3"             ? gl::Rgba{120, 40, 40}
                       : op.name.rfind("anum_", 0) == 0 ? gl::Rgba{200, 120, 40}
                       : digit                          ? gl::Rgba{90, 150, 90}
                                                        : gl::Rgba{70, 100, 160};
    gl::fill_rect(p, op.x + 1, op.y + 1, bw - 2, 22, c);
    const std::string label =
        digit ? op.name.substr(op.name.find('_') + 1, 1) : op.name.substr(0, 4);
    gl::draw_text(p, op.x + 3, op.y + 8, label, {255, 255, 255});
  }
  return p;
}

void report(gl::Lab& lab, const sv::SequelServer& s, const Moment& m) {
  const int16_t* st = s.world.clients[0].numbers.stats;
  for (int k : {1, 3, 5, 10, 12, 14, 15})
    lab.metric(std::string(m.id) + ".stat-" + std::to_string(k), st[k], "count");
  for (int k : {0, 2, 4, 6, 7, 9, 11}) {
    const int v = st[k];
    const std::string name =
        v > 0 && size_t(v) < s.world.images.size() ? s.world.images[size_t(v)] : "-";
    lab.note(std::string(m.id) + ": stat " + std::to_string(k) + " = " + name);
  }
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const int w = size.size() > 0 ? static_cast<int>(size[0]) : 640;
  const int h = size.size() > 1 ? static_cast<int>(size[1]) : 480;
  std::string error;
  if (lab.arg("game").empty()) {
    const auto map = hall(error);
    if (!map) {
      lab.fail(error, "The hall should write and read. Skill: brush.");
      return;
    }
    for (int m = 0; m < int(std::size(kMoments)); m++) {
      sv::RulesRunner r = sv::sequel_runner(sv::make_sequel_server({}));
      r.console("deathmatch 0");
      sv::MapSpec spec;
      spec.name = "hall";
      spec.brushes = map;
      if (!r.start(spec, error)) {
        lab.fail(error, "The hall should start. Skill: shooter.");
        return;
      }
      r.connect(0, "Ana", false, false);
      sh::Packet pk;
      pk.client = 0;
      for (int i = 0; i < 16; i++) {
        pk.sequence = uint32_t(i + 1);
        sh::MoveCmd c;
        c.msec = 13;
        pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
        r.frame(&pk, 1);
      }
      sv::SequelServer& s = *sv::sequel_of(r);
      stage(r, s, m);
      report(lab, s, kMoments[m]);
      cl::SequelBar bar;
      cl::run_sequel_bar(bar, s.world, 0, s.frame_number, w, h);
      lab.metric(std::string(kMoments[m].id) + ".drawn", double(bar.ops.size()), "count");
      lab.shot({std::string(kMoments[m].id) + "-plan",
                std::string(kMoments[m].title) + " (plan)",
                "What the sequel's own bar program draws for this moment, as boxes named by "
                "picture (digits by their figure; coloured digits orange, the blow's background "
                "red) on a " +
                    std::to_string(w) + " x " + std::to_string(h) + " screen.",
                kMoments[m].look_for,
                {{"moment", kMoments[m].id}}},
               plan(bar.ops, w, h));
    }
    return;
  }
  const pf::GameProfile& game = *pf::find_game("sequel");
  const cl::GameAdapter& adapter = cl::adapter_of(game);
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                        "/pak0.pak>. Skill: shooter.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-sequel-hud", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  if (!device) {
    lab.note("No GPU adapter on this machine: no pictures.");
    return;
  }
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  const std::string map_name = lab.arg("map");
  for (int m = 0; m < int(std::size(kMoments)); m++) {
    cl::ListenOptions opt = shooters_edge::net::listen_options();
    opt.console = {"deathmatch 0"};
    cl::Listen l;
    std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
    if (!map || !cl::listen_start(l, progs, std::move(map), opt, error)) {
      lab.fail(error, "Check --map names a map in the game's packs. Skill: shooter.");
      return;
    }
    constexpr double kTic = 0.013;
    double t = 0;
    for (int i = 0; i < 120; i++, t += kTic) cl::listen_frame(l, {});
    sv::SequelServer& s = *sv::sequel_of(*l.rules);
    stage(*l.rules, s, m);
    report(lab, s, kMoments[m]);
    shooters_edge::look::Picture look;
    look.look = "classic";
    const cl::ModelLoad load{l.server->models, {}};
    cl::WorldDrawPtr frame =
        cl::create_world_draw(*device, files, *l.map, uint32_t(w), uint32_t(h),
                              wgpu::TextureFormat::RGBA8Unorm, look, {}, error, load, adapter);
    if (!frame) {
      lab.fail(error, "Read the gpu: lines above. Skill: render.");
      return;
    }
    ge::gpu::Target out =
        ge::gpu::create_target(*device, "se-sequel-hud.final", uint32_t(w), uint32_t(h));
    cl::ViewState shown;
    cl::FxState fx_state;
    cl::FxFrame fx;
    cl::frame_view(l, shown, 1, t, kTic);
    cl::step_fx(l, shown, fx_state, t + 1.0, 0, fx);
    const ge::render::FrameView v = cl::frame_view(l, shown, 0, t, 0);
    cl::draw_world(*device, *frame, v, fx, nullptr, 0, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    ge::ui::Image canvas;
    canvas.width = int(img.width);
    canvas.height = int(img.height);
    canvas.rgba = img.rgba;
    cl::SequelBar bar;
    bar.files = &files;
    cl::run_sequel_bar(bar, s.world, 0, s.frame_number, w, h);
    ge::ui::DrawList dl;
    dl.begin(w, h);
    const bool drew = cl::draw_sequel_bar(dl, bar, cl::sequel_bar_scale(h));
    ge::ui::render(dl, canvas);
    lab.metric(std::string(kMoments[m].id) + ".drawn", double(bar.ops.size()), "count",
               gl::Det::measured);
    if (!drew)
      lab.fail(std::string(kMoments[m].id) + ": the bar drew nothing",
               "Check the packs hold pics/num_0.pcx. Skill: ui.");
    lab.shot({kMoments[m].id,
              kMoments[m].title,
              std::string("The sequel's own status bar over the player's view on ") + map_name +
                  ", drawn as the window draws it in the game's own pictures.",
              kMoments[m].look_for,
              {{"map", map_name}, {"moment", kMoments[m].id}}},
             gl::Picture{canvas.width, canvas.height, canvas.rgba}, gl::Det::measured);
    cl::listen_stop(l);
  }
  ls::checked(lab, static_cast<int>(device->errors.load()),
              static_cast<int>(device->pipelines_after_load));
}

const gl::LabSpec kSpec{
    "se-sequel-hud",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the sequel's status bar: server/sequel_bar.cpp's stats and program, hud/sequel_layout.cpp's "
    "language and client/sequel_bar.cpp's drawing",
    "ui",
    "Does the sequel's own status bar show health, ammo, armour, pickups, timers and frags in the "
    "game's own pictures, where its program puts them?",
    {{"size", "640,480", "the screen's width and height, px"},
     {"game", "", "the folder holding the sequel's base folder (empty: a hall, plans only)"},
     {"map", "base1", "the map to stand in (with --game)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
