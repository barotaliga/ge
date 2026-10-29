// THE AREA TREE: 31 nodes four levels deep, each split across its longer side at the middle; a
// box sinks to the deepest node whose plane it does not cross; a list query finds what overlaps
// and nothing else, triggers and solids apart; unlinking takes a box out of its node's list
// wherever it sits in it.
//
// ge:summary Tests: the area tree's shape, linking depth, list queries and unlinking
// ge:keywords test area area-tree area_link area_list area_unlink node split
// ge:skill shooter testing
#include <algorithm>
#include <vector>

#include "check.hpp"
#include "shooter/area.hpp"
#include "shooter/entity.hpp"

namespace sh = ge::shooter;

namespace {

struct Pool {
  sh::AreaTree tree = sh::create_area_tree({-1024, -512, -64}, {1024, 512, 256});
  std::vector<sh::Edict> edicts = std::vector<sh::Edict>(16);

  int put(int e, sh::Vec3f lo, sh::Vec3f hi, int solid) {
    sh::Edict& ed = edicts[static_cast<size_t>(e)];
    ed.free = false;
    ed.solid = solid;
    ed.absmin = lo;
    ed.absmax = hi;
    sh::area_unlink(tree, edicts.data(), e);
    sh::area_link(tree, edicts.data(), e);
    return ed.area_node;
  }

  std::vector<int> list(sh::Vec3f lo, sh::Vec3f hi, sh::AreaKind kind) {
    int out[16];
    const int n = sh::area_list(tree, edicts.data(), lo, hi, kind, out, 16);
    std::vector<int> v(out, out + n);
    std::sort(v.begin(), v.end());
    return v;
  }
};

}  // namespace

TEST(the_tree_has_31_nodes_split_across_the_longer_side) {
  const Pool p;
  CHECK(p.tree.count == sh::kAreaNodes);
  CHECK(p.tree.nodes[0].axis == 0);  // 2048 wide in x, 1024 in y
  CHECK(p.tree.nodes[0].dist == 0);
  // The upper half (x 0..1024, y -512..512) is square: it splits across y.
  const sh::AreaNode& upper = p.tree.nodes[p.tree.nodes[0].children[0]];
  CHECK(upper.axis == 1);
  CHECK(upper.dist == 0);
  int leaves = 0;
  for (int i = 0; i < p.tree.count; i++)
    if (p.tree.nodes[i].axis < 0) leaves++;
  CHECK(leaves == 16);
}

TEST(a_box_sinks_to_the_deepest_node_it_does_not_cross) {
  Pool p;
  CHECK(p.put(1, {-8, -8, 0}, {8, 8, 16}, sh::kSolidBbox) == 0);  // crosses x = 0
  const int deep = p.put(2, {700, 300, 0}, {716, 316, 16}, sh::kSolidBbox);
  CHECK(deep != 0);
  CHECK(p.tree.nodes[deep].axis < 0);  // a leaf
  // A box touching a plane exactly (absmax == dist) stays above it: neither strict test holds.
  CHECK(p.put(3, {-16, 100, 0}, {0, 116, 16}, sh::kSolidBbox) == 0);
}

TEST(a_list_finds_what_overlaps_triggers_and_solids_apart) {
  Pool p;
  p.put(1, {-8, -8, 0}, {8, 8, 16}, sh::kSolidBbox);
  p.put(2, {700, 300, 0}, {716, 316, 16}, sh::kSolidBbox);
  p.put(3, {690, 290, 0}, {720, 320, 64}, sh::kSolidTrigger);
  p.put(4, {-900, -400, 0}, {-880, -380, 16}, sh::kSolidSlidebox);
  CHECK((p.list({600, 200, 0}, {800, 400, 32}, sh::AreaKind::solids) == std::vector<int>{2}));
  CHECK((p.list({600, 200, 0}, {800, 400, 32}, sh::AreaKind::triggers) == std::vector<int>{3}));
  CHECK((p.list({-1024, -512, -64}, {1024, 512, 256}, sh::AreaKind::solids) ==
         std::vector<int>{1, 2, 4}));
  // Touching faces overlap; a gap of any size does not.
  CHECK((p.list({716, 316, 16}, {800, 400, 32}, sh::AreaKind::solids) == std::vector<int>{2}));
  CHECK(p.list({716.5f, 316, 16}, {800, 400, 32}, sh::AreaKind::solids).empty());
}

TEST(unlinking_takes_a_box_out_from_anywhere_in_its_list) {
  Pool p;
  for (int e = 1; e <= 4; e++) p.put(e, {-8, -8, 0}, {8, 8, 16}, sh::kSolidBbox);
  sh::area_unlink(p.tree, p.edicts.data(), 2);  // the middle
  CHECK((p.list({-8, -8, 0}, {8, 8, 16}, sh::AreaKind::solids) == std::vector<int>{1, 3, 4}));
  sh::area_unlink(p.tree, p.edicts.data(), 1);  // the head
  sh::area_unlink(p.tree, p.edicts.data(), 4);  // the tail
  CHECK((p.list({-8, -8, 0}, {8, 8, 16}, sh::AreaKind::solids) == std::vector<int>{3}));
  CHECK(p.edicts[1].area_node == -1);
  sh::area_unlink(p.tree, p.edicts.data(), 1);  // already out: nothing happens
  sh::area_unlink(p.tree, p.edicts.data(), 3);
  CHECK(p.tree.nodes[0].solids.head == -1);
  CHECK(p.tree.nodes[0].solids.tail == -1);
}

TEST(an_unsolid_box_left_in_a_list_is_never_listed) {
  Pool p;
  p.put(1, {-8, -8, 0}, {8, 8, 16}, sh::kSolidBbox);
  p.edicts[1].solid = sh::kSolidNot;
  CHECK(p.list({-8, -8, 0}, {8, 8, 16}, sh::AreaKind::solids).empty());
}
