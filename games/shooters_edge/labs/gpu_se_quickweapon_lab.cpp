// LAB `se-quickweapon`: THE QUICK WEAPON IN THE GAME'S OWN FRAME. The baseline mod as a listen
// server on a map, the player handed the shotgun, the double shotgun, the rocket launcher and the
// beam gun with their ammunition, the shotgun in hand and `cl_quickweapon 1`
// (client/quick_fire.hpp over shooter/quick_weapon.hpp). Each frame goes through the quick weapon
// as the window's does: the fire keys into the command's attack and weapon impulse, the weapon the
// preview holds handed to the hand before it steps, the look laid on the frame it made.
//
// What it shows: AT REST, the launcher +attack would fire drawn see-through in hand
// (`cl_quickweapon_preview 1`), the hand hidden (`cl_quickweapon_hide 1`, the preview off) and the
// plain hand (`cl_quickweapon 0`); then a TAP of +attack filmed a thirtieth of a second at a time,
// the ghost turning solid, one rocket fired and the ghost fading back while the shotgun comes back
// unseen (the launcher never leaves the hand: no switch is drawn); then a tap of +attack2
// (`cl_quickweapon_prio2 "8 3 2"`): the beam gun fired and the shotgun back, the hand switching as
// it always does. A chart follows the +attack tap's opacity and how far the hand is put away; a
// table times each tap (the shot, the way back) and counts what each weapon spent. Those are
// deterministic metrics, the pictures measured.
//
// The default map is the fixture halls (plugins/brush's sample), so its pictures may go anywhere.
// `--game=<folder>` (a game folder of packs, `--map=<name>`) draws the player's own maps with the
// packs' own weapons in hand, for PRIVATE pictures only.
//
// ge:summary Lab: the quick weapon in the game's frame: the preview, a tap's switch, shot and
// ge:summary way back
// ge:keywords lab se-quickweapon quick-weapon cl_quickweapon preview ghost-weapon hide attack2
// ge:keywords tap switch-back film ghost-fade opacity-chart
// ge:skill shooter render labs
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "console/console.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/quick_fire.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/labs/staged.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace hud = shooters_edge::hud;
namespace ls = ge::lab::liquid_scene;
namespace es = ge::lab::explosion_scene;

constexpr double kTic = 0.013;  // a server frame, s (77 Hz, as the window's)

// The halls with a deathmatch spot facing the far wall and two wall torches.
bool halls_pack(std::vector<uint8_t>& out, std::string& error) {
  ge::brush::Fixture f = ge::brush::sample_fixture();
  const auto add = [&f](std::initializer_list<std::pair<std::string, std::string>> pairs) {
    ge::brush::Entity e;
    for (const auto& p : pairs) e.pairs.push_back(p);
    f.entities.push_back(e);
  };
  add({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}});
  add({{"classname", "light_torch_small_walltorch"}, {"origin", "24 120 136"}, {"light", "200"}});
  add({{"classname", "light_torch_small_walltorch"}, {"origin", "488 264 136"}, {"light", "200"}});
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(f, ge::brush::Family::v29, &bsp, &error)) return false;
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &out, &error);
}

const char* weapon_name(hud::Weapon w) {
  constexpr const char* kNames[] = {"melee",         "shotgun",  "double shotgun",  "nailgun",
                                    "heavy nailgun", "grenades", "rocket launcher", "beam gun"};
  return kNames[static_cast<int>(w)];
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
  if (!map ||
      !cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error)) {
    lab.fail(error, "The map should load under the baseline mod. Skill: shooter.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-quickweapon", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  // The weapons in hand: a game folder's own (its packs' first-person models), else ours.
  std::vector<ge::viewmodel::HeldModel> weapons;
  bool at_eye = false;
  if (!folder.empty())
    at_eye = cl::load_held_set(cl::classic_adapter(), files, {}, weapons);
  else if (!shooters_edge::view::load_weapon_models(lab.arg("models"), weapons, &error))
    lab.note("no weapon is drawn: " + error);
  shooters_edge::look::Picture look;
  ge::console::Console look_con;
  shooters_edge::look::add_cvars(look_con, look);
  look_con.execute("r_look classic");
  look_con.execute(lab.arg("cvars"));
  look_con.execute(lab.cvars());
  cl::WorldDrawPtr world;
  ge::gpu::Target out;
  if (device) {
    world = cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                  look, weapons, error, {l.server->models, {}});
    if (!world) {
      lab.fail(error, "Read the gpu: lines above. Skill: render.");
      return;
    }
    out = ge::gpu::create_target(*device, "se-quickweapon.final", w, h);
  } else {
    lab.note("No GPU adapter on this machine: no pictures.");
  }
  // The player's controls and their cvars, as the window registers them.
  ge::console::Console con;
  cl::ControlSettings controls;
  cl::add_cvars(con, controls);
  con.execute("cl_quickweapon 1");
  con.execute(lab.arg("quick"));
  cl::QuickFire& quick = controls.quick;

  cl::ViewState shown;
  cl::FxState fx_state;
  fx_state.on = cl::fx_switches(look);
  cl::FxFrame fx;
  cl::HeldState held;
  held.hand.at_eye = at_eye;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    fx_state.palette.assign(pal.begin(), pal.end());
  }
  const shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::HandFrame hand;
  double t = 0;
  // The shotgun, the double shotgun, the launcher and the beam gun (rules/common_decl.hpp's W_ bits),
  // a few rockets and cells.
  const auto arm = [&] {
    cl::set_field_named(l, 1, "items", 1 + 2 + 32 + 64 + 4096);
    if (cl::field_named(l, 1, "ammo_shells") < 1) cl::set_field_named(l, 1, "ammo_shells", 25);
  };
  arm();
  cl::set_field_named(l, 1, "ammo_shells", 25);
  cl::set_field_named(l, 1, "ammo_rockets", 5);
  cl::set_field_named(l, 1, "ammo_cells", 30);
  // One server frame with the keys through the quick weapon, the hand stepped and its look laid on.
  int own_impulse = 0;
  const auto frame = [&](bool attack, bool attack2) {
    arm();
    cl::Moves m;
    m.attack = attack;
    m.attack2 = attack2;
    m.impulse = std::exchange(own_impulse, 0);
    cl::quick_moves(quick, cl::status_of(l, t), t, m);
    sh::MoveCmd cmd;
    cmd.msec = 13;
    cmd.angles = {l.view.x, l.view.y, 0};
    if (m.attack) cmd.buttons |= sh::kButtonAttack;
    l.impulse = m.impulse;
    cl::listen_frame(l, cmd);
    t += kTic;
    cl::frame_view(l, shown, 1, t, kTic);
    fx_state.models = device ? &cl::world_models(*world) : nullptr;
    cl::step_fx(l, shown, fx_state, t, kTic, fx);
    held.show = cl::quick_show(quick, cl::status_of(l, t), t);
    hand = cl::step_held(l, shown, held, view_settings, weapons, t, kTic);
    if (hand.flashing)
      fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
    cl::quick_hand(quick, hand);
  };
  uint64_t drawn = 0;  // the frames drawn: each its own (the ghost's pattern moves with it)
  const auto picture = [&]() -> gl::Picture {
    if (!device) {
      gl::Picture none = gl::create_picture(static_cast<int>(w), static_cast<int>(h), {40, 40, 48});
      gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      return none;
    }
    ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    cl::FxFrame still = fx;
    still.births.clear();
    // Several frames on the same moment: the ghost's screen door moves frame to frame and the
    // TAA-U history averages it see-through, as the window shows it.
    for (int k = 0; k < 8; ++k, view.frame = ++drawn)
      cl::draw_world(*device, *world, view, still, hand.drawn ? &hand : nullptr, kTic, {},
                     out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
  };

  own_impulse = 2;  // the shotgun in hand
  for (int i = 0; i < 120; i++) frame(false, false);
  // AT REST: the preview, the hidden hand, the plain hand.
  struct Rest {
    const char* id;
    const char* cvars;
    const char* title;
    const char* caption;
  };
  const Rest kRest[] = {
      {"rest-preview", "cl_quickweapon 1; cl_quickweapon_preview 1; cl_quickweapon_hide 0",
       "at rest: the launcher +attack would fire, see-through",
       "cl_quickweapon 1 and cl_quickweapon_preview 1: the shotgun is in hand, and the rocket "
       "launcher +attack would fire is drawn in its place, see-through."},
      {"rest-hide", "cl_quickweapon 1; cl_quickweapon_preview 0; cl_quickweapon_hide 1",
       "at rest: the hand hidden",
       "cl_quickweapon_hide 1 with the preview off: no weapon is drawn while +attack would switch "
       "to another."},
      {"rest-plain", "cl_quickweapon 0", "at rest: the plain hand",
       "cl_quickweapon 0: the shotgun in hand, drawn as it always was."},
  };
  for (const Rest& r : kRest) {
    con.execute(r.cvars);
    frame(false, false);
    lab.shot({r.id,
              r.title,
              r.caption,
              "the weapon in hand: a see-through launcher, nothing, or the solid shotgun",
              {{"map", map_name}}},
             picture(), gl::Det::measured);
  }
  con.execute("cl_quickweapon 1; cl_quickweapon_preview 1; cl_quickweapon_hide 0");
  con.execute(lab.arg("quick"));
  for (int i = 0; i < 40; i++) frame(false, false);

  gl::LineChart lc;
  lc.frame = {"a tap of +attack: the preview's opacity and the hand put away",
              "cl_quickweapon_preview 1, one frame of +attack", "t from the press (s)",
              "share (opacity: 1 solid; put away: 1 out of view)"};
  lc.spikes = false;
  gl::LabTable table{
      "each tap", {"key", "fired", "shot after (ms)", "back in hand after (ms)", "spent"}, {}};
  // A TAP of `key` (0: +attack, 1: +attack2): one frame held, then filmed until the weapon kept
  // is back in hand and has rested.
  const auto tap = [&](int key, const std::string& id) {
    const hud::Status before = cl::status_of(l, t);
    std::vector<gl::Picture> sheet, gif;
    std::vector<std::string> names;
    const double start = t;
    double shot_ms = -1, back_ms = -1, next_draw = t;
    hud::Weapon fired = before.weapon;
    gl::Trace opacity{"opacity", {}, {}}, lowered{"put away", {}, {}};
    double lowered_max = 0;
    int shown_changes = 0;
    auto shown_was = hand.weapon;
    for (int i = 0; i < 160; ++i) {
      frame(i == 0 && key == 0, i == 0 && key == 1);
      const hud::Status s = cl::status_of(l, t);
      const int ms = static_cast<int>(std::lround((t - start) * 1000));
      opacity.x.push_back(t - start);
      opacity.y.push_back(hand.opacity);
      lowered.x.push_back(t - start);
      lowered.y.push_back(hand.lowered);
      lowered_max = std::max(lowered_max, hand.lowered);
      shown_changes += hand.weapon != shown_was ? 1 : 0;
      shown_was = hand.weapon;
      if (shot_ms < 0 && s.ammo != before.ammo) {
        shot_ms = ms;
        fired = s.weapon;
      }
      if (shot_ms >= 0 && back_ms < 0 && s.weapon == before.weapon) back_ms = ms;
      if (t < next_draw) continue;
      next_draw += 1.0 / 30;
      const gl::Picture pic = picture();
      gif.push_back(gl::shrink(pic, 360));
      if (sheet.size() < 16 && gif.size() % 4 == 1) {  // every 4th thirtieth: 2 s on the sheet
        sheet.push_back(gl::shrink(pic, 320));
        names.push_back(std::to_string(ms) + " ms: " +
                        (hand.drawn ? std::string(weapon_name(static_cast<hud::Weapon>(
                                          static_cast<int>(hand.weapon)))) +
                                          (hand.opacity < 1 ? " (ghost)" : "")
                                    : std::string("none")));
      }
    }
    const hud::Status after = cl::status_of(l, t);
    std::string spent;
    const char* kAmmo[] = {"shells", "nails", "rockets", "cells"};
    for (size_t k = 0; k < 4; ++k)
      if (after.ammo[k] != before.ammo[k])
        spent += (spent.empty() ? "" : ", ") + std::to_string(before.ammo[k] - after.ammo[k]) +
                 " " + kAmmo[k];
    table.rows.push_back({key == 0 ? "+attack" : "+attack2", weapon_name(fired),
                          std::to_string(static_cast<int>(shot_ms)),
                          std::to_string(static_cast<int>(back_ms)), spent});
    lab.metric(id + ".shot-ms", shot_ms, "ms");
    lab.metric(id + ".back-ms", back_ms, "ms");
    lab.metric(id + ".fired", static_cast<double>(static_cast<int>(fired) + 1), "weapon");
    lab.metric(id + ".kept", after.weapon == before.weapon ? 1 : 0, "bool");
    // The hand: how far it was put away, and how often the weapon drawn changed (+attack: never).
    lab.metric(id + ".lowered-max", lowered_max, "share");
    lab.metric(id + ".shown-changes", shown_changes, "count");
    if (key == 0) {
      lc.traces.push_back(opacity);
      lc.traces.push_back(lowered);
      if (shot_ms >= 0) lc.marks.push_back({shot_ms / 1000, "shot"});
      if (back_ms >= 0) lc.marks.push_back({back_ms / 1000, "shotgun back"});
    }
    for (size_t k = 0; k < 4; ++k)
      lab.metric(id + "." + kAmmo[k], before.ammo[k] - after.ammo[k], "spent");
    lab.shot({id,
              key == 0 ? "a tap of +attack: the ghost turns solid, fires, and fades back"
                       : "a tap of +attack2: the beam gun fired, the shotgun back",
              key == 0 ? "One frame of +attack under cl_quickweapon 1: the see-through launcher "
                         "turns solid, the server brings it up and fires one rocket, and the "
                         "shotgun (cl_quickweapon_default \"2 1\") is asked for back while the "
                         "launcher fades back to its ghost; no weapon switch is drawn."
                       : "One frame of +attack2 (cl_quickweapon_prio2 \"8 3 2\"): the beam gun "
                         "brought up and fired once, the shotgun back.",
              key == 0 ? "the launcher in every frame, solid as it fires, then see-through again"
                       : "the beam gun brought up, one shot, then the shotgun in hand again",
              {{"map", map_name}}},
             es::sheet_of(sheet, names, 4), gl::Det::measured);
    lab.attach(id + ".gif", gl::encode_gif_frames(gif, 3), gl::Det::measured);
    for (int i = 0; i < 60; i++) frame(false, false);  // rest before the next
  };
  tap(0, "tap-attack");
  tap(1, "tap-attack2");
  lab.table(table);
  const gl::Chart chart = gl::line_chart(lc);
  lab.shot({"tap-chart",
            chart.title,
            "The +attack tap frame by frame: the preview's opacity (0.5 the ghost, 1 solid) rises "
            "to solid as the key is pressed, holds through the shot and eases back to the ghost "
            "once let go; how far the hand is put away stays at 0, since the launcher never leaves "
            "it while the shotgun is brought back.",
            "opacity smooth both ways; put away flat at 0",
            {{"map", map_name}}},
           gl::to_picture(chart.plot), gl::Det::measured);
  lab.metric("weapons.pack", at_eye ? 1 : 0, "");
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-quickweapon",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/quick_fire over plugins/shooter/quick_weapon (the quick weapon)",
    "shooter",
    "Does a tap of +attack fire one shot of the best weapon carried and put the kept one back, "
    "and does the hand show which weapon a press would fire?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"models", "games/shooters_edge/content/models/view", "the held weapons' folder"},
     {"game", "", "a game folder of packs (private pictures); empty: the fixture halls"},
     {"map", "start", "with --game: the map's name"},
     {"cvars", "", "the look's cvars, as the console takes them (after r_look classic)"},
     {"quick", "", "the quick weapon's cvars for the taps (cl_quickweapon_prio \"6 7\", say)"}},
    "--size=240,135",
    run,
};
GE_LAB(kSpec);

}  // namespace
