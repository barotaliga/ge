// The area tree's build, its linked lists and the depth-first listing (area.hpp states the rules).
//
// ge:summary The area tree's node build, list links and depth-first box listing
// ge:keywords create_area_tree area_link area_unlink area_list node split list
// ge:skill shooter
// ge:test tests/shooter_area_test.cpp
#include "shooter/area.hpp"

namespace ge::shooter {

namespace {

int32_t build(AreaTree& t, int depth, Vec3f mins, Vec3f maxs) {
  const auto n = static_cast<int32_t>(t.count++);
  AreaNode& node = t.nodes[n];
  if (depth == kAreaDepth) {
    node.axis = -1;
    return n;
  }
  const Vec3f size = maxs - mins;
  node.axis = size.x > size.y ? 0 : 1;
  node.dist = 0.5f * (maxs[node.axis] + mins[node.axis]);
  Vec3f mins1 = mins, maxs1 = maxs, mins2 = mins, maxs2 = maxs;
  maxs1[node.axis] = node.dist;  // the lower half
  mins2[node.axis] = node.dist;  // the upper half
  const int32_t upper = build(t, depth + 1, mins2, maxs2);
  const int32_t lower = build(t, depth + 1, mins1, maxs1);
  t.nodes[n].children[0] = upper;
  t.nodes[n].children[1] = lower;
  return n;
}

bool meets(const Edict& e, Vec3f mins, Vec3f maxs) {
  return !(e.absmin.x > maxs.x || e.absmin.y > maxs.y || e.absmin.z > maxs.z ||
           e.absmax.x < mins.x || e.absmax.y < mins.y || e.absmax.z < mins.z);
}

void list_node(const AreaTree& t, int32_t n, const Edict* edicts, Vec3f mins, Vec3f maxs,
               AreaKind kind, int* out, int cap, int& count) {
  const AreaNode& node = t.nodes[n];
  const AreaList& l = kind == AreaKind::triggers ? node.triggers : node.solids;
  for (int32_t e = l.head; e != -1; e = edicts[e].link_next) {
    const Edict& ed = edicts[e];
    if (ed.solid == kSolidNot || !meets(ed, mins, maxs)) continue;
    if (count < cap) out[count++] = e;
  }
  if (node.axis < 0) return;
  if (maxs[node.axis] > node.dist)
    list_node(t, node.children[0], edicts, mins, maxs, kind, out, cap, count);
  if (mins[node.axis] < node.dist)
    list_node(t, node.children[1], edicts, mins, maxs, kind, out, cap, count);
}

}  // namespace

AreaTree create_area_tree(Vec3f world_mins, Vec3f world_maxs) {
  AreaTree t;
  build(t, 0, world_mins, world_maxs);
  return t;
}

void area_unlink(AreaTree& t, Edict* edicts, int e) {
  Edict& ed = edicts[e];
  if (ed.area_node < 0) return;
  AreaNode& node = t.nodes[ed.area_node];
  AreaList& l = ed.in_triggers ? node.triggers : node.solids;
  if (ed.link_prev != -1)
    edicts[ed.link_prev].link_next = ed.link_next;
  else
    l.head = ed.link_next;
  if (ed.link_next != -1)
    edicts[ed.link_next].link_prev = ed.link_prev;
  else
    l.tail = ed.link_prev;
  ed.area_node = -1;
  ed.link_prev = ed.link_next = -1;
}

void area_link(AreaTree& t, Edict* edicts, int e) {
  Edict& ed = edicts[e];
  int32_t n = 0;
  for (;;) {
    const AreaNode& node = t.nodes[n];
    if (node.axis < 0) break;
    if (ed.absmin[node.axis] > node.dist)
      n = node.children[0];
    else if (ed.absmax[node.axis] < node.dist)
      n = node.children[1];
    else
      break;  // the box crosses this node's plane
  }
  ed.in_triggers = ed.solid == kSolidTrigger;
  AreaList& l = ed.in_triggers ? t.nodes[n].triggers : t.nodes[n].solids;
  ed.area_node = n;
  ed.link_next = -1;
  ed.link_prev = l.tail;
  if (l.tail != -1)
    edicts[l.tail].link_next = e;
  else
    l.head = e;
  l.tail = e;
}

int area_list(const AreaTree& t, const Edict* edicts, Vec3f mins, Vec3f maxs, AreaKind kind,
              int* out, int cap) {
  int count = 0;
  list_node(t, 0, edicts, mins, maxs, kind, out, cap, count);
  return count;
}

}  // namespace ge::shooter
