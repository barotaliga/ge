// LAB `se-weapongroups`: WEAPON GROUPS IN THE GAME'S OWN FRAME. The baseline mod as a listen
// server on a map, the player handed every weapon with its ammunition, the shotgun in hand and
// `cl_weapongroups 1` (client/weapon_groups.hpp over shooter/weapon_groups.hpp). Each frame goes
// through the weapon groups as the window's does: the number key into the weapon impulse, the
// attack folding the group, the group's look laid on the HUD's status and the HUD drawn over the
// world with the weapon in hand.
//
// What it shows: FOLDED at rest, the shotgun alone in the HUD's group column with the gold mark
// (the double shotgun is carried too); KEY 1, the launchers' group open with the rocket launcher
// lit; KEY 1 AGAIN, the grenade launcher lit; the ATTACK, the group folding down into the grenade
// launcher, filmed a thirtieth of a second at a time; then KEY 3 left alone, folding by itself
// kOpenS later. A table lists each step's weapon in hand and the column's cells; a chart follows
// the column's open share through the sequence. Deterministic metrics, the pictures measured.
//
// The default map is the fixture halls (plugins/brush's sample) with the free HUD art, so its
// pictures may go anywhere. `--game=<folder>` (a game folder of packs, `--map=<name>`) draws the
// player's own map with the packs' own weapons and HUD pictures, for PRIVATE pictures only.
//
// ge:summary Lab: weapon groups in the game's frame: a number key opens its group on the HUD, a
// ge:summary second press takes the next, the attack folds it
// ge:keywords lab se-weapongroups weapon-groups cl_weapongroups wgroup group-column fold mark
// ge:skill shooter ui labs
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "console/console.hpp"
#include "core/num_text.hpp"
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
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/weapon_groups.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/labs/staged.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
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

// The column's cells as words: "rocket launcher*, grenades" (* lit), "+" for the mark.
std::string column_words(const hud::GroupShow& g) {
  std::string out;
  for (int i = 0; i < g.count; ++i) {
    if (g.open <= 0 && i != g.lit) continue;
    if (!out.empty()) out += ", ";
    out += weapon_name(g.weapons[static_cast<size_t>(i)]);
    if (i == g.lit) out += "*";
  }
  if (g.more && g.open <= 0) out += " +";
  return out.empty() ? "-" : out;
}

// The right quarter of `p` from 15% to 85% of its height (where the column stands), doubled
// nearest: the column large enough to read on a sheet.
gl::Picture column_close_up(const gl::Picture& p) {
  const int x0 = p.width * 3 / 4, y0 = p.height * 15 / 100;
  const int cw = p.width - x0, ch = p.height * 70 / 100;
  gl::Picture out{cw * 2, ch * 2, std::vector<uint8_t>(static_cast<size_t>(cw * ch) * 16)};
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x) {
      const auto from = (static_cast<size_t>(y0 + y / 2) * static_cast<size_t>(p.width) +
                         static_cast<size_t>(x0 + x / 2)) *
                        4;
      const auto to =
          (static_cast<size_t>(y) * static_cast<size_t>(out.width) + static_cast<size_t>(x)) * 4;
      for (size_t c = 0; c < 4; ++c) out.rgba[to + c] = p.rgba[from + c];
    }
  return out;
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
  auto device = ge::gpu::create_device({.label = "se-weapongroups", .validation = 1});
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
    out = ge::gpu::create_target(*device, "se-weapongroups.final", w, h);
  } else {
    lab.note("No GPU adapter on this machine: no pictures.");
  }
  // The HUD: the packs' own pictures with a game folder, else the free set.
  hud::Hud hd = hud::make_hud();
  hud::HudArt free_art = hud::free_art();
  free_art.finish();
  hud::HudArt classic_art;
  if (!folder.empty()) {
    const auto find = [&files](std::string_view p, std::vector<uint8_t>& store) {
      return cl::find_file(files, p, store);
    };
    std::string why;
    if (!hud::classic_art_from_packs(find, &classic_art, &why)) lab.note(why);
  }
  const hud::HudArt& art = hud::pick_art(folder.empty() ? "free" : "auto", classic_art, free_art);
  ge::console::Console hud_con;
  hud::add_cvars(hud_con, hd);
  hud_con.execute(lab.arg("hudcvars"));
  // The player's controls and their cvars, as the window registers them.
  ge::console::Console con;
  cl::ControlSettings controls;
  cl::add_cvars(con, controls);
  con.execute("cl_weapongroups 1");
  con.execute(lab.arg("groups"));
  cl::WeaponGroups& groups = controls.groups;
  groups.game = shooters_edge::profiles::classic_game().weapon_groups;

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
  // Every weapon (qc/common/defs.qc's W_ bits: shotgun 1 .. beam gun 64), and ammunition for each.
  const auto arm = [&] {
    cl::set_field_named(l, 1, "items", 1 + 2 + 4 + 8 + 16 + 32 + 64 + 4096);
    for (const char* f : {"ammo_shells", "ammo_nails", "ammo_rockets", "ammo_cells"})
      if (cl::field_named(l, 1, f) < 20) cl::set_field_named(l, 1, f, 40);
  };
  arm();
  // One server frame with the keys through the weapon groups, the hand stepped.
  const auto frame = [&](int key, bool attack) {
    arm();
    cl::Moves m;
    m.attack = attack;
    m.impulse = key;
    cl::group_moves(groups, cl::status_of(l, t), t, m);
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
    hand = cl::step_held(l, shown, held, view_settings, weapons, t, kTic);
    if (hand.flashing)
      fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
  };
  // The HUD's status with the group's look on it.
  const auto status = [&] {
    hud::Status s = cl::status_of(l, t);
    cl::group_status(groups, s, t);
    return s;
  };
  uint64_t drawn = 0;
  const auto picture = [&]() -> gl::Picture {
    if (!device) {
      gl::Picture none = gl::create_picture(static_cast<int>(w), static_cast<int>(h), {40, 40, 48});
      gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      return none;
    }
    ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    view.frame = ++drawn;
    cl::FxFrame still = fx;
    still.births.clear();
    ge::ui::DrawList overlay;
    overlay.begin(static_cast<int>(w), static_cast<int>(h));
    hud::fill_hud(hd, status(), art);
    hd.layout.draw(overlay,
                   hd.layout.frame(static_cast<int>(w), static_cast<int>(h), hud::hud_style()),
                   hud::hud_style());
    hand.lift = hud::status_lift(hd, static_cast<int>(h));
    cl::draw_world(*device, *world, view, still, hand.drawn ? &hand : nullptr, kTic, overlay,
                   out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
  };

  gl::LabTable table{"each step", {"step", "t (s)", "in hand", "column (* lit, + mark)"}, {}};
  gl::Trace open_trace{"open", {}, {}};
  gl::LineChart lc;
  lc.frame = {"the group column's open share", "cl_weapongroups 1: key 1, key 1, fire, key 3",
              "t (s)", "open (1 open .. 0 folded)"};
  lc.spikes = false;
  double t0 = t;
  const auto step = [&](int key, bool attack, int frames) {
    for (int i = 0; i < frames; ++i) {
      frame(i == 0 ? key : 0, attack && i == 0);
      open_trace.x.push_back(t - t0);
      open_trace.y.push_back(status().group.open);
    }
  };
  const auto record = [&](const std::string& id, const std::string& title,
                          const std::string& caption, const std::string& look_for) {
    const hud::Status s = status();
    table.rows.push_back(
        {id, ge::core::num::to_fixed(t - t0, 2), weapon_name(s.weapon), column_words(s.group)});
    lab.metric(id + ".in_hand", static_cast<double>(static_cast<int>(s.weapon) + 1), "weapon");
    lab.metric(id + ".cells", s.group.open > 0 ? s.group.count : 1, "count");
    lab.metric(id + ".mark", s.group.more && s.group.open <= 0 ? 1 : 0, "bool");
    lab.shot({id, title, caption, look_for, {{"map", map_name}}}, picture(), gl::Det::measured);
  };

  frame(4, false);      // the shotguns' group: the shotgun
  step(0, false, 260);  // in hand, the group folded (kOpenS and the fold)
  t0 = t;
  open_trace.x.clear(), open_trace.y.clear();
  record("folded", "folded: the shotgun alone, marked",
         "cl_weapongroups 1 at rest: the HUD's group column shows the weapon in hand alone, the "
         "gold + saying its group (4: the shotguns) holds another weapon carried.",
         "one gun on the right edge with a small gold + on its corner");
  lc.marks.push_back({t - t0, "key 1"});
  step(1, false, 30);
  record("key1", "key 1: the launchers' group opens, the rocket launcher taken",
         "Key 1 picks the best ready weapon of its group (cl_weapongroup1 empty: the game's own, "
         "\"7 6\"): the rocket launcher, lit and framed, the grenade launcher under it.",
         "two launchers stacked on the right edge, the upper one framed");
  lc.marks.push_back({t - t0, "key 1 again"});
  step(1, false, 40);
  record("key1-again", "key 1 again: the grenade launcher",
         "Pressed again while the group is open, key 1 takes the group's next weapon.",
         "the lower launcher framed now, the grenade launcher in hand");
  // The attack: the group folds, filmed.
  lc.marks.push_back({t - t0, "fire"});
  {
    std::vector<gl::Picture> sheet, gif;
    std::vector<std::string> names;
    const double start = t;
    double next_draw = t;
    for (int i = 0; i < 40; ++i) {
      frame(0, i == 0);
      open_trace.x.push_back(t - t0);
      open_trace.y.push_back(status().group.open);
      if (t < next_draw) continue;
      next_draw += 1.0 / 30;
      const gl::Picture pic = picture();
      gif.push_back(gl::shrink(pic, 480));
      if (sheet.size() < 8) {
        sheet.push_back(column_close_up(pic));
        names.push_back(std::to_string(static_cast<int>((t - start) * 1000 + 0.5)) +
                        " ms: " + column_words(status().group));
      }
    }
    lab.shot(
        {"fold",
         "the attack folds the group into the grenade launcher",
         "One frame of +attack: the open group folds over 0.2 s, the rocket launcher shrinking "
         "and fading into the grenade launcher that fired, which keeps the gold mark. Each "
         "cell is the frame's right edge, doubled; the GIF has the whole frame.",
         "the column shrinking down to one gun",
         {{"map", map_name}}},
        es::sheet_of(sheet, names, 4), gl::Det::measured);
    lab.attach("fold.gif", gl::encode_gif_frames(gif, 3), gl::Det::measured);
  }
  record("fired", "after the shot: folded into the grenade launcher",
         "Folded: the weapon in hand alone, marked while its group holds another.",
         "one launcher, a gold + on its corner");
  lc.marks.push_back({t - t0, "key 3"});
  step(3, false, 40);
  record("key3", "key 3: the nailguns, the heavy nailgun first",
         "Key 3's group (\"5 4\"): the heavy nailgun taken and lit, the nailgun under it.",
         "two nailguns on the right edge, the upper one framed");
  step(0, false, 160);
  record("key3-later", "two seconds on: folded by itself",
         "Without the attack the group folds kOpenS (2 s) after the last press.",
         "the heavy nailgun alone, marked");
  lc.traces.push_back(open_trace);
  lab.table(table);
  const gl::Chart chart = gl::line_chart(lc);
  lab.shot({"open-chart",
            chart.title,
            "The column's open share through the steps: 1 while a group is open, falling to 0 over "
            "0.2 s on the attack or 2 s after the last press.",
            "steps to 1 at each key, ramps to 0 after the fire and 2 s after key 3",
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
    "se-weapongroups",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/weapon_groups over plugins/shooter/weapon_groups (weapon groups)",
    "shooter",
    "Does a number key take its group's best weapon, a second press the next, and does the HUD's "
    "column open, fold on the attack and mark a second weapon?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"models", "games/shooters_edge/content/models/view", "the held weapons' folder"},
     {"game", "", "a game folder of packs (private pictures); empty: the fixture halls"},
     {"map", "start", "with --game: the map's name"},
     {"hudcvars", "", "the HUD's cvars (hud_style modern, say)"},
     {"groups", "", "the weapon groups' cvars (cl_weapongroup1 \"6 7\", say)"}},
    "--size=240,135",
    run,
};
GE_LAB(kSpec);

}  // namespace
