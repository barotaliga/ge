// THE AREA TREE: where the server keeps every linked entity so a trace or a touch asks only the
// ones near it. A fixed tree of 31 nodes over the world's bounds, built once a map: a node at
// depth 4 is a leaf; otherwise it splits along x when its bounds are longer in x than in y (else
// along y) at their middle, child 0 the upper half and child 1 the lower. Each node keeps two
// lists, triggers and solids, as links through the entities' own `link_prev`/`link_next` (no
// allocation).
//
// LINKING (`area_link`): an entity goes into the first node, walking down from the root, whose
// plane its absolute box crosses (or the leaf it reaches), appended at the end of the trigger or
// the solid list; a `not` solid is never linked. The ORDER of the lists is part of the result:
// traces, touches and the player's list of solids visit entities in it.
//
// LISTING (`area_list`): depth first, a node's own list in order, then child 0 when the box
// reaches above the split (strictly), then child 1 when it reaches below; entities whose absolute
// box misses the asked box are skipped (touching counts as meeting), as are `not` solids.
//
// ge:summary The area tree: 31 nodes over the world, trigger and solid lists, link and list
// ge:keywords area-tree areanode link unlink area_link area_unlink area_list triggers solids
// ge:keywords spatial broadphase depth split
// ge:skill shooter
// ge:test tests/shooter_area_test.cpp
#pragma once

#include <cstdint>

#include "shooter/entity.hpp"

namespace ge::shooter {

constexpr int kAreaDepth = 4;
constexpr int kAreaNodes = 31;

struct AreaList {
  int32_t head = -1, tail = -1;  // entity numbers; -1 empty
};

struct AreaNode {
  int axis = -1;  // 0 x, 1 y; -1 a leaf
  float dist = 0;
  int32_t children[2] = {-1, -1};  // [0] the upper half, [1] the lower
  AreaList triggers, solids;
};

struct AreaTree {
  AreaNode nodes[kAreaNodes];
  int count = 0;
};

// The tree over a world's bounds.
AreaTree create_area_tree(Vec3f world_mins, Vec3f world_maxs);

// Takes entity `e` out of its node's list (nothing when it is not linked).
void area_unlink(AreaTree& tree, Edict* edicts, int e);
// Puts entity `e` (its absmin/absmax already set, its solid not `not`) at the end of the trigger
// or solid list of the first node its box crosses.
void area_link(AreaTree& tree, Edict* edicts, int e);

enum class AreaKind : uint8_t { triggers, solids };

// The entities of one kind whose absolute boxes meet (mins, maxs), in listing order, into `out`
// (at most `cap`); returns how many.
int area_list(const AreaTree& tree, const Edict* edicts, Vec3f mins, Vec3f maxs, AreaKind kind,
              int* out, int cap);

}  // namespace ge::shooter
