// The movers (movers.hpp): the entities read into doors, plats, buttons, trains and triggers, the
// world they make with the still level, and the step that sets them off and moves them.
//
// ge:summary Builds and steps a room level's movers and answers traces against them
// ge:keywords mover door plat button train trigger fire team carry trace world offset
// ge:skill rooms move
// ge:test tests/rooms_movers_test.cpp
#include "rooms/movers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/libm/libm.hpp"
#include "move/push.hpp"
#include "rooms/clip.hpp"

namespace ge::rooms {

namespace {

using move::Vec3;

constexpr double kTouchM = 0.02;  // a body this near a part's bounds touches it
constexpr double kPi = 3.14159265358979323846;

double number(const Entity& e, const char* key, double fallback) {
  const std::string& v = e.get(key);
  if (v.empty()) return fallback;
  char* end = nullptr;
  const double x = std::strtod(v.c_str(), &end);
  return end == v.c_str() ? fallback : x;
}

bool point(const Entity& e, const char* key, Vec3* out) {
  double p[3];
  if (std::sscanf(e.get(key).c_str(), "%lf %lf %lf", &p[0], &p[1], &p[2]) != 3) return false;
  *out = {p[0], p[1], p[2]};
  return true;
}

// A unit direction from `heading` (degrees clockwise from +z; -1 up, -2 down).
Vec3 direction(double heading) {
  if (heading == -1) return {0, 1, 0};
  if (heading == -2) return {0, -1, 0};
  const double r = heading * kPi / 180;
  return {core::libm::sin(r), 0, core::libm::cos(r)};
}

// How far a part of size `size` travels along unit `dir` to clear its own length, less `lip`.
double travel(const Vec3& size, const Vec3& dir, double lip) {
  return std::fabs(dir.x) * size.x + std::fabs(dir.y) * size.y + std::fabs(dir.z) * size.z - lip;
}

bool overlaps(const Vec3& alo, const Vec3& ahi, const Vec3& blo, const Vec3& bhi, double pad) {
  return alo.x <= bhi.x + pad && blo.x <= ahi.x + pad && alo.y <= bhi.y + pad &&
         blo.y <= ahi.y + pad && alo.z <= bhi.z + pad && blo.z <= ahi.z + pad;
}

// Whether the box `lo`..`hi` reaches into a convex brush: no face plane has the whole box in front.
bool box_in_brush(std::span<const HalfSpace> planes, const Vec3& lo, const Vec3& hi) {
  for (const HalfSpace& h : planes) {
    const double low = (h.n[0] >= 0 ? h.n[0] * lo.x : h.n[0] * hi.x) +
                       (h.n[1] >= 0 ? h.n[1] * lo.y : h.n[1] * hi.y) +
                       (h.n[2] >= 0 ? h.n[2] * lo.z : h.n[2] * hi.z);
    if (low > h.d) return false;
  }
  return true;
}

void body_box(const move::MoveSpec& spec, const move::MoveState& s, Vec3* lo, Vec3* hi) {
  const move::Box b = move::hull(spec, s.ducked);
  *lo = move::add(s.pos, b.mins);
  *hi = move::add(s.pos, b.maxs);
}

move::Hit trace_movers(const void* ctx, const move::Box& box, const Vec3& from, const Vec3& to) {
  const Movers& m = *static_cast<const Movers*>(ctx);
  move::Hit best = move::trace_mesh_world(m.still, box, from, to);
  const Vec3 slo{std::min(from.x, to.x) + box.mins.x, std::min(from.y, to.y) + box.mins.y,
                 std::min(from.z, to.z) + box.mins.z};
  const Vec3 shi{std::max(from.x, to.x) + box.maxs.x, std::max(from.y, to.y) + box.maxs.y,
                 std::max(from.z, to.z) + box.maxs.z};
  for (size_t i = 0; i < m.movers.size(); ++i) {
    const Mover& mv = m.movers[i];
    if (!overlaps(slo, shi, move::add(mv.lo, mv.offset), move::add(mv.hi, mv.offset), 0.1)) {
      continue;
    }
    move::Hit h =
        move::trace_mesh_world(mv.mesh, box, move::sub(from, mv.offset), move::sub(to, mv.offset));
    if (!h.touched() && !h.start_solid) continue;
    if (h.fraction < best.fraction || (h.start_solid && !best.start_solid)) {
      h.end = move::add(h.end, mv.offset);
      h.solid = uint32_t(i + 1);
      h.surface_vel = mv.vel;
      best = h;
    }
  }
  return best;
}

// The mover's next stop: where its phase is taking it.
Vec3 goal(const Mover& mv) {
  if (mv.kind == MoverKind::train) return mv.path[mv.leg];
  return mv.phase == Phase::going ? mv.pos2 : mv.pos1;
}

void set_going(Movers& m, size_t i) {
  Mover& mv = m.movers[i];
  mv.phase = Phase::going;
  m.events.push_back({MoverEventKind::started, mv.entity});
}

void set_coming(Movers& m, size_t i) {
  Mover& mv = m.movers[i];
  mv.phase = Phase::coming;
  m.events.push_back({MoverEventKind::started, mv.entity});
}

// A door set off (by a touch or a name), with every door of its team.
void open_door(Movers& m, size_t i, bool toggle) {
  const int32_t team = m.movers[i].team;
  for (size_t j = 0; j < m.movers.size(); ++j) {
    if (j != i && (team < 0 || m.movers[j].team != team)) continue;
    const Phase p = m.movers[j].phase;
    if (p == Phase::rest || p == Phase::coming) set_going(m, j);
    if (toggle && p == Phase::out) set_coming(m, j);
  }
}

// Mover `i` set off by a touch, a rider or a name.
void activate(Movers& m, size_t i, bool fired) {
  Mover& mv = m.movers[i];
  switch (mv.kind) {
    case MoverKind::door:
      open_door(m, i, fired);
      break;
    case MoverKind::plat:
      if (mv.phase == Phase::rest) set_going(m, i);
      if (fired && mv.phase == Phase::out && mv.timer < 0) set_coming(m, i), mv.timer = 0;
      break;
    case MoverKind::button:
      if (mv.phase == Phase::rest) set_going(m, i);
      break;
    case MoverKind::train:
      if (mv.phase == Phase::rest) set_going(m, i);
      break;
  }
}

}  // namespace

void fire(Movers& m, const std::string& name) {
  if (name.empty()) return;
  for (size_t i = 0; i < m.movers.size(); ++i) {
    if (m.names[i] == name) activate(m, i, true);
  }
}

move::World movers_world(const Movers& m) { return {&m, &trace_movers}; }

bool build_movers(const Level& level, size_t workers, Movers* out, std::string* error) {
  Movers m;
  m.still = clip_world(level, workers, 1.0, false);
  const auto fail = [&](const std::string& what) {
    if (error) *error = what;
    return false;
  };
  for (size_t e = 0; e < level.entities.size(); ++e) {
    const Entity& ent = level.entities[e];
    const std::string& c = ent.classname;
    if (ent.group == kWorld || ent.group >= level.groups.size()) continue;
    const std::string where =
        "entity " + std::to_string(e) + " (" + c + ", group " + level.groups[ent.group].name + ")";
    if (c == "trigger_multiple" || c == "trigger_once") {
      Trigger t;
      t.entity = uint32_t(e);
      t.once = c == "trigger_once";
      t.wait = number(ent, "wait", 0.2);
      for (const Brush& b : level.brushes) {
        if (b.group != ent.group) continue;
        t.brush.push_back(uint32_t(t.planes.size()));
        for (uint32_t f = b.first_face; f < b.first_face + b.faces; ++f) {
          const Face& fc = level.faces[f];
          t.planes.push_back({{fc.n[0], fc.n[1], fc.n[2]}, fc.d});
        }
      }
      t.brush.push_back(uint32_t(t.planes.size()));
      if (t.brush.size() < 2) return fail(where + ": the group has no brush; give it one");
      m.triggers.push_back(std::move(t));
      continue;
    }
    MoverKind kind;
    if (c == "func_door") {
      kind = MoverKind::door;
    } else if (c == "func_plat") {
      kind = MoverKind::plat;
    } else if (c == "func_button") {
      kind = MoverKind::button;
    } else if (c == "func_train") {
      kind = MoverKind::train;
    } else {
      ++m.skipped;
      continue;
    }
    Mover mv;
    mv.kind = kind;
    mv.group = ent.group;
    mv.entity = uint32_t(e);
    mv.mesh = clip_group(level, ent.group, workers);
    if (mv.mesh.tri.empty()) return fail(where + ": no closed brush in the group; close it");
    mv.lo = mv.hi = mv.mesh.tri[0];
    for (const Vec3& p : mv.mesh.tri) {
      mv.lo = {std::min(mv.lo.x, p.x), std::min(mv.lo.y, p.y), std::min(mv.lo.z, p.z)};
      mv.hi = {std::max(mv.hi.x, p.x), std::max(mv.hi.y, p.y), std::max(mv.hi.z, p.z)};
    }
    const Vec3 size = move::sub(mv.hi, mv.lo);
    mv.named = !ent.get("targetname").empty();
    const double heading = number(ent, "heading", 0);
    switch (kind) {
      case MoverKind::door: {
        mv.speed = number(ent, "speed", 3.125);
        mv.wait = number(ent, "wait", 3);
        const Vec3 dir = direction(heading);
        mv.pos2 = move::scale(dir, travel(size, dir, number(ent, "lip", 0.25)));
        break;
      }
      case MoverKind::button: {
        mv.speed = number(ent, "speed", 1.25);
        mv.wait = number(ent, "wait", 1);
        const Vec3 dir = direction(heading);
        mv.pos2 = move::scale(dir, travel(size, dir, number(ent, "lip", 0.125)));
        break;
      }
      case MoverKind::plat: {
        mv.speed = number(ent, "speed", 4.6875);
        mv.wait = 3;
        const double h = number(ent, "height", size.y - 0.25);
        mv.pos1 = {0, -h, 0};
        mv.pos2 = {0, 0, 0};
        if (mv.named) {
          mv.phase = Phase::out;
          mv.timer = -1;  // held up until fired
        } else {
          mv.offset = mv.pos1;
        }
        break;
      }
      case MoverKind::train: {
        mv.speed = number(ent, "speed", 3.125);
        // The corners, along their target chain from the train's own target, until it loops.
        std::string next = ent.get("target");
        while (!next.empty()) {
          const Entity* corner = nullptr;
          for (const Entity& o : level.entities) {
            if (o.classname == "path_corner" && o.get("targetname") == next) corner = &o;
          }
          if (!corner) return fail(where + ": no path_corner named '" + next + "'; name one");
          Vec3 at;
          if (!point(*corner, "origin", &at)) {
            return fail(where + ": path_corner '" + next + "' has no origin; give it one");
          }
          mv.path.push_back(move::sub(at, mv.lo));
          mv.path_wait.push_back(number(*corner, "wait", 0));
          next = corner->get("target");
          if (next == ent.get("target") || mv.path.size() > 4096) break;
        }
        if (mv.path.empty()) return fail(where + ": no target to follow; name its first corner");
        mv.offset = mv.path[0];
        mv.leg = mv.path.size() > 1 ? 1 : 0;
        if (!mv.named) mv.phase = Phase::going;
        break;
      }
    }
    m.movers.push_back(std::move(mv));
    m.names.push_back(ent.get("targetname"));
  }
  for (const Mover& mv : m.movers) m.targets.push_back(level.entities[mv.entity].get("target"));
  for (const Trigger& t : m.triggers) m.targets.push_back(level.entities[t.entity].get("target"));
  // Doors that touch each other move as one: each joins the team of the first it touches.
  for (size_t i = 0; i < m.movers.size(); ++i) {
    Mover& a = m.movers[i];
    if (a.kind != MoverKind::door) continue;
    for (size_t j = 0; j < i; ++j) {
      const Mover& b = m.movers[j];
      if (b.kind != MoverKind::door || !overlaps(a.lo, a.hi, b.lo, b.hi, 0.01)) continue;
      const int32_t team = b.team >= 0 ? b.team : int32_t(j);
      m.movers[j].team = team;
      if (a.team >= 0 && a.team != team) {
        const int32_t old = a.team;
        for (Mover& o : m.movers) {
          if (o.team == old) o.team = team;
        }
      }
      a.team = team;
    }
  }
  *out = std::move(m);
  return true;
}

void step_movers(Movers& m, const move::MoveSpec& spec, std::span<move::MoveState> bodies,
                 double dt) {
  m.events.clear();
  const size_t nm = m.movers.size();
  // Triggers: a body inside fires the target.
  for (size_t t = 0; t < m.triggers.size(); ++t) {
    Trigger& tr = m.triggers[t];
    if (tr.spent) continue;
    tr.timer = std::max(0.0, tr.timer - dt);
    if (tr.timer > 0) continue;
    bool hit = false;
    for (const move::MoveState& s : bodies) {
      Vec3 lo, hi;
      body_box(spec, s, &lo, &hi);
      for (size_t b = 0; b + 1 < tr.brush.size() && !hit; ++b) {
        hit = box_in_brush(std::span(tr.planes).subspan(tr.brush[b], tr.brush[b + 1] - tr.brush[b]),
                           lo, hi);
      }
      if (hit) break;
    }
    if (!hit) continue;
    m.events.push_back({MoverEventKind::fired, tr.entity});
    fire(m, m.targets[nm + t]);
    tr.timer = tr.wait;
    tr.spent = tr.once;
  }
  // Touches and riders.
  for (size_t i = 0; i < nm; ++i) {
    Mover& mv = m.movers[i];
    if (mv.kind == MoverKind::train || (mv.kind == MoverKind::door && mv.named)) continue;
    const Vec3 lo = move::add(mv.lo, mv.offset), hi = move::add(mv.hi, mv.offset);
    bool touched = false;
    for (const move::MoveState& s : bodies) {
      Vec3 blo, bhi;
      body_box(spec, s, &blo, &bhi);
      touched = touched || s.ground_solid == i + 1 || overlaps(blo, bhi, lo, hi, kTouchM);
    }
    if (!touched) continue;
    if (mv.kind == MoverKind::plat && mv.phase == Phase::out && mv.timer >= 0) {
      mv.timer = std::max(mv.timer, 1.0);  // a plat with a rider stays up
    } else if (!(mv.kind == MoverKind::plat && mv.named)) {
      activate(m, i, false);
    }
  }
  // Motion.
  const move::World world = movers_world(m);
  for (size_t i = 0; i < nm; ++i) {
    Mover& mv = m.movers[i];
    mv.vel = {0, 0, 0};
    if (mv.phase == Phase::out || (mv.kind == MoverKind::train && mv.phase == Phase::rest)) {
      if (mv.timer < 0) continue;  // held
      mv.timer -= dt;
      if (mv.timer > 0) continue;
      if (mv.kind == MoverKind::train) {
        mv.leg = (mv.leg + 1) % mv.path.size();
        if (mv.phase == Phase::out) set_going(m, i);
      } else if (mv.kind == MoverKind::door) {
        if (mv.wait < 0) {
          mv.timer = -1;
          continue;
        }
        for (size_t j = 0; j < nm; ++j) {
          if (j == i || (mv.team >= 0 && m.movers[j].team == mv.team)) {
            if (m.movers[j].phase == Phase::out) set_coming(m, j);
          }
        }
      } else {
        set_coming(m, i);
      }
      continue;
    }
    if (mv.phase == Phase::rest) continue;
    const Vec3 to = goal(mv);
    const Vec3 left = move::sub(to, mv.offset);
    const double dist = move::length(left);
    const double reach = mv.speed * dt;
    const bool arrive = dist <= reach;
    const Vec3 delta = arrive ? left : move::scale(left, reach / dist);
    mv.offset = move::add(mv.offset, delta);
    mv.vel = move::scale(delta, 1.0 / dt);
    bool blocked = false;
    for (move::MoveState& s : bodies) {
      if (!move::carry(spec, world, s, uint32_t(i + 1), delta)) blocked = true;
    }
    if (blocked) {
      mv.offset = move::sub(mv.offset, delta);
      mv.vel = {0, 0, 0};
      // A closing door or a falling plat goes back the way it came; anything else waits.
      if (mv.phase == Phase::coming && mv.kind == MoverKind::door) open_door(m, i, false);
      if (mv.phase == Phase::coming && mv.kind == MoverKind::plat) set_going(m, i);
      continue;
    }
    if (!arrive) continue;
    m.events.push_back({MoverEventKind::stopped, mv.entity});
    if (mv.kind == MoverKind::train) {
      mv.phase = Phase::out;
      mv.timer = mv.path_wait[mv.leg];
      if (mv.timer < 0) mv.timer = -1;
    } else if (mv.phase == Phase::going) {
      mv.phase = Phase::out;
      mv.timer = mv.wait < 0 ? -1 : mv.wait;
      if (mv.kind == MoverKind::button) {
        m.events.push_back({MoverEventKind::fired, mv.entity});
        fire(m, m.targets[i]);
      }
    } else {
      mv.phase = Phase::rest;
    }
  }
}

}  // namespace ge::rooms
