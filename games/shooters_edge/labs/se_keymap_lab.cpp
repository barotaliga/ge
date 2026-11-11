// LAB `se-keymap`: WHICH KEYS A BOT PRESSES AS IT MOVES, drawn from above on the map. A 4on4 of
// our bots on our server (net/classic_team.hpp), played twice from one seed: with the bots'
// features as given (the pilot's wish pressed as analog axes), then with `+keys` (the wish pressed
// by a hand on a keyboard, bots/keyboard.hpp). One bot's path over a stretch of each match is
// drawn to scale on the floor (nav_top.hpp, shaded by height), every command's segment in the
// colour of the key set its command carried (forward and side turned to the nearest of the eight
// sets; grey none), and every `--every` seconds three arrows from where it stood: the velocity
// (black, its length the speed), the wish under the view (the key set's colour: the way the keys
// push) and the view (grey, short). A jump pressed is a ring, a bump (120 units/s lost to the
// room in a command) a red cross, a ground speed past a run's (330) a dark dot.
//
// What it answers: where a bot's keys disagree with where it goes (an orbit round a node, the
// diagonal that pushes the body past a run's speed, the brake that becomes a full key), and how
// a keyboard's path differs from analog axes'. Metrics per variant over the stretch: each key
// set's share of the commands on the ground, the share on the ground past a run's speed, the
// bumps, the mean speed.
//
// Without `--basedir` it plays the fixture halls (what `make labs-smoke` runs).
//
// ge:summary Lab: one bot's path from above, each step coloured by the movement keys it pressed
// ge:keywords lab se-keymap keys keyboard key-set path top-view wish velocity arrows bumps
// ge:keywords overspeed analog-axes keymap movement-keys
// ge:skill bots labs
// ge:test tests/shooters_edge_footwork_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/entity.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/nav_top.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

constexpr double kBump = 120;       // speed lost in a command to the room: a bump, units/s
constexpr double kRun = 330;        // a ground speed past this is past a run's, units/s
constexpr double kVelScale = 0.15;  // the velocity arrow: pixels a unit/s
constexpr double kWishLen = 40;     // the wish arrow's length, pixels
constexpr double kViewLen = 22;     // the view's tick, pixels
const gl::Rgba kInk{30, 30, 30, 255};
const gl::Rgba kNone{150, 150, 150, 255};
// The key sets clockwise from forward: W, W+D, D, S+D, S, S+A, A, W+A.
const gl::Rgba kSet[8] = {{30, 80, 220, 255},  {0, 165, 165, 255}, {30, 155, 40, 255},
                          {170, 145, 0, 255},  {215, 40, 40, 255}, {230, 120, 0, 255},
                          {150, 50, 200, 255}, {220, 60, 170, 255}};
const char* kSetName[8] = {"W", "WD", "D", "SD", "S", "SA", "A", "WA"};

struct Sample {
  double t = 0;
  sh::Vec3f at, vel;
  float yaw = 0;
  int16_t forward = 0, side = 0;
  bool jump = false, ground = false, alive = false;
};

struct Recorder {
  int bot = 0;
  std::vector<Sample> samples;
};

// The key set of a command's forward and side (0 forward, clockwise; -1 none).
int key_set(int16_t forward, int16_t side) {
  if (forward == 0 && side == 0) return -1;
  double ang = ge::core::libm::atan2(side, forward) * (180.0 / ge::core::kPi);
  if (ang < 0) ang += 360;
  return static_cast<int>(std::floor(ang / 45 + 0.5)) % 8;
}

void arrow(gl::Picture& p, double x0, double y0, double x1, double y1, gl::Rgba c, double w) {
  gl::draw_line(p, x0, y0, x1, y1, c, w);
  const double dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
  if (len < 4) return;
  const double ux = dx / len, uy = dy / len, h = std::min(8.0, len * 0.4);
  gl::draw_line(p, x1, y1, x1 - h * (ux * 0.87 - uy * 0.5), y1 - h * (uy * 0.87 + ux * 0.5), c, w);
  gl::draw_line(p, x1, y1, x1 - h * (ux * 0.87 + uy * 0.5), y1 - h * (uy * 0.87 - ux * 0.5), c, w);
}

struct Stretch {
  std::vector<Sample> s;
  int64_t ground = 0, over = 0, bumps = 0;
  std::array<int64_t, 9> sets{};  // each set on the ground, the last none
  double speed = 0;
};

Stretch stretch_of(const std::vector<Sample>& all, double from, double seconds) {
  Stretch out;
  for (const Sample& s : all)
    if (s.t >= from && s.t < from + seconds && s.alive) out.s.push_back(s);
  for (size_t i = 0; i < out.s.size(); i++) {
    const Sample& s = out.s[i];
    const double sp = std::sqrt(s.vel.x * s.vel.x + s.vel.y * s.vel.y);
    out.speed += sp;
    if (s.ground) {
      out.ground++;
      out.over += sp > kRun;
      const int k = key_set(s.forward, s.side);
      out.sets[static_cast<size_t>(k < 0 ? 8 : k)]++;
    }
    if (i > 0) {
      const Sample& a = out.s[i - 1];
      const double was = std::sqrt(a.vel.x * a.vel.x + a.vel.y * a.vel.y);
      out.bumps += was - sp >= kBump;
    }
  }
  if (!out.s.empty()) out.speed /= static_cast<double>(out.s.size());
  return out;
}

// The window's centre: `at` ("x,y") when given, else the path's middle.
void centre_of(const Stretch& st, const std::string& at, double& cx, double& cy) {
  cx = cy = 0;
  const size_t comma = at.find(',');
  if (comma != std::string::npos) {
    cx = std::strtod(at.c_str(), nullptr);
    cy = std::strtod(at.c_str() + comma + 1, nullptr);
    return;
  }
  for (const Sample& s : st.s) {
    cx += s.at.x;
    cy += s.at.y;
  }
  const double n = std::max<double>(1, static_cast<double>(st.s.size()));
  cx = std::round(cx / n);
  cy = std::round(cy / n);
}

gl::Picture draw(const sh::MoveWorld& world, const Stretch& st, double cx, double cy, double span,
                 double every) {
  // The window: `span` across round (cx, cy); the floor traced under the path.
  float zlo = 1e9f, zhi = -1e9f;
  for (const Sample& s : st.s) {
    zlo = std::min(zlo, s.at.z);
    zhi = std::max(zhi, s.at.z);
  }
  if (st.s.empty()) zlo = zhi = 0;
  const auto half = static_cast<float>(span / 2);
  sl::TopView v{{static_cast<float>(cx) - half, static_cast<float>(cy) - half, zlo - 64},
                {static_cast<float>(cx) + half, static_cast<float>(cy) + half, zhi + 48}};
  gl::Picture p = sl::floor_picture(world, v);
  // The path, a command a segment in its key set's colour.
  for (size_t i = 1; i < st.s.size(); i++) {
    const Sample &a = st.s[i - 1], &b = st.s[i];
    const double dx = b.at.x - a.at.x, dy = b.at.y - a.at.y;
    if (dx * dx + dy * dy > 64 * 64) continue;  // a teleport or a respawn
    const int k = key_set(b.forward, b.side);
    gl::draw_line(p, v.px(a.at.x), v.py(a.at.y), v.px(b.at.x), v.py(b.at.y),
                  k < 0 ? kNone : kSet[k], 3);
  }
  // Every `every` s: the velocity, the wish under the view, the view; jumps, bumps, overspeed.
  double next = st.s.empty() ? 0 : st.s.front().t;
  for (size_t i = 0; i < st.s.size(); i++) {
    const Sample& s = st.s[i];
    const double x = v.px(s.at.x), y = v.py(s.at.y);
    const double sp = std::sqrt(s.vel.x * s.vel.x + s.vel.y * s.vel.y);
    if (s.ground && sp > kRun) gl::fill_disc(p, x, y, 2, kInk);
    if (s.jump && (i == 0 || !st.s[i - 1].jump)) gl::draw_ring(p, x, y, 5, kInk);
    if (i > 0) {
      const Sample& a = st.s[i - 1];
      const double was = std::sqrt(a.vel.x * a.vel.x + a.vel.y * a.vel.y);
      if (was - sp >= kBump) {
        const gl::Rgba red{230, 20, 20, 255};
        gl::draw_line(p, x - 6, y - 6, x + 6, y + 6, red, 3);
        gl::draw_line(p, x - 6, y + 6, x + 6, y - 6, red, 3);
      }
    }
    if (s.t < next) continue;
    next = s.t + every;
    const double yr = s.yaw * (ge::core::kPi / 180.0);
    const double fx = ge::core::libm::cos(yr), fy = ge::core::libm::sin(yr);
    // The picture's y runs down: a world vector (wx, wy) is (wx, -wy) on it.
    gl::draw_line(p, x, y, x + fx * kViewLen, y - fy * kViewLen, {120, 120, 120, 255}, 1);
    arrow(p, x, y, x + s.vel.x * kVelScale, y - s.vel.y * kVelScale, kInk, 1.5);
    const int k = key_set(s.forward, s.side);
    if (k >= 0) {
      const double f = s.forward, r = s.side, m = std::sqrt(f * f + r * r);
      // Forward along the view, right a quarter turn clockwise of it.
      const double wx = (f * fx + r * fy) / m, wy = (f * fy - r * fx) / m;
      arrow(p, x, y, x + wx * kWishLen, y - wy * kWishLen, kSet[k], 2);
    }
  }
  // The legend along the top margin.
  int lx = sl::kTopMargin;
  for (int k = 0; k < 8; k++) {
    gl::fill_rect(p, lx, 8, 14, 6, kSet[k]);
    gl::draw_text(p, lx + 17, 7, kSetName[k], kInk);
    lx += 17 + gl::text_width(kSetName[k]) + 10;
  }
  gl::fill_rect(p, lx, 8, 14, 6, kNone);
  gl::draw_text(p, lx + 17, 7, "NONE", kInk);
  lx += 17 + gl::text_width("NONE") + 14;
  gl::draw_text(p, lx, 7, "BLACK VELOCITY  GREY VIEW  RING JUMP  X BUMP  DOT PAST 330", kInk);
  return p;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double from = lab.arg_num("from"), seconds = lab.arg_num("seconds");
  const double span = lab.arg_num("span"), every = lab.arg_num("every");
  const int bot = static_cast<int>(lab.arg_num("bot"));
  const std::string basedir = lab.arg("basedir");
  std::string error;
  sl::StudyMap m;
  if (!sl::load_study_map("", basedir, lab.arg("game"), lab.arg("map"), m, error)) {
    lab.fail("se-keymap: " + error, "next: pass --basedir=<the folder that holds the game folder>");
    return;
  }
  std::vector<uint8_t> bytes;
  if (!basedir.empty()) {
    cl::SearchPath files;
    std::vector<uint8_t> store;
    if (cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      const std::span<const uint8_t> f =
          cl::find_file(files, "maps/" + lab.arg("map") + ".bsp", store);
      bytes.assign(f.begin(), f.end());
    }
  } else {
    ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes, &error);
  }
  ge::progs::Progs progs;
  if (bytes.empty() || !cl::baseline_progs(progs, error)) {
    lab.fail("se-keymap: the map or the baseline mod: " + error,
             "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  const std::string name = basedir.empty() ? "halls" : lab.arg("map");
  const In in = {{"map", name},
                 {"seed", lab.arg("seed")},
                 {"bot", lab.arg("bot")},
                 {"from", lab.arg("from")},
                 {"seconds", lab.arg("seconds")},
                 {"features", lab.arg("features")}};
  const struct {
    const char* id;
    const char* words;
    std::string features;
  } variants[2] = {{"analog", "ANALOG AXES", lab.arg("features") + ",-keys,-keysteer"},
                   {"keys", "A HAND ON THE KEYS", lab.arg("features") + ",+keys"}};
  for (const auto& var : variants) {
    cn::TeamMatchOptions o;
    o.map = bytes;
    o.map_name = name;
    if (basedir.empty()) o.entities = cn::kTeamHallsEntities;
    o.progs = &progs;
    o.seed = seed;
    o.client_fed = true;
    for (cn::TeamSide& side : o.teams) side.features = var.features;
    Recorder rec;
    rec.bot = bot;
    o.on_command_ctx = &rec;
    o.on_command = [](void* ctx, int k, const shooters_edge::bots::Fighter& f,
                      const shooters_edge::server::RulesCommon& w, const sh::MoveCmd& cmd) {
      Recorder& r = *static_cast<Recorder*>(ctx);
      if (k != r.bot) return;
      const auto& e = w.world.entities[static_cast<size_t>(f.slot) + 1];
      const sh::Edict& d = w.sv.edicts[static_cast<size_t>(f.slot) + 1];
      Sample s;
      s.t = w.sv.time;
      s.at = e.origin;
      s.vel = e.velocity;
      s.yaw = cmd.angles.y;
      s.forward = cmd.forward;
      s.side = cmd.side;
      s.jump = (cmd.buttons & sh::kButtonJump) != 0;
      s.ground = (d.flags & sh::kFlagOnGround) != 0;
      s.alive = e.health > 0;
      r.samples.push_back(s);
    };
    o.frames = static_cast<int>(
        std::lround((from + seconds + 1) * 1000 / shooters_edge::bots::kPilotMsec));
    const cn::TeamMatchReport r = cn::run_team_match(o);
    if (!r.ok) {
      lab.fail("se-keymap: " + r.error, "next: run tests/shooters_edge_team_test.cpp");
      return;
    }
    const Stretch st = stretch_of(rec.samples, from, seconds);
    const std::string key = std::string(var.id) + ".";
    const double g = static_cast<double>(std::max<int64_t>(1, st.ground));
    for (int k = 0; k < 8; k++)
      lab.metric(key + "ground." + kSetName[k],
                 static_cast<double>(st.sets[static_cast<size_t>(k)]) / g, "share");
    lab.metric(key + "ground.none", static_cast<double>(st.sets[8]) / g, "share");
    lab.metric(key + "ground.past-run", static_cast<double>(st.over) / g, "share");
    lab.metric(key + "bumps", static_cast<double>(st.bumps), "count");
    lab.metric(key + "speed", st.speed, "units/s");
    double cx = 0, cy = 0;
    centre_of(st, lab.arg("at"), cx, cy);
    const gl::Picture p = draw(*m.world, st, cx, cy, span, every);
    lab.shot({std::string("path-") + var.id,
              std::string("BOT ") + std::to_string(bot) + " FROM ABOVE: " + var.words,
              "To scale from above, north up, the floor shaded by height, centred on " +
                  to_fixed(cx, 0) + "," + to_fixed(cy, 0) + ": bot " + std::to_string(bot) +
                  "'s path from " + to_fixed(from, 0) + " s for " + to_fixed(seconds, 0) +
                  " s, each command's step in the colour of the key set it "
                  "carried (W blue, D green, S red, A purple, the diagonals between; grey none). "
                  "Every " +
                  to_fixed(every, 2) +
                  " s: the velocity (black, its length the speed), "
                  "the wish under the view (the key set's colour) and the view (grey). " +
                  std::to_string(st.bumps) + " bumps, " +
                  to_fixed(100.0 * static_cast<double>(st.over) / g, 0) +
                  " % of the ground past a run's speed, " + to_fixed(st.speed, 0) + " units/s.",
              "the wish arrows on the way the path goes; no circles round a point; few dots (a "
              "run past 330 on the ground) and crosses; keys: mostly blue on a straight",
              in},
             p);
  }
}

const gl::LabSpec kSpec{
    "se-keymap",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "one bot's path from above, each step coloured by the movement keys it pressed, analog axes "
    "against a hand on the keys (games/shooters_edge/labs/se_keymap_lab.cpp)",
    "bots",
    "Which keys does a bot press as it moves, and where do they disagree with where it goes?",
    {
        {"basedir", "", "a folder holding the game folder (empty: the fixture halls)"},
        {"game", "id1", "the game folder under --basedir"},
        {"map", "e1m2", "with --basedir: the map"},
        {"seed", "1", "the match's seed"},
        {"bot", "0", "the player drawn (0 to 7: red's four, then blue's)"},
        {"from", "5", "the stretch drawn starts this far into the match, s"},
        {"seconds", "20", "and lasts this long, s"},
        {"span", "1024", "the window across, units (a pixel a unit)"},
        {"at", "", "the window's centre, x,y units (empty: the path's middle)"},
        {"every", "0.25", "the arrows every this many seconds"},
        {"features", "", "the bots' features over each skill's preset (shooter/features.hpp)"},
    },
    "--from=2 --seconds=8 --span=768",
    run,
};
GE_LAB(kSpec);

}  // namespace
