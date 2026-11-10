// LAB `se-speedjump`: DOES THE EASY JUMP GAIN SPEED ON FORWARD AND JUMP ALONE, WEAVE ITS HOPS
// ACROSS THE AIM, STEER BY THE MOUSE, AND SHOW THE SWING AS ASKED? The easy jump (`cl_speedjump 1`;
// plugins/shooter/speedjump_assist.hpp: the client strafes for the player, every server running
// the commands with its classic move) measured three ways.
//
// THE MOVE ALONE: one command of 13 ms at a time through `player_move` on an open floor, a second's
// run-up to sv_maxspeed, then eight seconds of hands: forward and jump held without the aid (the
// classic move: one jump, then 320), the same hands through the aid at shares 0.5, 0.8 (the
// window's own server's pm_speedjump) and 1, and the best strafe (the side key alone, swapped each
// hop, the view on the way, jump pressed on landing).
//
// THE WEAVE AND THE STEER: the aid's paths from above with the aim held east, and with the aim
// turned a quarter left after two seconds; the way's angle off the aim, the sent swing and the
// shown swing at cl_speedjump_view 0.5, command by command.
//
// IN THE GAME: the listen server (the baseline mod) on `--map` from `--pak` (a real map; without
// one, an open arena of our own), the player running forward for a second and then holding forward
// and jump, the view turning 45 degrees a second throughout, once with `cl_speedjump 0` and once
// with `cl_speedjump 1` sent as his user info (as the window does), the speed read off the server's
// player each frame; then from his eyes `--at` seconds into the easy run, at cl_speedjump_view 0
// and 1 (the same moment: the swing shown or not; the pictures are measured).
//
// ge:summary Lab: the easy jump (cl_speedjump) gains, weaves, steers and shows its swing
// ge:keywords lab se-speedjump speedjump cl_speedjump cl_speedjump_view pm_speedjump easy-jump
// ge:keywords bunny-hop strafe-jump speed-over-time listen-server swing weave
// ge:skill shooter labs
// ge:test tests/shooter_speedjump_assist_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "progs/file.hpp"
#include "shooter/client.hpp"
#include "shooter/pmove.hpp"
#include "shooter/recording.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;
namespace sh = ge::shooter;
namespace ls = ge::lab::liquid_scene;
using ge::core::num::to_fixed;

constexpr int kRate = 77;       // commands a second, 13 ms each
constexpr int kSeconds = 8;     // of hands after the run-up
constexpr float kShare = 0.8f;  // the window's own server's pm_speedjump
constexpr double kPi = 3.14159265358979323846;

float plan_speed(sh::Vec3f v) { return sh::root_of(v.x * v.x + v.y * v.y); }
float heading_of(sh::Vec3f v) {
  return static_cast<float>(ge::core::libm::atan2(v.y, v.x) * 180 / kPi);
}
double wrap(double a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// An open floor 64k units across.
struct Field {
  sh::SceneWorld sw;
  Field() {
    sh::RecordedScene scene;
    sh::Brush b;
    const float far = 32768;
    b.planes = {{{1, 0, 0}, far},  {{-1, 0, 0}, far}, {{0, 1, 0}, far},
                {{0, -1, 0}, far}, {{0, 0, 1}, 0},    {{0, 0, -1}, 64}};
    scene.brushes.push_back(b);
    sh::build_scene_world(scene, {}, sw);
  }
};

// The hands; `aided` holds forward and jump and sends them through the easy jump's aid.
enum class Hands : uint8_t { forward_jump, best_strafe, aided };

// A run on the field: the run-up, then `hands` for kSeconds (the aim turned `turn` degrees left
// after two seconds); the plan speed, the path and the swings at every command.
struct MoveRun {
  gl::Trace speed, path, off, swing, shown;
  sh::MoveState end;
  int jumps = 0;
};

MoveRun move_run(const Field& f, Hands hands, float share, const std::string& label,
                 float turn = 0) {
  const sh::MoveSettings set;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  MoveRun r;
  r.speed.label = r.path.label = label;
  r.off.label = "the way off the aim";
  r.swing.label = "the sent yaw off the aim";
  r.shown.label = "the view shown off the aim (cl_speedjump_view 0.5)";
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  for (int i = 0; i < kRate; i++) {
    sh::player_move(s, cmd, set, f.sw.world);
    r.speed.x.push_back(static_cast<double>(i + 1) / kRate);
    r.speed.y.push_back(plan_speed(s.velocity));
  }
  sh::SpeedJumpAid aid;
  int side = 400;
  for (int i = 0; i < kSeconds * kRate; i++) {
    const bool down = s.ground != -1;
    const float aim = i >= 2 * kRate ? turn : 0;
    cmd = {};
    cmd.msec = 13;
    if (hands == Hands::forward_jump) {
      cmd.forward = 400;
      cmd.buttons = sh::kButtonJump;
    } else if (hands == Hands::aided) {
      cmd.forward = 400;
      cmd.buttons = sh::kButtonJump;
      cmd.angles = {0, aim, 0};
      cmd = sh::assisted_command(cmd, s, share, 0.5f, aid);
    } else {
      if (down) side = -side;
      cmd.side = static_cast<int16_t>(side);
      cmd.buttons = down ? sh::kButtonJump : 0;
      cmd.angles = {0, heading_of(s.velocity), 0};
    }
    sh::player_move(s, cmd, set, f.sw.world);
    if (down && s.ground == -1 && s.velocity.z > 200) r.jumps++;
    const double t = static_cast<double>(kRate + i + 1) / kRate;
    r.speed.x.push_back(t);
    r.speed.y.push_back(plan_speed(s.velocity));
    r.path.x.push_back(s.origin.x);
    r.path.y.push_back(s.origin.y);
    r.off.x.push_back(t);
    r.off.y.push_back(wrap(heading_of(s.velocity) - aim));
    r.swing.x.push_back(t);
    r.swing.y.push_back(aid.swing);
    r.shown.x.push_back(t);
    r.shown.y.push_back(aid.shown);
  }
  r.end = s;
  return r;
}

void shoot_chart(gl::Lab& lab, const std::string& id, const gl::LineChart& spec,
                 const std::string& caption, const std::string& look_for) {
  const gl::Chart chart = gl::line_chart(spec);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, {}}, gl::to_picture(chart.plot));
}

void move_alone(gl::Lab& lab) {
  const Field f;
  const MoveRun classic = move_run(f, Hands::forward_jump, 0, "forward + jump, cl_speedjump 0");
  const MoveRun half = move_run(f, Hands::aided, 0.5f, "cl_speedjump 1, pm_speedjump 0.5");
  const MoveRun most = move_run(f, Hands::aided, kShare, "cl_speedjump 1, pm_speedjump 0.8");
  const MoveRun all = move_run(f, Hands::aided, 1, "cl_speedjump 1, pm_speedjump 1");
  const MoveRun strafe = move_run(f, Hands::best_strafe, 0, "the best strafe by hand");
  const MoveRun steer = move_run(f, Hands::aided, kShare, "aim turned 90 left at 3 s", 90);

  gl::LabTable table{"the move alone: eight seconds of each pair of hands after a second's run-up",
                     {"hands", "jumps", "speed at 3 s", "speed at 9 s", "gain a second"},
                     {}};
  for (const MoveRun* r : {&classic, &half, &most, &all, &strafe}) {
    const double at3 = r->speed.y[static_cast<size_t>(3 * kRate - 1)];
    const double at9 = r->speed.y.back();
    table.rows.push_back({r->speed.label, std::to_string(r->jumps), to_fixed(at3, 1),
                          to_fixed(at9, 1), to_fixed((at9 - 320) / kSeconds, 1)});
  }
  lab.table(table);
  double lo = 0, hi = 0;
  for (size_t i = static_cast<size_t>(2) * kRate; i < most.off.y.size(); i++) {
    lo = std::min(lo, most.off.y[i]);
    hi = std::max(hi, most.off.y[i]);
  }
  const double steer_off = wrap(heading_of(steer.end.velocity) - 90);
  lab.metric("move.classic.speed", classic.speed.y.back(), "u/s");
  lab.metric("move.classic.jumps", classic.jumps, "count");
  lab.metric("move.easy-0.5.speed", half.speed.y.back(), "u/s");
  lab.metric("move.easy.speed", most.speed.y.back(), "u/s");
  lab.metric("move.easy.jumps", most.jumps, "count");
  lab.metric("move.easy-1.speed", all.speed.y.back(), "u/s");
  lab.metric("move.strafe.speed", strafe.speed.y.back(), "u/s");
  lab.metric("move.easy.swing-left", hi, "deg");
  lab.metric("move.easy.swing-right", lo, "deg");
  lab.metric("move.steer.off-aim", steer_off, "deg");
  if (most.speed.y.back() <= classic.speed.y.back() + 200)
    lab.fail("forward and jump with the easy jump gained too little: " +
                 to_fixed(most.speed.y.back(), 1) + " u/s after 8 s",
             "Read speedjump_assist.cpp (the wish's angle from the share). Skill: shooter.");
  if (strafe.speed.y.back() <= most.speed.y.back())
    lab.fail("the easy jump at 0.8 out-gained the best strafe",
             "Its share must stay under 1 (`kShare`). Skill: shooter.");
  if (hi < 10 || lo > -10)
    lab.fail("the hops did not weave across the aim (" + to_fixed(lo, 1) + ".." + to_fixed(hi, 1) +
                 " degrees)",
             "Read speedjump_assist.cpp's takeoff turn and kSpeedJumpSwing. Skill: shooter.");
  if (steer_off < -40 || steer_off > 40)
    lab.fail("the aid did not steer the hops to the turned aim",
             "Read speedjump_assist.cpp (the turn toward a far-off aim). Skill: shooter.");

  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"PLAN SPEED OVER TIME: THE MOVE ALONE",
                 "13 ms commands on an open floor; a second's run-up on the ground first", "t (s)",
                 "plan speed (units/s)"};
  chart.traces = {classic.speed, half.speed, most.speed, all.speed, strafe.speed};
  chart.rules = {320};
  shoot_chart(lab, "speed-move", chart,
              "The player's plan speed command by command. Forward and jump held without the "
              "easy jump is the classic move: one jump, then the run speed (dashed, 320). With "
              "cl_speedjump 1 the client strafes for him and every hop gains: by the server's "
              "pm_speedjump share of the best strafe's gain (0.5, 0.8 the window's own, 1). The "
              "best strafe by hand stays the fastest under 1.",
              "three easy lines climbing in order of their share, the strafe's above the 0.8 line");

  gl::LineChart top;
  top.spikes = false;
  top.frame = {"THE HOPS FROM ABOVE",
               "pm_speedjump 0.8: the aim held east (along x), and turned 90 degrees left at 3 s",
               "x (units)", "y (units)"};
  gl::Trace aim{"the aim east", {0, most.path.x.back()}, {0, 0}};
  MoveRun straight = most;
  straight.path.label = "aim held east";
  top.traces = {aim, straight.path, steer.path};
  shoot_chart(lab, "paths", top,
              "Both easy runs from above. With the aim held east the hops weave left and right "
              "across the aim line like a strafer's, each swing up to about 30 degrees; with the "
              "aim turned a quarter left the hops turn to it at the strafe's full rate, still "
              "gaining, and weave on along the new aim: the mouse steers.",
              "a weaving line along y = 0, and one that bends up to go north and weaves on");

  gl::LineChart swing;
  swing.spikes = false;
  swing.frame = {"THE SWING: ANGLES OFF THE AIM", "pm_speedjump 0.8, the aim held east, 4 s",
                 "t (s)", "degrees (+ left)"};
  const auto first = [](const gl::Trace& tr, double until) {
    gl::Trace out = tr;
    out.x.clear();
    out.y.clear();
    for (size_t i = 0; i < tr.x.size() && tr.x[i] <= until; i++) {
      out.x.push_back(tr.x[i]);
      out.y.push_back(tr.y[i]);
    }
    return out;
  };
  swing.traces = {first(most.off, 5), first(most.swing, 5), first(most.shown, 5)};
  swing.rules = {30, -30};
  shoot_chart(lab, "swing", swing,
              "Command by command: the way the player moves (off the aim), the yaw the client "
              "sends (where a strafer would look) and the view the screen shows at "
              "cl_speedjump_view 0.5 (half the sent swing, eased). The way turns back each time "
              "it is 30 degrees past the aim (dashed).",
              "the way's zig-zag inside +-30, the sent yaw near it, the shown line at half");
}

// THE ARENA: one open room 2560 across, a spawn in its middle facing east.
bool arena_pack(std::vector<uint8_t>& out, std::string& error) {
  ge::brush::Fixture f;
  f.message = "The speed jump arena";
  ge::brush::FixtureRoom r;
  r.name = "arena";
  r.min[0] = -1280, r.min[1] = -1280, r.min[2] = 0;
  r.max[0] = 1280, r.max[1] = 1280, r.max[2] = 384;
  r.floor = "flagstone";
  r.wall = "brick";
  r.ceiling = "plaster";
  f.rooms.push_back(r);
  f.ambient = 120;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
      f.lights.push_back(ge::brush::FixtureLight{
          {static_cast<float>(x * 850), static_cast<float>(y * 850), 120}, 900, 255});
  ge::brush::Entity spawn;
  spawn.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 -600 40"}, {"angle", "0"}};
  f.entities.push_back(spawn);
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(f, ge::brush::Family::v29, &bsp, &error)) return false;
  return ge::vfs::write_pak({{"maps/sjarena.bsp", bsp}}, &out, &error);
}

// On a real map: the player moved to the longest open run the map has, so his hops are not
// stopped by the first wall. From every placed entity (and his spawn) 16 headings are tried: the
// run's length is the shortest clear sweep of the player's box along three lines 64 units apart,
// cut where the floor under it falls away by more than 64 units. The best start and heading win.
void to_open_run(cl::Listen& l) {
  const sh::Server& sv = l.server->sv;
  sh::MoveWorld world;
  int ents[sh::kMaxSolids];
  const sh::Vec3f spawn = sv.edicts[1].origin;
  sh::build_player_world(sv, 1, spawn, world, ents);
  world.count = 1;  // the world alone: the map's walls and floors
  float best = -1, best_yaw = 0;
  sh::Vec3f best_at = spawn;
  const auto run_length = [&](sh::Vec3f from, double yaw) {
    const double r = yaw * kPi / 180;
    const auto cx = static_cast<float>(ge::core::libm::cos(r));
    const auto cy = static_cast<float>(ge::core::libm::sin(r));
    float most = 3000;
    for (const float side : {-32.0f, 0.0f, 32.0f}) {
      const sh::Vec3f a{from.x - cy * side, from.y + cx * side, from.z};
      if (sh::player_trace(world, a, a).startsolid) return 0.0f;
      const sh::HullTrace t = sh::player_trace(world, a, {a.x + cx * 3000, a.y + cy * 3000, a.z});
      most = std::min(most, t.fraction * 3000);
    }
    for (float d = 128; d < most; d += 128) {
      const sh::Vec3f p{from.x + cx * d, from.y + cy * d, from.z};
      if (sh::player_trace(world, p, {p.x, p.y, p.z - 96}).fraction == 1) return d;
    }
    return most;
  };
  for (int e = 1; e < sv.num_edicts; e++) {
    const sh::Edict& ed = sv.edicts[static_cast<size_t>(e)];
    if (ed.free) continue;
    sh::Vec3f from = ed.origin;
    if (e != 1) {  // stood on the floor under the entity
      const sh::HullTrace down =
          sh::player_trace(world, {from.x, from.y, from.z + 32}, {from.x, from.y, from.z - 256});
      if (down.startsolid || down.fraction == 1) continue;
      from = down.endpos;
      from.z = from.z + 1;
    }
    for (int k = 0; k < 16; k++) {
      const float len = run_length(from, k * 22.5);
      if (len > best) best = len, best_yaw = static_cast<float>(k * 22.5), best_at = from;
    }
  }
  sh::Edict& me = l.server->sv.edicts[1];
  me.origin = best_at;
  me.velocity = {};
  l.view = {0, best_yaw, 0};
}

struct GameRun {
  gl::Trace speed, path;
  float top = 0;
};

// The player on `map` with `cl_speedjump` as asked and `view` of the swing shown: two seconds to
// spawn and settle, a second of forward alone, then forward and jump held, the view turning left
// at `turn` degrees a second throughout (from the map's longest open run when `open_run`). `at` is
// called mid-swing from `at_s` seconds into the hopping.
template <class At>
bool game_run(const ge::progs::Progs& progs, const cl::SearchPath& files,
              const std::string& map_name, bool open_run, bool speedjump, float view, float turn,
              double at_s, GameRun& out, std::string& error, At at) {
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map ||
      !cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error))
    return false;
  l.rules->user_info(0, "cl_speedjump", speedjump ? "1" : "0");
  l.speedjump_view = view;
  for (int i = 0; i < 2 * kRate; i++) cl::listen_frame(l, {});
  if (open_run) {
    to_open_run(l);
    for (int i = 0; i < kRate / 2; i++) cl::listen_frame(l, {});
  }
  out.speed.label = speedjump ? "cl_speedjump 1" : "cl_speedjump 0";
  out.path.label = out.speed.label;
  sh::MoveCmd cmd;
  cmd.forward = 400;
  const int at_cmd = kRate + static_cast<int>(at_s * kRate);
  bool done = false;
  for (int i = 0; i < (1 + kSeconds) * kRate; i++) {
    if (i == kRate) cmd.buttons = sh::kButtonJump;  // a second's run-up, then the jump held
    l.view.y = l.view.y + turn / kRate;
    cl::listen_frame(l, cmd);
    const sh::Edict& me = l.server->sv.edicts[1];
    const float v = plan_speed(me.velocity);
    out.top = std::max(out.top, v);
    out.speed.x.push_back(static_cast<double>(i + 1) / kRate);
    out.speed.y.push_back(v);
    out.path.x.push_back(me.origin.x);
    out.path.y.push_back(me.origin.y);
    // The first command from `at_s` on whose sent swing is wide (the same in every run of the
    // same hands: the drawn share alone differs).
    if (!done && i + 1 >= at_cmd && std::abs(l.jump_aid.swing) > 15) {
      done = true;
      at(l);
    }
  }
  return true;
}

void in_game(gl::Lab& lab, uint32_t w, uint32_t h) {
  std::string error;
  cl::SearchPath arena, files;
  ge::progs::Progs progs;
  const bool own = lab.arg("pak").empty();
  const std::string map_name = own ? "sjarena" : lab.arg("map");
  std::vector<uint8_t> arena_bytes;
  if (!arena_pack(arena_bytes, error) || !cl::baseline_progs(progs, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  if (own) {
    std::vector<uint8_t> copy = arena_bytes;
    if (!cl::add_archive(files, "pak0.pak", std::move(copy), error)) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
  } else {
    ge::brush::app::Pack pack;
    if (!ge::brush::app::read_pack(lab.arg("pak"), &pack, &error) ||
        !cl::add_archive(files, lab.arg("pak"), std::move(pack.bytes), error)) {
      lab.fail(error, "Pass --pak=<a pack with maps, progs and gfx/palette.lmp>. Skill: shooter.");
      return;
    }
  }
  if (!cl::add_archive(arena, "pak0.pak", std::move(arena_bytes), error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  // The speed is measured on the open arena (a real map's walls stop a run of eight seconds);
  // the eyes are drawn on the map asked for, from the longest open run it has.
  const float turn = static_cast<float>(lab.arg_num("turn"));
  const double at_s = lab.arg_num("at");
  GameRun off, on, swing_run, still;
  const auto none = [](cl::Listen&) {};
  if (!game_run(progs, arena, "sjarena", false, false, 0, turn, 0, off, error, none) ||
      !game_run(progs, arena, "sjarena", false, true, 0.5f, turn, 0, on, error, none)) {
    lab.fail(error, "The arena should load under the baseline mod. Skill: shooter.");
    return;
  }

  auto device = ge::gpu::create_device({.label = "se-speedjump", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  const shooters_edge::look::Picture look;
  std::vector<ge::viewmodel::HeldModel> weapons;
  cl::HeldState held;
  held.hand.at_eye = cl::load_held_set(cl::classic_adapter(), files, {}, weapons);
  if (!held.hand.at_eye) lab.note("no weapon is drawn: the game's held weapons did not load");
  se::HudArt free_art = se::free_art();
  free_art.finish();
  se::HudArt classic_art;
  {
    const auto find = [&files](std::string_view p, std::vector<uint8_t>& store) {
      return cl::find_file(files, p, store);
    };
    std::string why;
    if (!se::classic_art_from_packs(find, &classic_art, &why)) lab.note(why);
  }
  const se::HudArt& art = se::pick_art("auto", classic_art, free_art);
  float speed_at = 0;
  // From his eyes at `view` of the swing shown.
  const auto eyes = [&](float view) {
    return [&, view](cl::Listen& l) {
      speed_at = plan_speed(l.server->sv.edicts[1].velocity);
      const std::string id = view > 0 ? "eyes-swing" : "eyes-still";
      if (!device) {
        ls::placeholders(lab, {id.c_str()});
        return;
      }
      std::string err;
      cl::WorldDrawPtr world =
          cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look,
                                weapons, err, {l.server->models, {}});
      if (!world) {
        lab.fail(err, "Read the gpu: lines above. Skill: render.");
        return;
      }
      ge::gpu::Target target = ge::gpu::create_target(*device, "se-speedjump.final", w, h);
      cl::ViewState shown;
      cl::FxState fx_state;
      cl::FxFrame fx;
      cl::HeldState hand = held;
      const shooters_edge::view::ViewSettings view_settings;
      const double t = 8;
      cl::frame_view(l, shown, 1, t, 0.013);
      const ge::render::FrameView frame = cl::frame_view(l, shown, 0, t, 0);
      cl::step_fx(l, shown, fx_state, t, 0, fx);
      shooters_edge::view::HandFrame hf =
          cl::step_held(l, shown, hand, view_settings, weapons, t, 0);
      se::Hud hud = se::make_hud();
      ge::ui::DrawList overlay;
      overlay.begin(static_cast<int>(w), static_cast<int>(h));
      se::fill_hud(hud, cl::status_of(l, t), art);
      hud.layout.draw(overlay,
                      hud.layout.frame(static_cast<int>(w), static_cast<int>(h), se::hud_style()),
                      se::hud_style());
      cl::draw_world(*device, *world, frame, fx, &hf, 0, overlay, target.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, target);
      const std::string swing = to_fixed(l.jump_aid.shown, 1);
      lab.shot({id,
                view > 0 ? "MID-HOP, CL_SPEEDJUMP_VIEW 1: THE SWING SHOWN"
                         : "MID-HOP, CL_SPEEDJUMP_VIEW 0: A STILL VIEW",
                "From the player's eyes on " + map_name + ", " + to_fixed(at_s, 1) +
                    " s into holding forward and jump with cl_speedjump 1, at " +
                    to_fixed(speed_at, 0) + " units/s (the run speed is 320). " +
                    (view > 0 ? "cl_speedjump_view 1: the view turned " + swing +
                                    " degrees off his aim, where a strafer would look."
                              : "cl_speedjump_view 0: the view on his own aim while the client "
                                "swings the sent yaw."),
                "the same moment as the other eyes picture, the view turned by the swing or not",
                {{"map", map_name}, {"size", lab.arg("size")}}},
               gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
               gl::Det::measured);
    };
  };
  if (!game_run(progs, files, map_name, !own, true, 1, 0, at_s, swing_run, error, eyes(1)) ||
      !game_run(progs, files, map_name, !own, true, 0, 0, at_s, still, error, eyes(0))) {
    lab.fail(error, "The map should load under the baseline mod. Skill: shooter.");
    return;
  }
  lab.metric("game.off.top-speed", off.top, "u/s");
  lab.metric("game.on.top-speed", on.top, "u/s");
  lab.metric("game.on.speed-at", speed_at, "u/s");
  if (on.top <= off.top + 100)
    lab.fail("in the game the easy jump gained too little: " + to_fixed(on.top, 1) + " u/s",
             "Check the user info reached the slot (server/commands.cpp `cl_speedjump`) and the "
             "listen server's pm_speedjump (client/listen.cpp). Skill: shooter.");
  if (swing_run.speed.y != still.speed.y)
    lab.fail(
        "the shown swing changed the move",
        "cl_speedjump_view only turns the drawn view (Listen::jump_aid.shown). Skill: shooter.");

  gl::LineChart speed;
  speed.spikes = false;
  speed.frame = {"PLAN SPEED OVER TIME: IN THE GAME",
                 "the listen server (baseline mod) on the open arena: a second's run, then forward "
                 "and jump held; the view turning " +
                     to_fixed(turn, 0) + " degrees a second",
                 "t (s)", "plan speed (units/s)"};
  speed.traces = {off.speed, on.speed};
  speed.rules = {320};
  shoot_chart(lab, "speed-game", speed,
              "The server's own player each frame, the same hands twice: with cl_speedjump 0 he "
              "jumps once and runs at 320 (dashed); with cl_speedjump 1 the client strafes for "
              "him and every hop adds speed while the hops follow the turning view.",
              "the cl_speedjump 1 line climbing in a saw-tooth of hops, the other flat at 320");

  gl::LineChart top;
  top.spikes = false;
  top.frame = {"THE PATHS FROM ABOVE", "the open arena, its walls at +-1280", "x (units)",
               "y (units)"};
  gl::Trace walls{"walls", {-1280, 1280, 1280, -1280, -1280}, {-1280, -1280, 1280, 1280, -1280}};
  top.traces = {walls, off.path, on.path};
  shoot_chart(lab, "paths-game", top,
              "Both runs from above. The easy run weaves its hops along the way the view turns, "
              "and goes further for its speed.",
              "two paths from the same spawn, the cl_speedjump 1 one weaving and longer");
}

void run(gl::Lab& lab) {
  move_alone(lab);
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  in_game(lab, w, h);
}

const gl::LabSpec kSpec{
    "se-speedjump",
    gl::LabKind::picture,
    "game:shooters_edge",
    "plugins/shooter/speedjump_assist.cpp's easy jump (cl_speedjump) and its wiring",
    "shooter",
    "Does holding forward and jump gain speed with cl_speedjump 1, weave its hops across the aim, "
    "steer by the mouse and show its swing as cl_speedjump_view asks?",
    {{"size", "960,540", "the eyes' pictures' width and height, px"},
     {"pak", "", "a pack holding the map (empty: an open arena of our own)"},
     {"map", "e1m1", "the map in --pak"},
     {"turn", "45", "degrees a second the view turns in the game"},
     {"at", "5", "seconds into the hopping the eyes are drawn"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
