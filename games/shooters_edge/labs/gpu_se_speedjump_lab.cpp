// LAB `se-speedjump`: DOES THE EASY JUMP GAIN SPEED ON FORWARD AND JUMP ALONE, AND DOES A STRAFE
// STILL GAIN MORE? The easy jump (`pm_speedjump` on the server, `cl_speedjump 1` on the player;
// plugins/shooter/pmove.hpp `speedjump`) measured two ways.
//
// THE MOVE ALONE: one command of 13 ms at a time through `player_move` on an open floor, a second's
// run-up to sv_maxspeed, then eight seconds of three pairs of hands: forward and jump held with the
// easy jump off (the classic move: one jump, then 320), the same hands with it on at the listen
// server's share (0.5), and the best strafe (the side key alone, swapped each hop, the view on the
// way, jump pressed on landing) with it on. The strafe is run with it off too and must agree to
// the bit: the easy jump never touches a strafe.
//
// IN THE GAME: the listen server (the baseline mod on an open arena of our own) with the player
// running forward for a second and then holding forward and jump, the view turning 45 degrees a
// second throughout, once with `cl_speedjump 0`
// and once with `cl_speedjump 1` sent as his user info (as the window does), the speed read off the
// server's player each frame, the paths drawn from above; then the frame from his eyes six seconds
// into the easy run (the picture is measured: each adapter rasterises its own way).
//
// ge:summary Lab: the easy jump (cl_speedjump) against the classic move and the best strafe
// ge:keywords lab se-speedjump speedjump cl_speedjump pm_speedjump easy-jump bunny-hop strafe-jump
// ge:keywords speed-over-time listen-server
// ge:skill shooter labs
// ge:test tests/shooter_pmove_test.cpp
#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "progs/file.hpp"
#include "shooter/pmove.hpp"
#include "shooter/recording.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/view/weapons.hpp"
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
constexpr float kShare = 0.5f;  // the listen server's pm_speedjump
constexpr double kPi = 3.14159265358979323846;

float plan_speed(sh::Vec3f v) { return sh::root_of(v.x * v.x + v.y * v.y); }
float heading_of(sh::Vec3f v) {
  return static_cast<float>(ge::core::libm::atan2(v.y, v.x) * 180 / kPi);
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

enum class Hands : uint8_t { forward_jump, best_strafe };

// A run on the field: the run-up, then `hands` for kSeconds; the plan speed at every command.
struct MoveRun {
  gl::Trace speed;
  sh::MoveState end;
  int jumps = 0;
};

MoveRun move_run(const Field& f, Hands hands, float share, const std::string& label) {
  sh::MoveSettings set;
  set.speedjump = share;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  MoveRun r;
  r.speed.label = label;
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  for (int i = 0; i < kRate; i++) {
    sh::player_move(s, cmd, set, f.sw.world);
    r.speed.x.push_back(static_cast<double>(i + 1) / kRate);
    r.speed.y.push_back(plan_speed(s.velocity));
  }
  int side = 400;
  for (int i = 0; i < kSeconds * kRate; i++) {
    const bool down = s.ground != -1;
    cmd = {};
    cmd.msec = 13;
    if (hands == Hands::forward_jump) {
      cmd.forward = 400;
      cmd.buttons = sh::kButtonJump;
    } else {
      if (down) side = -side;
      cmd.side = static_cast<int16_t>(side);
      cmd.buttons = down ? sh::kButtonJump : 0;
      cmd.angles = {0, heading_of(s.velocity), 0};
    }
    sh::player_move(s, cmd, set, f.sw.world);
    if (down && s.ground == -1 && s.velocity.z > 200) r.jumps++;
    r.speed.x.push_back(static_cast<double>(kRate + i + 1) / kRate);
    r.speed.y.push_back(plan_speed(s.velocity));
  }
  r.end = s;
  return r;
}

bool same_bits(const sh::MoveState& a, const sh::MoveState& b) {
  for (int i = 0; i < 3; i++)
    if (std::bit_cast<uint32_t>(a.origin[i]) != std::bit_cast<uint32_t>(b.origin[i]) ||
        std::bit_cast<uint32_t>(a.velocity[i]) != std::bit_cast<uint32_t>(b.velocity[i]))
      return false;
  return a.ground == b.ground && a.jump_held == b.jump_held;
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
  const MoveRun easy = move_run(f, Hands::forward_jump, kShare, "forward + jump, cl_speedjump 1");
  const MoveRun strafe = move_run(f, Hands::best_strafe, kShare, "best strafe, cl_speedjump 1");
  const MoveRun strafe_off = move_run(f, Hands::best_strafe, 0, "best strafe, cl_speedjump 0");
  const bool untouched =
      same_bits(strafe.end, strafe_off.end) && strafe.speed.y == strafe_off.speed.y;

  gl::LabTable table{"the move alone: eight seconds of each pair of hands after a second's run-up",
                     {"hands", "jumps", "speed at 3 s", "speed at 9 s", "gain a second"},
                     {}};
  for (const MoveRun* r : {&classic, &easy, &strafe}) {
    const double at3 = r->speed.y[static_cast<size_t>(3 * kRate - 1)];
    const double at9 = r->speed.y.back();
    table.rows.push_back({r->speed.label, std::to_string(r->jumps), to_fixed(at3, 1),
                          to_fixed(at9, 1), to_fixed((at9 - 320) / kSeconds, 1)});
  }
  lab.table(table);
  lab.metric("move.classic.speed", classic.speed.y.back(), "u/s");
  lab.metric("move.classic.jumps", classic.jumps, "count");
  lab.metric("move.easy.speed", easy.speed.y.back(), "u/s");
  lab.metric("move.easy.jumps", easy.jumps, "count");
  lab.metric("move.strafe.speed", strafe.speed.y.back(), "u/s");
  lab.metric("move.strafe.jumps", strafe.jumps, "count");
  lab.metric("move.strafe.untouched", untouched ? 1 : 0, "bool");
  if (!untouched)
    lab.fail("the best strafe moved differently with pm_speedjump on",
             "The easy jump must act on the forward key alone (pmove.cpp `speed_jump`, `easy` in "
             "walk_move). Skill: shooter.");
  if (easy.speed.y.back() <= classic.speed.y.back() + 100)
    lab.fail("forward and jump with the easy jump gained too little: " +
                 to_fixed(easy.speed.y.back(), 1) + " u/s after 8 s",
             "Read pmove.cpp `speed_jump` and `jump` (the held jump hops again). Skill: shooter.");
  if (strafe.speed.y.back() <= easy.speed.y.back())
    lab.fail("the easy jump out-gained the best strafe",
             "Its share must stay under 1 (`kShare`). Skill: shooter.");

  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"PLAN SPEED OVER TIME: THE MOVE ALONE",
                 "13 ms commands on an open floor; pm_speedjump " + to_fixed(kShare, 1) +
                     "; a second's run-up on the ground first",
                 "t (s)", "plan speed (units/s)"};
  chart.traces = {classic.speed, easy.speed, strafe.speed};
  chart.rules = {320};
  shoot_chart(lab, "speed-move", chart,
              "The player's plan speed command by command. Forward and jump held with the easy "
              "jump off is the classic move: one jump, then the run speed (dashed, 320). With "
              "cl_speedjump 1 the same hands hop on and gain about half what the best strafe "
              "gains; the best strafe (side key, view on the way) is the fastest and identical "
              "with the easy jump on or off.",
              "the easy line climbing steadily between the flat classic line and the strafe's");
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

struct GameRun {
  gl::Trace speed, path;
  float top = 0;
};

// The player on the arena with `cl_speedjump` as asked: two seconds to spawn and settle, a second
// of forward alone, then forward and jump held, the view turning left at 45 degrees a second
// throughout. `at_six` is called six
// seconds in.
template <class AtSix>
bool game_run(const ge::progs::Progs& progs, const cl::SearchPath& files, bool speedjump,
              GameRun& out, std::string& error, AtSix at_six) {
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "sjarena", error);
  if (!map || !cl::listen_start(l, progs, std::move(map), {}, error)) return false;
  l.rules->user_info(0, "cl_speedjump", speedjump ? "1" : "0");
  for (int i = 0; i < 2 * kRate; i++) cl::listen_frame(l, {});
  out.speed.label = speedjump ? "cl_speedjump 1" : "cl_speedjump 0";
  out.path.label = out.speed.label;
  sh::MoveCmd cmd;
  cmd.forward = 400;
  for (int i = 0; i < (1 + kSeconds) * kRate; i++) {
    if (i == kRate) cmd.buttons = sh::kButtonJump;  // a second's run-up, then the jump held
    l.view.y = l.view.y + 45.0f / kRate;
    cl::listen_frame(l, cmd);
    const sh::Edict& me = l.server->sv.edicts[1];
    const float v = plan_speed(me.velocity);
    out.top = std::max(out.top, v);
    out.speed.x.push_back(static_cast<double>(i + 1) / kRate);
    out.speed.y.push_back(v);
    out.path.x.push_back(me.origin.x);
    out.path.y.push_back(me.origin.y);
    if (i + 1 == 7 * kRate) at_six(l);  // six seconds of hopping
  }
  return true;
}

void in_game(gl::Lab& lab, uint32_t w, uint32_t h) {
  std::string error;
  cl::SearchPath files;
  std::vector<uint8_t> pak;
  ge::progs::Progs progs;
  if (!arena_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error) ||
      !cl::baseline_progs(progs, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  GameRun off, on;
  if (!game_run(progs, files, false, off, error, [](cl::Listen&) {})) {
    lab.fail(error, "The arena should load under the baseline mod. Skill: shooter.");
    return;
  }

  auto device = ge::gpu::create_device({.label = "se-speedjump", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  const shooters_edge::look::Picture look;
  std::vector<ge::viewmodel::HeldModel> weapons;
  if (!shooters_edge::view::load_weapon_models(lab.arg("models"), weapons, &error))
    lab.note("no weapon is drawn: " + error);
  float speed_at_six = 0;
  const auto at_six = [&](cl::Listen& l) {
    speed_at_six = plan_speed(l.server->sv.edicts[1].velocity);
    if (!device) {
      ls::placeholders(lab, {"eyes"});
      return;
    }
    std::string err;
    cl::WorldDrawPtr world = cl::create_world_draw(
        *device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, weapons, err);
    if (!world) {
      lab.fail(err, "Read the gpu: lines above. Skill: render.");
      return;
    }
    ge::gpu::Target target = ge::gpu::create_target(*device, "se-speedjump.final", w, h);
    cl::ViewState shown;
    cl::FxState fx_state;
    cl::FxFrame fx;
    cl::HeldState held;
    const shooters_edge::view::ViewSettings view_settings;
    const double t = 8;
    cl::frame_view(l, shown, 1, t, 0.013);
    const ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    cl::step_fx(l, shown, fx_state, t, 0, fx);
    shooters_edge::view::HandFrame hand =
        cl::step_held(l, shown, held, view_settings, weapons, t, 0);
    se::Hud hud = se::make_hud();
    se::HudArt art = se::free_art();
    art.finish();
    ge::ui::DrawList overlay;
    overlay.begin(static_cast<int>(w), static_cast<int>(h));
    se::fill_hud(hud, cl::status_of(l, t), art);
    hud.layout.draw(overlay,
                    hud.layout.frame(static_cast<int>(w), static_cast<int>(h), se::hud_style()),
                    se::hud_style());
    cl::draw_world(*device, *world, view, fx, &hand, 0, overlay, target.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    lab.shot({"eyes",
              "SIX SECONDS OF FORWARD AND JUMP, CL_SPEEDJUMP 1",
              "From the player's eyes in the arena, six seconds after he started holding forward "
              "and jump with cl_speedjump 1 while turning left: in the air mid-hop at " +
                  to_fixed(speed_at_six, 0) + " units/s (the run speed is 320).",
              "the floor below the eyes mid-hop, the arena's far walls, the HUD",
              {{"map", "sjarena"}, {"size", lab.arg("size")}}},
             gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
             gl::Det::measured);
  };
  if (!game_run(progs, files, true, on, error, at_six)) {
    lab.fail(error, "The arena should load under the baseline mod. Skill: shooter.");
    return;
  }
  lab.metric("game.off.top-speed", off.top, "u/s");
  lab.metric("game.on.top-speed", on.top, "u/s");
  lab.metric("game.on.speed-at-6s", speed_at_six, "u/s");
  if (on.top <= off.top + 100)
    lab.fail("in the game the easy jump gained too little: " + to_fixed(on.top, 1) + " u/s",
             "Check the user info reached the slot (server/commands.cpp `cl_speedjump`) and the "
             "listen server's pm_speedjump (client/listen.cpp). Skill: shooter.");

  gl::LineChart speed;
  speed.spikes = false;
  speed.frame = {"PLAN SPEED OVER TIME: IN THE GAME",
                 "the listen server (baseline mod): a second's run, then forward and jump held; "
                 "the view turning 45 degrees a second",
                 "t (s)", "plan speed (units/s)"};
  speed.traces = {off.speed, on.speed};
  speed.rules = {320};
  shoot_chart(lab, "speed-game", speed,
              "The server's own player each frame on the arena, the same hands twice: with "
              "cl_speedjump 0 he jumps once and runs at 320 (dashed); with cl_speedjump 1 the held "
              "jump hops on and every hop adds speed while the way follows the turning view.",
              "the cl_speedjump 1 line climbing in a saw-tooth of hops, the other flat at 320");

  gl::LineChart top;
  top.spikes = false;
  top.frame = {"THE PATHS FROM ABOVE", "the arena is 2560 units across, its walls at +-1280",
               "x (units)", "y (units)"};
  gl::Trace walls{"walls", {-1280, 1280, 1280, -1280, -1280}, {-1280, -1280, 1280, 1280, -1280}};
  top.traces = {walls, off.path, on.path};
  shoot_chart(
      lab, "paths", top,
      "Both runs from above. Turning the view at the same rate, the faster easy-jump run "
      "draws a wider circle: the easy jump turns the way toward the view, so the mouse "
      "still steers.",
      "two loops from the same spawn, the cl_speedjump 1 loop wider, both inside the walls");
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
    "plugins/shooter/pmove.cpp's easy jump (pm_speedjump) and its cl_speedjump wiring",
    "shooter",
    "Does holding forward and jump gain speed with cl_speedjump 1, and does the best strafe "
    "still gain more?",
    {{"size", "960,540", "the eyes' picture's width and height, px"},
     {"models", "games/shooters_edge/content/models/view", "the held weapons' folder"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
