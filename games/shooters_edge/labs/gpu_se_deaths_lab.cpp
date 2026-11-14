// LAB `se-deaths`: DO MONSTERS DIE AS THE CLASSIC GAME'S DO? A campaign on the listen server
// (client/listen.hpp) with a single-player program (our campaign in C++, games/shooters_edge/
// rules; `--progs=pack` the packs' own progs.dat). For each kind in `--kinds`, on a fresh map,
// the map's first monster of that kind is found and the player stood `--range` units from it,
// looking at it, unseen (no-target) and unhurt (god mode), and shoots it twice over:
//
// - DEATH: the monster left on 1 health, one shotgun shot: the body plays its death frames and
//   lies on the last, bleeding where the pellets struck.
// - GIB: the quad on and the double-barrelled shotgun: the body bursts, its pieces and its head
//   thrown, each piece leaving a trail of blood as it flies, blood bursting where it stood.
//
// Every server frame for `--seconds` after the shot is read (deterministic: the server's state is
// exact): the body's frame and model, how many pieces fly and how far, and the specks of blood the
// window's effects throw that frame (client/world_fx.hpp's births, red ones). A table per kind; a
// chart of the body's frame and the pieces' height against time.
//
// With `--game=<base>/<folder>` (a game folder of packs, `--map` one of its maps) it is that map,
// for PRIVATE pictures only: each shot filmed from the player's eyes at 30 frames a second, a sheet
// of moments and a GIF. Without `--game` it is the fixture halls (plugins/brush) with one soldier
// of our campaign program standing in them: the numbers alone (no models in the fixture), so the
// smoke run checks the death and the gibbing everywhere.
//
// ge:summary Lab: monsters shot dead and gibbed on a real map: death frames, gibs, trails, blood
// ge:keywords lab se-deaths death death-frames corpse gib gibs gibbed head blood blood-trail
// ge:keywords quad super-shotgun monsters campaign throw-gib meat-spray
// ge:skill shooter labs
// ge:test tests/shooters_edge_listen_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "progs/file.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace es = ge::lab::explosion_scene;

constexpr float kFlMonster = 32, kFlGod = 64, kFlNoTarget = 128;  // the program's `flags` bits
constexpr float kItShotgun = 1, kItSuperShotgun = 2, kItQuad = 4194304;  // its `items` bits

// The fixture halls with a start in the hall facing east and one soldier across it, in a pack.
bool fixture_files(cl::SearchPath& files, std::string& error) {
  br::Fixture f = br::sample_fixture();
  br::Entity start, soldier;
  start.pairs = {{"classname", "info_player_start"}, {"origin", "96 192 40"}, {"angle", "0"}};
  soldier.pairs = {{"classname", "monster_army"}, {"origin", "320 192 40"}, {"angle", "180"}};
  f.entities.push_back(start);
  f.entities.push_back(soldier);
  std::vector<uint8_t> map, pak;
  if (!br::write_bsp(f, br::Family::v29, &map, &error)) return false;
  if (!ge::vfs::write_pak({{"maps/halls.bsp", map}}, &pak, &error)) return false;
  return cl::add_archive(files, "pak0.pak", std::move(pak), error);
}

// The model's name an entity shows ("" for none).
std::string model_of(const cl::Listen& l, int e) {
  const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
  if (ed.free || ed.modelindex <= 0 || ed.modelindex >= int(l.server->models.size())) return "";
  return l.server->models[size_t(ed.modelindex)];
}

// A piece of a body: a gib or a head (by its model's name).
bool is_piece(const std::string& m) {
  return m.find("gib") != std::string::npos || m.find("/h_") != std::string::npos;
}

// The living monsters whose model is `kind`'s ("soldier": progs/soldier.mdl), in entity order.
std::vector<int> monsters_of(const cl::Listen& l, const std::string& kind) {
  std::vector<int> out;
  for (int e = sh::kMaxClients + 1; e < l.server->sv.num_edicts; e++) {
    const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
    if (ed.free) continue;
    const int flags = static_cast<int>(cl::field_named(l, e, "flags"));
    if ((flags & static_cast<int>(kFlMonster)) == 0) continue;
    if (cl::field_named(l, e, "health") <= 0) continue;
    if (model_of(l, e) == "progs/" + kind + ".mdl") out.push_back(e);
  }
  return out;
}

// Inside a brush entity's box (a door, a plat, a wall that moves): the map's hull does not hold
// them.
bool in_mover(const cl::Listen& l, const float q[3]) {
  for (int e = 1; e < l.server->sv.num_edicts; e++) {
    const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
    if (ed.free || model_of(l, e).rfind('*', 0) != 0) continue;
    if (q[0] >= ed.absmin.x && q[0] <= ed.absmax.x && q[1] >= ed.absmin.y && q[1] <= ed.absmax.y &&
        q[2] >= ed.absmin.z && q[2] <= ed.absmax.z)
      return true;
  }
  return false;
}

// Where the player stands to shoot `at`: `range` units off along the first of eight ways with
// nothing solid (the map or a brush entity) on the line (the eye's height above the floor kept),
// and his yaw toward it.
bool stand_off(const cl::Listen& l, sh::Vec3f at, float range, sh::Vec3f& out, float& yaw) {
  const br::Map& m = l.map->bsp.map;
  for (int k = 0; k < 8; k++) {
    const double a = k * 3.14159265358979 / 4;
    const float dx = static_cast<float>(ge::core::libm::cos(a)),
                dy = static_cast<float>(ge::core::libm::sin(a));
    bool clear = true;
    for (float s = 16; s <= range + 24 && clear; s += 8) {
      for (const float z : {0.f, 24.f, -20.f}) {
        const float q[3] = {at.x + dx * s, at.y + dy * s, at.z + z};
        clear = clear && br::hull_contents(m, q, 0, 0) != -2 && !in_mover(l, q);  // solid
      }
    }
    if (!clear) continue;
    out = {at.x + dx * range, at.y + dy * range, at.z};
    yaw = static_cast<float>(k * 45 + 180);
    return true;
  }
  return false;
}

struct Row {
  double t = 0;
  int frame = 0;
  std::string model;
  int pieces = 0;       // gibs and heads flying or lying
  float piece_top = 0;  // the highest piece above the body's place, units
  float piece_far = 0;  // the farthest piece across the plan from it, units
  int specks = 0;       // red specks the window's effects threw this frame
  int all_specks = 0;   // every speck they threw
};

// A speck of blood: redder than it is green or blue by half again.
bool red(const ge::render::Particle& p) {
  return p.tint[0] > 1.5f * p.tint[1] && p.tint[0] > 1.5f * p.tint[2];
}

struct Shot {
  std::string kind;  // the model's name: soldier, dog, ...
  bool gib = false;
  std::vector<Row> rows;
  std::vector<gl::Picture> film;
  bool found = false;
};

struct Setup {
  const cl::SearchPath* files = nullptr;
  const ge::progs::Progs* progs = nullptr;
  std::string map;
  float range = 192;
  double seconds = 2.5;
  ge::gpu::Device* device = nullptr;  // null: no pictures
  std::vector<uint8_t> palette;       // the pack's (gfx/palette.lmp)
  ge::particles::Book book;           // the game's effects (particles/*.fx)
  shooters_edge::look::Picture look;
  uint32_t w = 480, h = 300;
};

void shoot(gl::Lab& lab, const Setup& s, Shot& shot) {
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(*s.files, s.map, error);
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  lo.console.emplace_back("deathmatch 0");  // as NEW GAME starts it: the monsters stay
  cl::Listen l;
  if (!map || !cl::listen_start(l, *s.progs, std::move(map), lo, error)) {
    lab.fail("se-deaths: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  const double tic = cl::listen_tic(l);
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});  // the monsters stand
  // The first of them with a clear way to it from `range` units off (or less, down to a half).
  int victim = -1;
  sh::Vec3f at, stand;
  float yaw = 0;
  for (const int e : monsters_of(l, shot.kind)) {
    at = l.server->sv.edicts[size_t(e)].origin;
    for (const float share : {1.f, 0.75f, 0.5f})
      if (victim < 0 && stand_off(l, at, s.range * share, stand, yaw)) victim = e;
    if (victim >= 0) break;
  }
  if (victim < 0) {
    lab.note("se-deaths: " + s.map + " has no " + shot.kind + " in the open: skipped");
    return;
  }
  const float range = static_cast<float>(ge::core::hypot(stand.x - at.x, stand.y - at.y));
  shot.found = true;
  // The player: unseen, unhurt, the gun the shot needs, the quad for the gibbing. `flags` and
  // `health` are the loop's (the server mirrors them into the program each call): set there.
  sh::Edict& me = l.server->sv.edicts[1];
  me.flags |= static_cast<int>(kFlGod) | static_cast<int>(kFlNoTarget);
  float items = kItShotgun + kItSuperShotgun;
  if (shot.gib) items += kItQuad;
  cl::set_field_named(l, 1, "items", items);
  cl::set_field_named(l, 1, "ammo_shells", 50);
  cl::set_field_named(l, 1, "currentammo", 50);
  const float now = cl::global_named(l, "time");
  if (shot.gib) cl::set_field_named(l, 1, "super_damage_finished", now + 30);
  if (!shot.gib) l.server->sv.edicts[size_t(victim)].health = 1;
  // Aim: the eye 22 above the feet, at the body's middle.
  const float dz = at.z - (stand.z + 22);
  const float pitch = -static_cast<float>(ge::core::libm::atan2(dz, range) * 180.0 /
                                          3.14159265358979);  // down positive
  l.view = {pitch, yaw, 0};
  sh::MoveCmd cmd;
  cmd.msec = static_cast<uint8_t>(std::lround(tic * 1000));
  cmd.angles = {pitch, yaw, 0};
  cmd.impulse = shot.gib ? 3 : 2;  // the gun
  cl::ViewState shown;
  cl::FxState fx_state;
  fx_state.palette = s.palette;  // as the window sets them: the pack's colours, the game's effects
  fx_state.book = &s.book;
  cl::FxFrame fx;
  double t = 0;
  for (int i = 0; i < 60; i++) {  // the gun comes up; he stays put
    me.origin = stand;
    me.velocity = {};
    cl::listen_frame(l, cmd);
    cmd.impulse = 0;
    t += tic;
    cl::frame_view(l, shown, 1, t, tic);
    cl::step_fx(l, shown, fx_state, t, tic, fx);
  }
  cl::WorldDrawPtr drawn;
  ge::gpu::Target out;
  if (s.device != nullptr) {
    drawn = cl::create_world_draw(*s.device, *s.files, *l.map, s.w, s.h,
                                  wgpu::TextureFormat::RGBA8Unorm, s.look, {}, error,
                                  {l.server->models, {}});
    if (!drawn) lab.note("se-deaths: no pictures: " + error);
    out = ge::gpu::create_target(*s.device, "se-deaths.final", s.w, s.h);
  }
  // The models drawn: what the pieces' trails read their model files' flags from.
  if (drawn) fx_state.models = &cl::world_models(*drawn);
  const sh::Vec3f body = at;
  double next_draw = t;
  const int steps = static_cast<int>(s.seconds / tic);
  for (int i = 0; i < steps; i++) {
    me.origin = stand;
    me.velocity = {};
    cmd.buttons = i == 0 ? sh::kButtonAttack : 0;
    cl::listen_frame(l, cmd);
    t += tic;
    cl::frame_view(l, shown, 1, t, tic);
    cl::step_fx(l, shown, fx_state, t, tic, fx);
    Row r;
    r.t = (i + 1) * tic;
    r.frame = static_cast<int>(cl::field_named(l, victim, "frame"));
    r.model = model_of(l, victim);
    for (int e = sh::kMaxClients + 1; e < l.server->sv.num_edicts; e++) {
      if (!is_piece(model_of(l, e))) continue;
      const sh::Edict& p = l.server->sv.edicts[size_t(e)];
      r.pieces++;
      r.piece_top = std::max(r.piece_top, p.origin.z - body.z);
      r.piece_far =
          std::max(r.piece_far,
                   static_cast<float>(ge::core::hypot(p.origin.x - body.x, p.origin.y - body.y)));
    }
    for (const ge::render::Particle& p : fx.births) {
      r.all_specks++;
      r.specks += int(red(p));
    }
    shot.rows.push_back(r);
    if (!drawn || t < next_draw) continue;
    next_draw += 1.0 / 30;
    const ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    cl::draw_world(*s.device, *drawn, view, fx, nullptr, 1.0 / 30, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*s.device, out);
    shot.film.push_back(gl::Picture{int(img.width), int(img.height), img.rgba});
  }
}

void report(gl::Lab& lab, const Shot& shot, const std::string& map) {
  const std::string id = shot.kind + (shot.gib ? ".gib" : ".death");
  std::set<int> frames;
  int last_frame = -1, changes = 0, pieces = 0, specks = 0, all_specks = 0;
  float top = 0, far = 0;
  double settled = 0;
  for (const Row& r : shot.rows) {
    frames.insert(r.frame);
    if (last_frame >= 0 && r.frame != last_frame) {
      changes++;
      settled = r.t;
    }
    last_frame = r.frame;
    pieces = std::max(pieces, r.pieces);
    top = std::max(top, r.piece_top);
    far = std::max(far, r.piece_far);
    specks += r.specks;
    all_specks += r.all_specks;
  }
  lab.metric(id + ".frame-changes", changes, "count");
  lab.metric(id + ".last-change", settled, "s");
  lab.metric(id + ".pieces", pieces, "count");
  lab.metric(id + ".piece-top", top, "units");
  lab.metric(id + ".piece-far", far, "units");
  lab.metric(id + ".blood-specks", specks, "count");
  lab.metric(id + ".specks", all_specks, "count");
  gl::LabTable table{id + ": every tenth of a second after the shot",
                     {"t (s)", "frame", "model", "pieces", "top (u)", "far (u)", "red specks"},
                     {}};
  int specks_tenth = 0;
  for (size_t i = 0; i < shot.rows.size(); i++) {
    const Row& r = shot.rows[i];
    specks_tenth += r.specks;
    if ((i + 1) % 8 != 0 && i + 1 != shot.rows.size()) continue;
    table.rows.push_back({ge::core::num::to_fixed(r.t, 2), std::to_string(r.frame), r.model,
                          std::to_string(r.pieces), ge::core::num::to_fixed(r.piece_top, 0),
                          ge::core::num::to_fixed(r.piece_far, 0), std::to_string(specks_tenth)});
    specks_tenth = 0;
  }
  lab.table(table);

  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {(shot.kind + (shot.gib ? " GIBBED (QUAD, DOUBLE SHOTGUN)" : " SHOT DEAD")),
              map + ": the body's frame, its pieces' height and the blood thrown", "t (s)",
              "frame / units / specks"};
  gl::Trace fr{"body frame", {}, {}}, tp{"highest piece / 10 (units)", {}, {}},
      bl{"red specks this frame", {}, {}};
  for (const Row& r : shot.rows) {
    fr.x.push_back(r.t);
    fr.y.push_back(r.frame);
    tp.x.push_back(r.t);
    tp.y.push_back(r.piece_top / 10.0);
    bl.x.push_back(r.t);
    bl.y.push_back(r.specks);
  }
  ch.traces = {fr, tp, bl};
  lab.shot({id + ".chart",
            ch.frame.title,
            "Every server frame for the seconds after the shot: the frame the body shows, the "
            "highest of its flying pieces above where it stood (a tenth), and the red specks the "
            "window's effects threw that frame.",
            shot.gib ? "the pieces rise and fall, specks all through their flight"
                     : "the frame climbing through the death frames and holding the last",
            {{"map", map}}},
           gl::to_picture(gl::line_chart(ch).plot));

  if (shot.film.empty()) return;
  std::vector<gl::Picture> sheet, gif;
  std::vector<std::string> names;
  const size_t n = shot.film.size();
  for (size_t k = 0; k < 8; k++) {
    const size_t i = std::min(n - 1, k * (n - 1) / 7 / 2 + (k >= 4 ? (k - 3) * (n - 1) / 8 : 0));
    sheet.push_back(gl::shrink(shot.film[i], 320));
    names.push_back(ge::core::num::to_fixed(double(i) / 30.0, 2) + " s");
  }
  for (const gl::Picture& p : shot.film) gif.push_back(gl::shrink(p, std::min(p.width, 960)));
  lab.shot({id + ".sheet",
            id,
            shot.gib ? "The quad and the double shotgun at a " + shot.kind +
                           ": the moments after the shot from the player's eyes."
                     : "One shotgun shot at a " + shot.kind +
                           " on 1 health: the moments after the shot from the player's eyes.",
            shot.gib ? "the body gone in a burst of blood, its pieces flying with red trails"
                     : "the body falling through its death frames and lying",
            {{"map", map}}},
           es::sheet_of(sheet, names, 4), gl::Det::measured);
  lab.attach(id + ".gif", gl::encode_gif_frames(gif, 3), gl::Det::measured);
}

void run(gl::Lab& lab) {
  const std::string folder = lab.arg("game");
  std::string error;
  cl::SearchPath files;
  if (folder.empty()) {
    if (!fixture_files(files, error)) {
      lab.fail("se-deaths: the fixture halls: " + error, "Read the reason above. Skill: brush.");
      return;
    }
  } else {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("se-deaths: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
      return;
    }
  }
  ge::progs::Progs progs;
  if (lab.arg("progs") == "pack") {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = cl::find_file(files, "progs.dat", store);
    if (!ge::progs::load_progs(bytes.data(), bytes.size(), progs, error, ge::progs::every_form())) {
      lab.fail("se-deaths: the pack's progs.dat: " + error,
               "Pass a game folder whose packs hold a progs.dat. Skill: progs.");
      return;
    }
  } else {
    if (!shooters_edge::server::campaign_mod(progs, error)) {
      lab.fail("se-deaths: the campaign: " + error, "Fix the campaign's rules. Skill: progs.");
      return;
    }
  }
  Setup s;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    s.palette.assign(pal.begin(), pal.end());
    std::vector<std::string> faults;
    cl::load_effects(files, s.book, faults);
    for (const std::string& f : faults) lab.note("particles: " + f);
  }
  s.files = &files;
  s.progs = &progs;
  s.map = folder.empty() ? "halls" : lab.arg("map");
  s.range = static_cast<float>(lab.arg_num("range"));
  s.seconds = lab.arg_num("seconds");
  if (const auto size = lab.arg_nums("size"); size.size() == 2 && size[0] >= 64 && size[1] >= 64) {
    s.w = static_cast<uint32_t>(size[0]);
    s.h = static_cast<uint32_t>(size[1]);
  }
  std::unique_ptr<ge::gpu::Device> device;
  if (!folder.empty()) {
    device = ge::gpu::create_device({.label = "se-deaths", .validation = 1});
    if (!device) lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
    s.device = device.get();
    ge::console::Console con;
    shooters_edge::look::add_cvars(con, s.look);
    con.execute(lab.arg("cvars"));
    con.execute(lab.cvars());
  }
  std::vector<std::string> kinds;
  const std::string list = folder.empty() ? "soldier" : lab.arg("kinds");
  for (size_t a = 0; a < list.size();) {
    const size_t b = std::min(list.find(',', a), list.size());
    if (b > a) kinds.push_back(list.substr(a, b - a));
    a = b + 1;
  }
  for (const std::string& kind : kinds)
    for (const bool gib : {false, true}) {
      Shot shot;
      shot.kind = kind;
      shot.gib = gib;
      shoot(lab, s, shot);
      if (shot.found) report(lab, shot, s.map);
    }
}

const gl::LabSpec kSpec{
    "se-deaths",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the campaign's monsters' deaths (games/shooters_edge/rules/campaign_ai.cpp, monster_die) and "
    "their gibs and blood as the window draws them (client/world_fx.hpp, world_models.hpp)",
    "shooter",
    "Do monsters die as the classic game's do? Each kind shot dead (its death frames) and gibbed "
    "by the quad and the double shotgun (its pieces, their blood trails, the blood), on a real "
    "map, filmed from the player's eyes.",
    {
        {"game", "", "a game folder (<base>/<folder>); empty: the fixture halls and one soldier"},
        {"map", "e1m1", "with --game: the map"},
        {"progs", "", "pack: the packs' own progs.dat; else our campaign program"},
        {"kinds", "soldier,dog", "with --game: the monsters by model name (soldier, dog, ogre...)"},
        {"range", "192", "units from the monster the player stands"},
        {"seconds", "2.5", "seconds read (and filmed) after the shot"},
        {"size", "480,300", "with --game: the film's width and height in pixels"},
        {"cvars", "", "with --game: picture cvars (r_look ...)"},
    },
    "--seconds=1.5",
    run,
};
GE_LAB(kSpec);

}  // namespace
