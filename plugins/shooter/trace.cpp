// The world sweep: the hull for each entity and the walk over the area tree's solids (trace.hpp
// states the rules).
//
// ge:summary The world sweep's hull choice, box hulls and the area tree walk over solids
// ge:keywords trace_move clip_to_entity hull-offset clip-links walk test_entity_position
// ge:skill shooter
// ge:test tests/shooter_trace_test.cpp
#include "shooter/trace.hpp"

namespace ge::shooter {

namespace {

constexpr float kMissileHalf = 15;  // a missile meets a monster as this cube's half width

struct Clip {
  Vec3f start, end;
  Vec3f mins, maxs;    // the mover's box
  Vec3f mins2, maxs2;  // the box met against monsters (a missile's cube, else the same)
  Vec3f boxmins, boxmaxs;
  int kind = kTraceNormal;
  int passent = -1;
  HullTrace best;
};

void clip_links(const Server& sv, int32_t n, Clip& c) {
  const AreaNode& node = sv.area.nodes[n];
  for (int32_t t = node.solids.head; t != -1; t = sv.edicts[static_cast<size_t>(t)].link_next) {
    const Edict& touch = sv.edicts[static_cast<size_t>(t)];
    if (touch.solid == kSolidNot || touch.solid == kSolidTrigger || t == c.passent) continue;
    if (c.kind == kTraceNoMonsters && touch.solid != kSolidBsp) continue;
    if (c.boxmins.x > touch.absmax.x || c.boxmins.y > touch.absmax.y ||
        c.boxmins.z > touch.absmax.z || c.boxmaxs.x < touch.absmin.x ||
        c.boxmaxs.y < touch.absmin.y || c.boxmaxs.z < touch.absmin.z)
      continue;
    if (c.passent >= 0) {
      const Edict& pass = sv.edicts[static_cast<size_t>(c.passent)];
      if (pass.size.x != 0 && touch.size.x == 0) continue;  // points never interact
      if (touch.owner == c.passent || pass.owner == t) continue;
    }
    if (c.best.allsolid) return;
    const bool monster = (touch.flags & kFlagMonster) != 0;
    const HullTrace tr = monster ? clip_to_entity(sv, t, c.start, c.mins2, c.maxs2, c.end)
                                 : clip_to_entity(sv, t, c.start, c.mins, c.maxs, c.end);
    if (tr.allsolid || tr.startsolid || tr.fraction < c.best.fraction) {
      const bool was_startsolid = c.best.startsolid;
      if (tr.allsolid || tr.fraction < c.best.fraction) {
        c.best = tr;
        c.best.ent = t;
      }
      if (was_startsolid || tr.startsolid) c.best.startsolid = true;
    }
  }
  if (node.axis < 0) return;
  if (c.boxmaxs[node.axis] > node.dist) clip_links(sv, node.children[0], c);
  if (c.boxmins[node.axis] < node.dist) clip_links(sv, node.children[1], c);
}

}  // namespace

HullTrace clip_to_entity(const Server& sv, int ent, Vec3f start, Vec3f mins, Vec3f maxs,
                         Vec3f end) {
  const Edict& e = sv.edicts[static_cast<size_t>(ent)];
  Vec3f offset;
  HullTrace tr;
  if (e.solid == kSolidBsp) {
    const BrushModel& m = sv.models[static_cast<size_t>(e.modelindex)];
    const float width = maxs.x - mins.x;
    const int h = width < 3 ? 0 : width <= 32 ? 1 : 2;
    offset = m.clip_mins[h] - mins + e.origin;
    tr = trace_hull(m.hulls[h], start - offset, end - offset);
  } else {
    const BoxHull box = make_box_hull(e.mins - maxs, e.maxs - mins);
    offset = e.origin;
    tr = trace_hull(box.hull(), start - offset, end - offset);
  }
  if (tr.allsolid)
    tr.endpos = start;
  else if (tr.fraction == 1)
    tr.endpos = end;
  else
    tr.endpos = tr.endpos + offset;
  tr.ent = tr.fraction < 1 || tr.startsolid ? ent : -1;
  return tr;
}

HullTrace trace_move(const Server& sv, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end, int kind,
                     int passent) {
  Clip c;
  c.start = start;
  c.end = end;
  c.mins = mins;
  c.maxs = maxs;
  c.kind = kind;
  c.passent = passent;
  if (kind == kTraceMissile) {
    c.mins2 = {-kMissileHalf, -kMissileHalf, -kMissileHalf};
    c.maxs2 = {kMissileHalf, kMissileHalf, kMissileHalf};
  } else {
    c.mins2 = mins;
    c.maxs2 = maxs;
  }
  c.best = clip_to_entity(sv, 0, start, mins, maxs, end);
  if (c.best.allsolid) return c.best;

  for (int i = 0; i < 3; i++) {
    const float lo = start[i] < end[i] ? start[i] : end[i];
    const float hi = start[i] < end[i] ? end[i] : start[i];
    const float bmin = mins[i] < c.mins2[i] ? mins[i] : c.mins2[i];
    const float bmax = maxs[i] > c.maxs2[i] ? maxs[i] : c.maxs2[i];
    c.boxmins[i] = lo + bmin - 1;
    c.boxmaxs[i] = hi + bmax + 1;
  }
  clip_links(sv, 0, c);
  return c.best;
}

bool test_entity_position(const Server& sv, int e) {
  const Edict& ed = sv.edicts[static_cast<size_t>(e)];
  const int kind = ed.solid == kSolidTrigger || ed.solid == kSolidNot ? kTraceNoMonsters
                                                                       : kTraceNormal;
  const HullTrace tr = trace_move(sv, ed.origin, ed.mins, ed.maxs, ed.origin, kind, e);
  return tr.startsolid;
}

}  // namespace ge::shooter
