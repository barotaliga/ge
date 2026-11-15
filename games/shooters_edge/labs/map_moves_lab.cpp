// LAB `map-moves`: HOW HIGH AND HOW FAR CAN A PLAYER GET, BY EACH WAY OF MOVING, IN THE FIRST GAME?
// The numbers a map is laid out by (the `map-design` skill), measured on the move itself rather
// than remembered: a course of test cells is compiled by the map compiler (plugins/brush/compile),
// read back as the move's world, and every way up and across is flown on it by the nav's proofs
// (plugins/shooter/nav.hpp), which run the player's move command by command with the rules' own
// push (games/shooters_edge/rules: `blow_push` gives damage x 8 units/s, the blast takes the
// rocket's 120 less half the distance, half of it to its own shooter and all of it to a teammate).
//
// THE CELLS: a LEDGE cell is a room with a block of height h across its far end, the player
// starting on the floor at several distances from its foot; a GAP cell is two floors at the same
// height with a pit w wide between them. Each way of moving is tried in every cell, and the
// tables keep the highest ledge and the widest gap it reached:
// - jump: standing, and running at 320 (the walk's top speed);
// - speed jump: leaving the floor at 480 and 640 (a strafe jump's or a bunny hop's speed);
// - head jump: the same jump from a teammate's head (the start lifted 56, a player's height);
// - rocket jump: the shot at the feet while running, and the flip (the shot behind);
// - grenade jump: the grenade rolled ahead and jumped over as it bursts;
// - team boost: a teammate's rocket at the jumper's feet (all of the push, no harm under teamplay)
//   and a teammate's double barrel point-blank in the back (14 pellets of 4: 448 units/s added to
//   the run).
// Each rocket and grenade jump's health cost is the blast's points to the jumper.
//
// ge:summary Lab: the heights and gaps each way of moving reaches (jump, hop, head, rocket, boost)
// ge:keywords lab map-moves map-design jump-height gap rocket-jump grenade-jump head-jump boost
// ge:keywords team-boost flip speed-jump ledge measurements level-design numbers
// ge:skill map-design labs
// ge:test tests/brush_compile_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "brush/read.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/bsp_world.hpp"
#include "shooter/nav.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace mc = ge::brush::mapc;

constexpr double kCellW = 256, kStride = 384;  // a cell's width; cells this far apart
constexpr double kLedgeAt = 768;               // a ledge cell's block starts this far along y
constexpr double kRoomTop = 1536;
constexpr int kPerRow = 8;

// The ways of moving tried, in the tables' order.
enum Way : uint8_t {
  stand_jump,
  run_jump,
  hop_480,
  hop_640,
  head_stand,
  head_run,
  rocket_feet,
  rocket_flip,
  grenade,
  boost_rocket,
  boost_double,
  kWays
};
constexpr const char* kWayNames[kWays] = {
    "jump, standing",      "jump, running 320",  "speed jump 480",   "speed jump 640",
    "head jump, standing", "head jump, running", "rocket jump",      "rocket jump, flip",
    "grenade jump",        "team rocket boost",  "team double boost"};
constexpr gl::Rgba kWayColours[kWays] = {
    {150, 150, 150, 255}, {110, 110, 110, 255}, {90, 140, 210, 255}, {50, 100, 190, 255},
    {120, 190, 120, 255}, {60, 150, 70, 255},   {230, 120, 50, 255}, {200, 70, 40, 255},
    {200, 170, 40, 255},  {170, 80, 170, 255},  {120, 60, 140, 255}};

struct Cell {
  bool gap = false;
  double size = 0;  // the ledge's height or the gap's width
  double x0 = 0, y0 = 0;
};

std::string n(double v) { return ge::core::num::to_fixed(v, 0); }

// The course as a plan: every cell its own sealed room, lit by one light (nothing is drawn).
std::string course_plan(std::vector<Cell>& cells, const std::vector<double>& heights,
                        const std::vector<double>& gaps) {
  std::string p = "world message=\"Moves\" light=200\ntextures floor=f wall=w ceil=c solid=w\n";
  int k = 0;
  auto place = [&](Cell c) {
    c.x0 = (k % kPerRow) * kStride;
    c.y0 = double(k / kPerRow) * (kRoomTop + 2048 + 256);
    ++k;
    cells.push_back(c);
    return cells.back();
  };
  for (const double h : heights) {
    for (int head = 0; head < 2; ++head) {
      Cell c = place({false, h});
      const double x1 = c.x0 + kCellW, ym = c.y0 + kLedgeAt, y1 = c.y0 + kLedgeAt + 512;
      p += "room cell z=0.." + n(kRoomTop) + " box " + n(c.x0) + "," + n(c.y0) + " " + n(x1) + "," +
           n(y1) + "\n";
      p += "solid ledge z=0.." + n(h) + " box " + n(c.x0) + "," + n(ym) + " " + n(x1) + "," +
           n(y1) + "\n";
      p += "light " + n(c.x0 + 128) + "," + n(c.y0 + 64) + ",256\n";
      (void)head;
    }
  }
  for (const double w : gaps) {
    Cell c = place({true, w});
    const double x1 = c.x0 + kCellW, ym = c.y0 + 768, y1 = ym + w + 512;
    p += "room cell z=-512.." + n(kRoomTop) + " box " + n(c.x0) + "," + n(c.y0) + " " + n(x1) +
         "," + n(y1) + "\n";
    p += "solid near z=-512..0 box " + n(c.x0) + "," + n(c.y0) + " " + n(x1) + "," + n(ym) + "\n";
    p += "solid far z=-512..0 box " + n(c.x0) + "," + n(ym + w) + " " + n(x1) + "," + n(y1) + "\n";
    p += "light " + n(c.x0 + 128) + "," + n(c.y0 + 64) + ",256\n";
  }
  // The compiler wants a start; the first cell's corner.
  p += "spawn " + n(cells[0].x0 + 64) + "," + n(cells[0].y0 + 64) + ",24\n";
  return p;
}

struct Try {
  bool reached = false;
  float health = 0;
};

// Way `w` in cell `c` (`head`: the start on a teammate's head), from each run-up distance.
Try fly(const sh::MoveWorld& world, const sh::MoveSettings& move, const Cell& c, Way w) {
  const bool head = w == head_stand || w == head_run;
  sh::NavSettings set;
  set.limit_s = 4;
  const float xc = float(c.x0 + kCellW / 2);
  const float edge = float(c.gap ? c.y0 + 768 : c.y0 + kLedgeAt);
  const float top = c.gap ? 0.f : float(c.size);
  const float stand_z = head ? 24.f + 56.f : 24.f;
  // From right at the foot to a long run-up, to a landing near the lip or further on (a proof
  // stands where it lands: a running jump comes down far past the lip).
  const float offs[] = {20, 48, 96, 160, 256, 384, 512};
  const float lands[] = {48, 112, 192, 288};
  Try best;
  for (const float land : lands)
    for (const float back : offs) {
      const sh::Vec3f to{xc, c.gap ? float(edge + c.size + land) : edge + land, top + 24};
      const sh::Vec3f from{xc, edge - back, stand_z};
      sh::NavProof p;
      float hp = 0;
      switch (w) {
        case stand_jump:
        case head_stand:
          p = sh::prove_link(world, move, from, to, sh::NavKind::jump, 0, set);
          break;
        case run_jump:
        case head_run:
          p = sh::prove_link(world, move, from, to, sh::NavKind::jump, 320, set);
          break;
        case hop_480:
          p = sh::prove_link(world, move, from, to, sh::NavKind::jump, 480, set);
          break;
        case hop_640:
          p = sh::prove_link(world, move, from, to, sh::NavKind::jump, 640, set);
          break;
        case boost_double:
          p = sh::prove_link(world, move, from, to, sh::NavKind::jump, 320 + 448, set);
          break;
        case rocket_feet: {
          sh::RocketJumpSpec rj;
          for (const float pitch : {80.f, 70.f, 60.f}) {
            rj.pitch = pitch;
            p = sh::prove_rocket_jump(world, move, from, to, 320, set, rj, &hp);
            if (p.reached) break;
          }
          break;
        }
        case rocket_flip: {
          sh::RocketJumpSpec rj;
          rj.back = true;
          p = sh::prove_rocket_jump(world, move, from, to, 320, set, rj, &hp);
          break;
        }
        case boost_rocket: {
          sh::RocketJumpSpec rj;
          rj.self_share = 1;  // a teammate's blast: all of the push
          for (const float pitch : {80.f, 70.f, 60.f}) {
            rj.pitch = pitch;
            p = sh::prove_rocket_jump(world, move, from, to, 320, set, rj, &hp);
            if (p.reached) break;
          }
          hp = 0;  // teamplay spares a teammate
          break;
        }
        case grenade: {
          for (const float lead : {0.f, 16.f, 32.f}) {
            p = sh::prove_grenade_jump(world, move, from, to, lead, set, {}, &hp);
            if (p.reached) break;
          }
          break;
        }
        default:
          break;
      }
      if (p.reached) {
        if (!best.reached || hp < best.health) best = {true, hp};
        if (hp == 0) return best;
      }
    }
  return best;
}

void run(gl::Lab& lab) {
  const bool smoke = lab.arg_bool("quick");
  std::vector<double> heights, gaps;
  const double hstep = smoke ? 64 : 16, gstep = smoke ? 128 : 32;
  for (int i = 1; double(i) * hstep <= 512; ++i) heights.push_back(double(i) * hstep);
  for (int i = 0; 64 + double(i) * gstep <= 1024; ++i) gaps.push_back(64 + double(i) * gstep);
  std::vector<Cell> cells;
  const std::string plan = course_plan(cells, heights, gaps);
  mc::Source src;
  std::string err;
  if (!mc::read_plan(plan, &src, &err)) {
    lab.fail("map-moves: the course does not read: " + err, "next: fix course_plan");
    return;
  }
  mc::CompileOptions opt;
  opt.light = false;
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  if (!mc::compile_map(src, opt, &bytes, &rep, &err)) {
    lab.fail("map-moves: the course does not compile: " + err, "next: fix course_plan");
    return;
  }
  ge::brush::Map map;
  sh::BspWorld bsp;
  sh::MoveWorld world;
  if (!ge::brush::read_bsp(bytes, &map, &err) || !sh::bsp_world(map, &bsp, &err)) {
    lab.fail("map-moves: the course does not read back: " + err, "next: run brush_compile_test");
    return;
  }
  sh::map_world(bsp.models[0], world);
  const sh::MoveSettings move;

  // Every way in every cell.
  double best_h[kWays] = {}, best_g[kWays] = {}, hp_h[kWays] = {}, hp_g[kWays] = {};
  std::vector<std::vector<uint8_t>> up(kWays), across(kWays);
  size_t li = 0;
  for (size_t ci = 0; ci < cells.size(); ++ci) {
    const Cell& c = cells[ci];
    const bool head_cell = !c.gap && (li++ % 2 == 1);
    for (int w = 0; w < kWays; ++w) {
      const bool head_way = w == head_stand || w == head_run;
      if (!c.gap && head_way != head_cell) continue;
      if (c.gap && head_way) continue;
      const Try t = fly(world, move, c, Way(w));
      (c.gap ? across : up)[size_t(w)].push_back(t.reached ? 1 : 0);
      if (!t.reached) continue;
      if (c.gap && c.size > best_g[w]) {
        best_g[w] = c.size;
        hp_g[w] = t.health;
      }
      if (!c.gap && c.size > best_h[w]) {
        best_h[w] = c.size;
        hp_h[w] = t.health;
      }
    }
  }

  gl::LabTable t{"the highest ledge and the widest pit each way reaches (units; health spent)",
                 {"way", "ledge", "health", "pit", "health"},
                 {}};
  for (int w = 0; w < kWays; ++w) {
    const bool head_way = w == head_stand || w == head_run;
    t.rows.push_back({kWayNames[w], n(best_h[w]), n(hp_h[w]), head_way ? "-" : n(best_g[w]),
                      head_way ? "-" : n(hp_g[w])});
    std::string key = kWayNames[w];
    for (char& ch : key) {
      if (ch == ' ' || ch == ',') ch = '-';
    }
    key.erase(
        std::unique(key.begin(), key.end(), [](char a, char b) { return a == '-' && b == '-'; }),
        key.end());
    lab.metric("ledge." + key, best_h[w], "units");
    if (!head_way) lab.metric("pit." + key, best_g[w], "units");
  }
  lab.table(t);

  // The picture: each way a bar to its highest ledge, beside each way's widest pit.
  const int W = 1100, H = 560, left = 190, top = 40, bar = 16;
  gl::Picture p = gl::create_picture(W, H, {248, 247, 243, 255});
  const gl::Rgba ink{40, 40, 46, 255}, faint{215, 213, 205, 255};
  const double hmax = 512, gmax = 1024;
  const int span = (W - left - 60) / 2 - 20;
  gl::draw_text(p, left, 14, "HIGHEST LEDGE (UNITS)", ink);
  gl::draw_text(p, left + span + 40, 14, "WIDEST PIT (UNITS)", ink);
  for (int v = 0; v <= 512; v += 64) {
    const double x = left + span * v / hmax;
    gl::draw_line(p, x, top - 4, x, top + kWays * (bar + 26), faint);
    gl::draw_text(p, x - 6, top + kWays * (bar + 26) + 4, std::to_string(v), ink);
  }
  for (int v = 0; v <= 1024; v += 128) {
    const double x = left + span + 40 + span * v / gmax;
    gl::draw_line(p, x, top - 4, x, top + kWays * (bar + 26), faint);
    gl::draw_text(p, x - 8, top + kWays * (bar + 26) + 4, std::to_string(v), ink);
  }
  for (int w = 0; w < kWays; ++w) {
    const int y = top + w * (bar + 26);
    gl::draw_text(p, 8, y + 4, kWayNames[w], ink);
    gl::fill_rect(p, left, y, span * best_h[w] / hmax, bar, kWayColours[w]);
    gl::draw_text(p, left + span * best_h[w] / hmax + 4, y + 4, n(best_h[w]), ink);
    const bool head_way = w == head_stand || w == head_run;
    if (!head_way) {
      gl::fill_rect(p, left + span + 40, y, span * best_g[w] / gmax, bar, kWayColours[w]);
      gl::draw_text(p, left + span + 40 + span * best_g[w] / gmax + 4, y + 4, n(best_g[w]), ink);
    }
  }
  lab.shot({"reach",
            "how high and how far each way of moving gets",
            "Every way of moving flown on the move over a course of compiled test cells: the "
            "highest ledge each reached (left) and the widest pit at the same height it crossed "
            "(right), from the best of seven run-ups.",
            "a plain jump clears a little over 40; a rocket jump several times that; a team "
            "boost reaches what no one reaches alone; the flip carries further than the shot at "
            "the feet",
            {{"cells", std::to_string(cells.size())}}},
           p);
  lab.metric("cells", double(cells.size()), "count");
}

const gl::LabSpec kSpec{
    "map-moves",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/nav.hpp's proofs (jump, rocket and grenade jumps) on a course the map "
    "compiler builds, with the first game's rules' push",
    "map-design",
    "How high and how far does a player get by each way of moving in the first game (jump, speed "
    "jump, head jump, rocket and grenade jumps, team boosts), measured on the move?",
    {
        {"quick", "0", "1: a coarse course (every 64 up, every 128 across)"},
    },
    "--quick=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
