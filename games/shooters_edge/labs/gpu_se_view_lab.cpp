// LAB `se-view`: DOES THE FIRST-PERSON VIEW BOB, ROLL, KICK AND CLIMB AS THE CLASSIC LOOK DOES?
// Shooter's Edge's listen server (client/listen.hpp, the baseline mod) with the window's own view
// (client/present.hpp `frame_view` over engine/camera's `classic_first_person`, the camera cvars at
// their classic defaults). The player is driven through a SCRIPT, one server frame at a time:
//
//   stand 0.5 s, run forward 2 s (the bob), jump while running (the bob held in the air), strafe
//   right 1 s and left 1 s (the roll), stand and take a blow from the right then one from in front
//   (the kick toward each; the blows are handed to the view as the server's damage would be:
//   half-and-half armour and blood), run on (in the fixture up a 16-unit step: the step eased),
//   then die (`kill`: the eye on the floor, the view rolled over).
//
// Beside every frame the CLASSIC FORMULAS are worked out here on their own from the server's state
// (docs/camera.md § Shooter's Edge's view): the bob as 0.3 and 0.7 of the horizontal speed times
// `cl_bob` over a sine of a stride `cl_bobcycle` long half rising, its clock run on the ground only
// and its value held in the air, clamped to -7..+4 units; the roll as the side speed times
// `cl_rollangle` over `cl_rollspeed`, capped; the kick as the count (half the armour and blood, at
// least 10) times the blow's side and front shares times `v_kickroll` and `v_kickpitch`, toward
// the blow, fading linearly over `v_kicktime`; the step's eye rising at 80 units a second never
// more than 12 behind; dead, the eye 16 units under the origin and the roll 80 degrees. The worst
// difference of each against the view is the metric, and the Exit holds it under 0.01 units and
// 0.01 degrees.
//
// What it shows: the eye's height over the origin, the roll and the pitch against time, the view's
// and the formulas' on top of each other, the script's phases shaded. With `--game=<base>/<folder>`
// (a game folder of packs, `--map` one of its maps) the run is on that map and four frames are
// drawn from the eyes (running at the bob's top, strafing right, kicked from the right, dead), for
// PRIVATE pictures only; without it, the fixture: a hall and a room up a step from it. The run
// heads the way of the longest clear run from where the player spawned.
//
// THE SEQUEL'S RULES (a map of brushes: `--rules=sequel` for the fixture, or a sequel's game folder
// and map) work the view out on the server (plugins/shooter/sequel_view.hpp: `run_pitch`,
// `run_roll`, `bob_up`, `bob_pitch`, `bob_roll`, the blows' and the shot's kicks, the dead view)
// and the window eases what they send between their frames. There the reference is the rules' view
// as sent; the blows are the rules' own (`sequel_view_blow`), the player fires the blaster once
// while standing (its kick), and on the fixture the Exit holds the bob's lift, the strafe's roll,
// the blows' kicks, the shot's and the dead roll to the rules' numbers.
//
// ge:summary Lab: the first-person view's bob, roll, kick, step and death against the classic
// ge:keywords lab se-view view bob roll kick step-smoothing dead-roll first-person classic-view
// ge:keywords cl_bob cl_rollangle v_kickpitch v_kickroll formulas listen-server
// ge:keywords sequel-view run_pitch run_roll bob_up bob_pitch bob_roll
// ge:skill camera shooter labs
// ge:test tests/camera_first_person_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace cm = ge::camera;
namespace sv = shooters_edge::server;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The classic look's numbers (the camera cvars' defaults in Shooter's Edge).
constexpr double kBob = 0.02, kBobCycle = 0.6, kBobUp = 0.5, kBobLo = -7, kBobHi = 4;
constexpr double kRollAngle = 2, kRollSpeed = 200;
constexpr double kKickTime = 0.5, kKickPitch = 0.6, kKickRoll = 0.6, kKickMin = 10;
constexpr double kStepRate = 80, kStepMax = 12;
constexpr double kDeadEye = -16, kDeadRoll = 80;
// The sequel's: the standing eye and the bob's top at a run (300 x bob_up).
constexpr double kSequelEye = 22, kSequelBobTop = 1.2;

// The fixture: a long hall (the writer's start in its middle, facing east) and a room 16 units up
// at its east end (a step).
bool fixture_files(cl::SearchPath& files, bool sequel, std::string& error) {
  std::vector<uint8_t> map, pak;
  if (sequel) {
    // The sequel's rules move through brushes: the same hall and step as floors (no faces: the
    // run is measured, not drawn).
    br::BrushWorld w;
    w.models.resize(1);
    const float hall_lo[3] = {0, 0, -16}, hall_hi[3] = {3072, 1024, 0};
    const float step_lo[3] = {3072, 0, -16}, step_hi[3] = {3584, 1024, 16};
    w.models[0].push_back(br::box_brush(hall_lo, hall_hi, sh::kSqSolid, "world/floor"));
    w.models[0].push_back(br::box_brush(step_lo, step_hi, sh::kSqSolid, "world/floor"));
    br::Entity start;
    start.pairs = {{"classname", "info_player_start"}, {"origin", "1536 512 24"}, {"angle", "0"}};
    w.entities.push_back(start);
    if (!br::write_brush_world(w, &map, &error)) return false;
    if (!ge::vfs::write_pak({{"maps/steps.bsp", map}}, &pak, &error)) return false;
    return cl::add_archive(files, "pak0.pak", std::move(pak), error);
  }
  br::Fixture f;
  f.rooms.push_back({"hall", {0, 0, 0}, {3072, 1024, 256}, "floor", "wall", "ceiling", {}});
  f.rooms.push_back({"landing", {3072, 0, 16}, {3584, 1024, 256}, "floor", "wall", "ceiling", {}});
  f.lights.push_back({{1536, 512, 200}, 1600, 220});
  f.lights.push_back({{3328, 512, 200}, 600, 220});
  if (!br::write_bsp(f, br::Family::v29, &map, &error)) return false;
  if (!ge::vfs::write_pak({{"maps/steps.bsp", map}}, &pak, &error)) return false;
  return cl::add_archive(files, "pak0.pak", std::move(pak), error);
}

// The classic formulas, run beside the view on the server's state alone.
struct Classic {
  double bob_clock = 0, bob = 0;  // the stride's clock (on the ground) and the last bob
  double kick_left = 0, kick_roll = 0, kick_pitch = 0;
  double old_z = 0;
  bool fresh = true;

  // One frame of `dt` s: the eye over the origin (units), the roll and the pitch's change from
  // the look (degrees, down positive), for the player `p` looking at yaw `yaw` (degrees).
  void frame(const cl::PlayerState& p, double yaw, const cl::Blow* blow, double dt, double& eye,
             double& roll, double& pitch) {
    const double cy = ge::core::libm::cos(yaw * kDeg), sy = ge::core::libm::sin(yaw * kDeg);
    // The bob: the horizontal speed, a stride of kBobCycle s half rising, held in the air.
    if (p.on_ground) {
      bob_clock += dt;
      double c = bob_clock - std::floor(bob_clock / kBobCycle) * kBobCycle;
      c /= kBobCycle;
      c = c < kBobUp ? kPi * c / kBobUp : kPi + kPi * (c - kBobUp) / (1 - kBobUp);
      const double a = ge::core::hypot(p.velocity.x, p.velocity.y) * kBob;
      bob = std::clamp(a * 0.3 + a * 0.7 * ge::core::libm::sin(c), kBobLo, kBobHi);
    }
    // The step: on the ground and higher, the eye rises at kStepRate within kStepMax; else snaps.
    const double z = p.origin.z;
    if (fresh) old_z = z;
    fresh = false;
    if (p.on_ground && z > old_z) {
      old_z = std::min(old_z + kStepRate * dt, z);
      old_z = std::max(old_z, z - kStepMax);
    } else {
      old_z = z;
    }
    // The roll: the side speed along the view's right (yaw - 90 degrees in the map's plan).
    const double side = p.velocity.x * sy - p.velocity.y * cy;
    const double mag =
        std::fabs(side) < kRollSpeed ? std::fabs(side) * kRollAngle / kRollSpeed : kRollAngle;
    roll = side < 0 ? -mag : mag;
    // The kick: toward the blow, by its count, fading over kKickTime.
    if (blow != nullptr) {
      const double count = std::max(kKickMin, 0.5 * blow->armor + 0.5 * blow->blood);
      double dx = blow->from.x - p.origin.x, dy = blow->from.y - p.origin.y,
             dz = blow->from.z - p.origin.z;
      const double len = ge::core::hypot3(dx, dy, dz);
      if (len > 0) dx /= len, dy /= len;  // the blow's height plays no part
      kick_roll = count * (dx * sy - dy * cy) * kKickRoll;
      kick_pitch = count * (dx * cy + dy * sy) * kKickPitch;
      kick_left = kKickTime;
    }
    pitch = 0;
    if (kick_left > 0) {
      roll += kick_roll * kick_left / kKickTime;
      pitch += kick_pitch * kick_left / kKickTime;
      kick_left -= dt;
    }
    eye = (p.dead ? kDeadEye : static_cast<double>(p.view_ofs.z)) + (old_z - z) + bob;
    if (p.dead) roll = kDeadRoll;
  }
};

// The heading (of eight) with the longest clear run for the player's box from where he stands, to
// 1024 units, so the run's stride is not cut short by a wall.
float longest_run(const cl::Listen& l) {
  if (const sv::SequelServer* sq = sv::sequel_of(*l.rules)) {  // the sequel's map and its trace
    const sh::Vec3f at = l.server->world.entities[1].origin;
    const sh::Vec3f mins{-16, -16, -24}, maxs{16, 16, 32};
    float best = -1, yaw = l.view.y;
    for (int k = 0; k < 8; k++) {
      const double a = k * 45 * kDeg;
      const sh::Vec3f to{at.x + static_cast<float>(1024 * ge::core::libm::cos(a)),
                         at.y + static_cast<float>(1024 * ge::core::libm::sin(a)), at.z};
      const float run = sv::map_trace(*sq, at, mins, maxs, to, sh::kSqSolid).fraction;
      if (run > best) best = run, yaw = static_cast<float>(k * 45);
    }
    return yaw;
  }
  const sh::Server& srv = l.server->sv;
  const sh::Vec3f at = srv.edicts[1].origin;
  sh::MoveWorld world;
  int ents[sh::kMaxSolids];
  sh::build_player_world(srv, 1, at, world, ents);
  world.count = 1;  // the map's walls and floors alone
  float best = -1, yaw = l.view.y;
  for (int k = 0; k < 8; k++) {
    const double a = k * 45 * kDeg;
    const sh::Vec3f to{at.x + static_cast<float>(1024 * ge::core::libm::cos(a)),
                       at.y + static_cast<float>(1024 * ge::core::libm::sin(a)), at.z};
    const float run = sh::player_trace(world, at, to).fraction;
    if (run > best) best = run, yaw = static_cast<float>(k * 45);
  }
  return yaw;
}

enum class Phase : uint8_t { stand, run, jump, strafe_right, strafe_left, hit, climb, dead };
const char* const kPhaseNames[] = {"stand",       "run",   "jump",   "strafe right",
                                   "strafe left", "blows", "run on", "dead"};

struct Step {
  Phase phase;
  double seconds;
};
const Step kScript[] = {{Phase::stand, 0.5},        {Phase::run, 2.0},         {Phase::jump, 1.0},
                        {Phase::strafe_right, 1.0}, {Phase::strafe_left, 1.0}, {Phase::hit, 1.4},
                        {Phase::climb, 2.5},        {Phase::dead, 1.2}};

struct Row {
  double t = 0;
  Phase phase = Phase::stand;
  double eye = 0, eye_classic = 0;      // the eye over the origin, units
  double roll = 0, roll_classic = 0;    // degrees, right side down positive
  double pitch = 0, pitch_classic = 0;  // degrees from the look, down positive
  bool ground = false;
};

struct Pictures {
  ge::gpu::Device* device = nullptr;
  const cl::SearchPath* files = nullptr;
  shooters_edge::look::Picture look;
  uint32_t w = 480, h = 300;
};

struct Film {
  std::string id, title, caption, look_for;
  gl::Picture picture;
};

void run_script(gl::Lab& lab, const cl::SearchPath& files, const std::string& map_name,
                const Pictures& pics, std::vector<Row>& rows, std::vector<Film>& film,
                bool& sequel) {
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  cl::Listen l;
  if (!map ||
      !cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error)) {
    lab.fail("se-view: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  const double tic = cl::listen_tic(l);
  // A map of brushes plays on the sequel's rules, whose server works the view out itself.
  sequel = sv::sequel_of(*l.rules) != nullptr;
  for (int i = 0; i < 100; i++) cl::listen_frame(l, {});  // spawned and landed
  const float yaw = longest_run(l);
  cl::ViewState shown;
  {
    ge::console::Cvars cvars;
    cm::CameraCvars cam;
    cl::add_view_camera_cvars(cvars, cam);
    cl::apply_view_cvars(cam, shown);
  }
  cl::WorldDrawPtr drawn;
  ge::gpu::Target out;
  if (pics.device != nullptr) {
    drawn = cl::create_world_draw(*pics.device, *pics.files, *l.map, pics.w, pics.h,
                                  wgpu::TextureFormat::RGBA8Unorm, pics.look, {}, error,
                                  {l.server->models, {}});
    if (!drawn) lab.note("se-view: no pictures: " + error);
    out = ge::gpu::create_target(*pics.device, "se-view.final", pics.w, pics.h);
  }
  cl::FxState fx_state;
  cl::FxFrame fx;
  const auto draw = [&](const std::string& id, const std::string& title, const std::string& caption,
                        const std::string& look_for, double t) {
    if (!drawn) return;
    const ge::render::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    cl::draw_world(*pics.device, *drawn, view, fx, nullptr, 1.0 / 30, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*pics.device, out);
    film.push_back(
        {id, title, caption, look_for, gl::Picture{int(img.width), int(img.height), img.rgba}});
  };
  Classic classic;
  double t = 0;
  bool drew_bob = false, drew_strafe = false, drew_kick = false, drew_dead = false;
  bool drew_shot = !sequel;
  for (const Step& step : kScript) {
    const int frames = static_cast<int>(step.seconds / tic);
    for (int i = 0; i < frames; i++) {
      sh::MoveCmd cmd;
      cmd.msec = static_cast<uint8_t>(std::lround(tic * 1000));
      cmd.angles = {0, yaw, 0};
      switch (step.phase) {
        case Phase::run:
        case Phase::climb:
          cmd.forward = 400;
          break;
        case Phase::jump:
          cmd.forward = 400;
          if (i == 0) cmd.buttons = sh::kButtonJump;
          break;
        case Phase::strafe_right:
          cmd.side = 350;
          break;
        case Phase::strafe_left:
          cmd.side = -350;
          break;
        case Phase::stand:  // the sequel's blaster, once: its kick for one rules frame
          if (sequel && i == int(0.25 / tic)) cmd.buttons = sh::kButtonAttack;
          break;
        default:
          break;
      }
      if (step.phase == Phase::dead && i == 0) cl::listen_command(l, "kill");
      l.view = {0, yaw, 0};
      cl::listen_frame(l, cmd);
      t += tic;
      // The blows: from the right 0.2 s in, from in front 0.9 s in (128 units off, eye high).
      const cl::PlayerState p = cl::player_state(l);
      const cl::Blow* blow = nullptr;
      cl::Blow b;
      if (step.phase == Phase::hit && (i == int(0.2 / tic) || i == int(0.9 / tic))) {
        const bool right = i == int(0.2 / tic);
        const double a = (yaw + (right ? -90.0 : 0.0)) * kDeg;
        b.armor = right ? 0 : 20;
        b.blood = right ? 30 : 20;
        b.from = {p.origin.x + static_cast<float>(128 * ge::core::libm::cos(a)),
                  p.origin.y + static_cast<float>(128 * ge::core::libm::sin(a)), p.origin.z};
        if (sequel) {  // the rules' own blow, as a hit by the sequel's fight lands it
          sh::sequel_view_blow(sv::sequel_of(*l.rules)->players[0].view, right ? 30 : 20,
                               right ? 30 : 20, b.from - p.origin);
        } else {
          l.blows.push_back(b);
          blow = &b;
        }
      }
      const ge::render::FrameView view = cl::frame_view(l, shown, 1, t, tic);
      cl::step_fx(l, shown, fx_state, t, tic, fx);
      Row r;
      r.t = t;
      r.phase = step.phase;
      r.ground = p.on_ground;
      r.eye = (shown.pose.eye.y - cl::engine_point(p.origin).y) / br::kUnitM;
      r.roll = shown.pose.roll / kDeg;
      r.pitch = -(shown.pose.pitch - cl::engine_pitch(0.0f)) / kDeg;  // from the look, level
      if (sequel) {
        // The rules' view as sent this frame: the window shows it eased between rules frames.
        const shooters_edge::server::PlayerNumbers& n = l.server->world.clients[0].numbers;
        r.eye_classic = n.view_offset[2];
        r.roll_classic = p.dead ? sh::kSequelDeadRoll : n.kick[2];
        r.pitch_classic = p.dead ? sh::kSequelDeadPitch : n.kick[0];
      } else {
        classic.frame(p, yaw, blow, tic, r.eye_classic, r.roll_classic, r.pitch_classic);
      }
      rows.push_back(r);
      (void)view;
      // The pictures: the bob's top while running, the roll at full strafe, the kick's first
      // frame, the dead view a second on (and the sequel's shot).
      const double bob_now = r.eye_classic - (sequel ? kSequelEye : p.view_ofs.z);
      if (!drew_shot && step.phase == Phase::stand && r.pitch_classic < -0.5) {
        drew_shot = true;
        draw("shot", "A SHOT: THE WEAPON'S KICK",
             "The frame the blaster fires: the view tipped 1 degree up and the eye 2 units back, "
             "for one rules frame.",
             "the view a touch higher than standing", t);
      }
      if (!drew_bob && step.phase == Phase::run && i > frames / 8 && p.on_ground &&
          bob_now >= (sequel ? kSequelBobTop : kBobHi - 1e-6)) {
        drew_bob = true;
        draw("run-bob", "RUNNING: THE BOB AT ITS TOP",
             sequel ? "From the eyes running at full speed, at the top of the stride (the eye 1.5 "
                      "units up, the view nodded down by the run and the stride)."
                    : "From the eyes running at full speed, at the top of the stride's bob (+4 "
                      "units).",
             "the view a little higher than standing, level", t);
      }
      if (!drew_strafe && step.phase == Phase::strafe_right && i == frames - 1) {
        drew_strafe = true;
        draw("strafe-right", "STRAFING RIGHT: THE ROLL",
             sequel ? "From the eyes strafing right at full speed: the horizon rolled 1.5 degrees "
                      "(300 x run_roll), right side down."
                    : "From the eyes strafing right at full speed: the horizon rolled 2 degrees, "
                      "right side down.",
             "the horizon tipped down to the right, a little", t);
      }
      if (!drew_kick &&
          (sequel ? step.phase == Phase::hit && r.roll_classic > 5 : blow != nullptr)) {
        drew_kick = true;
        draw("kick-right", "A BLOW FROM THE RIGHT: THE KICK",
             sequel ? "The rules frame a blow of 30 from the right lands: the view tipped 9 "
                      "degrees toward it (0.3 of 30 x 100 / health)."
                    : "The frame a blow of 30 from the right lands: the view tipped 9 degrees "
                      "toward it.",
             "the horizon tipped down to the right, more than the strafe's", t);
      }
      if (!drew_dead && step.phase == Phase::dead && i == frames - 1 && p.dead) {
        drew_dead = true;
        draw("dead", "DEAD: ON THE FLOOR",
             sequel ? "A second after the player died: the eye low, the view rolled 40 degrees "
                      "and looking 15 up."
                    : "A second after the player died: the eye 16 units under the origin, the "
                      "view rolled 80 degrees.",
             "the floor close and the world on its side", t);
      }
    }
  }
}

void chart(gl::Lab& lab, const std::vector<Row>& rows, bool sequel, const std::string& id,
           const std::string& title, const std::string& y_label, double Row::*view,
           double Row::*classic, const std::string& caption, const std::string& look_for) {
  gl::LineChart spec;
  spec.frame = {title,
                sequel ? "the sequel's rules, their view worked out by the server; a frame a point"
                       : "the classic look's defaults; one server frame a point",
                "t (s)", y_label};
  gl::Trace a{"the view", {}, {}},
      b{sequel ? "the rules' view as sent" : "the classic formula", {}, {}};
  for (const Row& r : rows) {
    a.x.push_back(r.t);
    a.y.push_back(r.*view);
    b.x.push_back(r.t);
    b.y.push_back(r.*classic);
  }
  spec.traces = {a, b};
  spec.spikes = false;
  for (size_t i = 0; i < rows.size();) {
    size_t j = i;
    while (j < rows.size() && rows[j].phase == rows[i].phase) j++;
    spec.bands.push_back({rows[i].t, rows[j - 1].t, kPhaseNames[int(rows[i].phase)]});
    i = j;
  }
  const gl::Chart c = gl::line_chart(spec);
  const std::string svg = gl::to_svg(c.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, c.title, caption, look_for, {}}, gl::to_picture(c.plot));
}

// The sequel's view, checked by its own numbers: the window eases the rules' view between their
// frames, so it trails what they sent by up to a frame and is not held to it.
// On a real map the walls cut the script's moves short, so only the fixture is held to them.
void check_sequel(gl::Lab& lab, const std::vector<Row>& rows, bool held) {
  double top = -1e9, strafe = 0, blow_roll = 0, blow_pitch = 0, shot = 0, dead_roll = 0;
  for (const Row& r : rows) {
    if (r.phase == Phase::run) top = std::max(top, r.eye_classic);
    if (r.phase == Phase::strafe_right) strafe = std::max(strafe, r.roll_classic);
    if (r.phase == Phase::hit) blow_roll = std::max(blow_roll, r.roll_classic);
    if (r.phase == Phase::hit) blow_pitch = std::min(blow_pitch, r.pitch_classic);
    if (r.phase == Phase::stand) shot = std::min(shot, r.pitch_classic);
    if (r.phase == Phase::dead) dead_roll = r.roll;
  }
  lab.metric("sequel.run.eye-top", top, "units");
  lab.metric("sequel.strafe.roll", strafe, "deg");
  lab.metric("sequel.blows.roll", blow_roll, "deg");
  lab.metric("sequel.blows.pitch", blow_pitch, "deg");
  lab.metric("sequel.shot.pitch", shot, "deg");
  lab.metric("sequel.dead.roll", dead_roll, "deg");
  std::string off;
  if (top < kSequelEye + 1) off += " the run's eye tops at " + std::to_string(top) + ";";
  if (strafe < 1.2 || strafe > 2.5) off += " the strafe rolls " + std::to_string(strafe) + ";";
  if (blow_roll < 8) off += " the blow from the right rolls " + std::to_string(blow_roll) + ";";
  if (blow_pitch > -4) off += " the blow from ahead tips " + std::to_string(blow_pitch) + ";";
  if (shot > -0.9) off += " the shot tips " + std::to_string(shot) + ";";
  if (std::fabs(dead_roll - sh::kSequelDeadRoll) > 0.5)
    off += " the dead view rolls " + std::to_string(dead_roll) + ";";
  if (held && !off.empty())
    lab.fail("se-view: the sequel's view is off its rules:" + off,
             "Chart the phase where it parts (sequel-eye.png, sequel-roll.png, sequel-pitch.png) "
             "against plugins/shooter/sequel_view.hpp's rules and its test. Skill: camera.");
}

void report(gl::Lab& lab, const std::vector<Row>& rows, bool sequel, bool fixture) {
  double eye = 0, roll = 0, pitch = 0, top = -1e9, low = 1e9, most_roll = 0, most_kick = 0;
  for (const Row& r : rows) {
    eye = std::max(eye, std::fabs(r.eye - r.eye_classic));
    roll = std::max(roll, std::fabs(r.roll - r.roll_classic));
    pitch = std::max(pitch, std::fabs(r.pitch - r.pitch_classic));
    if (r.phase == Phase::run) top = std::max(top, r.eye), low = std::min(low, r.eye);
    if (r.phase == Phase::strafe_right) most_roll = std::max(most_roll, r.roll);
    if (r.phase == Phase::hit) most_kick = std::max(most_kick, std::fabs(r.pitch));
  }
  if (sequel) check_sequel(lab, rows, fixture);
  if (!sequel) {
    lab.metric("eye.worst-off", eye, "units");
    lab.metric("roll.worst-off", roll, "deg");
    lab.metric("pitch.worst-off", pitch, "deg");
    lab.metric("run.eye-top", top, "units");
    lab.metric("run.eye-low", low, "units");
    lab.metric("strafe.roll", most_roll, "deg");
    lab.metric("blows.pitch", most_kick, "deg");
  }
  if (!sequel && (eye > 0.01 || roll > 0.01 || pitch > 0.01))
    lab.fail("se-view: the view is off the classic formulas by up to " + std::to_string(eye) +
                 " units, " + std::to_string(roll) + " degrees of roll and " +
                 std::to_string(pitch) + " of pitch",
             "Chart the phase where it parts (eye.png, roll.png, pitch.png) and compare "
             "engine/camera/first_person.cpp's classic row with docs/camera.md § Shooter's Edge's "
             "view. Skill: camera.");
  const char* ref = sequel ? "rules" : "classic";
  gl::LabTable table{sequel ? "the script's phases: the view against the rules' view as sent"
                            : "the script's phases: the view against the classic formulas",
                     {"phase", "from (s)", "eye (u)", ref, "roll (deg)", ref, "pitch (deg)", ref},
                     {}};
  for (size_t i = 0; i < rows.size();) {
    size_t j = i;
    while (j < rows.size() && rows[j].phase == rows[i].phase) j++;
    const Row& m = rows[(i + j) / 2];
    char b[8][32];
    std::snprintf(b[0], 32, "%.2f", rows[i].t);
    std::snprintf(b[1], 32, "%.3f", m.eye);
    std::snprintf(b[2], 32, "%.3f", m.eye_classic);
    std::snprintf(b[3], 32, "%.3f", m.roll);
    std::snprintf(b[4], 32, "%.3f", m.roll_classic);
    std::snprintf(b[5], 32, "%.3f", m.pitch);
    std::snprintf(b[6], 32, "%.3f", m.pitch_classic);
    table.rows.push_back({kPhaseNames[int(m.phase)], b[0], b[1], b[2], b[3], b[4], b[5], b[6]});
    i = j;
  }
  lab.table(table);
  if (sequel) {
    chart(lab, rows, true, "sequel-eye", "THE EYE OVER THE ORIGIN", "eye (units)", &Row::eye,
          &Row::eye_classic,
          "The eye over the origin through the script on the sequel's rules: 22 standing, the "
          "stride's lift running (at most 300 x bob_up), the shot's push back, eased up the step, "
          "low dead.",
          "the view's line following the rules' a frame behind");
    chart(lab, rows, true, "sequel-roll", "THE VIEW'S ROLL", "roll (deg, right side down)",
          &Row::roll, &Row::roll_classic,
          "The roll: the stride's sway, +1.5 strafing right and -1.5 left (run_roll), the kick "
          "toward a blow from the right fading over half a second, 40 dead.",
          "the view's line following the rules' a frame behind");
    chart(lab, rows, true, "sequel-pitch", "THE VIEW'S PITCH FROM THE LOOK", "pitch (deg, down)",
          &Row::pitch, &Row::pitch_classic,
          "The pitch off the look: the blaster's one-frame kick up, the run's nod down "
          "(run_pitch, bob_pitch), a blow from ahead tipping it up, 15 up dead.",
          "the view's line following the rules' a frame behind");
    return;
  }
  chart(lab, rows, false, "eye", "THE EYE OVER THE ORIGIN", "eye (units)", &Row::eye,
        &Row::eye_classic,
        "The eye's height over the player's origin through the script: 22 standing, the bob "
        "while running (-7..+4), held in the air, eased up the step, 16 under the origin dead.",
        "the view's line on the formula's everywhere");
  chart(lab, rows, false, "roll", "THE VIEW'S ROLL", "roll (deg, right side down)", &Row::roll,
        &Row::roll_classic,
        "The roll: +2 strafing right, -2 strafing left, the kick toward a blow from the right "
        "fading over half a second, 80 dead.",
        "the view's line on the formula's everywhere");
  chart(lab, rows, false, "pitch", "THE VIEW'S PITCH FROM THE LOOK", "pitch (deg, down)",
        &Row::pitch, &Row::pitch_classic,
        "The pitch off the player's look: the kick of a blow from in front, down toward it, "
        "fading over half a second.",
        "the view's line on the formula's everywhere");
}

// One run of the script: on the fixture by `sequel_fixture`'s rules, or on the folder's map.
void run_one(gl::Lab& lab, const std::string& folder, bool sequel_fixture) {
  std::string error;
  cl::SearchPath files;
  if (folder.empty()) {
    if (!fixture_files(files, sequel_fixture, error)) {
      lab.fail("se-view: the fixture: " + error, "Read the reason above. Skill: brush.");
      return;
    }
  } else {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("se-view: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
      return;
    }
  }
  Pictures pics;
  pics.files = &files;
  std::unique_ptr<ge::gpu::Device> device;
  if (!folder.empty()) {
    device = ge::gpu::create_device({.label = "se-view", .validation = 1});
    if (!device) lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
    pics.device = device.get();
    ge::console::Console con;
    shooters_edge::look::add_cvars(con, pics.look);
    con.execute(lab.cvars());
  }
  std::vector<Row> rows;
  std::vector<Film> film;
  bool sequel = false;
  run_script(lab, files, folder.empty() ? "steps" : lab.arg("map"), pics, rows, film, sequel);
  if (rows.empty()) return;
  report(lab, rows, sequel, folder.empty());
  for (const Film& f : film)
    lab.shot({(sequel ? "sequel-" : "") + f.id,
              f.title,
              f.caption,
              f.look_for,
              {{"map", lab.arg("map")}}},
             f.picture, gl::Det::measured);
}

void run(gl::Lab& lab) {
  const std::string folder = lab.arg("game");
  const std::string& rules = lab.arg("rules");
  if (!folder.empty()) {
    run_one(lab, folder, false);
    return;
  }
  if (rules != "sequel") run_one(lab, "", false);
  if (rules != "classic") run_one(lab, "", true);
}

const gl::LabSpec kSpec{
    "se-view",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the first-person view (client/present.hpp frame_view, engine/camera classic_first_person) "
    "against the classic look's formulas",
    "camera",
    "Does the first-person view bob, roll, kick and climb as the classic look does? A scripted "
    "run on the listen server, every frame's eye, roll and pitch against the classic formulas, "
    "and frames from the eyes on a real map.",
    {
        {"game", "", "a game folder (<base>/<folder>); empty: the fixture hall and its step"},
        {"map", "e1m1", "with --game: the map (a sequel's map plays on the sequel's rules)"},
        {"rules", "both", "without --game: the fixture's rules: classic, sequel or both"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
