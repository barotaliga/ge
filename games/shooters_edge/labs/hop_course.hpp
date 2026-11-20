// THE HOP COURSE: a practice map made only for bunny hopping, for the first game's move, written as
// a list of PIECES (straights, arcs, a corridor that narrows) and turned into a map plan
// (plugins/brush/compile/plan.hpp) by `hop_course_plan`, so the map, the sections a lab reports
// by and its pictures all come from one list. The `se-bhop` lab compiles it and runs bots down it;
// `--write-plan=<file>` writes the plan for `secli map build <file> --textures=<game folder>`, to
// hop it by hand.
//
// THE COURSE (a snake of three lanes, run once from the spawn to the end, 12 400 units along its
// middle): a run-up straight, a long straight to weave on, gentle S-bends, a wide corner (radius
// 640), a middle one (320), a room 768 across with its exit a 128-wide mouth flush with one side
// (as a hop leaves a room by a side door), a tight corner (176) the other way, a U-turn (400), and
// a corridor that narrows from 320 across to 128. Floor and walls plain, 256 high, no items, no
// harm, one spawn. The corners' radii are the middle's: a hopper cuts them on the inside. Widths
// are the floor's.
//
// THE STATION (`course_station`): where a point lies along the course, the distance along its
// middle to its nearest point and the section that holds it, so a run is read section by section
// whatever line it takes.
//
// ge:summary The hop course: a bunny-hop practice map as pieces, its plan text and its stations
// ge:keywords hop-course bunny-hop practice-map plan pieces straight arc s-bends corner u-turn
// ge:keywords narrowing-corridor room-exit station section se-bhop
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"

namespace shooters_edge::labs {

struct HopPiece {
  const char* section;           // the section it belongs to (consecutive pieces may share one)
  float length = 0;              // a straight's, units; 0: an arc
  float radius = 0;              // an arc's middle, units
  float turn = 0;                // an arc's turn, degrees (+ left, counter-clockwise from above)
  float width0 = 0, width1 = 0;  // the floor across at its start and its end, units
  float shift = 0;  // a straight's floor moved off the middle, units (+ left): a room's side exit
};

// The course, in order from the spawn.
inline const std::vector<HopPiece>& hop_pieces() {
  static const std::vector<HopPiece> p = {
      {"run-up", 640, 0, 0, 256, 256},         {"straight", 2304, 0, 0, 320, 320},
      {"s-bends", 0, 1024, 30, 320, 320},      {"s-bends", 0, 1024, -60, 320, 320},
      {"s-bends", 0, 1024, 30, 320, 320},      {"corner-640", 0, 640, 90, 320, 320},
      {"corner-640", 384, 0, 0, 320, 320},     {"corner-320", 0, 320, 90, 288, 288},
      {"room-exit", 768, 0, 0, 768, 768, 256}, {"room-exit", 256, 0, 0, 128, 128},
      {"lane", 768, 0, 0, 288, 288},           {"corner-176", 0, 176, -90, 256, 256},
      {"corner-176", 384, 0, 0, 256, 256},     {"u-turn", 0, 400, 180, 288, 288},
      {"u-turn", 512, 0, 0, 288, 288},         {"narrows", 896, 0, 0, 320, 128},
      {"narrows", 128, 0, 0, 128, 128},
  };
  return p;
}

inline constexpr float kHopCourseHigh = 256;  // the corridors' height, units
inline constexpr float kHopArcStep = 10;      // an arc is cut into pieces of this turn, degrees

struct HopQuad {
  float x[4], y[4];  // counter-clockwise from above
  int section = 0;
};

// The course drawn out: the floor's quads, the middle line with its distance along at each point
// and the section each point's segment belongs to, and the sections' names and where each starts.
struct HopCourse {
  std::vector<HopQuad> quads;
  std::vector<float> mx, my, at;  // the middle line, and the distance along it at each point
  std::vector<int> seg_section;   // the section of the segment from point i to i + 1
  std::vector<std::string> sections;
  std::vector<float> section_from;  // where each section starts along the middle
  float length = 0;
  float spawn_x = 64, spawn_y = 0;  // the spawn, facing +x
};

inline HopCourse make_hop_course() {
  namespace lm = ge::core::libm;
  HopCourse c;
  double x = 0, y = 0, h = 0;  // the middle's point and heading (radians)
  c.mx.push_back(0);
  c.my.push_back(0);
  c.at.push_back(0);
  auto section_of = [&](const char* name) {
    if (c.sections.empty() || c.sections.back() != name) {
      c.sections.push_back(name);
      c.section_from.push_back(c.length);
    }
    return static_cast<int>(c.sections.size()) - 1;
  };
  auto add_point = [&](double px, double py, int sec) {
    const double dx = px - c.mx.back(), dy = py - c.my.back();
    c.length += static_cast<float>(std::sqrt(dx * dx + dy * dy));
    c.mx.push_back(static_cast<float>(px));
    c.my.push_back(static_cast<float>(py));
    c.at.push_back(c.length);
    c.seg_section.push_back(sec);
  };
  for (const HopPiece& p : hop_pieces()) {
    const int sec = section_of(p.section);
    if (p.length > 0) {
      const double ca = lm::cos(h), sa = lm::sin(h), nx = -sa, ny = ca;
      // Each straight laid 4 units longer at both ends, so the pieces' carves overlap.
      const double x0 = x - 4 * ca, y0 = y - 4 * sa;
      const double x1 = x + (p.length + 4) * ca, y1 = y + (p.length + 4) * sa;
      const double a = p.width0 / 2, b = p.width1 / 2, m = p.shift;
      HopQuad q;
      q.section = sec;
      const double qx[4] = {x0 + nx * (a + m), x0 - nx * (a - m), x1 - nx * (b - m),
                            x1 + nx * (b + m)};
      const double qy[4] = {y0 + ny * (a + m), y0 - ny * (a - m), y1 - ny * (b - m),
                            y1 + ny * (b + m)};
      for (int k = 0; k < 4; k++) {
        q.x[k] = static_cast<float>(qx[k]);
        q.y[k] = static_cast<float>(qy[k]);
      }
      c.quads.push_back(q);
      x += p.length * ca;
      y += p.length * sa;
      add_point(x, y, sec);
    } else {
      const double sgn = p.turn > 0 ? 1 : -1, d = p.turn * ge::core::kPi / 180;
      const double cx = x - sgn * p.radius * lm::sin(h), cy = y + sgn * p.radius * lm::cos(h);
      const int n = std::max(2, static_cast<int>(std::abs(p.turn) / kHopArcStep + 0.5f));
      auto at = [&](double t, double r, double& ox, double& oy) {
        ox = cx + sgn * r * lm::sin(t);
        oy = cy - sgn * r * lm::cos(t);
      };
      const double ri = p.radius - p.width0 / 2, ro = p.radius + p.width0 / 2;
      for (int i = 0; i < n; i++) {
        const double t0 = h + d * i / n, t1 = h + d * (i + 1) / n;
        double px[4], py[4];
        at(t0, ri, px[0], py[0]);
        at(t1, ri, px[1], py[1]);
        at(t1, ro, px[2], py[2]);
        at(t0, ro, px[3], py[3]);
        HopQuad q;
        q.section = sec;
        // Inner side forward, outer back: clockwise on a left turn, so a left turn's reversed.
        for (int k = 0; k < 4; k++) {
          const int s = sgn > 0 ? 3 - k : k;
          q.x[k] = static_cast<float>(px[s]);
          q.y[k] = static_cast<float>(py[s]);
        }
        c.quads.push_back(q);
        double mx = 0, my = 0;
        at(t1, p.radius, mx, my);
        add_point(mx, my, sec);
      }
      h += d;
      x = c.mx.back();
      y = c.my.back();
    }
  }
  return c;
}

// The plan's text: a room a quad, the spawn, lights along the middle every 512 units.
inline std::string hop_course_plan(const HopCourse& c) {
  using ge::core::num::to_fixed;
  std::string t =
      "# THE HOP COURSE: a bunny-hop practice map for the first game, written by\n"
      "# games/shooters_edge/labs/hop_course.hpp (the se-bhop lab's --write-plan). Build it with\n"
      "# `secli map build hop_course.plan --textures=<the game folder>`. A course of straights,\n"
      "# S-bends, corners of three radii, a room's side exit, a U-turn and a corridor that "
      "narrows;\n"
      "# no items, no harm.\n"
      "world message=\"Hop Course\" light=40 worldtype=2\n"
      "textures floor=sfloor4_6 wall=city4_6 ceil=metal1_3 solid=tech04_1\n"
      "modes ffa\n";
  for (size_t i = 0; i < c.quads.size(); i++) {
    const HopQuad& q = c.quads[i];
    t += "room p" + std::to_string(i) + " z=0.." + to_fixed(kHopCourseHigh, 0) + " poly";
    for (int k = 0; k < 4; k++)
      t += " " + std::to_string(static_cast<int>(std::lround(q.x[k]))) + "," +
           std::to_string(static_cast<int>(std::lround(q.y[k])));
    t += "\n";
  }
  t += "spawn " + to_fixed(c.spawn_x, 0) + "," + to_fixed(c.spawn_y, 0) + ",24 angle=0\n";
  for (float d = 256; d < c.length; d += 512) {
    size_t i = 1;
    while (i + 1 < c.at.size() && c.at[i] < d) i++;
    const float f = (d - c.at[i - 1]) / std::max(1.0f, c.at[i] - c.at[i - 1]);
    const float lx = c.mx[i - 1] + (c.mx[i] - c.mx[i - 1]) * f;
    const float ly = c.my[i - 1] + (c.my[i] - c.my[i - 1]) * f;
    t += "light " + std::to_string(static_cast<int>(std::lround(lx))) + "," +
         std::to_string(static_cast<int>(std::lround(ly))) + ",200 value=300\n";
  }
  return t;
}

// Where (x, y) lies along the course: the distance along the middle to its nearest point, and that
// segment's section (`section`).
inline float course_station(const HopCourse& c, float x, float y, int* section = nullptr) {
  float best = 1e30f, along = 0;
  int sec = 0;
  for (size_t i = 0; i + 1 < c.mx.size(); i++) {
    const float ax = c.mx[i], ay = c.my[i], bx = c.mx[i + 1] - ax, by = c.my[i + 1] - ay;
    const float len2 = bx * bx + by * by;
    float f = len2 > 0 ? ((x - ax) * bx + (y - ay) * by) / len2 : 0;
    f = std::clamp(f, 0.0f, 1.0f);
    const float dx = x - (ax + bx * f), dy = y - (ay + by * f);
    const float d2 = dx * dx + dy * dy;
    if (d2 < best) {
      best = d2;
      along = c.at[i] + (c.at[i + 1] - c.at[i]) * f;
      sec = c.seg_section[i];
    }
  }
  if (section) *section = sec;
  return along;
}

}  // namespace shooters_edge::labs
