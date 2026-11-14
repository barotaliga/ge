// LAB `se-swim`: DOES A SWIMMING PLAYER RUN? A listen server (client/listen.hpp) with the window's
// program (the baseline mod; `--progs=pack` the packs' own progs.dat) puts the player in deep water
// (`--at`, else the point of the map's water with the most water round it), swims him forward for
// `--swim` seconds, lets go for as long again, and reads the frame the program gives his body at
// every server frame. The classic rule is the one the baseline mod keeps (common/player.qc): a
// frame every 0.1 s, the run cycle while he has any speed across the plan (on the ground, in the
// air or in the water), the stand cycle when he has none; nothing looks at the water.
//
// With `--game=<base>/<folder>` (a game folder of packs, `--map` one of its maps with water) it is
// that map, for PRIVATE pictures only: the swimmer drawn as the window draws another player (its
// models, its water, its light), from beside him a few metres off, at `--moments` moments through
// the swim and the drift after it, the frame he shows in each caption. Without `--game` it is the
// fixture halls (plugins/brush, no water), the same run on dry land: the numbers alone (no
// player model in the fixture, so no pictures), so the smoke run checks the cycles everywhere.
//
// The numbers (deterministic: the server's state is exact): the run cycle's frames seen while he
// swims, the frame changes over the run, the share of the still frames at the end in the stand
// cycle, and the water level he swam at. A table gives
// every tenth of a second; a chart the body's frame against time with his speed across the plan.
//
// ge:summary Lab: a player swimming on a real map: the run cycle his body plays, drawn from beside
// ge:keywords lab se-swim swim swimming water player-frame run-cycle stand-cycle animation body
// ge:keywords walkframe player-model drift
// ge:skill shooter labs
// ge:test tests/shooters_edge_listen_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/demo.hpp"
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

constexpr double kTic = 0.013;  // the listen server's frame, s (sv_mintic)

// The body's cycles in the player model (the classic layout).
bool in_run(int f) { return f >= 0 && f <= 11; }
bool in_stand(int f) { return f >= 12 && f <= 28; }

struct Row {
  double t = 0;
  int frame = 0, waterlevel = 0;
  float across = 0, vz = 0;  // speed across the plan and up, units/s
  bool swimming = false;     // the swim's commands (else he has let go)
  sh::Vec3f origin, angles;
};

// The fixture halls with a deathmatch spot in the hall facing east, in a pack.
bool fixture_files(cl::SearchPath& files, std::string& error) {
  br::Fixture f = br::sample_fixture();
  br::Entity spot;
  spot.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}};
  f.entities.push_back(spot);
  std::vector<uint8_t> map, pak;
  if (!br::write_bsp(f, br::Family::v29, &map, &error)) return false;
  if (!ge::vfs::write_pak({{"maps/halls.bsp", map}}, &pak, &error)) return false;
  return cl::add_archive(files, "pak0.pak", std::move(pak), error);
}

// The point of the map's water with the most water round it: on a 16-unit lattice through every
// water leaf, a point with water at it, 24 below and 40 above (the player's box under water),
// counted by the water within 128 units each way across. False when the map has none.
bool deep_water(const br::Map& m, float at[3]) {
  const auto wet = [&](float x, float y, float z) {
    const float q[3] = {x, y, z};
    return br::hull_contents(m, q, 0, 0) == -3;  // water
  };
  int best = -1;
  for (const br::Leaf& leaf : m.leaves) {
    if (leaf.contents != -3) continue;
    const int nx = static_cast<int>((leaf.maxs[0] - leaf.mins[0]) / 16);
    const int ny = static_cast<int>((leaf.maxs[1] - leaf.mins[1]) / 16);
    const int nz = static_cast<int>((leaf.maxs[2] - leaf.mins[2]) / 16);
    for (int i = 0; i < nx; i++)
      for (int j = 0; j < ny; j++)
        for (int k = 0; k < nz; k++) {
          const float x = leaf.mins[0] + 8 + 16.f * static_cast<float>(i);
          const float y = leaf.mins[1] + 8 + 16.f * static_cast<float>(j);
          const float z = leaf.mins[2] + 8 + 16.f * static_cast<float>(k);
          if (!wet(x, y, z) || !wet(x, y, z - 24) || !wet(x, y, z + 40)) continue;
          int n = 0;
          for (int d = 16; d <= 128; d += 16) {
            const float s = static_cast<float>(d);
            n += int(wet(x + s, y, z)) + int(wet(x - s, y, z)) + int(wet(x, y + s, z)) +
                 int(wet(x, y - s, z));
          }
          if (n > best) {
            best = n;
            at[0] = x, at[1] = y, at[2] = z;
          }
        }
  }
  return best >= 0;
}

// The open way across the water from `at` (of the four along the axes): the yaw (degrees) with
// the most water ahead before a wall.
float open_yaw(const br::Map& m, const float at[3]) {
  float best_yaw = 0;
  int best = -1;
  for (int k = 0; k < 4; k++) {
    const float dx = k == 0 ? 1.f : k == 2 ? -1.f : 0.f, dy = k == 1 ? 1.f : k == 3 ? -1.f : 0.f;
    int n = 0;
    for (int step = 1; step <= 32; step++, n++) {
      const float s = 16.f * static_cast<float>(step);
      const float q[3] = {at[0] + dx * s, at[1] + dy * s, at[2]};
      if (br::hull_contents(m, q, 0, 0) != -3) break;
    }
    if (n > best) {
      best = n;
      best_yaw = static_cast<float>(90 * k);
    }
  }
  return best_yaw;
}

// Where an eye `side` units from `at` sees him: across his way (`yaw`, degrees) on either hand,
// then aslant, then behind and ahead, the first with nothing solid on the line to him.
sh::Vec3f eye_beside(const cl::Listen& l, sh::Vec3f at, float yaw, float side) {
  const br::Map& m = l.map->bsp.map;
  for (const float turn : {90.f, -90.f, 45.f, -45.f, 135.f, -135.f, 180.f, 0.f}) {
    const double a = (yaw + turn) * 3.14159265358979 / 180.0;
    const float dx = static_cast<float>(ge::core::libm::cos(a)),
                dy = static_cast<float>(ge::core::libm::sin(a));
    bool clear = true;
    for (int step = 1; 8.f * static_cast<float>(step) <= side && clear; step++) {
      const float s = 8.f * static_cast<float>(step);
      const float q[3] = {at.x + dx * s, at.y + dy * s, at.z + 8};
      clear = br::hull_contents(m, q, 0, 0) != -2;  // solid
    }
    if (clear) return {at.x + dx * side, at.y + dy * side, at.z + 8};
  }
  return {at.x, at.y, at.z + 96};
}

// `--demo`: every player of a recording in the water (his middle in water on the recording's map
// from `--game`), sorted by whether he moved across the plan since the last frame, and the body
// frames he showed there: is the run cycle what a moving swimmer shows?
void run_demo(gl::Lab& lab, const cl::SearchPath& files) {
  std::ifstream in(lab.arg("demo"), std::ios::binary);
  if (!in) {
    lab.fail("se-swim: " + lab.arg("demo") + " does not open",
             "Pass --demo=<a demo's path>. Skill: shooter.");
    return;
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  cl::Demo d;
  std::string error;
  if (!cl::open_demo(std::move(bytes), d, error)) {
    lab.fail("se-swim: " + error, "Check the file. Skill: shooter.");
    return;
  }
  std::unique_ptr<cl::LoadedMap> map;
  std::string map_name;
  struct Tally {
    int64_t moving = 0, moving_run = 0, still = 0, still_stand = 0, changes = 0;
    std::set<int> frames;
  };
  Tally all, dry;
  std::vector<cl::DemoEntity> last(32);
  std::vector<int> last_frame(32, -1);
  cl::DemoRead r;
  while ((r = cl::next_frame(d, error)) == cl::DemoRead::frame) {
    const cl::DemoState& s = d.state;
    if (s.map.empty()) continue;
    if (s.map != map_name) {
      map_name = s.map;
      map = cl::load_named_map(files, map_name, error);
      if (!map) lab.note("se-swim: " + error);
    }
    if (!map) continue;
    for (size_t k = 0; k < 32; k++) {
      const cl::DemoEntity& p = s.players[k];
      if (!p.present || p.dead || p.gib) {
        last_frame[k] = -1;
        continue;
      }
      const float mid[3] = {p.origin.x, p.origin.y, p.origin.z};
      const bool wet = br::hull_contents(map->bsp.map, mid, 0, 0) == -3;
      if (last_frame[k] >= 0) {
        Tally& all_ = wet ? all : dry;
        const float dx = p.origin.x - last[k].origin.x, dy = p.origin.y - last[k].origin.y;
        const bool moved = dx != 0 || dy != 0;
        all_.frames.insert(p.frame);
        if (p.frame != last_frame[k]) all_.changes++;
        if (moved) {
          all_.moving++;
          all_.moving_run += int(in_run(p.frame));
        } else {
          all_.still++;
          all_.still_stand += int(in_stand(p.frame));
        }
      }
      last[k] = p;
      last_frame[k] = p.frame;
    }
  }
  if (r == cl::DemoRead::error) lab.note("se-swim: the demo stopped: " + error);
  lab.metric("demo.wet.moving", static_cast<double>(all.moving), "frames");
  lab.metric("demo.wet.moving-run-share",
             all.moving ? double(all.moving_run) / double(all.moving) : 0, "ratio");
  lab.metric("demo.wet.still", static_cast<double>(all.still), "frames");
  lab.metric("demo.wet.still-stand-share",
             all.still ? double(all.still_stand) / double(all.still) : 0, "ratio");
  lab.metric("demo.wet.frame-changes", static_cast<double>(all.changes), "count");
  lab.metric("demo.dry.moving", static_cast<double>(dry.moving), "frames");
  lab.metric("demo.dry.moving-run-share",
             dry.moving ? double(dry.moving_run) / double(dry.moving) : 0, "ratio");
  lab.metric("demo.dry.frame-changes", static_cast<double>(dry.changes), "count");
  for (const Tally* t : {&all, &dry}) {
    std::string seen;
    for (const int f : t->frames) seen += (seen.empty() ? "" : " ") + std::to_string(f);
    lab.note(std::string("body frames seen ") + (t == &all ? "in water: " : "out of it: ") + seen);
  }
}

void run(gl::Lab& lab) {
  const std::string folder = lab.arg("game");
  std::string error;
  cl::SearchPath files;
  if (folder.empty()) {
    if (!fixture_files(files, error)) {
      lab.fail("se-swim: the fixture halls: " + error, "Read the reason above. Skill: brush.");
      return;
    }
  } else {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("se-swim: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
      return;
    }
  }
  if (!lab.arg("demo").empty()) {
    run_demo(lab, files);
    return;
  }
  ge::progs::Progs progs;
  if (lab.arg("progs") == "pack") {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = cl::find_file(files, "progs.dat", store);
    if (!ge::progs::load_progs(bytes.data(), bytes.size(), progs, error, ge::progs::every_form())) {
      lab.fail("se-swim: the pack's progs.dat: " + error,
               "Pass a game folder whose packs hold a progs.dat. Skill: progs.");
      return;
    }
  } else if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-swim: the baseline mod: " + error, "Read the reason above. Skill: progs.");
    return;
  }
  const std::string map_name = folder.empty() ? "halls" : lab.arg("map");
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-swim: " + error, "Pass --map=<a map of the folder with water>. Skill: shooter.");
    return;
  }
  // Where he swims: --at, else the deepest water; on the fixture, the hall's floor.
  float at[3] = {128, 192, 24};
  float yaw = 0;
  if (!lab.arg("at").empty()) {
    std::sscanf(lab.arg("at").c_str(), "%f,%f,%f,%f", &at[0], &at[1], &at[2], &yaw);
  } else if (!folder.empty()) {
    if (!deep_water(map->bsp.map, at)) {
      lab.fail("se-swim: " + map_name + " has no water deep enough to swim in",
               "Pass --map=<a map with a pool> or --at=x,y,z[,yaw]. Skill: shooter.");
      return;
    }
    yaw = open_yaw(map->bsp.map, at);
  }
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), lo, error)) {
    lab.fail("se-swim: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});  // the spawn settles
  {
    sh::Edict& me = l.server->sv.edicts[1];
    me.origin = {at[0], at[1], at[2]};
    me.oldorigin = me.origin;
    me.velocity = {};
  }
  l.view = {0, yaw, 0};

  // The run: --swim seconds forward, as long again let go, the body's frame read each frame.
  const int swim = std::max(10, static_cast<int>(lab.arg_num("swim") / kTic));
  std::vector<Row> rows;
  sh::MoveCmd ahead;
  ahead.forward = 200;
  for (int i = 0; i < 2 * swim; i++) {
    const bool swimming = i < swim;
    cl::listen_frame(l, swimming ? ahead : sh::MoveCmd{});
    const sh::Edict& e = l.server->sv.edicts[1];
    Row r;
    r.t = (i + 1) * kTic;
    r.frame = static_cast<int>(cl::field_named(l, 1, "frame"));
    r.waterlevel = static_cast<int>(e.waterlevel);
    r.across = std::sqrt(e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y);
    r.vz = e.velocity.z;
    r.swimming = swimming;
    r.origin = e.origin;
    r.angles = e.angles;
    rows.push_back(r);
  }

  // The numbers.
  int changes = 0, deepest = 0;
  std::set<int> run_seen;
  for (size_t i = 0; i < rows.size(); i++) {
    const Row& r = rows[i];
    if (i > 0 && r.frame != rows[i - 1].frame) changes++;
    if (!r.swimming) continue;
    deepest = std::max(deepest, r.waterlevel);
    // The first tenth of a second is the cycle's turn to change: counted from then on.
    if (r.t < 0.1) continue;
    if (in_run(r.frame)) run_seen.insert(r.frame);
  }
  int still = 0, still_stand = 0;
  for (const Row& r : rows)
    if (!r.swimming && r.across == 0) {
      still++;
      still_stand += int(in_stand(r.frame));
    }
  lab.metric("swim.run-frames-seen", static_cast<double>(run_seen.size()), "count");
  lab.metric("swim.frame-changes", changes, "count");
  lab.metric("swim.waterlevel", deepest, "level");
  lab.metric("still.frames", still, "count");
  lab.metric("still.stand-share", still ? double(still_stand) / still : 0, "ratio");

  gl::LabTable table{
      "the body's frame every tenth of a second",
      {"t (s)", "commands", "water level", "across (u/s)", "up (u/s)", "frame", "cycle"},
      {}};
  for (size_t i = 7; i < rows.size(); i += 8) {
    const Row& r = rows[i];
    table.rows.push_back({ge::core::num::to_fixed(r.t, 2), r.swimming ? "forward" : "none",
                          std::to_string(r.waterlevel), ge::core::num::to_fixed(r.across, 1),
                          ge::core::num::to_fixed(r.vz, 1), std::to_string(r.frame),
                          in_run(r.frame)     ? "run"
                          : in_stand(r.frame) ? "stand"
                                              : "other"});
  }
  lab.table(table);

  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {
      "THE BODY'S FRAME WHILE HE SWIMS AND LETS GO",
      (folder.empty() ? std::string("the fixture halls, on dry land") : map_name + ", in water") +
          (lab.arg("progs") == "pack" ? ", the pack's program" : ", the baseline mod"),
      "t (s)", "frame / speed"};
  gl::Trace fr{"frame (0-11 run, 12-28 stand)", {}, {}}, sp{"speed across / 10 (u/s)", {}, {}};
  for (const Row& r : rows) {
    fr.x.push_back(r.t);
    fr.y.push_back(r.frame);
    sp.x.push_back(r.t);
    sp.y.push_back(r.across / 10.0);
  }
  ch.traces = {fr, sp};
  lab.shot({"frames",
            ch.frame.title,
            "The frame his body shows at every server frame (the program changes it every 0.1 s) "
            "and his speed across the plan. Forward until " +
                ge::core::num::to_fixed(swim * kTic, 2) + " s, then nothing.",
            "a saw-tooth through the run cycle while he swims, on while he drifts, the stand "
            "cycle once he is still; a flat line is a body that never moves",
            {{"map", map_name}}},
           gl::to_picture(gl::line_chart(ch).plot));

  if (folder.empty()) return;  // the fixture has no player model to draw

  // THE PICTURES: the swimmer as the window draws another player, from beside him.
  auto device = ge::gpu::create_device({.label = "se-swim", .validation = 1});
  if (!device) {
    lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
    return;
  }
  constexpr uint32_t kW = 640, kH = 400;
  shooters_edge::look::Picture look;
  ge::console::Console con;
  shooters_edge::look::add_cvars(con, look);
  con.execute(lab.arg("cvars"));
  con.execute(lab.cvars());
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *l.map, kW, kH, wgpu::TextureFormat::RGBA8Unorm, look,
                            {}, error, {l.server->models, {}});
  if (!world) {
    lab.fail("se-swim: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-swim.final", kW, kH);
  const int shots = std::max(2, static_cast<int>(lab.arg_num("moments")));
  const float side = static_cast<float>(lab.arg_num("side"));
  std::vector<gl::Picture> pics;
  std::vector<std::string> names;
  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  for (int s = 0; s < shots; s++) {
    const size_t i = std::min(rows.size() - 1, (rows.size() - 1) * size_t(s) / size_t(shots - 1));
    const Row& r = rows[i];
    const double t = r.t;
    cl::frame_view(l, shown, 0, t, 0);
    cl::step_fx(l, shown, fx_state, t, kTic, fx);
    // The swimmer himself, as another player is drawn (the local player is not).
    cl::Placed me;
    me.entity = 2;  // a client's slot the eye is not
    me.modelindex = l.server->sv.edicts[1].modelindex;
    me.frame = r.frame;
    me.origin = r.origin;
    me.angles = r.angles;
    me.top = 4;
    me.bottom = 12;
    fx.models.push_back(me);
    // The eye: beside him across his way, at his height, looking at him; the first side
    // (across, then aslant, then behind) with nothing solid between.
    const sh::Vec3f eye = eye_beside(l, r.origin, yaw, side);
    const ge::camera::Vec3 e = cl::engine_point(eye);
    ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    view.eye[0] = e.x, view.eye[1] = e.y, view.eye[2] = e.z;
    view.heading = cl::engine_heading(static_cast<float>(
        ge::core::libm::atan2(r.origin.y - eye.y, r.origin.x - eye.x) * 180.0 / 3.14159265358979));
    view.pitch = 0;
    view.time_s = t;
    for (int f = 0; f < 8; f++) {  // the water and the light settle
      ge::render::FrameView run_in = view;
      run_in.time_s = t - (8 - f) / 30.0;
      cl::draw_world(*device, *world, run_in, fx, nullptr, 1.0 / 30, {}, out.view);
    }
    cl::draw_world(*device, *world, view, fx, nullptr, 1.0 / 30, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    pics.push_back(
        gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba});
    names.push_back(ge::core::num::to_fixed(t, 2) + " s, " + (r.swimming ? "swimming" : "let go") +
                    ", frame " + std::to_string(r.frame) +
                    (in_run(r.frame)     ? " (run)"
                     : in_stand(r.frame) ? " (stand)"
                                         : ""));
  }
  for (size_t k = 0; k < pics.size(); k++)
    lab.shot({"swim-" + std::to_string(k),
              names[k],
              "The swimmer from " + ge::core::num::to_fixed(side * br::kUnitM, 1) +
                  " m beside him, as the window draws another player: " + names[k] + ".",
              "his legs and arms in a stride while he swims; standing once still",
              {{"map", map_name}}},
             pics[k], gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-swim",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the baseline mod's body cycles (games/shooters_edge/rules/common_player.cpp, player_body) on "
    "the "
    "listen server (client/listen.hpp), drawn by client/world_models.hpp",
    "shooter",
    "Does a swimming player run? The frame the program gives his body while he swims, lets go and "
    "drifts, on a real map's water, drawn from beside him.",
    {
        {"game", "", "a game folder (<base>/<folder>) to swim in; empty: the fixture halls, dry"},
        {"map", "e1m1", "with --game: the map (one with water)"},
        {"progs", "", "pack: the packs' own progs.dat; else the baseline mod"},
        {"at", "", "x,y,z[,yaw] where he swims (else the map's deepest water)"},
        {"swim", "1.2", "seconds he swims forward (then as long with nothing pressed)"},
        {"moments", "6", "with --game: pictures through the run"},
        {"side", "96", "with --game: units from him the eye stands"},
        {"cvars", "", "with --game: picture cvars (r_look ...)"},
        {"demo", "", "with --game: a recording's swimmers tallied instead (its maps from --game)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
